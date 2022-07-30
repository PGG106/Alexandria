#pragma once
#include "Board.h"
#include "stdlib.h"

int GetPvLine(const int depth, S_Board* pos);

void ClearHashTable(S_HASHTABLE* table);
//Initialize an Hashtable of size MB
void InitHashTable(S_HASHTABLE* table, int MB);
/// <summary>
/// 
/// </summary>
/// <param name="pos"></param>
/// <param name="alpha"></param>
/// <param name="beta"></param>
/// <param name="depth"></param>
/// <param name="tte"></param>
/// <returns></returns>
int ProbeHashEntry(S_Board* pos, int alpha, int beta, int depth,
	S_HASHENTRY* tte);
/// <summary>
/// Store a move in the TT
/// </summary>
/// <param name="pos">The position from which the move can the played</param>
/// <param name="move">The move we have to store</param>
/// <param name="score">The search score of the move</param>
/// <param name="flags">a flag that represents if the score is exact or an alpha/beta bound</param>
/// <param name="depth"><The depth we've searched the move at/param>
/// <param name="pv">if the node we've found the move in was or not a pv node</param>
void StoreHashEntry(S_Board* pos, const int move, int score, const int flags,
	const int depth, const bool pv);
int ProbePvMove(S_Board* pos);
