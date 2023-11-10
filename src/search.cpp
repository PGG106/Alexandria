#include "search.h"
#include "history.h"
#include "piece_data.h"
#include "attack.h"
#include "eval.h"
#include "magic.h"
#include "makemove.h"
#include "misc.h"
#include "threads.h"
#include "movepicker.h"
#include "ttable.h"
#include <cassert>
#include "movegen.h"
#include "time_manager.h"
#include "io.h"
#include <iostream>
#include <algorithm>

// Returns true if the position is a 2-fold repetition, false otherwise
static bool IsRepetition(const S_Board* pos, const bool pvNode) {
	assert(pos->hisPly >= pos->fiftyMove);
	int counter = 1;
	// we only need to check for repetition the moves since the last 50mr reset
	for (int index = std::max(static_cast<int>(pos->played_positions.size()) - pos->Get50mrCounter(), 0);
		index < static_cast<int>(pos->played_positions.size()); index++)
		// if we found the same position hashkey as the current position
		if (pos->played_positions[index] == pos->posKey) {
			// we found a repetition
			counter++;
			if(counter >= 2 + pvNode) return true;
		}

	return false;
}

// Returns true if the position is a draw via the 50mr rule
static bool Is50MrDraw(S_Board* pos) {
	if(pos->Get50mrCounter() >= 100){
	// If there's no risk we are being checkmated return true
	if (!pos->checkers)
		return true;
	// if we are in check make sure it's not checkmate 
	S_MOVELIST move_list[1];
	// generate moves
	GenerateMoves(move_list, pos);
	return move_list->count > 0;
	}
	return false;
}

// If we triggered any of the rules that forces a draw or we know the position is a draw return a draw score
bool IsDraw(S_Board* pos, const bool pvNode) {
	// if it's a 3-fold repetition, the fifty moves rule kicked in or there isn't enough material on the board to give checkmate then it's a draw
	return IsRepetition(pos, pvNode)
		|| Is50MrDraw(pos)
		|| MaterialDraw(pos);
}

// ClearForSearch handles the cleaning of the post and the info parameters to start search from a clean state
void ClearForSearch(S_ThreadData* td) {
	// Extract data structures from ThreadData
	S_SearchINFO* info = &td->info;
	PvTable* pv_table = &td->pv_table;

	// Clean the Pv array
	std::memset(pv_table, 0, sizeof(td->pv_table));

	// Clean the node table
	std::memset(td->nodeSpentTable, 0, sizeof(td->nodeSpentTable));
	// Reset plies and search info
	info->starttime = GetTimeMs();
	info->nodes = 0;
	info->seldepth = 0;
	// Main thread only unpauses any eventual search thread
	if (td->id == 0) {
		for (auto& helper_thread : threads_data) {
			helper_thread.info.stopped = false;
		}
	}
}

// returns a bitboard of all the attacks to a specific square
static inline Bitboard AttacksTo(const S_Board* pos, int to, Bitboard occ) {
	Bitboard attackingBishops = GetPieceBB(pos, BISHOP) | GetPieceBB(pos, QUEEN);
	Bitboard attackingRooks = GetPieceBB(pos, ROOK) | GetPieceBB(pos, QUEEN);

	return (pawn_attacks[WHITE][to] & pos->GetPieceColorBB(PAWN, BLACK))
		| (pawn_attacks[BLACK][to] & pos->GetPieceColorBB(PAWN, WHITE))
		| (knight_attacks[to] & GetPieceBB(pos, KNIGHT))
		| (king_attacks[to] & GetPieceBB(pos, KING))
		| (GetBishopAttacks(to, occ) & attackingBishops)
		| (GetRookAttacks(to, occ) & attackingRooks);
}

// inspired by the Weiss engine
bool SEE(const S_Board* pos, const int move, const int threshold) {
	if (isPromo(move)) return true;

	int to = To(move);
	int from = From(move);

	int target = pos->PieceOn(to);
	// Making the move and not losing it must beat the threshold
	int value = PieceValue[target] - threshold;

	if (value < 0)
		return false;

	int attacker = pos->PieceOn(from);
	// Trivial if we still beat the threshold after losing the piece
	value -= PieceValue[attacker];

	if (value >= 0)
		return true;

	// It doesn't matter if the to square is occupied or not
	Bitboard occupied = pos->Occupancy(BOTH) ^ (1ULL << from) ^ (1ULL << to);
	Bitboard attackers = AttacksTo(pos, to, occupied);

	Bitboard bishops = GetPieceBB(pos, BISHOP) | GetPieceBB(pos, QUEEN);
	Bitboard rooks = GetPieceBB(pos, ROOK) | GetPieceBB(pos, QUEEN);

	int side = !Color[attacker];

	// Make captures until one side runs out, or fail to beat threshold
	while (true) {
		// Remove used pieces from attackers
		attackers &= occupied;

		Bitboard myAttackers = attackers & pos->Occupancy(side);
		if (!myAttackers) {
			break;
		}

		// Pick next least valuable piece to capture with
		int pt;
		for (pt = PAWN; pt < KING; ++pt) {
			if (myAttackers & GetPieceBB(pos, pt))
				break;
		}

		side ^= 1;

		value = -value - 1 - PieceValue[pt];

		// Value beats threshold, or can't beat threshold (negamaxed)
		if (value >= 0) {
			if (pt == KING && (attackers & pos->Occupancy(side)))
				side ^= 1;

			break;
		}
		// Remove the used piece from occupied
		occupied ^= (1ULL << (GetLsbIndex(myAttackers & pos->GetPieceColorBB( pt,side^1))));

		if (pt == PAWN || pt == BISHOP || pt == QUEEN)
			attackers |= GetBishopAttacks(to, occupied) & bishops;
		if (pt == ROOK || pt == QUEEN)
			attackers |= GetRookAttacks(to, occupied) & rooks;
	}

	return side != Color[attacker];
}

// score_moves takes a list of move as an argument and assigns a score to each move
static inline void score_moves(S_Board* pos, Search_data* sd, Search_stack* ss, S_MOVELIST* move_list, int PvMove) {
	// Loop through all the move in the movelist
	for (int i = 0; i < move_list->count; i++) {
		int move = move_list->moves[i].move;
		// If the move is from the TT (aka it's our hashmove) give it the highest score
		if (move == PvMove) {
			move_list->moves[i].score = INT32_MAX - 100;
			continue;
		}
		// Sort promotions based on the promoted piece type
		else if (isPromo(move)) {
			switch (getPromotedPiecetype(move)) {
			case QUEEN:
				move_list->moves[i].score = queenPromotionScore;
				break;
			case KNIGHT:
				move_list->moves[i].score = knightPromotionScore;
				break;
			case ROOK:
				move_list->moves[i].score = badPromotionScore;
				break;
			case BISHOP:
				move_list->moves[i].score = badPromotionScore;
				break;
			default:
				break;
			}
		}
		else if (IsCapture(move)) {
			// Good captures get played before any move that isn't a promotion or a TT move
			if (SEE(pos, move, -107)) {
				int captured_piece = isEnpassant(move) ? PAWN : GetPieceType(pos->PieceOn(To(move)));
				// Sort by most valuable victim and capthist, with LVA as tiebreaks
				move_list->moves[i].score = mvv_lva[GetPieceType(Piece(move))][captured_piece] + GetCapthistScore(pos, sd, move) + goodCaptureScore;
			}
			// Bad captures are always played last, no matter how bad the history score of a move is, it will never be played after a bad capture
			else {
				int captured_piece = isEnpassant(move) ? PAWN : GetPieceType(pos->PieceOn(To(move)));
				// Sort by most valuable victim and capthist, with LVA as tiebreaks
				move_list->moves[i].score = badCaptureScore + mvv_lva[GetPieceType(Piece(move))][captured_piece] + GetCapthistScore(pos, sd, move);
			}
			continue;
		}
		// First killer move always comes after the TT move,the promotions and the good captures and before anything else
		else if (ss->searchKillers[0] == move) {
			move_list->moves[i].score = killerMoveScore0;
			continue;
		}
		// Second killer move always comes after the first one
		else if (ss->searchKillers[1] == move) {
			move_list->moves[i].score = killerMoveScore1;
			continue;
		}
		// After the killer moves try the Counter moves
		else if (move == sd->CounterMoves[From(ss->move)][To(ss->move)]) {
			move_list->moves[i].score = counterMoveScore;
			continue;
		}
		// if the move isn't in any of the previous categories score it according to the history heuristic
		else {
			move_list->moves[i].score = GetHistoryScore(pos, sd, move, ss);
			continue;
		}
	}
}

int GetBestMove(const PvTable* pv_table) {
	return pv_table->pvArray[0][0];
}

// Starts the search process, this is ideally the point where you can start a multithreaded search
void RootSearch(int depth, S_ThreadData* td, S_UciOptions* options) {
	// Init a thread_data object for each helper thread that doesn't have one already
	for (int i = threads_data.size(); i < options->Threads - 1; i++) {
		threads_data.emplace_back();
		threads_data.back().id = i + 1;
	}

	// Init thread_data objects
	for (size_t i = 0; i < threads_data.size(); i++) {
		threads_data[i].info = td->info;
		threads_data[i].pos = td->pos;
	}

	// Start Threads-1 helper search threads
	for (int i = 0; i < options->Threads - 1; i++) {
		threads.emplace_back(SearchPosition, 1, depth, &threads_data[i], options);
	}
	// MainThread search
	SearchPosition(1, depth, td, options);
	// Stop helper threads before returning the best move
	StopHelperThreads();
	// Print final bestmove found
	std::cout << "bestmove ";
	PrintMove(GetBestMove(&td->pv_table));
	std::cout << "\n";
}

// SearchPosition is the actual function that handles the search, it sets up the variables needed for the search , calls the AspirationWindowSearch function and handles the console output
void SearchPosition(int start_depth, int final_depth, S_ThreadData* td, S_UciOptions* options) {
	// variable used to store the score of the best move found by the search (while the move itself can be retrieved from the triangular pv table)
	int score = 0;
	int average_score = score_none;

	// Clean the position and the search info to start search from a clean state
	ClearForSearch(td);

	// Call the Negamax function in an iterative deepening framework
	for (int current_depth = start_depth; current_depth <= final_depth; current_depth++) {
		score = AspirationWindowSearch(average_score, current_depth, td);
		if (average_score == score_none) average_score = score;
		else average_score = (average_score + score) / 2;
		// Only the main thread handles time related tasks
		if (td->id == 0) {
			// use the previous search to adjust some of the time management parameters
			if (td->RootDepth > 7) {
				ScaleTm(td);
			}

			// check if we just cleared a depth and more than OptTime passed, or we used more than the give nodes
			if (StopEarly(&td->info) || NodesOver(&td->info)) {
				// Stop mainthread search
				td->info.stopped = true;
			}
		}
		// stop calculating and return best move so far
		if (td->info.stopped)
			break;

		// If it's the main thread print the uci output
		if (td->id == 0)
			PrintUciOutput(score, current_depth, td, options);
	}
}

int AspirationWindowSearch(int prev_eval, int depth, S_ThreadData* td) {
	int score = 0;
	td->RootDepth = depth;
	Search_stack stack[MAXDEPTH + 4], * ss = stack + 4;
	// Explicitely clean stack
	for (int i = -4; i < MAXDEPTH; i++) {
		(ss + i)->move = NOMOVE;
		(ss + i)->static_eval = score_none;
		(ss + i)->excludedMove = NOMOVE;
	}
	for (int i = 0; i < MAXDEPTH; i++) {
		(ss + i)->ply = i;
	}
	// We set an expected window for the score at the next search depth, this window is not 100% accurate so we might need to try a bigger window and re-search the position
	int delta = 12;
	// define initial alpha beta bounds
	int alpha = -MAXSCORE;
	int beta = MAXSCORE;

	// only set up the windows is the search depth is bigger or equal than Aspiration_Depth to avoid using windows when the search isn't accurate enough
	if (depth >= 3) {
		alpha = std::max(-MAXSCORE, prev_eval - delta);
		beta = std::min(prev_eval + delta, MAXSCORE);
	}

	// Stay at current depth if we fail high/low because of the aspiration windows
	while (true) {
		score = Negamax<true>(alpha, beta, depth, false, td, ss);

		// Check if more than Maxtime passed and we have to stop
		if (td->id == 0 && TimeOver(&td->info)) {
			StopHelperThreads();
			td->info.stopped = true;
			break;
		}

		// Stop calculating and return best move so far
		if (td->info.stopped) break;

		// We fell outside the window, so try again with a bigger window, since we failed low we can adjust beta to decrease the total window size
		if ((score <= alpha)) {
			beta = (alpha + beta) / 2;
			alpha = std::max(-MAXSCORE, score - delta);
			depth = td->RootDepth;
		}

		// We fell outside the window, so try again with a bigger window
		else if ((score >= beta)) {
			beta = std::min(score + delta, MAXSCORE);
			depth = std::max(depth - 1, td->RootDepth - 5);
		}
		else break;
		// Progressively increase how much the windows are increased by at each fail
		delta *= 1.44;
	}
	return score;
}

// Negamax alpha beta search
template <bool pvNode>
int Negamax(int alpha, int beta, int depth, const bool cutNode, S_ThreadData* td, Search_stack* ss) {
	// Extract data structures from ThreadData
	S_Board* pos = &td->pos;
	Search_data* sd = &td->ss;
	S_SearchINFO* info = &td->info;
	PvTable* pv_table = &td->pv_table;

	// Initialize the node
	const bool in_check = pos->checkers;
	const bool root_node = (ss->ply == 0);
	int eval;
	bool improving = false;
	int Score = -MAXSCORE;
	S_HashEntry tte;
	bool ttPv = pvNode;

	const int excludedMove = ss->excludedMove;

	pv_table->pvLength[ss->ply] = ss->ply;

	// Check for the highest depth reached in search to report it to the cli
	if (ss->ply > info->seldepth)
		info->seldepth = ss->ply;

	// recursion escape condition
	if (depth <= 0) {
		return Quiescence<pvNode>(alpha, beta, td, ss);
	}

	// check if more than Maxtime passed and we have to stop
	if (td->id == 0 && TimeOver(&td->info)) {
		StopHelperThreads();
		td->info.stopped = true;
	}

	// Check for early return conditions
	if (!root_node) {
		// If position is a draw return a randomized draw score to avoid 3-fold blindness
		if (IsDraw(pos, pvNode)) {
			return 0;
		}

		// If we reached maxdepth we return a static evaluation of the position
		if (ss->ply >= MAXDEPTH - 1) {
			return in_check ? 0 : EvalPosition(pos);
		}

		// Mate distance pruning
		alpha = std::max(alpha, -mate_score + ss->ply);
		beta = std::min(beta, mate_score - ss->ply - 1);
		if (alpha >= beta)
			return alpha;
	}

	// Probe the TT for useful previous search informations, we avoid doing so if we are searching a singular extension
	const bool ttHit = excludedMove ? false : ProbeHashEntry(pos, &tte);
	const int ttScore = ttHit ? ScoreFromTT(tte.score, ss->ply) : score_none;
	const int ttMove = ttHit ? MoveFromTT(tte.move, pos->PieceOn(From(tte.move))) : NOMOVE;
	const uint8_t ttFlag = ttHit ? tte.wasPv_flags & 3 : HFNONE;
	// If we found a value in the TT for this position, and the depth is equal or greater we can return it (pv nodes are excluded)
	if (!pvNode
		&& ttScore != score_none
		&& tte.depth >= depth) {
		if ((ttFlag == HFUPPER && ttScore <= alpha)
			|| (ttFlag == HFLOWER && ttScore >= beta)
			|| (ttFlag == HFEXACT))
			return ttScore;
	}

	if (ttHit)
		ttPv = pvNode || (tte.wasPv_flags >> 2);

	// IIR by Ed Schroder (That i find out about in Berserk source code)
	// http://talkchess.com/forum3/viewtopic.php?f=7&t=74769&sid=64085e3396554f0fba414404445b3120
	// https://github.com/jhonnold/berserk/blob/dd1678c278412898561d40a31a7bd08d49565636/src/search.c#L379
	if (depth >= 4 && ttFlag == HFNONE)
		depth--;

	// If we are in check or searching a singular extension we avoid pruning before the move loop
	if (in_check || excludedMove) {
		ss->static_eval = eval = score_none;
		improving = false;
		goto moves_loop;
	}

	// get an evaluation of the position:
	if (ttHit) {
		// If the value in the TT is valid we use that, otherwise we call the static evaluation function
		eval = ss->static_eval = (tte.eval != score_none) ? tte.eval : EvalPosition(pos);
		// We can also use the tt score as a more accurate form of eval
		if (ttScore != score_none
		    && ((ttFlag == HFUPPER && ttScore < eval)
			|| (ttFlag == HFLOWER && ttScore > eval)
			|| (ttFlag == HFEXACT)))
			eval = ttScore;
	}
	else {
		// If we don't have anything in the TT we have to call evalposition
		eval = ss->static_eval = EvalPosition(pos);
		if (!excludedMove)
		{
	    	// Save the eval into the TT
	    	StoreHashEntry(pos->posKey, NOMOVE, score_none, eval, HFNONE, 0, pvNode, ttPv);
		}
	}

	// improving is a very important modifier to a lot of heuristic, in short it just checks if our current static eval has improved since our last move, some extra logic is needed to account for the fact we don't evaluate positions that are in check
	// We look for the irst ply we weren't in check in between 2 and 4 plies ago, if we found one we check if the static eval increased, if we don't we just assume we have improved
	if ((ss - 2)->static_eval != score_none) {
		if (ss->static_eval > (ss - 2)->static_eval)
			improving = true;
	}
	else if ((ss - 4)->static_eval != score_none) {
		if (ss->static_eval > (ss - 4)->static_eval)
			improving = true;
	}
	else
		improving = true;
	
	// clean killers and excluded move for the next ply
	(ss + 1)->excludedMove = NOMOVE;
	(ss + 1)->searchKillers[0] = NOMOVE;
	(ss + 1)->searchKillers[1] = NOMOVE;

	if (!pvNode) {
		// Reverse futility pruning
		if (depth < 9
			&& eval - 66 * (depth - improving) >= beta
			&& abs(eval) < mate_found)
			return eval;

		// null move pruning: If we can give our opponent a free move and still be above beta after a reduced search we can return beta, we check if the board has non pawn pieces to avoid zugzwangs
		if (eval >= ss->static_eval
			&& eval >= beta
			&& ss->ply
			&& (ss - 1)->move != NOMOVE
			&& depth >= 3
			&& ss->ply >= td->nmpPlies
			&& BoardHasNonPawns(pos, pos->side)) {
			ss->move = NOMOVE;
			MakeNullMove(pos);
			int R = 3 + depth / 3 + std::min((eval - beta) / 200, 3);
			/* search moves with reduced depth to find beta cutoffs
			   depth - 1 - R where R is a reduction limit */
			int nmpScore = -Negamax<false>(-beta, -beta + 1, depth - R, !cutNode, td, ss + 1);

			TakeNullMove(pos);

			if (info->stopped)
				return 0;

			// fail-soft beta cutoff
			if (nmpScore >= beta) {
				// Don't return unproven mates but still return beta
				if (nmpScore > mate_found) nmpScore = beta;

				// If we don't have to do a verification search just return the score
				if (td->nmpPlies || depth < 15) {
					return nmpScore;
				}
				// Verification search to avoid zugzwangs: if we are at an high enough depth we perform another reduced search without nmp for at least nmpPlies
				td->nmpPlies = ss->ply + (depth - R) * 2 / 3;
				int verification_score = Negamax<false>(beta - 1, beta, depth - R, false, td, ss);
				td->nmpPlies = 0;

				// If the verification search holds return the score
				if (verification_score >= beta) return nmpScore;
			}
		}
		// Razoring
		if (depth <= 5 && eval + 256 * depth < alpha)
		{
			int razor_score = Quiescence<false>(alpha, beta, td, ss);
			if (razor_score <= alpha)
				return razor_score;
		}
	}

moves_loop:
	// create move list instance
	S_MOVELIST move_list[1];

	// generate moves
	GenerateMoves(move_list, pos);
	// assign a score to every move based on how promising it is
	score_moves(pos, sd, ss, move_list, ttMove);

	// old value of alpha
	int old_alpha = alpha;
	int BestScore = -MAXSCORE;
	int bestmove = NOMOVE;

	int moves_searched = 0;
	bool SkipQuiets = false;

	// Keep track of the played quiet and noisy moves
	S_MOVELIST quiet_moves, noisy_moves;
	quiet_moves.count = 0, noisy_moves.count = 0;

	// loop over moves within a movelist
	for (int count = 0; count < move_list->count; count++) {
		// take the most promising move that hasn't been played yet
		PickMove(move_list, count);
		// get the move with the highest score in the move ordering
		int move = move_list->moves[count].move;
		if (move == excludedMove) continue;
		bool isQuiet = IsQuiet(move);

		if (isQuiet && SkipQuiets) continue;

		int movehistory = GetHistoryScore(pos, sd, move, ss);

		// if the move isn't a quiet move we update the quiet moves list and counter
		if (isQuiet) {
			quiet_moves.moves[quiet_moves.count].move = move;
			quiet_moves.count++;
		}
		else {
			noisy_moves.moves[noisy_moves.count].move = move;
			noisy_moves.count++;
		}
		if (!root_node
			&& BoardHasNonPawns(pos, pos->side)
			&& BestScore > -mate_found) {
			// Movecount pruning: if we searched enough moves and we are not in check we skip the rest
			if (!pvNode
				&& !in_check
				&& depth < 9
				&& isQuiet
				&& moves_searched > lmp_margin[depth][improving]) {
				SkipQuiets = true;
				continue;
			}

			// lmrDepth is the current depth minus the reduction the move would undergo in lmr, this is helpful because it helps us discriminate the bad moves with more accuracy
			int lmrDepth = std::max(0, depth - reductions[isQuiet][depth][moves_searched]);

			// Futility pruning: if the static eval is so low that even after adding a bonus we are still under alpha we can stop trying quiet moves
			if (!in_check
				&& lmrDepth < 12
				&& isQuiet
				&& ss->static_eval + 100 + 150 * lmrDepth <= alpha) {
				SkipQuiets = true;
				continue;
			}

			// See pruning: prune all the moves that have a SEE score that is lower than our threshold
			if (depth <= 8
				&& !SEE(pos, move, see_margin[depth][isQuiet])) {
				continue;
			}
		}

		int extension = 0;
		// Limit Extensions to try and curb search explosions
		if (ss->ply < td->RootDepth * 2) {
			// Search extension
			if (!root_node
				&& depth >= 7
				&& move == ttMove
				&& !excludedMove
				&& (ttFlag & HFLOWER)
				&& abs(ttScore) < mate_found
				&& tte.depth >= depth - 3) {
				const int singularBeta = ttScore - depth;
				const int singularDepth = (depth - 1) / 2;

				ss->excludedMove = ttMove;
				int singularScore = Negamax<false>(singularBeta - 1, singularBeta, singularDepth, cutNode, td, ss);
				ss->excludedMove = NOMOVE;

				if (singularScore < singularBeta) {
					extension = 1;
					// Avoid search explosion by limiting the number of double extensions
					if (!pvNode
						&& singularScore < singularBeta - 17
						&& ss->double_extensions <= 11) {
						extension = 2;
						ss->double_extensions = (ss - 1)->double_extensions + 1;
					}
				}

				else if (singularBeta >= beta)
					return (singularBeta);

				// If we didn't successfully extend and our eval is above beta reduce the search depth
				else if (ttScore >= beta)
					extension = -2;
			}
			// Check extension
			else if (in_check)
				extension = 1;
		}
		// we adjust the search depth based on potential extensions
		int newDepth = depth - 1 + extension;
		int depth_reduction = 0;
		ss->move = move;
		// Play the move
		MakeMove(move, pos);
		// increment nodes count
		info->nodes++;
		uint64_t nodes_before_search = info->nodes;
		bool do_full_search = false;
		// conditions to consider LMR
		if (moves_searched >= 2 + 2 * pvNode && depth >= 3) {
			if (isQuiet) {
				// calculate by how much we should reduce the search depth
				// Get base reduction value
				depth_reduction = reductions[isQuiet][depth][moves_searched];
				// Reduce more if we aren't improving
				depth_reduction += !improving;
				// Reduce more if we aren't in a pv node
				depth_reduction += !ttPv;
				// Decrease the reduction for moves that have a good history score and increase it for moves with a bad score
				depth_reduction -= std::clamp(movehistory / 16384, -2, 2);
				// Fuck
				depth_reduction += 2 * cutNode;
				// Decrease the reduction for moves that give check
				if (pos->checkers) depth_reduction -= 1;
			}
			else if (!ttPv) {
				depth_reduction = reductions[false][depth][moves_searched];
				// Decrease the reduction for moves that have a good history score and increase it for moves with a bad score
				depth_reduction -= std::clamp(movehistory / 16384, -2, 2);
				// Decrease the reduction for moves that give check
				if (pos->checkers) depth_reduction -= 1;
			}
			// adjust the reduction so that we can't drop into Qsearch and to prevent extensions
			depth_reduction = std::clamp(depth_reduction, 0, newDepth - 1);
			// search current move with reduced depth:
			Score = -Negamax<false>(-alpha - 1, -alpha, newDepth - depth_reduction, true, td, ss + 1);
			// if we failed high on a reduced node we'll search with a reduced window and full depth
			do_full_search = Score > alpha && depth_reduction;
		}
		else {
			// If we skipped LMR and this isn't the first move of the node we'll search with a reduced window and full depth
			do_full_search = !pvNode || moves_searched > 0;
		}
		// Search every move (excluding the first of every node) that skipped or failed LMR with full depth but a reduced window
		if (do_full_search)
		{
			Score = -Negamax<false>(-alpha - 1, -alpha, newDepth, !cutNode, td, ss + 1);
			if (depth_reduction)
			{
				// define the conthist bonus
				int bonus = std::min(16 * (depth + 1) * (depth + 1), 1200);
				updateCHScore(sd, ss, move, Score > alpha ? bonus : -bonus);
            }
		}

		// PVS Search: Search the first move and every move that is within bounds with full depth and a full window
		if (pvNode && (moves_searched == 0 || Score > alpha))
			Score = -Negamax<true>(-beta, -alpha, newDepth, false, td, ss + 1);

		// take move back
		UnmakeMove(move, pos);
		if (td->id == 0
			&& root_node)
			td->nodeSpentTable[From(move)][To(move)] += info->nodes - nodes_before_search;

		if (info->stopped)
			return 0;

		moves_searched++;
		// If the Score of the current move is the best we've found until now
		if (Score > BestScore) {
			// Update what the best score is
			BestScore = Score;

			// found a better move
			if (Score > alpha) {
				bestmove = move;
				// Update the pv table
				pv_table->pvArray[ss->ply][ss->ply] = move;
				for (int next_ply = ss->ply + 1; next_ply < pv_table->pvLength[ss->ply + 1]; next_ply++) {
					pv_table->pvArray[ss->ply][next_ply] = pv_table->pvArray[ss->ply + 1][next_ply];
				}
				pv_table->pvLength[ss->ply] = pv_table->pvLength[ss->ply + 1];

				if (Score >= beta) {
					// If the move that caused the beta cutoff is quiet we have a killer move
					if (IsQuiet(move)) {
						// Don't update killer moves if it would result in having 2 identical killer moves
						if (ss->searchKillers[0] != bestmove) {
							// store killer moves
							ss->searchKillers[1] = ss->searchKillers[0];
							ss->searchKillers[0] = bestmove;
						}

						// Save CounterMoves
						if (ss->ply >= 1)
							sd->CounterMoves[From((ss - 1)->move)][To((ss - 1)->move)] = move;
					}
                    // Update the history heuristics based on the new best move
					UpdateHistories(pos, sd, ss, depth + (eval <= alpha), bestmove, &quiet_moves, &noisy_moves);

					// node (move) fails high
					break;
				}
				// Update alpha iff alpha < beta
				alpha = Score;
			}
		}
	}

		// We don't have any legal moves to make in the current postion
	if (move_list->count == 0) {
		// If we are veryfing a singular move return alpha else if the king is in check return mating score (assuming closest distance to mating position) otherwise return stalemate
		return excludedMove ? alpha : in_check ? (-mate_score + ss->ply) : 0;
	}

	// Set the TT flag based on whether the BestScore is better than beta and if it's not based on if we changed alpha or not
	int flag = BestScore >= beta ? HFLOWER : (alpha != old_alpha) ? HFEXACT : HFUPPER;

	if (!excludedMove) StoreHashEntry(pos->posKey, MoveToTT(bestmove), ScoreToTT(BestScore, ss->ply), ss->static_eval, flag, depth, pvNode, ttPv);
	// return best score
	return BestScore;
}

// Quiescence search to avoid the horizon effect
template <bool pvNode>
int Quiescence(int alpha, int beta, S_ThreadData* td, Search_stack* ss) {
	S_Board* pos = &td->pos;
	Search_data* sd = &td->ss;
	S_SearchINFO* info = &td->info;
	const bool in_check = pos->checkers;
	// tte is an hashtable entry, it will store the values fetched from the TT
	S_HashEntry tte;
	int BestScore;
	bool ttPv = pvNode;

	// check if more than Maxtime passed and we have to stop
	if (td->id == 0 && TimeOver(&td->info)) {
		StopHelperThreads();
		td->info.stopped = true;
	}

	// If position is a draw return a randomized draw score to avoid 3-fold blindness
	if (IsDraw(pos, pvNode)) {
		return 0;
	}

	// If we reached maxdepth we return a static evaluation of the position
	if (ss->ply >= MAXDEPTH - 1) {
		return in_check ? 0 : EvalPosition(pos);
	}
	// ttHit is true if and only if we find something in the TT
	const bool ttHit = ProbeHashEntry(pos, &tte);
	const int ttScore = ttHit ? ScoreFromTT(tte.score, ss->ply) : score_none;
	const int ttMove = ttHit ? MoveFromTT(tte.move, pos->PieceOn(From(tte.move))) : NOMOVE;
	const uint8_t ttFlag = ttHit ? tte.wasPv_flags & 3 : HFNONE;
	// If we found a value in the TT we can return it
	if (!pvNode && ttScore != score_none) {
		if ((ttFlag == HFUPPER && ttScore <= alpha)
			|| (ttFlag == HFLOWER && ttScore >= beta)
			|| (ttFlag == HFEXACT))
			return ttScore;
	}

	if (ttHit)
		ttPv = pvNode || (tte.wasPv_flags >> 2);

	if (in_check) {
		ss->static_eval = score_none;
		BestScore = -MAXSCORE;
	}
	// If we have a ttHit with a valid eval use that
	else if (ttHit) {
		ss->static_eval = BestScore = (tte.eval != score_none) ? tte.eval : EvalPosition(pos);
		if (ttScore != score_none && 
		    ((ttFlag == HFUPPER && ttScore < ss->static_eval)
			|| (ttFlag == HFLOWER && ttScore > ss->static_eval)
			|| (ttFlag == HFEXACT)))
			BestScore = ttScore;
	}
	// If we don't have any useful info in the TT just call Evalpos
	else {
		BestScore = ss->static_eval = EvalPosition(pos);
	}

	// Stand pat
	if (BestScore >= beta) return BestScore;
	// Adjust alpha based on eval
	alpha = std::max(alpha, BestScore);

	// create move list instance
	S_MOVELIST move_list[1];
	// If we aren't in check we generate just the captures, otherwise we generate all the moves
	if (!in_check)
		GenerateCaptures(move_list, pos);
	else
		GenerateMoves(move_list, pos);

	// score the generated moves
	score_moves(pos, sd, ss, move_list, ttMove);

	int bestmove = NOMOVE;

	// loop over moves within the movelist
	for (int count = 0; count < move_list->count; count++) {
		PickMove(move_list, count);
		int move = move_list->moves[count].move;
		int score = move_list->moves[count].score;
		// See pruning
		if (score < goodCaptureScore 
			&& BestScore > -mate_found) {
			break;
		}
		ss->move = move;
		MakeMove(move, pos);
		// increment nodes count
		info->nodes++;
		// Call Quiescence search recursively
		int Score = -Quiescence<pvNode>(-beta, -alpha, td, ss + 1);

		// take move back
		UnmakeMove(move, pos);

		if (info->stopped)
			return 0;

		// If the Score of the current move is the best we've found until now
		if (Score > BestScore) {
			// Update  what the best score is
			BestScore = Score;

			// if the Score is better than alpha update our best move
			if (Score > alpha) {
				bestmove = move;

				// if the Score is better than or equal to beta break the loop because we failed high
				if (Score >= beta) {
					break;
				}
				// Update alpha iff alpha < beta
				alpha = Score;
			}
		}
	}

	if (move_list->count == 0 && in_check) {
		// return mate score (assuming closest distance to mating position)
		return (-mate_score + ss->ply);
	}

	// Set the TT flag based on whether the BestScore is better than beta, for qsearch we never use the exact flag
	int flag = BestScore >= beta ? HFLOWER : HFUPPER;

	StoreHashEntry(pos->posKey, MoveToTT(bestmove), ScoreToTT(BestScore, ss->ply), ss->static_eval, flag, 0, pvNode, ttPv);

	// return the best score we got
	return BestScore;
}
