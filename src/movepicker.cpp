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
        if (isTactical(move)) {
            int capturedPiece = isPromo(move)     ? getPromotedPiecetype(move)
                              : isEnpassant(move) ? PAWN
                                                  : GetPieceType(pos->PieceOn(To(move)));
            moveList->moves[i].score = SEEValue[capturedPiece] * 32 - SEEValue[Piece(move)];
        }
        else {
            moveList->moves[i].score = 0;
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
    mp->stage = mp->ttMove ? PICK_TT : GEN_NOISY;
}

Move NextMove(Movepicker* mp, const bool skip) {
    top:
    if (skip) {
        // In search, the skip variable is used to dictate whether we skip quiet moves
        if (   mp->movepickerType == SEARCH
            && mp->stage > PICK_GOOD_NOISY
            && mp->stage < GEN_BAD_NOISY) {
            mp->stage = GEN_BAD_NOISY;
        }

        // In qsearch, the skip variable is used to dictate whether we skip quiet moves and bad captures
        if (   mp->movepickerType == QSEARCH
            && mp->stage > PICK_GOOD_NOISY) {
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

    case GEN_NOISY:
        GenerateMoves(&mp->moveList, mp->pos, MOVEGEN_NOISY);
        ScoreMoves(mp);
        ++mp->stage;
        goto top;

    case PICK_GOOD_NOISY:
        while (mp->idx < mp->moveList.count) {
            partialInsertionSort(&mp->moveList, mp->idx);
            const Move move = mp->moveList.moves[mp->idx].move;
            const int score = mp->moveList.moves[mp->idx].score;
            ++mp->idx;
            if (move == mp->ttMove)
                continue;

            assert(isTactical(move));

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

    case GEN_BAD_NOISY:
        // Nothing to generate lol, just reset mp->idx
        mp->idx = 0;
        ++mp->stage;
        goto top;

    case PICK_BAD_NOISY:
        return NOMOVE;

    default:
        // we should never end up here because a movepicker stage should be always be valid and accounted for
        assert(false);
        __builtin_unreachable();
    }
}
