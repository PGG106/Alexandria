#include "search.h"
#include "Board.h"
#include "History.h"
#include "PieceData.h"
#include "attack.h"
#include "eval.h"
#include "io.h"
#include "magic.h"
#include "makemove.h"
#include "misc.h"
#include "move.h"
#include "movegen.h"
#include "movepicker.h"
#include "ttable.h"
#include <cassert>
#include <cstring>
#include <thread>
#include <vector>

#define Aspiration_Depth 3

#define Resize_limit 5

int CounterMoves[Board_sq_num][Board_sq_num];

int razoring_margin1 = 125;
int razoring_margin2 = 175;

//Contains the material Values of the pieces
int PieceValue[12] = { 100, 325, 325, 500, 900, -10000,
					  100, 325, 325, 500, 900, -10000 };

void CheckUp(S_SearchINFO* info) {
	// check if time up or interrupt from GUI
	if (info->timeset == TRUE && GetTimeMs() > info->stoptime) {
		info->stopped = TRUE;
	}
	ReadInput(info);
}

// IsRepetition handles the repetition detection of a position
static int IsRepetition(const S_Board* pos) {

	for (int index = 0; index < pos->hisPly; index++)
		// if we found the hash key same with a current
		if (pos->history[index].posKey == pos->posKey)
			// we found a repetition
			return TRUE;

	return FALSE;
}

//ClearForSearch handles the cleaning of the post and the info parameters to start search from a clean state
void ClearForSearch(S_Board* pos, S_SearchINFO* info) {

	//For every piece [12] moved to every square [64] we reset the searchHistory value
	for (int index = 0; index < 12; ++index) {
		for (int index2 = 0; index2 < 64; ++index2) {
			pos->searchHistory[index][index2] = 0;
		}
	}

	//Reset the 2 killer moves that are stored for any searched depth
	for (int index = 0; index < 2; ++index) {
		for (int index2 = 0; index2 < MAXDEPTH; ++index2) {
			pos->searchKillers[index][index2] = 0;
		}
	}

	//Reset HashTable statistics (these aren't actually used as of now)
	HashTable->overWrite = 0;
	HashTable->hit = 0;
	HashTable->cut = 0;

	//Reset plies and search info
	pos->ply = 0;
	info->starttime = GetTimeMs();
	info->stopped = 0;
	info->nodes = 0;
}

static inline Bitboard AttacksTo(const S_Board* pos, int to) {
	//Take the occupancies of obth positions, encoding where all the pieces on the board reside
	Bitboard occ = pos->bitboards[BOTH];
	Bitboard attackers = 0ULL;
	//For every piece type get a bitboard that encodes the squares occupied by that piece type
	Bitboard attackingBishops = GetBishopsBB(pos);
	Bitboard attackingRooks = GetRooksBB(pos);
	Bitboard attackingQueens = GetQueensBB(pos);
	Bitboard attackingKnights = GetKnightsBB(pos);
	Bitboard attackingKings = GetKingsBB(pos);
	//Get the possible attacks for the sliding pieces to the target square according to the current board occupancies
	Bitboard intercardinalRays = get_bishop_attacks(to, occ);
	Bitboard cardinalRaysRays = get_rook_attacks(to, occ);
	//Set the attackers up as the pieces actually on the board that have an attack avaliable 
	attackers |= intercardinalRays & (attackingBishops | attackingQueens);
	attackers |= cardinalRaysRays & (attackingRooks | attackingQueens);

	attackers |= (pawn_attacks[BLACK][to] & pos->bitboards[WP]) |
		(pawn_attacks[WHITE][to] & pos->bitboards[BP]);
	attackers |= (knight_attacks[to] & (attackingKnights));

	attackers |= king_attacks[to] & (attackingKings);
	//Return the bitboard encoding all the attackers to the square "to"
	return attackers;
}

// inspired by the Weiss engine
static inline bool SEE(const S_Board* pos, const int move,
	const int threshold) {

	int to = get_move_target(move);
	int target = pos->pieces[to];
	// Making the move and not losing it must beat the threshold
	int value = PieceValue[target] - threshold;
	if (value < 0)
		return false;

	int from = get_move_source(move);
	int attacker = pos->pieces[from];
	// Trivial if we still beat the threshold after losing the piece
	value -= PieceValue[attacker];
	if (value >= 0)
		return true;
	// It doesn't matter if the to square is occupied or not
	Bitboard occupied = pos->occupancies[BOTH] ^ (1ULL << from);
	Bitboard attackers = AttacksTo(pos, to);
	Bitboard bishops = GetBishopsBB(pos) | GetQueensBB(pos);
	Bitboard rooks = GetRooksBB(pos) | GetQueensBB(pos);

	int side = !Color[attacker];

	// Make captures until one side runs out, or fail to beat threshold
	while (true) {
		// Remove used pieces from attackers
		attackers &= occupied;
		Bitboard myAttackers = attackers & pos->occupancies[side];
		if (!myAttackers)
			break;
		// Pick next least valuable piece to capture with
		int pt;
		for (pt = PAWN; pt <= KING; ++pt)
			if (myAttackers & GetGenericPiecesBB(pos, pt))
				break;

		side = !side;
		// Value beats threshold, or can't beat threshold (negamaxed)
		if ((value = -value - 1 - PieceValue[pt]) >= 0) {

			if (pt == KING && (attackers & pos->occupancies[side]))
				side = !side;

			break;
		}
		// Remove the used piece from occupied
		occupied ^= (1ULL << (get_ls1b_index(myAttackers & GetGenericPiecesBB(pos, pt))));
		if (pt == PAWN || pt == BISHOP || pt == QUEEN)
			attackers |= get_bishop_attacks(to, occupied) & bishops;
		if (pt == ROOK || pt == QUEEN)
			attackers |= get_rook_attacks(to, occupied) & rooks;
	}

	return side != Color[attacker];
}

// score_moves takes a list of move as an argument and assigns a score to each move
static inline void score_moves(S_Board* pos, S_MOVELIST* move_list,
	int PvMove) {

	//Loop through all the move in the movelist
	for (int i = 0; i < move_list->count; i++) {
		int move = move_list->moves[i].move;
		//If the move is from the TT (aka it's our hashmove) give it the highest score
		if (move == PvMove) {
			move_list->moves[i].score = INT32_MAX - 100;
			continue;
		}
		else if (get_move_promoted(move)) {
			move_list->moves[i].score = 2000000000 + get_move_promoted(move);
			continue;
		}
		//if the move is an enpassant or a promotion give it a score that a good capture of type pawn-pwan would have
		else if (get_move_enpassant(move)) {

			move_list->moves[i].score = 105 + 1000000000;
			continue;
		}
		//if the mvoe is a capture sum the mvv-lva score to a variable that depends on whether the capture has a positive SEE or not 
		else if (get_move_capture(move)) {

			move_list->moves[i].score =
				mvv_lva[get_move_piece(move)][pos->pieces[get_move_target(move)]] +
				500000000 + 400000000 * SEE(pos, move, -100);
			continue;
		}
		//First  killer move always comes after the TT move,the promotions and the good captures and before anything else
		else if (pos->searchKillers[0][pos->ply] == move) {

			move_list->moves[i].score = 800000000;
			continue;
		}
		//Second killer move always comes after the first one
		else if (pos->searchKillers[1][pos->ply] == move) {

			move_list->moves[i].score = 700000000;
			continue;
		}
		//After the killer moves try the Counter moves
		else if (move == CounterMoves[get_move_source(pos->history[pos->hisPly].move)][get_move_target(pos->history[pos->hisPly].move)])
		{
			move_list->moves[i].score = 600000000;
			continue;
		}
		//if the move isn't in any of the previous categories score it according to the history heuristic
		else {

			move_list->moves[i].score = pos->searchHistory[pos->pieces[get_move_source(move)]][get_move_target(move)];
			continue;
		}
	}

	return;
}

//Calculate a futility margin based on depth and if the search is improving or not
int futility(int depth, bool improving) { return 70 * (depth - improving); }

//Quiescence search to avoid the horizon effect
int Quiescence(int alpha, int beta, S_Board* pos, S_SearchINFO* info,
	int pv_node) {

	//Check if we recieved a stop command from the GUI
	if ((info->nodes & 2047) == 0) {
		CheckUp(info);
	}

	// increment nodes count
	info->nodes++;

	//If we triggered any of the rules that forces a draw or we know the position is a draw return a draw score
	if (((IsRepetition(pos)) && pos->ply) || (pos->fiftyMove >= 100) ||
		MaterialDraw(pos)) {
		return 0;
	}

	//If we reached maxdepth we return a static evaluation of the position
	if (pos->ply > MAXDEPTH - 1) {
		return EvalPosition(pos);
	}

	//Get a static evaluation of the position
	int standing_pat = EvalPosition(pos);

	// If the static eval is better than beta we can return beta
	if (standing_pat >= beta) {
		// node (move) fails high
		return standing_pat;
	}

	// If the static eval is better than alpha we can use the eval as an improved alpha
	if (standing_pat > alpha) {
		// PV node (move)
		alpha = standing_pat;
	}

	//tte is an hashtable entry, it will store the values fetched from the TT
	S_HASHENTRY tte;
	//TThit is true if and only if we find something in the TT
	bool TThit = ProbeHashEntry(pos, alpha, beta, 0, &tte);

	//If we found a value in the TT we can return it
	if (pos->ply && TThit && MoveExists(pos, tte.move)) {
		HashTable->cut++;
		return tte.score;
	}

	// create move list instance
	S_MOVELIST move_list[1];

	// generate the captures
	generate_captures(move_list, pos);

	//score the generated moves
	score_moves(pos, move_list, tte.move);

	//set up variables needed for the search
	int BestScore = standing_pat;
	int bestmove = NOMOVE;
	int Score = -MAXSCORE;

	// old value of alpha
	int old_alpha = alpha;

	int moves_searched = 0;

	// loop over moves within a movelist
	for (int count = 0; count < move_list->count; count++) {

		pick_move(move_list, count);
		int move = move_list->moves[count].move;
		make_move(move, pos);

		//Call Quiescence search recursively
		Score = -Quiescence(-beta, -alpha, pos, info, pv_node);

		// take move back
		Unmake_move(pos);

		if (info->stopped == 1)
			return 0;

		moves_searched++;

		//If the Score of the current move is the best we've found until now
		if (Score > BestScore) {
			//Update the best move found and what the best score is
			BestScore = Score;

			// if the Score is better than alpha update alpha
			if (Score > alpha) {
				alpha = Score;
				bestmove = move;

				// if the Score is better than beta save the move in the TT and return beta
				if (Score >= beta) break;
			}
		}
	}
	//if we updated alpha we have an exact score, otherwise we only have an upper bound (for now the beta flag isn't actually ever used)

	int flag = BestScore >= beta ? HFBETA : (alpha != old_alpha) ? HFEXACT : HFALPHA;

	StoreHashEntry(pos, bestmove, BestScore, flag, 0, pv_node);

	// node (move) fails low
	return BestScore;
}

// full depth moves counter
const int full_depth_moves = 4;

// depth limit to consider reduction
const int lmr_depth = 3;

//Calculate a reduction margin based on the search depth and the number of moves played
static inline int reduction(int depth, int num_moves) {
	return reductions[depth] * reductions[num_moves];
}

// negamax alpha beta search
int negamax(int alpha, int beta, int depth, S_Board* pos, S_SearchINFO* info,
	int DoNull, Stack* ss) {

	// recursion escape condition
	if (depth <= 0) {
		int pv_node = beta - alpha > 1;
		return Quiescence(alpha, beta, pos, info, pv_node);
	}

	//Check if we recieved a stop command from the GUI
	if ((info->nodes & 2047) == 0) {
		CheckUp(info);
	}
	// increment nodes count
	info->nodes++;
	//If we triggered any of the rules that forces a draw or we know the position is a draw return a draw score
	if (((IsRepetition(pos)) && pos->ply) || (pos->fiftyMove >= 100) ||
		MaterialDraw(pos)) {
		return 0;
	}
	//If we reached maxdepth we return a static evaluation of the position
	if (pos->ply > MAXDEPTH - 1) {
		return EvalPosition(pos);
	}
	// Initialize the node
	ss->inCheck = is_square_attacked(
		pos, get_ls1b_index(pos->bitboards[KING + pos->side * 6]), pos->side ^ 1);
	int quietCount = 0;
	S_MOVELIST quiet_moves;
	quiet_moves.count = 0;
	ss->moveCount = 0;
	int root_node = (pos->ply == 0);
	int static_eval;
	bool improving;
	bool ttHit;
	int Score = -MAXSCORE;
	S_HASHENTRY tte;

	// Mate distance pruning
	int mating_value = mate_value - pos->ply;

	if (mating_value < beta) {
		beta = mating_value;
		if (alpha >= mating_value)
			return mating_value;
	}

	int pv_node = beta - alpha > 1;

	ttHit = ProbeHashEntry(pos, alpha, beta, depth, &tte);
	//If we found a value in the TT we can return it
	if (pos->ply && !pv_node && ttHit && MoveExists(pos, tte.move)) {

		HashTable->cut++;
		return tte.score;
	}

	// IIR by Ed Schroder (That i find out about in Berserk source code)
	// http://talkchess.com/forum3/viewtopic.php?f=7&t=74769&sid=64085e3396554f0fba414404445b3120
	// https://github.com/jhonnold/berserk/blob/dd1678c278412898561d40a31a7bd08d49565636/src/search.c#L379
	if (depth >= 4 && !tte.move)
		depth--;

	// is king in check
	int in_check = is_square_attacked(
		pos, get_ls1b_index(GetKingColorBB(pos, pos->side)), pos->side ^ 1);

	if (in_check) {
		depth++;
		static_eval = value_none;
		pos->history[pos->hisPly].eval = value_none;
		improving = false;
		goto moves_loop; //if we are in check we jump directly to the move loop because the net isn't good when evaluating positions that are in check
	}

	// get static evaluation score
	static_eval = ttHit ? tte.score : EvalPosition(pos);
	pos->history[pos->hisPly].eval = static_eval;

	//if we aren't in check and the eval of this position is better than the position of 2 plies ago (or we were in check 2 plies ago), it means that the position is "improving" this is later used in some forms of pruning
	improving = (!in_check) && (pos->hisPly >= 2) &&
		(static_eval > (pos->history[pos->hisPly - 2].eval) ||
			(pos->history[pos->hisPly - 2].eval) == value_none);

	// evaluation pruning / static null move pruning
	if (depth < 3 && !pv_node && !in_check && abs(beta - 1) > -MAXSCORE + 100) {
		// define evaluation margin
		int eval_margin = 120 * depth;

		// evaluation margin substracted from static evaluation score fails high
		if (static_eval - eval_margin >= beta)
			// evaluation margin substracted from static evaluation score
			return static_eval - eval_margin;
	}

	// Reverse futility pruning (depth 8 limit was taken from stockfish)
	if (!pv_node
		&& depth < 8
		&& static_eval - futility(depth, improving) >= beta)
		return static_eval;

	// null move pruning: If we can give our opponent a free move and still be above beta after a reduced search we can return beta
	if (!pv_node
		&& DoNull
		&& static_eval >= beta
		&& !in_check
		&& pos->ply
		&& depth >= 3) {

		MakeNullMove(pos);
		int R = 3 + reductions[depth];
		/* search moves with reduced depth to find beta cutoffs
		   depth - 1 - R where R is a reduction limit */
		Score = -negamax(-beta, -beta + 1, depth - R, pos, info, FALSE, ss);

		TakeNullMove(pos);

		if (info->stopped == 1)
			return 0;

		// fail-hard beta cutoff
		if (Score >= beta && abs(Score) < ISMATE)
			// node (position) fails high
			return beta;
	}

	// razoring
	if (!pv_node && !in_check && (depth <= 3) &&
		(static_eval <=
			(alpha - razoring_margin1 - razoring_margin2 * (depth - 1)))) {

		return Quiescence(alpha, beta, pos, info, pv_node);
	}

moves_loop:
	// create move list instance
	S_MOVELIST move_list[1];

	// generate moves
	generate_moves(move_list, pos);

	score_moves(pos, move_list, tte.move);

	// old value of alpha
	int old_alpha = alpha;
	int BestScore = -MAXSCORE;
	int bestmove = NOMOVE;

	int moves_searched = 0;

	// loop over moves within a movelist
	for (int count = 0; count < move_list->count; count++) {
		pick_move(move_list, count);

		int move = move_list->moves[count].move;
		//if the move isn't a quiet move we update the quiet moves list and counter
		if (IsQuiet(move)) {
			quiet_moves.moves[quiet_moves.count].move = move;
			quiet_moves.count++;
		}
		//Movecount pruning: if we searched enough quiet moves and we are not in check we skip the others
		if (!root_node && !pv_node && !in_check && depth < 4 && IsQuiet(move) &&
			(quiet_moves.count > (depth * 8))) {
			continue;
		}
		//Play the move
		make_move(move, pos);

		// full depth search
		if (moves_searched == 0)

			// do normal alpha beta search
			Score = -negamax(-beta, -alpha, depth - 1, pos, info, TRUE, ss);

		// late move reduction: After we've searched /full_depth_moves/ and if we are at an appropriate depth we can search the remaining moves at a reduced depth
		else {
			// condition to consider LMR
			if (moves_searched >= full_depth_moves && depth >= lmr_depth &&
				!in_check && IsQuiet(move))

			{
				//calculate by how much we should reduce the search depth 
				int depth_reduction = reduction(depth, moves_searched);

				// search current move with reduced depth:
				Score = -negamax(-alpha - 1, -alpha, depth - depth_reduction, pos, info,
					TRUE, ss);
			}

			// hack to ensure that full-depth search is done
			else
				Score = alpha + 1;

			// principle variation search PVS
			if (Score > alpha) {

				Score = -negamax(-alpha - 1, -alpha, depth - 1, pos, info, TRUE, ss);

				if ((Score > alpha) && (Score < beta))

					Score = -negamax(-beta, -alpha, depth - 1, pos, info, TRUE, ss);
			}
		}

		// take move back
		Unmake_move(pos);

		if (info->stopped == 1)
			return 0;

		moves_searched++;
		//If the Score of the current move is the best we've found until now
		if (Score > BestScore) {
			//Update the best move found and what the best score is
			BestScore = Score;

			// found a better move
			if (Score > alpha) {
				bestmove = move;
				alpha = Score;

				if (Score >= beta)
				{
					//If the move that caused the beta cutoff is quiet we have a killer move
					if (IsQuiet(move)) {

						if (pos->searchKillers[0][pos->ply] != bestmove) {
							// store killer moves
							pos->searchKillers[1][pos->ply] = pos->searchKillers[0][pos->ply];
							pos->searchKillers[0][pos->ply] = bestmove;
						}

						//Save CounterMoves
						int previousMove = pos->history[pos->hisPly].move;
						CounterMoves[get_move_source(previousMove)]
							[get_move_target(previousMove)] = move;
					}

					// node (move) fails high
					break;
				}
			}
		}
	}


	if (BestScore >= beta && IsQuiet(bestmove)) {
		updateHH(pos, depth, bestmove, &quiet_moves);
	}
	// we don't have any legal moves to make in the current postion
	if (move_list->count == 0) {
		// if the king is in check return mating score (assuming closest distance to mating position) otherwise return stalemate 
		BestScore = in_check ? (-mate_value + pos->ply) : 0;
	}
	//if we updated alpha we have an exact score, otherwise we only have an upper bound (for now the beta flag isn't actually ever used)

	int flag = BestScore >= beta ? HFBETA : (alpha != old_alpha) ? HFEXACT : HFALPHA;

	StoreHashEntry(pos, bestmove, BestScore, flag, depth, pv_node);
	// node (move) fails low
	return BestScore;
}

//Starts the search process, this is ideally the point where you can start a multithreaded search
void Root_search_position(int depth, S_Board* pos, S_SearchINFO* info) {

	search_position(1, depth, pos, info, TRUE); // root search

}

// search_position is the actual function that handles the search, it sets up the variables needed for the search , calls the negamax function and handles the console output
void search_position(int start_depth, int final_depth, S_Board* pos,
	S_SearchINFO* info, int show) {

	// Stack initialization taken from stockfish, it offers support for
	// initializing continuation histories even if not needed as of now
	Stack stack[MAXGAMEMOVES + 10], * ss = stack + 7;
	(std::memset)(ss - 7, 0, 10 * sizeof(Stack));
	for (int i = 0; i <= MAXGAMEMOVES + 2; ++i)
		(ss + i)->ply = i;

	//variable used to store the score of the best move found by the search (while the move itself can be retrieved from the TT)
	int score = 0;

	//Clean the position and the search info to start search from a clean state 
	ClearForSearch(pos, info);

	//We set an expected window for the score at the next search depth, this window is not 100% accurate so we might need to try a bigger window and re-search the position, resize counter keeps track of how many times we had to re-search
	int alpha_window = -16;
	int resize_counter = 0;
	int beta_window = 16;

	// define initial alpha beta bounds
	int alpha = -MAXSCORE;
	int beta = MAXSCORE;

	// Call the negamax function in an iterative deepening framework
	for (int current_depth = start_depth; current_depth <= final_depth;
		current_depth++) {

		score = negamax(alpha, beta, current_depth, pos, info, TRUE, ss);

		// we fell outside the window, so try again with a bigger window for up to Resize_limit times, if we still fail after we just search with a full window
		if ((score <= alpha)) {
			if (resize_counter > Resize_limit)
				alpha = -MAXSCORE;
			beta = (alpha + beta) / 2;
			alpha_window *= 1.5;
			alpha += alpha_window;
			resize_counter++;
			current_depth--;
			continue;
		}

		// we fell outside the window, so try again with a bigger window for up to Resize_limit times, if we still fail after we just search with a full window
		else if ((score >= beta)) {
			if (resize_counter > Resize_limit)
				beta = MAXSCORE;
			beta_window *= 1.5;
			beta += beta_window;
			resize_counter++;
			current_depth--;
			continue;
		}

		// only set up the windows is the search depth is bigger or equal than Aspiration_Depth to avoid using windows when the search isn't accurate enough
		if (current_depth >= Aspiration_Depth) {
			alpha = score + alpha_window;
			beta = score + beta_window;
		}

		if (info->stopped == 1)
			// stop calculating and return best move so far
			break;

		//This handles the basic console output, show is always true by default is we are dealing with a single thread
		if (show) {
			unsigned long  time = GetTimeMs() - info->starttime;
			uint64_t nps = info->nodes / (time + !time) * 1000;
			if (score > -mate_value && score < -mate_score)
				printf("info score mate %d depth %d nodes %lu nps %lld time %d pv ",
					-(score + mate_value) / 2, current_depth, info->nodes, nps,
					GetTimeMs() - info->starttime);

			else if (score > mate_score && score < mate_value)
				printf("info score mate %d depth %d nodes %lu nps %lld time %d pv ",
					(mate_value - score) / 2 + 1, current_depth, info->nodes, nps,
					GetTimeMs() - info->starttime);

			else
				printf("info score cp %d depth %d nodes %lu nps %lld time %d pv ", score,
					current_depth, info->nodes, nps, GetTimeMs() - info->starttime);

			int PvCount = GetPvLine(current_depth, pos);

			// loop over the moves within a PV line
			for (int count = 0; count < PvCount; count++) {
				// print PV move
				print_move(pos->pvArray[count]);
				printf(" ");
			}
			// print new line
			printf("\n");
		}
	}

	//Print the best move we've found
	if (show) {
		printf("bestmove ");
		print_move(pos->pvArray[0]);
		printf("\n");
	}
}
