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


int CounterMoves[MAXDEPTH][MAXDEPTH];


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
		if (repetition_table[index] == pos->posKey)
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



// score moves
static inline void score_moves(S_Board* pos, S_MOVELIST* move_list)
{
	for (int i = 0;i < move_list->count;i++) {
		int move = move_list->moves[i].move;

		if (get_move_enpassant(move)) {

			move_list->moves[i].score = 105 + 10000;
			continue;
		}



		else if (get_move_capture(move)) {

			move_list->moves[i].score = mvv_lva[get_move_piece(move)][pos->pieces[get_move_target(move)]] + 10000;
			continue;

		}


		else if (pos->searchKillers[0][pos->ply] == move) {

			move_list->moves[i].score = 9000;
			continue;
		}

		else if (pos->searchKillers[1][pos->ply] == move) {

			move_list->moves[i].score = 8000;
			continue;

		}



		else if (move == CounterMoves[get_move_source(pos->history[pos->hisPly].move)][get_move_target(pos->history[pos->hisPly].move)]) {

			move_list->moves[i].score = 7000;
			continue;

		}

		else {

			move_list->moves[i].score = pos->searchHistory[pos->pieces[get_move_source(move)]][get_move_target(move)];
			continue;
		}


	}


	return;
}




void  pick_move(S_MOVELIST* move_list, int moveNum)
{

	S_MOVE temp;
	int index = 0;
	int bestScore = 0;
	int bestNum = moveNum;


	for (index = moveNum; index < move_list->count; ++index) {

		if (move_list->moves[index].score > bestScore) {
			bestScore = move_list->moves[index].score;
			bestNum = index;
		}
	}

	temp = move_list->moves[moveNum];
	move_list->moves[moveNum] = move_list->moves[bestNum];
	move_list->moves[bestNum] = temp;

}

int futility(int depth) {
	return 150 * (depth - 1);
}

int Quiescence(int alpha, int beta, S_Board* pos, S_SearchINFO* info)
{

	if ((info->nodes & 2047) == 0) {
		CheckUp(info);
	}


	info->nodes++;

	if (pos->ply && IsRepetition(pos)) {
		return 0;
	}

	if (pos->ply > MAXDEPTH - 1) {
		// evaluate position
		return EvalPosition(pos);
	}


	// evaluate position
	int score = EvalPosition(pos);



	// fail-hard beta cutoff
	if (score >= beta)
	{
		// node (move) fails high
		return beta;
	}

	// found a better move
	if (score > alpha)
	{
		// PV node (move)
		alpha = score;

	}

	// create move list instance
	S_MOVELIST move_list[1];

	// generate moves
	generate_captures(move_list, pos);
	score_moves(pos, move_list);
	int legal = 0;
	score = -MAXSCORE;




	// loop over moves within a movelist
	for (int count = 0; count < move_list->count; count++)
	{


		pick_move(move_list, count);


		make_move(move_list->moves[count].move, pos);

		repetition_table[pos->hisPly] = pos->posKey;

		legal++;
		// score current move
		int score = -Quiescence(-beta, -alpha, pos, info);


		// take move back
		Unmake_move(pos);


		// return latest best score  if time is up
		if (info->stopped == 1) return 0;


		// fail-hard beta cutoff
		if (score >= beta)
		{

			// node (move) fails high
			return beta;
		}

		// found a better move
		if (score > alpha)
		{
			// PV node (move)
			alpha = score;


		}
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

// negamax alpha beta search
int negamax(int alpha, int beta, int depth, S_Board* pos, S_SearchINFO* info, int DoNull)
{

	// init PV length
	// 
	// recursion escape condition
	if (depth <= 0) {

		return Quiescence(alpha, beta, pos, info);
	}


	if ((info->nodes & 2047) == 0) {
		CheckUp(info);
	}


	// increment nodes count
	info->nodes++;

	if ((IsRepetition(pos)) && pos->ply) {

		return 0;
	}


	if (pos->fiftyMove >= 100) {
		return 0;
	}

	if (pos->ply > MAXDEPTH - 1) {
		return EvalPosition(pos);
	}

	int mating_value = mate_value - pos->ply;

	if (mating_value < beta) {
		beta = mating_value;
		if (alpha >= mating_value) return mating_value;
	}

	// is king in check
	int in_check = is_square_attacked(pos, get_ls1b_index(pos->bitboards[pos->side * 6 + 5]),
		pos->side ^ 1);

	if (in_check) depth++;



	int PvMove = NOMOVE;
	int Score = -MAXSCORE;
	int pv_node = beta - alpha > 1;

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
		repetition_table[pos->hisPly] = pos->posKey;

		MakeNullMove(pos);
		/* search moves with reduced depth to find beta cutoffs
		   depth - 1 - R where R is a reduction limit */
		Score = -negamax(-beta, -beta + 1, depth - 4, pos, info, FALSE);

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
				new_score = Quiescence(alpha, beta, pos, info);

				// return quiescence score if it's greater then static evaluation score
				return (new_score > Score) ? new_score : Score;
			}

			// add second bonus to static evaluation
			Score += 175;

			// static evaluation indicates a fail-low node
			if (Score < beta && depth <= 2)
			{
				// get quiscence score
				new_score = Quiescence(alpha, beta, pos, info);

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


	score_moves(pos, move_list);

	// old value of alpha
	int old_alpha = alpha;
	int BestScore = -MAXSCORE;
	int bestmove = NOMOVE;
	int MoveNum = 0;


	if (PvMove != NOMOVE) {
		for (MoveNum = 0; MoveNum < move_list->count; ++MoveNum) {
			if (move_list->moves[MoveNum].move == PvMove) {
				move_list->moves[MoveNum].score = 2000000;
				break;
			}
		}
	}



	int moves_searched = 0;

	// loop over moves within a movelist
	for (int count = 0; count < move_list->count; count++)
	{
		pick_move(move_list, count);


		// make sure to make only legal moves
		make_move(move_list->moves[count].move, pos);


		// increment legal moves
		legal_moves++;



		// full depth search
		if (moves_searched == 0)

			// do normal alpha beta search
			Score = -negamax(-beta, -alpha, depth - 1, pos, info, TRUE);


		// late move reduction (LMR)
		else
		{
			// condition to consider LMR
			if (
				moves_searched >= full_depth_moves &&
				depth >= reduction_limit &&
				!in_check &&
				!get_move_capture(move_list->moves[count].move) &&
				!get_move_promoted(move_list->moves[count].move)
				)

			{
				int depth_reduction = reduction(depth, moves_searched);

				// search current move with reduced depth:
				Score = -negamax(-alpha - 1, -alpha, depth - depth_reduction, pos, info, TRUE);
			}

			// hack to ensure that full-depth search is done
			else Score = alpha + 1;

			// principle variation search PVS
			if (Score > alpha)
			{

				Score = -negamax(-alpha - 1, -alpha, depth - 1, pos, info, TRUE);


				if ((Score > alpha) && (Score < beta))

					Score = -negamax(-beta, -alpha, depth - 1, pos, info, TRUE);
			}
		}




		// take move back
		Unmake_move(pos);

		if (info->stopped == 1) return 0;

		moves_searched++;

		if (Score > BestScore) {
			BestScore = Score;
			bestmove = move_list->moves[count].move;

			// found a better move
			if (Score > alpha)
			{


				// store history moves
				if (!get_move_capture(move_list->moves[count].move)) {
					pos->searchHistory[pos->pieces[get_move_source(move_list->moves[count].move)]][get_move_target(move_list->moves[count].move)] += depth;
				}

				// PV node (move)
				alpha = Score;
				bestmove = move_list->moves[count].move;

			}



			// fail-hard beta cutoff
			if (Score >= beta)
			{

				if (!get_move_capture(move_list->moves[count].move)) {
					// store killer moves
					pos->searchKillers[1][pos->ply] = pos->searchKillers[0][pos->ply];
					pos->searchKillers[0][pos->ply] = move_list->moves[count].move;

					int previousMove = pos->history[pos->hisPly].move;
					CounterMoves[get_move_source(previousMove)][get_move_target(previousMove)] = move_list->moves[count].move;


				}

				StoreHashEntry(pos, bestmove, beta, HFBETA, depth);
				// node (move) fails high
				return beta;
			}


		}
	}
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
		StoreHashEntry(pos, bestmove, BestScore, HFEXACT, depth);
	}
	else {
		StoreHashEntry(pos, bestmove, alpha, HFALPHA, depth);
	}


	// node (move) fails low
	return alpha;
}

void Root_search_position(int depth, S_Board* pos, S_SearchINFO* info) {
	int num_threads = 1;
	ClearForSearch(pos, info);
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

	search_position(1, depth, pos, info, TRUE); //root search 

	for (auto& th : threads) {
		th.join(); //stop helper threads
	}

}







// search position for the best move
void search_position(int start_depth, int final_depth, S_Board* pos, S_SearchINFO* info, int show)
{
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


		score = negamax(alpha, beta, current_depth, pos, info, TRUE);

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




