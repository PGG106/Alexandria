#include "movepicker.h"
#include "move.h"
#include "movegen.h"
#include "piece_data.h"
#include "history.h"

// ScoreMoves takes a list of move as an argument and assigns a score to each move
void ScoreMoves(Movepicker* mp) {
    S_MOVELIST* moveList = mp->moveList;
    S_Board* pos = mp->pos;
    Search_data* sd = mp->sd;
    Search_stack* ss = mp->ss;
    // Loop through all the move in the movelist
    for (int i = 0; i < moveList->count; i++) {
        int move = moveList->moves[i].move;
        // We have no need to sort the TT move first since the ttmove always gets played before movegen if it's an acceptable move.
        // start sorting from the promotions based on the promoted piece type.
         if (isPromo(move)) {
            switch (getPromotedPiecetype(move)) {
            case QUEEN:
                moveList->moves[i].score = queenPromotionScore;
                break;
            case KNIGHT:
                moveList->moves[i].score = knightPromotionScore;
                break;
            case ROOK:
                moveList->moves[i].score = badPromotionScore;
                break;
            case BISHOP:
                moveList->moves[i].score = badPromotionScore;
                break;
            default:
                break;
            }
        }
        else if (IsCapture(move)) {
            // Good captures get played before any move that isn't a promotion or a TT move
            if (SEE(pos, move, mp->SEEThreshold)) {
                int captured_piece = isEnpassant(move) ? PAWN : GetPieceType(pos->PieceOn(To(move)));
                // Sort by most valuable victim and capthist, with LVA as tiebreaks
                moveList->moves[i].score = mvv_lva[GetPieceType(Piece(move))][captured_piece] + GetCapthistScore(pos, sd, move) + goodCaptureScore;
            }
            // Bad captures are always played last, no matter how bad the history score of a move is, it will never be played after a bad capture
            else {
                int captured_piece = isEnpassant(move) ? PAWN : GetPieceType(pos->PieceOn(To(move)));
                // Sort by most valuable victim and capthist, with LVA as tiebreaks
                moveList->moves[i].score = badCaptureScore + mvv_lva[GetPieceType(Piece(move))][captured_piece] + GetCapthistScore(pos, sd, move);
            }
            continue;
        }
        // First killer move always comes after the TT move,the promotions and the good captures and before anything else
        else if (move == mp->killer0) {
            moveList->moves[i].score = killerMoveScore0;
            continue;
        }
        // Second killer move always comes after the first one
        else if (move == mp->killer1) {
            moveList->moves[i].score = killerMoveScore1;
            continue;
        }
        // After the killer moves try the Counter moves
        else if (move == mp->counter) {
            moveList->moves[i].score = counterMoveScore;
            continue;
        }
        // if the move isn't in any of the previous categories score it according to the history heuristic
        else {
            moveList->moves[i].score = GetHistoryScore(pos, sd, move, ss);
            continue;
        }
    }
}

void partialInsertionSort(S_MOVELIST* moveList, const int moveNum) {
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

void InitMP(Movepicker* mp, S_Board* pos, Search_data* sd, Search_stack* ss, const int ttMove, const bool capturesOnly, const int SEEThreshold) {
    nnue.update(pos->AccumulatorTop(), pos->NNUEAdd, pos->NNUESub);
    mp->pos = pos;
    mp->sd = sd;
    mp->ss = ss;
    mp->ttMove = (!capturesOnly || !IsQuiet(ttMove)) && IsLegal(pos, ttMove) ? ttMove : NOMOVE;
    mp->idx = 0;
    mp->stage = mp->ttMove ? PICK_TT : GEN_MOVES;
    mp->capturesOnly = capturesOnly;
    mp->SEEThreshold = SEEThreshold;
    mp->killer0 = ss->searchKillers[0];
    mp->killer1 = ss->searchKillers[1];
    mp->counter = sd->CounterMoves[From((ss - 1)->move)][To((ss - 1)->move)];
}

int NextMove(Movepicker* mp, const bool skipNonGood) {
    switch (mp->stage) {
    case PICK_TT:
        ++mp->stage;
        return mp->ttMove;

    case GEN_MOVES: {
        if (mp->capturesOnly) {
            GenerateCaptures(mp->moveList, mp->pos);
        }
        else {
            GenerateMoves(mp->moveList, mp->pos);
        }
        ScoreMoves(mp);
        ++mp->stage;
        [[fallthrough]];
    }
    case PICK_MOVES: {
        while (mp->idx < mp->moveList->count) {
            partialInsertionSort(mp->moveList, mp->idx);
            const int move = mp->moveList->moves[mp->idx].move;
            ++mp->idx;
            if (move == mp->ttMove)
                continue;

            if (skipNonGood && mp->moveList->moves[mp->idx-1].score < goodCaptureScore)
                return NOMOVE;

            return move;
        }
        return NOMOVE;
    }
    }
    return NOMOVE;
}
