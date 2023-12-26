#include "movepicker.h"
#include "move.h"
#include "movegen.h"
#include "piece_data.h"
#include "history.h"

// ScoreMoves takes a list of move as an argument and assigns a score to each move
void ScoreMoves(S_Board* pos, Search_data* sd, Search_stack* ss, S_MOVELIST* move_list, int ttMove) {
    // Loop through all the move in the movelist
    for (int i = 0; i < move_list->count; i++) {
        int move = move_list->moves[i].move;
        // If the move is from the TT (aka it's our hashmove) give it the highest score
        if (move == ttMove) {
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
        else if (move == sd->CounterMoves[From((ss - 1)->move)][To((ss - 1)->move)]) {
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


void PickMove(S_MOVELIST* moveList, const int moveNum) {
    int bestScore = -2147483645;
    int bestNum = moveNum;
    // starting at the number of the current move and stopping at the end of the list
    for (int index = moveNum; index < moveList->count; ++index) {
        // if we find a move with a better score than our bestmove we use that as the new best move
        if (moveList->moves[index].score > bestScore) {
            bestScore = moveList->moves[index].score;
            bestNum = index;
        }
    }
    // swap the move with the best score with the move in place moveNum
    S_MOVE temp = moveList->moves[moveNum];
    moveList->moves[moveNum] = moveList->moves[bestNum];
    moveList->moves[bestNum] = temp;
}


void InitMP(Movepicker* mp, S_Board* pos, Search_data* sd, Search_stack* ss, const int ttMove, const bool capturesOnly) {
    mp->pos = pos;
    mp->sd = sd;
    mp->ss = ss;
    mp->ttMove = ttMove;
    mp->idx = 0;
    mp->stage = GEN_MOVES;
    mp->capturesOnly = capturesOnly;
}

int NextMove(Movepicker* mp, const bool skipNonGood) {
top:
    if (mp->stage == GEN_MOVES) {
        if (mp->capturesOnly) {
            GenerateCaptures(mp->moveList, mp->pos);
        }
        else {
            GenerateMoves(mp->moveList, mp->pos);
        }
        ScoreMoves(mp->pos,mp->sd, mp->ss, mp->moveList, mp->ttMove);
        ++mp->stage;
        goto top;
    }
    else if (mp->stage == PICK_MOVES) {
        while (mp->idx < mp->moveList->count) {
            PickMove(mp->moveList, mp->idx);
            int move = mp->moveList->moves[mp->idx].move;
            int moveScore = mp->moveList->moves[mp->idx].score;
            ++mp->idx;
            if (skipNonGood && moveScore < goodCaptureScore)
                return NOMOVE;
            return move;
        }
    }
    return NOMOVE;
}