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
    bool rootNode = mp->rootNode;
    // Loop through all the move in the movelist
    for (int i = mp->idx; i < moveList->count; i++) {
        const Move move = moveList->moves[i].move;
        if (isTactical(move)) {
            // Score by most valuable victim and capthist
            int capturedPiece = isEnpassant(move) ? PAWN : GetPieceType(pos->PieceOn(To(move)));
            // If we captured an empty piece this means the move is a non capturing promotion, we can pretend we captured a pawn to use a slot of the table that would've otherwise went unused (you can't capture pawns on the 1st/8th rank)
            if (capturedPiece == EMPTY) capturedPiece = PAWN;
            moveList->moves[i].score = SEEValue[capturedPiece] * 16 + GetCapthistScore(pos, sd, move);
        }
        else {
            moveList->moves[i].score = GetHistoryScore(pos, sd, move, ss, rootNode);
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

void InitMP(Movepicker* mp, Position* pos, SearchData* sd, SearchStack* ss, const Move ttMove, const int SEEThreshold, const MovepickerType movepickerType, const bool rootNode) {

    const Move killer = ss->searchKiller;
    const Move counter = sd->counterMoves[FromTo((ss - 1)->move)];

    mp->movepickerType = movepickerType;
    mp->pos = pos;
    mp->sd = sd;
    mp->ss = ss;
    mp->ttMove = ttMove;
    mp->idx = 0;
    mp->rootNode = rootNode;
    mp->stage = mp->ttMove ? PICK_TT : GEN_NOISY;
    mp->killer = killer != ttMove ? killer : NOMOVE;
    mp->counter = counter != ttMove && counter != killer ? counter : NOMOVE;
    mp->SEEThreshold = SEEThreshold;
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

        // In probcut, we only search captures that pass the threshold
        if (   mp->movepickerType == PROBCUT
               && mp->stage > PICK_GOOD_NOISY) {
            return NOMOVE;
        }
    }
    switch (mp->stage) {
    case PICK_TT:
        ++mp->stage;
            // If we are in qsearch and not in check, or we are in probcut, skip quiet TT moves
            if ((mp->movepickerType == PROBCUT || (mp->movepickerType == QSEARCH && skip))
                && !isTactical(mp->ttMove))
                goto top;

        // If the TT move if not pseudo legal we skip it too
        if (!IsPseudoLegal(mp->pos, mp->ttMove))
            goto top;

        // If we are in probcut and the TT move does not pass SEE, we skip it
        if (mp->movepickerType == PROBCUT && !SEE(mp->pos, mp->ttMove, -1))
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
            const int SEEThreshold =  mp->movepickerType == PROBCUT ? mp->SEEThreshold : -score / 32 + 236;
            ++mp->idx;
            if (move == mp->ttMove)
                continue;

            if (!SEE(mp->pos, move, SEEThreshold)) {
                AddMove(move, score, &mp->badCaptureList);
                continue;
            }

            assert(isTactical(move));

            return move;
        }
        ++mp->stage;
        goto top;

    case PICK_KILLER:
        ++mp->stage;
        if (IsPseudoLegal(mp->pos, mp->killer))
            return mp->killer;
        goto top;

    case PICK_COUNTER:
        ++mp->stage;
        if (IsPseudoLegal(mp->pos, mp->counter))
            return mp->counter;
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
            if (   move == mp->ttMove
                || move == mp->killer
                || move == mp->counter)
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
        while (mp->idx < mp->badCaptureList.count) {
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
