#include <algorithm>
#include <cassert>
#include <iostream>
#include "bitboard.h"
#include "search.h"
#include "history.h"
#include "piece_data.h"
#include "attack.h"
#include "eval.h"
#include "magic.h"
#include "makemove.h"
#include "misc.h"
#include "threads.h"
#include "movepicker.h"
#include "ttable.h"
#include "movegen.h"
#include "time_manager.h"
#include "io.h"

// Returns true if the position is a 2-fold repetition, false otherwise
static bool IsRepetition(const Position* pos) {
    assert(pos->hisPly >= pos->get50MrCounter());
    int counter = 0;
    // How many moves back should we look at most, aka our distance to the last irreversible move
    int distance = std::min(pos->get50MrCounter(), pos->getPlyFromNull());
    // Get the point our search should start from
    int startingPoint = pos->played_positions.size();
    // Scan backwards from the first position where a repetition is possible (4 half moves ago) for at most distance steps
    for (int index = 4; index <= distance; index += 2)
        // if we found the same position hashkey as the current position
        if (pos->played_positions[startingPoint - index] == pos->posKey) {

            // we found a 2-fold repetition within the search tree
            if (index < pos->historyStackHead)
                return true;

            counter++;
            // we found a 3-fold repetition which occurred in part before or at root
            if (counter >= 2)
                return true;
        }
    return false;
}

// Returns true if the position is a draw via the 50mr rule
static bool Is50MrDraw(Position* pos) {

    if (pos->get50MrCounter() >= 100) {

        // If there's no risk we are being checkmated return true
        if (!pos->getCheckers())
            return true;

        // if we are in check make sure it's not checkmate 
        MoveList moveList;
        // generate moves
        GenerateMoves(&moveList, pos, MOVEGEN_ALL);
        for (int i = 0; i < moveList.count; i++) {
            const Move move = moveList.moves[i].move;
            if (IsLegal(pos, move)) {
                return true; // We have at least one legal move, so it is a draw
            }
        }
        return false; // We have no legal moves, it is checkmate
    }

    return false;
}

// If we triggered any of the rules that forces a draw or we know the position is a draw return a draw score
bool IsDraw(Position* pos) {
    // if it's a 3-fold repetition, the fifty moves rule kicked in or there isn't enough material on the board to give checkmate then it's a draw
    return IsRepetition(pos)
        || Is50MrDraw(pos)
        || MaterialDraw(pos);
}

// ClearForSearch handles the cleaning of the post and the info parameters to start search from a clean state
void ClearForSearch(ThreadData* td) {
    // Extract data structures from ThreadData
    SearchInfo* info = &td->info;
    PvTable* pvTable = &td->pvTable;

    // Clean the Pv array
    std::memset(pvTable, 0, sizeof(td->pvTable));
    // Clean the node table
    std::memset(td->nodeSpentTable, 0, sizeof(td->nodeSpentTable));
    // Reset plies and search info
    info->starttime = GetTimeMs();
    info->nodes = 0;
    info->seldepth = 0;
    // Main thread only unpauses any eventual search thread
    if (td->id == 0)
        for (auto& helper_thread : threads_data)
            helper_thread.info.stopped = false;
}

// returns a bitboard of all the attacks to a specific square
static inline Bitboard AttacksTo(const Position* pos, int to, Bitboard occ) {
    Bitboard attackingBishops = GetPieceBB(pos, BISHOP) | GetPieceBB(pos, QUEEN);
    Bitboard attackingRooks = GetPieceBB(pos, ROOK) | GetPieceBB(pos, QUEEN);

    return (pawn_attacks[WHITE][to] & pos->GetPieceColorBB(PAWN, BLACK))
         | (pawn_attacks[BLACK][to] & pos->GetPieceColorBB(PAWN, WHITE))
         | (knight_attacks[to] & GetPieceBB(pos, KNIGHT))
         | (king_attacks[to] & GetPieceBB(pos, KING))
         | (GetBishopAttacks(to, occ) & attackingBishops)
         | (GetRookAttacks(to, occ) & attackingRooks);
}

// inspired by the Weiss engine
bool SEE(const Position* pos, const int move, const int threshold) {

    // We can't win any material from castling, nor can we lose any
    if (isCastle(move))
        return threshold <= 0;

    int to = To(move);
    int from = From(move);

    int target = isEnpassant(move) ? PAWN : pos->PieceOn(to);
    int promo = getPromotedPiecetype(move);
    int value = SEEValue[target] - threshold;

    // If we promote, we get the promoted piece and lose the pawn
    if (isPromo(move))
        value += SEEValue[promo] - SEEValue[PAWN];

    // If we can't beat the threshold despite capturing the piece,
    // it is impossible to beat the threshold
    if (value < 0)
        return false;

    int attacker = pos->PieceOn(from);

    // If we get captured, we lose the moved piece,
    // or the promoted piece in the case of promotions
    value -= isPromo(move) ? SEEValue[promo] : SEEValue[attacker];

    // If we still beat the threshold after losing the piece,
    // we are guaranteed to beat the threshold
    if (value >= 0)
        return true;

    // It doesn't matter if the to square is occupied or not
    Bitboard occupied = pos->Occupancy(BOTH) ^ (1ULL << from);
    if (isEnpassant(move))
        occupied ^= pos->getEpSquare();

    Bitboard attackers = AttacksTo(pos, to, occupied);

    Bitboard bishops = GetPieceBB(pos, BISHOP) | GetPieceBB(pos, QUEEN);
    Bitboard rooks = GetPieceBB(pos, ROOK) | GetPieceBB(pos, QUEEN);

    int side = Color[attacker] ^ 1;

    // Make captures until one side runs out, or fail to beat threshold
    while (true) {
        // Remove used pieces from attackers
        attackers &= occupied;

        Bitboard myAttackers = attackers & pos->Occupancy(side);
        if (!myAttackers) {
            break;
        }

        // Pick next least valuable piece to capture with
        int pt;
        for (pt = PAWN; pt < KING; ++pt)
            if (myAttackers & GetPieceBB(pos, pt))
                break;

        side ^= 1;

        value = -value - 1 - SEEValue[pt];

        // Value beats threshold, or can't beat threshold (negamaxed)
        if (value >= 0) {
            if (pt == KING && (attackers & pos->Occupancy(side)))
                side ^= 1;
            break;
        }
        // Remove the used piece from occupied
        occupied ^= 1ULL << (GetLsbIndex(myAttackers & pos->GetPieceColorBB(pt, side ^ 1)));

        if (pt == PAWN || pt == BISHOP || pt == QUEEN)
            attackers |= GetBishopAttacks(to, occupied) & bishops;
        if (pt == ROOK || pt == QUEEN)
            attackers |= GetRookAttacks(to, occupied) & rooks;
    }

    return side != Color[attacker];
}

Move GetBestMove(const PvTable* pvTable) {
    return pvTable->pvArray[0][0];
}

// Starts the search process, this is ideally the point where you can start a multithreaded search
void RootSearch(int depth, ThreadData* td, UciOptions* options) {
    // Init a thread_data object for each helper thread that doesn't have one already
    for (int i = threads_data.size(); i < options->Threads - 1; i++) {
        threads_data.emplace_back();
        threads_data.back().id = i + 1;
    }

    // Init thread_data objects
    for (size_t i = 0; i < threads_data.size(); i++) {
        threads_data[i].info = td->info;
        threads_data[i].pos = td->pos;
    }

    // Start Threads-1 helper search threads
    for (int i = 0; i < options->Threads - 1; i++)
        threads.emplace_back(SearchPosition, 1, depth, &threads_data[i], options);

    // MainThread search
    SearchPosition(1, depth, td, options);
    // Stop helper threads before returning the best move
    StopHelperThreads();
    // Print final bestmove found
    std::cout << "bestmove ";
    PrintMove(GetBestMove(&td->pvTable));
    std::cout << std::endl;
}

// SearchPosition is the actual function that handles the search, it sets up the variables needed for the search, calls the AspirationWindowSearch function and handles the console output
void SearchPosition(int startDepth, int finalDepth, ThreadData* td, UciOptions* options) {
    // variable used to store the score of the best move found by the search (while the move itself can be retrieved from the triangular PV table)
    int score = 0;
    int averageScore = SCORE_NONE;
    int bestMoveStabilityFactor = 0;
    int evalStabilityFactor = 0;
    Move previousBestMove = NOMOVE;
    // Clean the position and the search info to start search from a clean state
    ClearForSearch(td);
    UpdateTableAge();

    // Call the Negamax function in an iterative deepening framework
    for (int currentDepth = startDepth; currentDepth <= finalDepth; currentDepth++) {
        score = AspirationWindowSearch(averageScore, currentDepth, td);
        averageScore = averageScore == SCORE_NONE ? score : (averageScore + score) / 2;
        // Only the main thread handles time related tasks
        if (td->id == 0) {
            // Keep track of how many times in a row the best move stayed the same
            if (GetBestMove(&td->pvTable) == previousBestMove) {
                bestMoveStabilityFactor = std::min(bestMoveStabilityFactor + 1, 4);
            }
            else {
                bestMoveStabilityFactor = 0;
                previousBestMove = GetBestMove(&td->pvTable);
            }

            // Keep track of eval stability
            if (std::abs(score - averageScore) < 10) {
                evalStabilityFactor = std::min(evalStabilityFactor + 1, 4);
            }
            else {
                evalStabilityFactor = 0;
            }

            // use the previous search to adjust some of the time management parameters, do not scale movetime time controls
            if (   td->RootDepth > 7
                && td->info.timeset) {
                ScaleTm(td, bestMoveStabilityFactor, evalStabilityFactor);
            }

            // check if we just cleared a depth and more than OptTime passed, or we used more than the give nodes
            if (StopEarly(&td->info) || NodesOver(&td->info))
                // Stop main-thread search
                td->info.stopped = true;
        }
        // stop calculating and return best move so far
        if (td->info.stopped)
            break;

        // If it's the main thread print the uci output
        if (td->id == 0)
            PrintUciOutput(score, currentDepth, td, options);
        // Seldepth should only be related to the current ID loop
        td->info.seldepth = 0;
    }
}

int AspirationWindowSearch(int prev_eval, int depth, ThreadData* td) {
    int score;
    td->RootDepth = depth;
    SearchData* sd = &td->sd;
    SearchStack stack[MAXDEPTH + 4], *ss = stack + 4;
    // Explicitly clean stack
    for (int i = -4; i < MAXDEPTH; i++) {
        (ss + i)->staticEval = SCORE_NONE;
        (ss + i)->move = NOMOVE;
    }
    for (int i = 0; i < MAXDEPTH; i++) {
        (ss + i)->ply = i;
    }

    int alpha = -MAXSCORE;
    int beta  = MAXSCORE;
    int delta = aspWindowStart();

    if (depth >= minAspDepth()) {
        alpha = std::max(-MAXSCORE, prev_eval - delta);
        beta  = std::min( MAXSCORE, prev_eval + delta);
    }

    // Stay at current depth if we fail high/low because of the aspiration windows
    while (true) {
        score = Negamax<true>(alpha, beta, depth, false, td, ss);

        // Check if more than Maxtime passed and we have to stop
        if (td->id == 0 && TimeOver(&td->info)) {
            StopHelperThreads();
            td->info.stopped = true;
            break;
        }

        // Stop calculating and return best move so far
        if (td->info.stopped) break;

        // Exact bound, we no longer need to continue searching at this depth
        if (alpha < score && score < beta) break;

        alpha = std::max(-MAXSCORE, score - delta);
        beta  = std::min( MAXSCORE, score + delta);
        delta = delta * aspWindowWidenScale() / 64;
    }
    return score;
}

void adjustEval(Position *pos, SearchData *sd, int rawEval, int &staticEval, int &eval, int ttScore, uint8_t ttBound) {
    staticEval = sd->correctionHistory.adjust(pos, rawEval);

    // Adjust eval using TT score if it is more accurate
    if (    ttScore != SCORE_NONE
        && (   (ttBound == HFUPPER && ttScore < eval) // TT score is a better upper bound
            || (ttBound == HFLOWER && ttScore > eval) // TT score is a better lower bound
            ||  ttBound == HFEXACT))
        eval = ttScore;
    else
        eval = staticEval;
}

// Negamax alpha beta search
template <bool pvNode>
int Negamax(int alpha, int beta, int depth, bool predictedCutNode, ThreadData* td, SearchStack* ss, Move excludedMove) {
    // Extract data structures from ThreadData
    Position* pos = &td->pos;
    SearchData* sd = &td->sd;
    SearchInfo* info = &td->info;
    PvTable* pvTable = &td->pvTable;

    // Initialize the node
    const bool inCheck = pos->getCheckers();
    const bool rootNode = (ss->ply == 0);
    int rawEval;
    int eval;
    int score = -MAXSCORE;
    TTEntry tte;
    pvTable->pvLength[ss->ply] = ss->ply;

    // Check for the highest depth reached in search to report it to the cli
    info->seldepth = std::max(info->seldepth, ss->ply);

    // recursion escape condition
    if (depth <= 0)
        return Quiescence<pvNode>(alpha, beta, td, ss);

    // check if more than Maxtime passed and we have to stop
    if (td->id == 0 && TimeOver(&td->info)) {
        StopHelperThreads();
        td->info.stopped = true;
    }

    // Check for early return conditions
    if (!rootNode) {
        // If position is a draw return a draw score
        if (IsDraw(pos))
            return 0;

        // If we reached maxdepth we return a static evaluation of the position
        if (ss->ply >= MAXDEPTH - 1)
            return inCheck ? 0 : EvalPosition(pos);

        // Mate distance pruning
        alpha = std::max(alpha, -MATE_SCORE + ss->ply);
        beta = std::min(beta, MATE_SCORE - ss->ply - 1);
        if (alpha >= beta)
            return alpha;

        // Upcoming repetition detection
        if (alpha < 0 && hasGameCycle(pos, ss->ply))
        {
            alpha = 0;
            if (alpha >= beta)
                return alpha;
        }
    }

    // Probe the TT for useful previous search informations, we avoid doing so if we are searching a singular extension
    const bool ttHit = !excludedMove && ProbeTTEntry(pos->getPoskey(), &tte);
    const int ttScore = ttHit ? ScoreFromTT(tte.score, ss->ply) : SCORE_NONE;
    const Move ttMove = ttHit ? MoveFromTT(pos, tte.move) : NOMOVE;
    const uint8_t ttBound = ttHit ? BoundFromTT(tte.ageBoundPV) : uint8_t(HFNONE);
    const uint8_t ttDepth = tte.depth;
    const bool ttPv = pvNode || (ttHit && FormerPV(tte.ageBoundPV));

    // If we found a value in the TT for this position, and the depth is equal or greater we can return it (pv nodes are excluded)
    if (   !pvNode
        &&  ttScore != SCORE_NONE
        &&  ttDepth >= depth
        && (   (ttBound == HFUPPER && ttScore <= alpha)
            || (ttBound == HFLOWER && ttScore >= beta)
            ||  ttBound == HFEXACT))
        return ttScore;

    // If we are in check we skip static evaluation
    if (inCheck) {
        rawEval = eval = ss->staticEval = SCORE_NONE;
    }
    // If we are in an excluded search just re-use the existing eval
    else if (excludedMove) {
        rawEval = eval = ss->staticEval;
    }
    // get an evaluation of the position:
    else if (ttHit) {
        // If the value in the TT is valid we use that, otherwise we call the static evaluation function
        rawEval = eval = ss->staticEval = tte.eval != SCORE_NONE ? tte.eval : EvalPosition(pos);
        adjustEval(pos, sd, rawEval, ss->staticEval, eval, ttScore, ttBound);
    }
    else {
        // If we don't have anything in the TT we have to call evalposition
        rawEval = eval = ss->staticEval = EvalPosition(pos);
        adjustEval(pos, sd, rawEval, ss->staticEval, eval, ttScore, ttBound);

        // Save the eval into the TT
        StoreTTEntry(pos->posKey, NOMOVE, SCORE_NONE, rawEval, HFNONE, 0, pvNode, ttPv);
    }

    int prevEval;
    if (inCheck)
        prevEval = ss->staticEval;
    else if ((ss - 2)->staticEval != SCORE_NONE)
        prevEval = (ss - 2)->staticEval;

    else if ((ss - 4)->staticEval != SCORE_NONE)
        prevEval = (ss - 4)->staticEval;
    else
        prevEval = ss->staticEval;

    const int improvement = ss->staticEval - prevEval;
    const bool improving = improvement > 0;
    const int improvementPer256 = prevEval == 0 ? maxImprovementPer256()
                                                : std::clamp(improvement / std::abs(prevEval), -maxImprovementPer256(), maxImprovementPer256());

    const bool canIIR = depth >= iirMinDepth() && ttBound == HFNONE;

    if (   !pvNode
        && !excludedMove
        && !inCheck) {
        // Reverse Futility Pruning (RFP) / Static Null Move Pruning (SNMP)
        // At low depths, if the evaluation is far above beta, we assume that at least one move will fail high
        // and return a fail high score.
        if (   depth <= rfpMaxDepth()
            && eval >= beta + rfpDepthCoeff() * depth - rfpImprCoeff() * improving - rfpIirCoeff() * canIIR
            && std::abs(eval) < MATE_FOUND)
            return eval;

        // Null Move Pruning (NMP)
        // If our eval indicates a fail high is likely, we try NMP.
        // We do a reduced search after giving the opponent a free turn, and if that fails high,
        // it means our position is so good we don't even need to make a move. Thus, we return a fail high score.
        if (   depth > nmpDepth()
            && eval >= beta
            && (ss - 1)->move != NOMOVE
            && BoardHasNonPawns(pos, pos->side)) {

            const int R = (nmpRedConst() + nmpRedDepthCoeff() * depth + nmpRedIirCoeff() * canIIR) / 1024
                        +  std::min(eval - beta, nmpRedEvalDiffMax()) / nmpRedEvalDiffDiv();

            ss->move = NOMOVE;
            MakeNullMove(pos);
            const int nmpScore = -Negamax<false>(-beta, -beta + 1, depth - R, !predictedCutNode, td, ss + 1);
            TakeNullMove(pos);

            // Don't return unproven mates from null move search
            if (nmpScore >= beta)
                return abs(nmpScore) > MATE_FOUND ? beta : nmpScore;
        }
    }

    // IIR by Ed Schroder (That i found out about in Berserk source code)
    // http://talkchess.com/forum3/viewtopic.php?f=7&t=74769&sid=64085e3396554f0fba414404445b3120
    // https://github.com/jhonnold/berserk/blob/dd1678c278412898561d40a31a7bd08d49565636/src/search.c#L379
    if (canIIR)
        depth -= iirDepthReduction();

    // old value of alpha
    const int old_alpha = alpha;
    int bestScore = -MAXSCORE;
    Move move;
    Move bestMove = NOMOVE;

    int totalMoves = 0;
    bool skipQuiets = false;

    Movepicker mp;
    InitMP(&mp, pos, sd, ss, ttMove, SEARCH);

    // Keep track of the played quiet and tactical moves
    MoveList quietMoves, tacticalMoves;

    // loop over moves within a movelist
    while ((move = NextMove(&mp, skipQuiets)) != NOMOVE) {

        if (move == excludedMove || !IsLegal(pos, move))
            continue;

        totalMoves++;
        bool isQuiet = !isTactical(move);

        if (bestScore > -MATE_FOUND) {

            // Late Move Pruning. If we have searched many moves, but no beta cutoff has occurred,
            // assume that there are no better quiet moves and skip the rest.
            if (totalMoves >= lmpMargins[std::min(depth, 63)])
                skipQuiets = true;

            // Futility Pruning. At low depths, if the eval is far below alpha,
            // only search tactical moves as trying quiets in such a bad position is futile.
            if (   depth <= fpDepth()
                && ss->staticEval + futilityMargins[std::min(depth, 63)] <= alpha)
                skipQuiets = true;

            // SEE Pruning. At low depths, if the SEE (Static Exchange Evaluation) of the move
            // is extremely low, skip considering it in our search.

            // SEE Pruning for quiets
            if (    depth <= quietSeeDepth()
                &&  isQuiet
                && !SEE(pos, move, seeMargins[true][std::min(depth, 63)]))
                continue;

            // SEE Pruning for tactical moves
            if (    depth <= tacticalSeeDepth()
                && !isQuiet
                && !SEE(pos, move, seeMargins[false][std::min(depth, 63)]))
                continue;
        }

        int extension = 0;

        // Singular extensions. If the TT bound suggests that the TT move is likely to fail high, and the TT entry
        // is of a certain quality, we do a search at reduced margins and depth, whilst excluding the TT move.
        // If this this excluded search fails low, then we know there are no moves which maintain 
        // the score close to the TT score, and so we extend the search of the TT move (search it at a higher depth).
        if (    move == ttMove
            && !rootNode
            && !excludedMove
            &&  depth >= seDepth()
            && (ttBound == HFEXACT || ttBound == HFLOWER)
            &&  abs(ttScore) < MATE_FOUND
            &&  ttDepth >= depth - seMinQuality()
            &&  ttDepth * 2 >= depth) {
            const int singularAlpha = std::max(ttScore - depth * seMarginMult() / 16, -MATE_FOUND);
            const int singularBeta  = singularAlpha + 1;
            const int singularDepth = std::max((depth - 1) / 2, 1);

            const int singularScore = Negamax<false>(singularAlpha, singularBeta, singularDepth, predictedCutNode, td, ss, ttMove);
            if (singularScore <= singularAlpha) {
                // If we fail low by a lot, we extend the search by more than one ply
                // (TT move is very singular; there are no close alternatives)
                const int doubleExtMargin = seDeBase() + seDePvCoeff() * pvNode;
                const int tripleExtMargin = seTeBase() + seTePvCoeff() * pvNode;
                extension = 1
                          + (singularScore + doubleExtMargin <= singularAlpha)
                          + (singularScore + tripleExtMargin <= singularAlpha);
            }
            // Multicut. If the lower bound of our singular search score is at least beta,
            // assume both it and the TT move fails high, and return a cutoff early.
            else if (singularScore >= beta) {
                return singularScore;
            }
        }

        // we adjust the search depth based on potential extensions
        int newDepth = depth - 1 + extension;
        // Speculative prefetch of the TT entry
        TTPrefetch(keyAfter(pos, move));
        ss->move = move;

        // Play the move
        MakeMove<true>(move, pos);

        // increment nodes count
        info->nodes++;
        const uint64_t nodesBeforeSearch = info->nodes;

        // Add the move to the corresponding list
        AddMove(move, isQuiet ? &quietMoves : &tacticalMoves);

        // Late Move Reductions (LMR):
        // After a certain depth and total moves searched, we search the rest first at a reduced depth and zero window.
        // Here we calulate the reduction that we are going to reduce for this move.
        if (   depth >= 3
            && totalMoves > 1 + pvNode
            && (isQuiet || !ttPv)) {

            // Get base reduction value (multiplied by 1024)
            int depthReductionGranular = lmrReductions[isQuiet][std::min(depth, 63)][std::min(totalMoves, 63)];

            // Reduce less if we are on or have been on the PV
            if (ttPv) depthReductionGranular -= ttPvLmrReduction();

            // Reduce less if we are predicted to fail high (i.e. we stem from an LMR search earlier in the tree)
            if (predictedCutNode) depthReductionGranular += predictedCutNodeLmrReduction();

            // Divide by 1024 once all the adjustments have been applied
            const int depthReduction = depthReductionGranular / 1024;

            // Clamp the reduced search depth so that we neither extend nor drop into qsearch
            // We use min/max instead of clamp due to issues that can arise if newDepth < 1
            int reducedDepth = std::min(std::max(newDepth - depthReduction, 1), newDepth);

            // Carry out the reduced depth, zero window search
            score = -Negamax<false>(-alpha - 1, -alpha, reducedDepth, true, td, ss + 1);

            // If the reduced depth search fails high, do a full depth search (but still on zero window).
            if (score > alpha && newDepth > reducedDepth) {
                // Based on the value returned by our reduced search see if we should search deeper or shallower,
                const bool doDeeperSearch = score > (bestScore + doDeeperBaseScore() + doDeeperDepthMultiplier() * newDepth);
                const bool doShallowerSearch = score < (bestScore + newDepth);
                newDepth += doDeeperSearch - doShallowerSearch;
                if (newDepth > reducedDepth)
                    score = -Negamax<false>(-alpha - 1, -alpha, newDepth, !predictedCutNode, td, ss + 1);
            }
        }

        // Principal Variation Search(PVS):
        // If we skipped LMR, and this isn't the first move of the node, we search at a full depth but zero window.
        // We assume that our move ordering is perfect, and so we expect all non-first moves to be worse.
        // This zero window search will either confirm or deny our prediction, as the score cannot be exact (no integer lies in (-alpha - 1, -alpha))
        else if (!pvNode || totalMoves > 1) {
            score = -Negamax<false>(-alpha - 1, -alpha, newDepth, !predictedCutNode, td, ss + 1);
        }

        // If this is our first move, we search with a full window.
        // Otherwise, if our zero window search fails low, this is a potential alpha raise and so we search the move fully.
        if (pvNode && (totalMoves == 1 || score > alpha))
            score = -Negamax<true>(-beta, -alpha, newDepth, false, td, ss + 1);

        // take move back
        UnmakeMove(move, pos);
        if (   td->id == 0
            && rootNode)
            td->nodeSpentTable[FromTo(move)] += info->nodes - nodesBeforeSearch;

        if (info->stopped)
            return 0;

        // If the score of the current move is the best we've found until now
        if (score > bestScore) {
            // Update what the best score is
            bestScore = score;

            // Found a better move that raised alpha
            if (score > alpha) {
                bestMove = move;

                if (pvNode) {
                    // Update the pv table
                    pvTable->pvArray[ss->ply][ss->ply] = move;
                    for (int nextPly = ss->ply + 1; nextPly < pvTable->pvLength[ss->ply + 1]; nextPly++) {
                        pvTable->pvArray[ss->ply][nextPly] = pvTable->pvArray[ss->ply + 1][nextPly];
                    }
                    pvTable->pvLength[ss->ply] = pvTable->pvLength[ss->ply + 1];
                }

                if (score >= beta) {
                    // node (move) fails high
                    UpdateAllHistories(pos, ss, sd, depth, move, quietMoves, tacticalMoves);
                    break;
                }
                // Update alpha iff alpha < beta
                alpha = score;
            }
        }
    }

    // We don't have any legal moves to make in the current postion. If we are in singular search, return -infinite.
    // Otherwise, if the king is in check, return a mate score, assuming closest distance to mating position.
    // If we are in neither of these 2 cases, it is stalemate.
    if (totalMoves == 0) {
        return excludedMove ? -MAXSCORE
             :      inCheck ? -MATE_SCORE + ss->ply
                            : 0;
    }

    // Do not save to the TT during singular search
    if (!excludedMove) {
        // Set the TT bound based on whether we failed high or raised alpha
        int bound = bestScore >= beta ? HFLOWER : alpha != old_alpha ? HFEXACT : HFUPPER;
        StoreTTEntry(pos->posKey, MoveToTT(bestMove), ScoreToTT(bestScore, ss->ply), rawEval, bound, depth, pvNode, ttPv);
        sd->correctionHistory.update(pos, bestMove, depth, bound, bestScore, rawEval);
    }

    return bestScore;
}

// Quiescence search to avoid the horizon effect
template <bool pvNode>
int Quiescence(int alpha, int beta, ThreadData* td, SearchStack* ss) {
    Position* pos = &td->pos;
    SearchData* sd = &td->sd;
    SearchInfo* info = &td->info;
    const bool inCheck = pos->getCheckers();
    // tte is an TT entry, it will store the values fetched from the TT
    TTEntry tte;
    int rawEval;
    int bestScore;

    // check if more than Maxtime passed and we have to stop
    if (td->id == 0 && TimeOver(&td->info)) {
        StopHelperThreads();
        td->info.stopped = true;
    }

    // If position is a draw return a draw score
    if (IsDraw(pos))
        return 0;

    // If we reached maxdepth we return a static evaluation of the position
    if (ss->ply >= MAXDEPTH - 1)
        return inCheck ? 0 : EvalPosition(pos);

    // Upcoming repetition detection
    if (alpha < 0 && hasGameCycle(pos, ss->ply))
    {
        alpha = 0;
        if (alpha >= beta)
            return alpha;
    }

    // ttHit is true if and only if we find something in the TT
    const bool ttHit = ProbeTTEntry(pos->getPoskey(), &tte);
    const int ttScore = ttHit ? ScoreFromTT(tte.score, ss->ply) : SCORE_NONE;
    const Move ttMove = ttHit ? MoveFromTT(pos, tte.move) : NOMOVE;
    const uint8_t ttBound = ttHit ? BoundFromTT(tte.ageBoundPV) : uint8_t(HFNONE);
    const bool ttPv = pvNode || (ttHit && FormerPV(tte.ageBoundPV));

    // If we found a value in the TT for this position, we can return it (pv nodes are excluded)
    if (   !pvNode
        &&  ttScore != SCORE_NONE
        && (   (ttBound == HFUPPER && ttScore <= alpha)
            || (ttBound == HFLOWER && ttScore >= beta)
            ||  ttBound == HFEXACT))
        return ttScore;

    if (inCheck) {
        rawEval = ss->staticEval = SCORE_NONE;
        bestScore = -MAXSCORE;
    }
    // If we have a ttHit with a valid eval use that
    else if (ttHit) {
        // If the value in the TT is valid we use that, otherwise we call the static evaluation function
        rawEval = ss->staticEval = bestScore = tte.eval != SCORE_NONE ? tte.eval : EvalPosition(pos);
        adjustEval(pos, sd, rawEval, ss->staticEval, bestScore, ttScore, ttBound);
    }
    // If we don't have any useful info in the TT just call Evalpos
    else {
        rawEval = bestScore = ss->staticEval = EvalPosition(pos);
        adjustEval(pos, sd, rawEval, ss->staticEval, bestScore, ttScore, ttBound);
        // Save the eval into the TT
        StoreTTEntry(pos->posKey, NOMOVE, SCORE_NONE, rawEval, HFNONE, 0, pvNode, ttPv);
    }

    // Stand pat
    if (bestScore >= beta)
        return bestScore;

    // Adjust alpha based on eval
    alpha = std::max(alpha, bestScore);

    Movepicker mp;
    // If we aren't in check we generate just the captures, otherwise we generate all the moves
    InitMP(&mp, pos, sd, ss, ttMove, QSEARCH);

    Move bestmove = NOMOVE;
    Move move;
    int totalMoves = 0;

    // loop over moves within the movelist
    while ((move = NextMove(&mp, !inCheck || bestScore > -MATE_FOUND)) != NOMOVE) {

        if (!IsLegal(pos, move))
            continue;

        totalMoves++;

        // Speculative prefetch of the TT entry
        TTPrefetch(keyAfter(pos, move));
        ss->move = move;
        // Play the move
        MakeMove<true>(move, pos);
        // increment nodes count
        info->nodes++;
        // Call Quiescence search recursively
        const int score = -Quiescence<pvNode>(-beta, -alpha, td, ss + 1);

        // take move back
        UnmakeMove(move, pos);

        if (info->stopped)
            return 0;

        // If the score of the current move is the best we've found until now
        if (score > bestScore) {
            // Update what the best score is
            bestScore = score;

            // if the score is better than alpha update our best move
            if (score > alpha) {
                bestmove = move;

                // if the score is better than or equal to beta break the loop because we failed high
                if (score >= beta)
                    break;

                // Update alpha iff alpha < beta
                alpha = score;
            }
        }
    }

    // return mate score (assuming closest distance to mating position)
    if (totalMoves == 0 && inCheck) {
        return -MATE_SCORE + ss->ply;
    }
    // Set the TT bound based on whether we failed high, for qsearch we never use the exact bound
    int bound = bestScore >= beta ? HFLOWER : HFUPPER;

    StoreTTEntry(pos->posKey, MoveToTT(bestmove), ScoreToTT(bestScore, ss->ply), rawEval, bound, 0, pvNode, ttPv);

    return bestScore;
}
