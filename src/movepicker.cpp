#include "movepicker.h"
#include "move.h"
#include "movegen.h"
#include "history.h"

// ScoreMoves takes a list of move as an argument and assigns a score to each move
void ScoreMoves(Movepicker* mp) {
    MoveList* moveList = &mp->moveList;
    Position* pos = mp->pos;
    SearchData* sd = mp->sd;
    SearchStack* ss = mp->ss;
    // Loop through all the move in the movelist
    for (int i = mp->idx; i < moveList->count; i++) {
        const Move move = moveList->moves[i].move;
        const int history = GetHistoryScore(pos, ss, sd, move);
        if (isTactical(move)) {
            int moveDelta = isPromo(move)     ? SEEValue[getPromotedPiecetype(move)] - SEEValue[PAWN]
                          : isEnpassant(move) ? SEEValue[PAWN]
                                              : SEEValue[GetPieceType(pos->PieceOn(To(move)))];
            moveList->moves[i].score = moveDelta * 64 + history;
        }
        else {
            moveList->moves[i].score = history;
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

void InitMP(Movepicker* mp, Position* pos, SearchData* sd, SearchStack* ss, const Move ttMove, const MovepickerType movepickerType) {

    mp->movepickerType = movepickerType;
    mp->pos = pos;
    mp->sd = sd;
    mp->ss = ss;
    mp->ttMove = ttMove;
    mp->idx = 0;
    mp->stage = mp->ttMove ? PICK_TT : GEN_TACTICAL;
}

Move NextMove(Movepicker* mp, const bool skip) {
    top:
    if (skip) {
        // In search, the skip variable is used to dictate whether we skip quiet moves
        if (   mp->movepickerType == SEARCH
            && mp->stage > PICK_GOOD_TACTICAL
            && mp->stage < GEN_BAD_TACTICAL) {
            mp->stage = GEN_BAD_TACTICAL;
        }

        // In qsearch, the skip variable is used to dictate whether we skip quiet moves and bad captures
        if (   mp->movepickerType == QSEARCH
            && mp->stage > PICK_GOOD_TACTICAL) {
            return NOMOVE;
        }
    }
    switch (mp->stage) {
    case PICK_TT:
        ++mp->stage;
        // If we are in qsearch and not in check, skip quiet TT moves
        if (mp->movepickerType == QSEARCH && skip && !isTactical(mp->ttMove))
            goto top;

        // If the TT move if not pseudo legal we skip it too
        if (!IsPseudoLegal(mp->pos, mp->ttMove))
            goto top;

        return mp->ttMove;

    case GEN_TACTICAL:
        GenerateMoves(&mp->moveList, mp->pos, MOVEGEN_NOISY);
        ScoreMoves(mp);
        ++mp->stage;
        goto top;

    case PICK_GOOD_TACTICAL:
        while (mp->idx < mp->moveList.count) {
            partialInsertionSort(&mp->moveList, mp->idx);
            const Move move = mp->moveList.moves[mp->idx].move;
            const int score = mp->moveList.moves[mp->idx].score;
            ++mp->idx;
            if (move == mp->ttMove)
                continue;

            assert(isTactical(move));

            if (!SEE(mp->pos, move, -108)) {
                AddMove(move, score, &mp->badCaptureList);
                continue;
            }

            return move;
        }
        ++mp->stage;
        goto top;

    case GEN_QUIETS:
        GenerateMoves(&mp->moveList, mp->pos, MOVEGEN_QUIET);
        ScoreMoves(mp);
        ++mp->stage;
        goto top;

    case PICK_QUIETS:
        while (mp->idx < mp->moveList.count) {
            partialInsertionSort(&mp->moveList, mp->idx);
            const Move move = mp->moveList.moves[mp->idx].move;
            ++mp->idx;
            if (move == mp->ttMove)
                continue;

            assert(!isTactical(move));
            return move;
        }
        ++mp->stage;
        goto top;

    case GEN_BAD_TACTICAL:
        // Nothing to generate lol, just reset mp->idx
        mp->idx = 0;
        ++mp->stage;
        goto top;

    case PICK_BAD_TACTICAL:
        while (mp->idx < mp->badCaptureList.count) {
            partialInsertionSort(&mp->badCaptureList, mp->idx);
            const Move move = mp->badCaptureList.moves[mp->idx].move;
            ++mp->idx;
            if (move == mp->ttMove)
                continue;

            assert(isTactical(move));
            return move;
        }
        return NOMOVE;

    default:
        // we should never end up here because a movepicker stage should be always be valid and accounted for
        assert(false);
        __builtin_unreachable();
    }
}
