#include "history.h"
#include <cstring>

void updateHHScore(const S_Board* pos, Search_data* ss, int move, int bonus)
{
	//Scale bonus to fix it in a [-32768;32768] range
	int scaled_bonus = bonus - GetHHScore(pos, ss, move) * std::abs(bonus) / 32768;
	//Update move score
	ss->searchHistory[pos->pieces[From(move)]]
		[To(move)] += scaled_bonus;
}

void updateCHScore(const S_Board* pos, Search_data* sd, const Search_stack* ss, const int move, const int bonus)
{
	//Scale bonus to fix it in a [-32768;32768] range
	int scaled_bonus = bonus - GetCHScore(pos, sd, ss, move) * std::abs(bonus) / 32768;
	//Update move score
	if (pos->ply > 0)
	{
		sd->cont_hist[get_move_piece((ss - 1)->move)][To((ss - 1)->move)]
			[get_move_piece(move)][To(move)] += scaled_bonus;
		//Score followup
		if (pos->ply > 1)
		{
			sd->cont_hist[get_move_piece((ss - 2)->move)][To((ss - 2)->move)]
				[get_move_piece(move)][To(move)] += scaled_bonus;
		}
	}

}

//Update the history heuristics of all the quiet moves passed to the function
void UpdateHH(const S_Board* pos, Search_data* ss, const int depth, const int bestmove, const S_MOVELIST* quiet_moves) {
	//define the history bonus
	int bonus = std::min(16 * depth * depth, 1200);
	//increase bestmove HH score
	updateHHScore(pos, ss, bestmove, bonus);
	//Loop through all the quiet moves
	for (int i = 0; i < quiet_moves->count; i++)
	{
		//For all the quiets moves that didn't cause a cut-off decrease the HH score
		int move = quiet_moves->moves[i].move;
		if (move == bestmove) continue;
		updateHHScore(pos, ss, move, -bonus);
	}
}

//Update the history heuristics of all the quiet moves passed to the function
void UpdateCH(const S_Board* pos, Search_data* sd, const Search_stack* ss, const int depth, const int bestmove, const S_MOVELIST* quiet_moves)
{
	//define the conthist bonus
	int bonus = std::min(16 * depth * depth, 1200);
	//increase bestmove CH score
	updateCHScore(pos, sd, ss, bestmove, bonus);
	//Loop through all the quiet moves
	for (int i = 0; i < quiet_moves->count; i++)
	{
		//For all the quiets moves that didn't cause a cut-off decrease the CH score
		int move = quiet_moves->moves[i].move;
		if (move == bestmove) continue;
		updateCHScore(pos, sd, ss, move, -bonus);
	}
}


//Returns the history score of a move
int GetHHScore(const S_Board* pos, const Search_data* sd, const int  move) {
	return sd->searchHistory[pos->pieces[From(move)]][To(move)];
}

int GetHistoryScore(const S_Board* pos, const Search_data* sd, const int  move, const Search_stack* ss) {
	return GetHHScore(pos, sd, move) + GetCHScore(pos, sd, ss, move);
}

//Returns the history score of a move
int GetCHScore(const S_Board* pos, const Search_data* sd, const Search_stack* ss, const int  move)
{
	int previous_move = pos->ply >= 1 ? (ss - 1)->move : NOMOVE;
	int previous_previous_move = pos->ply >= 2 ? (ss - 2)->move : NOMOVE;
	return pos->ply > 0 ? sd->cont_hist[get_move_piece(previous_move)][To(previous_move)]
		[get_move_piece(move)][To(move)] : 0 +
		pos->ply > 1 ? sd->cont_hist[get_move_piece(previous_previous_move)][To(previous_previous_move)]
		[get_move_piece(move)][To(move)] : 0;
}
//Resets the history table
void CleanHistories(Search_data* ss) {
	//For every piece [12] moved to every square [64] we reset the searchHistory value
	for (int index = 0; index < 12; ++index) {
		for (int index2 = 0; index2 < 64; ++index2) {
			ss->searchHistory[index][index2] = 0;
		}
	}
	std::memset(ss->cont_hist, 0, sizeof(ss->cont_hist));
}
