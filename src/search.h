#include "Board.h"
#include "move.h"
#include "movegen.h"
#include "makemove.h"
#include "PieceData.h"
#include "io.h"
#include "eval.h"
#include "misc.h"

void CheckUp(S_SearchINFO* info);


void ClearForSearch(S_Board* pos, S_SearchINFO* info);


//int sort_moves(S_MOVELIST* move_list, S_Board* pos);
int Quiescence(int alpha, int beta, S_Board* pos, S_SearchINFO* info);

// negamax alpha beta search
int negamax(int alpha, int beta, int depth, S_Board* pos, S_SearchINFO* info, int DoNull);


void Root_search_position(int depth, S_Board* pos, S_SearchINFO* info);



// search position for the best move
void search_position(int start_depth, int final_depth, S_Board* pos, S_SearchINFO* info, int show);

void  pick_move(S_MOVELIST* move_list, int moveNum);
