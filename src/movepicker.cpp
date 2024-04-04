#include "movepicker.h"
#include "move.h"
#include "movegen.h"
#include "piece_data.h"
#include "history.h"

// ScoreMoves takes a list of move as an argument and assigns a score to each move
void ScoreMoves(Movepicker* mp) {
    MoveList* moveList = &mp->moveList;
    Position* pos = mp->pos;
    SearchData* sd = mp->sd;
    SearchStack* ss = mp->ss;
    // Loop through all the move in the movelist
    for (int i = 0; i < moveList->count; i++) {
        int move = moveList->moves[i].move;
        if (isTactical(move)) {
            // Score by most valuable victim and capthist
            int capturedPiece = isEnpassant(move) ? PAWN : GetPieceType(pos->PieceOn(To(move)));
            // If we captured an empty piece this means the move is a non capturing promotion, we can pretend we captured a pawn to use a slot of the table that would've otherwise went unused (you can't capture pawns on the 1st/8th rank)
            if (capturedPiece == EMPTY) capturedPiece = PAWN;
            moveList->moves[i].score = SEEValue[capturedPiece] * 32 + GetCapthistScore(pos, sd, move);
        }
        else {
            moveList->moves[i].score = GetHistoryScore(pos, sd, move, ss);
        }
    }
}

void partialInsertionSort(MoveList* moveList, const int moveNum) {
    int bestScore = moveList->moves[moveNum].score;
    int bestNum = moveNum;
    // starting at the number of the current move and stopping at the end of the list
    for (int index = moveNum + 1; index < moveList->count; ++index) {
        // if we find a move with a better score than our bestmove we use that as the new best move
        if (moveList->moves[index].score > bestScore) {
            bestScore = moveList->moves[index].score;
            bestNum = index;
        }
    }
    // swap the move with the best score with the move in place moveNum
    std::swap(moveList->moves[moveNum], moveList->moves[bestNum]);
}

void InitMP(Movepicker* mp, Position* pos, SearchData* sd, SearchStack* ss, const int ttMove, const bool capturesOnly, const int SEEThreshold) {
    nnue.update(pos->AccumulatorTop(), pos->NNUEAdd, pos->NNUESub);
    mp->pos = pos;
    mp->sd = sd;
    mp->ss = ss;
    mp->ttMove = (!capturesOnly || isTactical(ttMove)) && IsPseudoLegal(pos, ttMove) ? ttMove : NOMOVE;
    mp->idx = 0;
    mp->stage = mp->ttMove ? PICK_TT : GEN_NOISY;
    mp->capturesOnly = capturesOnly;
    mp->SEEThreshold = SEEThreshold;
    mp->killer0 = ss->searchKillers[0];
    mp->killer1 = ss->searchKillers[1];
    mp->counter = sd->counterMoves[From((ss - 1)->move)][To((ss - 1)->move)];
}

int NextMove(Movepicker* mp, const bool skipNonGood) {
    top:
    switch (mp->stage) {
    case PICK_TT:
        ++mp->stage;
        return mp->ttMove;

    case GEN_NOISY:
        GenerateMoves(&mp->moveList, mp->pos, MOVEGEN_NOISY);
        ScoreMoves(mp);
        ++mp->stage;
        [[fallthrough]];

    case PICK_GOOD_NOISY:
        while (mp->idx < mp->moveList.count) {
            partialInsertionSort(&mp->moveList, mp->idx);
            const int move = mp->moveList.moves[mp->idx].move;
            const int score = mp->moveList.moves[mp->idx].score;
            const int SEEThreshold = mp->SEEThreshold != SCORE_NONE ? mp->SEEThreshold : -score / 64;
            ++mp->idx;
            if (move == mp->ttMove)
                continue;

            if (!SEE(mp->pos, move, SEEThreshold)) {
                AddMove(move, score, &mp->badCaptureList);
                continue;
            }

            return move;
        }
        ++mp->stage;
        [[fallthrough]];

    case PICK_KILLER_0:
        if (mp->capturesOnly) {
            mp->stage = GEN_BAD_NOISY;
            goto top;
        }
        ++mp->stage;
        if (!isTactical(mp->killer0) && IsPseudoLegal(mp->pos, mp->killer0))
            return mp->killer0;

        [[fallthrough]];

    case PICK_KILLER_1:
        if (mp->capturesOnly) {
            mp->stage = GEN_BAD_NOISY;
            goto top;
        }
        ++mp->stage;
        if (!isTactical(mp->killer1) && IsPseudoLegal(mp->pos, mp->killer1))
            return mp->killer1;

        [[fallthrough]];

    case PICK_COUNTER:
        if (mp->capturesOnly) {
            mp->stage = GEN_BAD_NOISY;
            goto top;
        }
        ++mp->stage;
        if (!isTactical(mp->counter) && IsPseudoLegal(mp->pos, mp->counter))
            return mp->counter;

        [[fallthrough]];

    case GEN_QUIETS:
        if (mp->capturesOnly) {
            mp->stage = GEN_BAD_NOISY;
            goto top;
        }
        GenerateMoves(&mp->moveList, mp->pos, MOVEGEN_QUIET);
        ++mp->stage;
        [[fallthrough]];

    case PICK_QUIETS:
        while (mp->idx < mp->moveList.count) {
            partialInsertionSort(&mp->moveList, mp->idx);
            const int move = mp->moveList.moves[mp->idx].move;
            ++mp->idx;
            if (   move == mp->ttMove
                || move == mp->killer0
                || move == mp->killer1
                || move == mp->counter)
                continue;

            return move;
        }
        ++mp->stage;
        [[fallthrough]];

    case GEN_BAD_NOISY:
        // Nothing to generate lol, just reset mp->idx
        mp->idx = 0;
        ++mp->stage;
        [[fallthrough]];

    case PICK_BAD_NOISY:
        while (mp->idx < mp->badCaptureList.count) {
            partialInsertionSort(&mp->badCaptureList, mp->idx);
            const int move = mp->badCaptureList.moves[mp->idx].move;
            ++mp->idx;
            if (move == mp->ttMove)
                continue;

            if (skipNonGood)
                return NOMOVE;

            return move;
        }
        return NOMOVE;
    }
    return NOMOVE;
}
