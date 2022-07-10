#include "Board.h"
#include "move.h"
#include "movegen.h"
#include "makemove.h"
#include "PieceData.h"
#include "ttable.h"
#include "io.h"
#include "eval.h"
#include "search.h"
#include "misc.h"
#include <thread>
#include <vector>
#include <cassert>
#include <cstring>
#include "attack.h"
#include "magic.h"
#include "movepicker.h"


int CounterMoves[MAXDEPTH][MAXDEPTH];


int PieceValue[12] = { 100, 325, 325, 500 ,900,-10000,100, 325, 325, 500 ,900,-10000 };

void CheckUp(S_SearchINFO* info) {
	//check if time up or interrupt from GUI

	if (info->timeset == TRUE && GetTimeMs() > info->stoptime) {
		info->stopped = TRUE;

	}

	ReadInput(info);


}

static int IsRepetition(const S_Board* pos) {

	for (int index = 0; index < pos->hisPly; index++)
		// if we found the hash key same with a current
		if (pos->history[index].posKey == pos->posKey)
			// we found a repetition
			return TRUE;


	return FALSE;
}



void ClearForSearch(S_Board* pos, S_SearchINFO* info) {

	int index = 0;
	int index2 = 0;
	for (index = 0;index < 12;++index) {
		for (index2 = 0;index2 < 64;++index2) {
			pos->searchHistory[index][index2] = 0;

		}
	}


	for (index = 0;index < 2;++index) {
		for (index2 = 0;index2 < MAXDEPTH;++index2) {
			pos->searchKillers[index][index2] = 0;
		}
	}

	HashTable->overWrite = 0;
	HashTable->hit = 0;
	HashTable->cut = 0;

	pos->ply = 0;
	info->starttime = GetTimeMs();
	info->stopped = 0;
	info->nodes = 0;
}



static inline Bitboard AttacksTo(const S_Board* pos, int to) {

	Bitboard occ = pos->bitboards[BOTH];
	Bitboard attackers = 0ULL;
	Bitboard attackingBishops = pos->bitboards[WB] | pos->bitboards[BB];
	Bitboard attackingRooks = pos->bitboards[WR] | pos->bitboards[BR];
	Bitboard attackingQueens = pos->bitboards[WQ] | pos->bitboards[BQ];
	Bitboard attackingKnights = pos->bitboards[WN] | pos->bitboards[BN];
	Bitboard attackingKings = pos->bitboards[WK] | pos->bitboards[BK];
	Bitboard intercardinalRays = get_bishop_attacks(to, occ);
	Bitboard cardinalRaysRays = get_rook_attacks(to, occ);

	attackers |= intercardinalRays & (attackingBishops | attackingQueens);
	attackers |= cardinalRaysRays & (attackingRooks | attackingQueens);

	attackers |=
		(pawn_attacks[BLACK][to] & pos->bitboards[WP]) | (pawn_attacks[WHITE][to] & pos->bitboards[BP]);
	attackers |= (knight_attacks[to] & (attackingKnights));


	attackers |=
		king_attacks[to] & (attackingKings);

	return attackers;
}



// inspired by the Weiss engine
static inline bool SEE(const S_Board* pos, const int move, const int threshold) {


	int to = get_move_target(move);
	int target = pos->pieces[to];

	// Making the move and not losing it must beat the threshold
	int value = PieceValue[target] - threshold;
	if (value < 0) return false;

	int from = get_move_source(move);
	int attacker = pos->pieces[from];

	// Trivial if we still beat the threshold after losing the piece
	value -= PieceValue[attacker];
	if (value >= 0) return true;

	// It doesn't matter if the to square is occupied or not
	Bitboard occupied = pos->occupancies[BOTH] ^ (1ULL << from);
	Bitboard attackers = AttacksTo(pos, to);
	Bitboard bishops = pieceBB(BISHOP) | pieceBB(QUEEN);
	Bitboard rooks = pieceBB(ROOK) | pieceBB(QUEEN);

	int side = !Color[attacker];

	// Make captures until one side runs out, or fail to beat threshold
	while (true) {

		// Remove used pieces from attackers
		attackers &= occupied;

		Bitboard myAttackers = attackers & pos->occupancies[side];
		if (!myAttackers) break;

		// Pick next least valuable piece to capture with
		int  pt;
		for (pt = PAWN; pt <= KING; ++pt)
			if (myAttackers & pieceBB(pt))
				break;

		side = !side;

		// Value beats threshold, or can't beat threshold (negamaxed)
		if ((value = -value - 1 - PieceValue[pt]) >= 0) {

			if (pt == KING && (attackers & pos->occupancies[side]))
				side = !side;

			break;
		}

		// Remove the used piece from occupied
		occupied ^= (1ULL << (get_ls1b_index(myAttackers & pieceBB(pt))));
		if (pt == PAWN || pt == BISHOP || pt == QUEEN)
			attackers |= get_bishop_attacks(to, occupied) & bishops;
		if (pt == ROOK || pt == QUEEN)
			attackers |= get_rook_attacks(to, occupied) & rooks;

	}

	return side != Color[attacker];
}




// score moves
static inline void score_moves(S_Board* pos, S_MOVELIST* move_list, int PvMove)
{
	for (int i = 0;i < move_list->count;i++) {
		int move = move_list->moves[i].move;


		if (move == PvMove) {
			move_list->moves[i].score = 200000;
			continue;
		}


		else if (get_move_enpassant(move)) {

			move_list->moves[i].score = 105 + 100000;
			continue;
		}
		else if (get_move_capture(move)) {

			move_list->moves[i].score = mvv_lva[get_move_piece(move)][pos->pieces[get_move_target(move)]] + 40000 + 60000 * SEE(pos, move, -100);
			continue;

		}


		else if (pos->searchKillers[0][pos->ply] == move) {

			move_list->moves[i].score = 90000;
			continue;
		}

		else if (pos->searchKillers[1][pos->ply] == move) {

			move_list->moves[i].score = 80000;
			continue;

		}



		else if (move == CounterMoves[get_move_source(pos->history[pos->hisPly].move)][get_move_target(pos->history[pos->hisPly].move)]) {

			move_list->moves[i].score = 70000;
			continue;

		}

		else {

			move_list->moves[i].score = pos->searchHistory[pos->pieces[get_move_source(move)]][get_move_target(move)];
			continue;
		}


	}

	return;
}


int futility(int depth) {
	return 150 * (depth - 1);
}


int Quiescence(int alpha, int beta, S_Board* pos, S_SearchINFO* info, int pv_node)
{

	if ((info->nodes & 2047) == 0) {
		CheckUp(info);
	}


	// increment nodes count
	info->nodes++;

	if (((IsRepetition(pos)) && pos->ply) || (pos->fiftyMove >= 100) || MaterialDraw(pos)) {

		return 0;
	}


	if (pos->ply > MAXDEPTH - 1) {
		return EvalPosition(pos);
	}



	int PvMove = NOMOVE;
	int Score = EvalPosition(pos);

	// fail-hard beta cutoff
	if (Score >= beta)
	{
		// node (move) fails high
		return beta;
	}

	// found a better move
	if (Score > alpha)
	{
		// PV node (move)
		alpha = Score;

	}


	if (pos->ply && ProbeHashEntry(pos, &PvMove, &Score, alpha, beta, 0)) {
		HashTable->cut++;
		return Score;
	}


	// legal moves counter
	int legal_moves = 0;




	// create move list instance
	S_MOVELIST move_list[1];

	// generate moves
	generate_captures(move_list, pos);


	score_moves(pos, move_list, PvMove);

	// old value of alpha
	int old_alpha = alpha;
	int BestScore = -MAXSCORE;
	int bestmove = NOMOVE;

	int moves_searched = 0;

	// loop over moves within a movelist
	for (int count = 0; count < move_list->count; count++)
	{
		pick_move(move_list, count);

		int move = move_list->moves[count].move;
		// make sure to make only legal moves
		make_move(move, pos);


		// increment legal moves
		legal_moves++;

		Score = -Quiescence(-beta, -alpha, pos, info, pv_node);


		// take move back
		Unmake_move(pos);

		if (info->stopped == 1) return 0;

		moves_searched++;

		if (Score > BestScore) {

			BestScore = Score;
			bestmove = move;

			// found a better move
			if (Score > alpha)
			{

				// PV node (move)
				alpha = Score;
				bestmove = move;

			}


			// fail-hard beta cutoff
			if (Score >= beta)
			{
				StoreHashEntry(pos, bestmove, beta, HFBETA, 0, pv_node);
				// node (move) fails high
				return beta;
			}


		}

	}


	if (alpha != old_alpha) {
		StoreHashEntry(pos, bestmove, BestScore, HFEXACT, 0, pv_node);
	}
	else {
		StoreHashEntry(pos, bestmove, alpha, HFALPHA, 0, pv_node);
	}

	// node (move) fails low
	return alpha;
}


// full depth moves counter
const int full_depth_moves = 4;

// depth limit to consider reduction
const int reduction_limit = 3;

static inline int reduction(int depth, int num_moves) {

	return reductions[depth] * reductions[num_moves];

}

void updateHH(S_Board* pos, int depth, int bestmove, S_MOVELIST* quiet_moves) {

	for (int i = 0;i < quiet_moves->count;i++) {
		int move = quiet_moves->moves[i].move;
		if (move == bestmove)
			continue;
		else
		{
			pos->searchHistory[pos->pieces[get_move_source(move)]][get_move_target(move)] -= 1 << depth;
		}
	}



}

// negamax alpha beta search
int negamax(int alpha, int beta, int depth, S_Board* pos, S_SearchINFO* info, int DoNull, Stack* ss)
{

	// recursion escape condition
	if (depth <= 0) {
		int pv_node = beta - alpha > 1;
		return Quiescence(alpha, beta, pos, info, pv_node);
	}


	if ((info->nodes & 2047) == 0) {
		CheckUp(info);
	}


	// increment nodes count
	info->nodes++;

	if (((IsRepetition(pos)) && pos->ply) || (pos->fiftyMove >= 100) || MaterialDraw(pos)) {

		return 0;
	}


	if (pos->ply > MAXDEPTH - 1) {
		return EvalPosition(pos);
	}

	//Initialize the node
	ss->inCheck = is_square_attacked(pos, get_ls1b_index(pos->bitboards[KING + pos->side * 6]),
		pos->side ^ 1);
	int quietCount = 0;
	S_MOVELIST quiet_moves;
	quiet_moves.count = 0;
	ss->moveCount = 0;


	//Mate distance pruning
	int mating_value = mate_value - pos->ply;

	if (mating_value < beta) {
		beta = mating_value;
		if (alpha >= mating_value) return mating_value;
	}

	(ss + 1)->ttPv = false;
	(ss + 2)->killers[0] = NOMOVE;
	(ss + 2)->killers[1] = NOMOVE;
	(ss + 2)->cutoffCnt = 0;

	// is king in check
	int in_check = is_square_attacked(pos, get_ls1b_index(pos->bitboards[KING + pos->side * 6]),
		pos->side ^ 1);

	if (in_check) depth++;
	int pv_node = beta - alpha > 1;
	int PvMove = NOMOVE;
	int Score = -MAXSCORE;


	if (pos->ply && ProbeHashEntry(pos, &PvMove, &Score, alpha, beta, depth) && !pv_node) {
		HashTable->cut++;
		return Score;
	}


	// legal moves counter
	int legal_moves = 0;

	// get static evaluation score
	int static_eval = EvalPosition(pos);


	// evaluation pruning / static null move pruning
	if (depth < 3 && !pv_node && !in_check && abs(beta - 1) > -MAXSCORE + 100)
	{
		// define evaluation margin
		int eval_margin = 120 * depth;

		// evaluation margin substracted from static evaluation score fails high
		if (static_eval - eval_margin >= beta)
			// evaluation margin substracted from static evaluation score
			return static_eval - eval_margin;
	}


	//Reverse futility pruning (depth 8 limit was taken from stockfish)
	if (!pv_node && depth < 8 && static_eval - futility(depth) >= beta)
		return static_eval;


	// null move pruning
	if (DoNull && depth >= 4 && !in_check && pos->ply)
	{


		MakeNullMove(pos);
		/* search moves with reduced depth to find beta cutoffs
		   depth - 1 - R where R is a reduction limit */
		Score = -negamax(-beta, -beta + 1, depth - 4, pos, info, FALSE, ss);

		TakeNullMove(pos);

		// reutrn 0 if time is up
		if (info->stopped == 1) return 0;

		// fail-hard beta cutoff
		if (Score >= beta && abs(Score) < ISMATE)
			// node (position) fails high
			return beta;
	}



	// razoring
	if (!pv_node && !in_check && depth <= 3)
	{
		// get static eval and add first bonus
		Score = static_eval + 125;

		// define new score
		int new_score;

		// static evaluation indicates a fail-low node
		if (Score < beta)
		{
			// on depth 1
			if (depth == 1)
			{
				// get quiscence score
				new_score = Quiescence(alpha, beta, pos, info, pv_node);

				// return quiescence score if it's greater then static evaluation score
				return (new_score > Score) ? new_score : Score;
			}

			// add second bonus to static evaluation
			Score += 175;

			// static evaluation indicates a fail-low node
			if (Score < beta && depth <= 2)
			{
				// get quiscence score
				new_score = Quiescence(alpha, beta, pos, info, pv_node);

				// quiescence score indicates fail-low node
				if (new_score < beta)
					// return quiescence score if it's greater then static evaluation score
					return (new_score > Score) ? new_score : Score;
			}
		}
	}



	// create move list instance
	S_MOVELIST move_list[1];

	// generate moves
	generate_moves(move_list, pos);

	score_moves(pos, move_list, PvMove);

	// old value of alpha
	int old_alpha = alpha;
	int BestScore = -MAXSCORE;
	int bestmove = NOMOVE;

	int moves_searched = 0;

	// loop over moves within a movelist
	for (int count = 0; count < move_list->count; count++)
	{
		pick_move(move_list, count);

		int move = move_list->moves[count].move;
		if (!get_move_capture(move))
		{
			quiet_moves.moves[quiet_moves.count].move = move;
			quiet_moves.count++;
		}
		// make sure to make only legal moves
		make_move(move, pos);


		// increment legal moves
		legal_moves++;


		// full depth search
		if (moves_searched == 0)

			// do normal alpha beta search
			Score = -negamax(-beta, -alpha, depth - 1, pos, info, TRUE, ss);


		// late move reduction (LMR)
		else
		{
			// condition to consider LMR
			if (
				moves_searched >= full_depth_moves &&
				depth >= reduction_limit &&
				!in_check &&
				!get_move_capture(move) &&
				!get_move_promoted(move)
				)

			{
				int depth_reduction = reduction(depth, moves_searched);

				// search current move with reduced depth:
				Score = -negamax(-alpha - 1, -alpha, depth - depth_reduction, pos, info, TRUE, ss);
			}

			// hack to ensure that full-depth search is done
			else Score = alpha + 1;

			// principle variation search PVS
			if (Score > alpha)
			{

				Score = -negamax(-alpha - 1, -alpha, depth - 1, pos, info, TRUE, ss);


				if ((Score > alpha) && (Score < beta))

					Score = -negamax(-beta, -alpha, depth - 1, pos, info, TRUE, ss);
			}
		}




		// take move back
		Unmake_move(pos);

		if (info->stopped == 1) return 0;

		moves_searched++;

		if (Score > BestScore) {
			BestScore = Score;
			bestmove = move;

			// found a better move
			if (Score > alpha)
			{


				// store history moves
				if (!get_move_capture(move)) {
					if (depth > 1)pos->searchHistory[pos->pieces[get_move_source(move)]][get_move_target(move)] += 1<<depth;
				}

				// PV node (move)
				alpha = Score;
				bestmove = move;

			}



			// fail-hard beta cutoff
			if (Score >= beta)
			{

				if (!get_move_capture(move)) {
					// store killer moves
					pos->searchKillers[1][pos->ply] = pos->searchKillers[0][pos->ply];
					pos->searchKillers[0][pos->ply] = move;

					int previousMove = pos->history[pos->hisPly].move;
					CounterMoves[get_move_source(previousMove)][get_move_target(previousMove)] = move;


				}

				StoreHashEntry(pos, bestmove, beta, HFBETA, depth, pv_node);
				// node (move) fails high
				return beta;
			}


		}
	}
	if (BestScore >= beta && !get_move_capture(bestmove))
		updateHH(pos, depth, bestmove, &quiet_moves);

	// we don't have any legal moves to make in the current postion
	if (legal_moves == 0)
	{
		// king is in check
		if (in_check)
			// return mating score (assuming closest distance to mating position)
			return -mate_value + pos->ply;

		// king is not in check
		else
			// return stalemate score
			return 0;
	}

	if (alpha != old_alpha) {
		StoreHashEntry(pos, bestmove, BestScore, HFEXACT, depth, pv_node);
	}
	else {
		StoreHashEntry(pos, bestmove, alpha, HFALPHA, depth, pv_node);
	}


	// node (move) fails low
	return alpha;
}

void Root_search_position(int depth, S_Board* pos, S_SearchINFO* info) {
	//int num_threads = 1;

	S_MOVELIST move_list[1];
	//store one random legal move in case we can't calculate the best one in time
	generate_moves(move_list, pos);
	pos->pvArray[0] = move_list->moves[0].move;

	/*
	S_Board pos_copy = *pos;
	int initial_depth = 1;
	std::vector<std::thread> threads;
	std::vector<S_Board> pos_copies;
	for (int i = 0; i < num_threads - 1; i++) {
		pos_copies.push_back(*pos);
	}


	for (int i = 0;i < num_threads - 1;i++) {
		threads.push_back(std::thread(search_position, initial_depth + 1, depth, &pos_copies[i], info, FALSE)); // init help threads
	}
	*/
	search_position(1, depth, pos, info, TRUE); //root search 


	/*
	for (auto& th : threads) {
		th.join(); //stop helper threads
	}*/

}







// search position for the best move
void search_position(int start_depth, int final_depth, S_Board* pos, S_SearchINFO* info, int show)
{
	//Stack initialization taken from stockfish, it offers support for initializing continuation histories 
	//even if not needed as of now
	Stack stack[MAXGAMEMOVES + 10], * ss = stack + 7;
	(std::memset)(ss - 7, 0, 10 * sizeof(Stack));
	for (int i = 0; i <= MAXGAMEMOVES + 2; ++i)
		(ss + i)->ply = i;
	int score = 0;

	ClearForSearch(pos, info);
	int alpha_window = -50;

	int beta_window = 50;

	// define initial alpha beta bounds
	int alpha = -MAXSCORE;
	int beta = MAXSCORE;




	// find best move within a given position
	  // find best move within a given position
	for (int current_depth = start_depth; current_depth <= final_depth; current_depth++)
	{


		score = negamax(alpha, beta, current_depth, pos, info, TRUE, ss);

		// we fell outside the window, so try again with a full-width window (and the same depth)
		if ((score <= alpha)) {

			alpha = -MAXSCORE;
			current_depth--;
			continue;
		}


		// we fell outside the window, so try again with a full-width window (and the same depth)
		else if ((score >= beta)) {


			beta = MAXSCORE;
			current_depth--;
			continue;
		}

		// set up the window for the next iteration
		alpha = score + alpha_window;
		beta = score + beta_window;

		if (info->stopped == 1)
			// stop calculating and return best move so far 
			break;

		if (show) {
			if (score > -mate_value && score < -mate_score)
				printf("info score mate %d depth %d nodes %ld time %d pv ", -(score + mate_value) / 2, current_depth, info->nodes, GetTimeMs() - info->starttime);


			else if (score > mate_score && score < mate_value)
				printf("info score mate %d depth %d nodes %ld time %d pv ", (mate_value - score) / 2 + 1, current_depth, info->nodes, GetTimeMs() - info->starttime);

			else
				printf("info score cp %d depth %d nodes %ld  time %d pv ", score, current_depth, info->nodes, GetTimeMs() - info->starttime);

			int PvCount = GetPvLine(current_depth, pos);

			// loop over the moves within a PV line
			for (int count = 0; count < PvCount; count++)
			{
				// print PV move
				print_move(pos->pvArray[count]);
				printf(" ");
			}
			// print new line
			printf("\n");


		}

	}

	if (show) {

		// best move placeholder
		printf("bestmove ");
		print_move(pos->pvArray[0]);
		printf("\n");
	}
}




