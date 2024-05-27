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
    assert(pos->hisPly >= pos->fiftyMove);
    int counter = 0;
    // How many moves back should we look at most, aka our distance to the last irreversible move
    int distance = std::min(pos->Get50mrCounter(), pos->plyFromNull);
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

    if (pos->Get50mrCounter() >= 100) {

        // If there's no risk we are being checkmated return true
        if (!pos->checkers)
            return true;

        // if we are in check make sure it's not checkmate 
        MoveList moveList;
        // generate moves
        GenerateMoves(&moveList, pos, MOVEGEN_ALL);
        for (int i = 0; i < moveList.count; i++) {
            const int move = moveList.moves[i].move;
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
    Bitboard occupied = pos->Occupancy(BOTH) ^ (1ULL << from) ^ (1ULL << to);
    if (isEnpassant(move))
        occupied ^= GetEpSquare(pos);

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

int GetBestMove(const PvTable* pvTable) {
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
    std::cout << "\n";
}

// SearchPosition is the actual function that handles the search, it sets up the variables needed for the search, calls the AspirationWindowSearch function and handles the console output
void SearchPosition(int startDepth, int finalDepth, ThreadData* td, UciOptions* options) {
    // variable used to store the score of the best move found by the search (while the move itself can be retrieved from the triangular PV table)
    int score = 0;
    int averageScore = SCORE_NONE;
    int bestMoveStabilityFactor = 0;
    int previousBestMove = NOMOVE;
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
            // use the previous search to adjust some of the time management parameters, do not scale movetime time controls
            if (   td->RootDepth > 7
                && td->info.timeset) {
                ScaleTm(td, bestMoveStabilityFactor);
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
        (ss + i)->move = NOMOVE;
        (ss + i)->excludedMove = NOMOVE;
        (ss + i)->searchKiller = NOMOVE;
        (ss + i)->staticEval = SCORE_NONE;
        (ss + i)->doubleExtensions = 0;
        (ss + i)->contHistEntry = &sd->contHist[PieceTo(NOMOVE)];
    }
    for (int i = 0; i < MAXDEPTH; i++) {
        (ss + i)->ply = i;
        (ss + i)->contHistEntry = &sd->contHist[PieceTo(NOMOVE)];
    }
    // We set an expected window for the score at the next search depth, this window is not 100% accurate so we might need to try a bigger window and re-search the position
    int delta = 12;
    // define initial alpha beta bounds
    int alpha = -MAXSCORE;
    int beta = MAXSCORE;

    // only set up the windows is the search depth is bigger or equal than Aspiration_Depth to avoid using windows when the search isn't accurate enough
    if (depth >= 3) {
        alpha = std::max(-MAXSCORE, prev_eval - delta);
        beta = std::min(prev_eval + delta, MAXSCORE);
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

        // We fell outside the window, so try again with a bigger window, since we failed low we can adjust beta to decrease the total window size
        if (score <= alpha) {
            beta = (alpha + beta) / 2;
            alpha = std::max(-MAXSCORE, score - delta);
            depth = td->RootDepth;
        }

        // We fell outside the window, so try again with a bigger window
        else if (score >= beta) {
            beta = std::min(score + delta, MAXSCORE);
            depth = std::max(depth - 1, td->RootDepth - 5);
        }
        else
            break;
        // Progressively increase how much the windows are increased by at each fail
        delta *= 1.44;
    }
    return score;
}

// Negamax alpha beta search
template <bool pvNode>
int Negamax(int alpha, int beta, int depth, const bool cutNode, ThreadData* td, SearchStack* ss) {
    // Extract data structures from ThreadData
    Position* pos = &td->pos;
    SearchData* sd = &td->sd;
    SearchInfo* info = &td->info;
    PvTable* pvTable = &td->pvTable;

    // Initialize the node
    const bool inCheck = pos->checkers;
    const bool rootNode = (ss->ply == 0);
    int eval;
    bool improving;
    int score = -MAXSCORE;
    TTEntry tte;

    const int excludedMove = ss->excludedMove;
    if (!excludedMove) {
        // if we are in a singular search and reusing the same ss entry, we have to guard this statement otherwise the pv length will get reset
        pvTable->pvLength[ss->ply] = ss->ply;
    }

    // Check for the highest depth reached in search to report it to the cli
    if (ss->ply > info->seldepth)
        info->seldepth = ss->ply;

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
        if (alpha < 0 && hasGameCycle(pos,ss->ply))
        {
            alpha = 0;
            if (alpha >= beta)
                return alpha;
        }
    }

    // Probe the TT for useful previous search informations, we avoid doing so if we are searching a singular extension
    const bool ttHit = !excludedMove && ProbeTTEntry(pos->GetPoskey(), &tte);
    const int ttScore = ttHit ? ScoreFromTT(tte.score, ss->ply) : SCORE_NONE;
    const int ttMove = ttHit ? MoveFromTT(pos, tte.move) : NOMOVE;
    const uint8_t ttBound = ttHit ? BoundFromTT(tte.ageBoundPV) : uint8_t(HFNONE);
    // If we found a value in the TT for this position, and the depth is equal or greater we can return it (pv nodes are excluded)
    if (   !pvNode
        &&  ttScore != SCORE_NONE
        &&  tte.depth >= depth
        && (   (ttBound == HFUPPER && ttScore <= alpha)
            || (ttBound == HFLOWER && ttScore >= beta)
            ||  ttBound == HFEXACT))
        return ttScore;

    const bool ttPv = pvNode || (ttHit && FormerPV(tte.ageBoundPV));

    // IIR by Ed Schroder (That i find out about in Berserk source code)
    // http://talkchess.com/forum3/viewtopic.php?f=7&t=74769&sid=64085e3396554f0fba414404445b3120
    // https://github.com/jhonnold/berserk/blob/dd1678c278412898561d40a31a7bd08d49565636/src/search.c#L379
    if (depth >= 4 && ttBound == HFNONE)
        depth--;

    // clean killers and excluded move for the next ply
    (ss + 1)->excludedMove = NOMOVE;
    (ss + 1)->searchKiller = NOMOVE;

    // If we are in check or searching a singular extension we avoid pruning before the move loop
    if (inCheck || excludedMove) {
        ss->staticEval = eval = SCORE_NONE;
        improving = false;
        goto moves_loop;
    }

    // get an evaluation of the position:
    if (ttHit) {
        // If the value in the TT is valid we use that, otherwise we call the static evaluation function
        eval = ss->staticEval = tte.eval != SCORE_NONE ? tte.eval : EvalPosition(pos);

        // We can also use the tt score as a more accurate form of eval
        if (    ttScore != SCORE_NONE
            && (   (ttBound == HFUPPER && ttScore < eval)
                || (ttBound == HFLOWER && ttScore > eval)
                ||  ttBound == HFEXACT))
            eval = ttScore;
    }
    else {
        // If we don't have anything in the TT we have to call evalposition
        eval = ss->staticEval = EvalPosition(pos);
        if (!excludedMove)
            // Save the eval into the TT
            StoreTTEntry(pos->posKey, NOMOVE, SCORE_NONE, eval, HFNONE, 0, pvNode, ttPv);
    }

    // Improving is a very important modifier to many heuristics. It checks if our static eval has improved since our last move.
    // As we don't evaluate in check, we look for the first ply we weren't in check between 2 and 4 plies ago. If we find that
    // static eval has improved, or that we were in check both 2 and 4 plies ago, we set improving to true.
    if ((ss - 2)->staticEval != SCORE_NONE) {
        improving = ss->staticEval > (ss - 2)->staticEval;
    }
    else if ((ss - 4)->staticEval != SCORE_NONE) {
        improving = ss->staticEval > (ss - 4)->staticEval;
    }
    else
        improving = true;

    if (!pvNode) {
        // Reverse futility pruning
        if (   depth < 10
            && abs(eval) < MATE_FOUND
            && eval - 91 * (depth - improving) >= beta)
            return eval;

        // Null move pruning: If our position is so good that we can give the opponent a free move and still fail high,
        // return early. At higher depth we do a reduced search with null move pruning disabled (ie verification search)
        // to prevent falling into zugzwangs.
        if (   eval >= ss->staticEval
            && eval >= beta
            && (ss - 1)->move != NOMOVE
            && depth >= nmpDepth()
            && ss->ply >= td->nmpPlies
            && BoardHasNonPawns(pos, pos->side)) {

            ss->move = NOMOVE;
            const int R = 3 + depth / 3 + std::min((eval - beta) / 200, 3);
            ss->contHistEntry = &sd->contHist[PieceTo(NOMOVE)];

            MakeNullMove(pos);

            // Search moves at a reduced depth to find beta cutoffs.
            int nmpScore = -Negamax<false>(-beta, -beta + 1, depth - R, !cutNode, td, ss + 1);

            TakeNullMove(pos);

            // fail-soft beta cutoff
            if (nmpScore >= beta) {
                // Don't return unproven mates but still return beta
                if (nmpScore > MATE_FOUND)
                    nmpScore = beta;

                // If we don't have to do a verification search just return the score
                if (td->nmpPlies || depth < 15)
                    return nmpScore;

                // Verification search to avoid zugzwangs: if we are at an high enough depth we perform another reduced search without nmp for at least nmpPlies
                td->nmpPlies = ss->ply + (depth - R) * 2 / 3;
                int verificationScore = Negamax<false>(beta - 1, beta, depth - R, false, td, ss);
                td->nmpPlies = 0;

                // If the verification search holds return the score
                if (verificationScore >= beta)
                    return nmpScore;
            }
        }
        // Razoring
        if (depth <= 5 && eval + 256 * depth < alpha)
        {
            const int razorScore = Quiescence<false>(alpha, beta, td, ss);
            if (razorScore <= alpha)
                return razorScore;
        }
    }

moves_loop:

    // old value of alpha
    const int old_alpha = alpha;
    int bestScore = -MAXSCORE;
    int move;
    int bestMove = NOMOVE;

    int totalMoves = 0;
    bool skipQuiets = false;

    Movepicker mp;
    InitMP(&mp, pos, sd, ss, ttMove, SEARCH);

    // Keep track of the played quiet and noisy moves
    MoveList quietMoves, noisyMoves;

    // loop over moves within a movelist
    while ((move = NextMove(&mp, skipQuiets)) != NOMOVE) {

        if (move == excludedMove || !IsLegal(pos, move))
            continue;

        totalMoves++;

        const bool isQuiet = !isTactical(move);

        const int moveHistory = GetHistoryScore(pos, sd, move, ss);
        if (   !rootNode
            &&  BoardHasNonPawns(pos, pos->side)
            &&  bestScore > -MATE_FOUND) {

            // lmrDepth is the current depth minus the reduction the move would undergo in lmr, this is helpful because it helps us discriminate the bad moves with more accuracy
            const int lmrDepth = std::max(0, depth - reductions[isQuiet][std::min(depth, 63)][std::min(totalMoves, 63)] + moveHistory / 16384);

            if (!skipQuiets) {

                // Movecount pruning: if we searched enough moves and we are not in check we skip the rest
                if (!pvNode
                    && !inCheck
                    && totalMoves > lmp_margin[std::min(depth, 63)][improving]) {
                    skipQuiets = true;
                }

                // Futility pruning: if the static eval is so low that even after adding a bonus we are still under alpha we can stop trying quiet moves
                if (!inCheck
                    && lmrDepth < 11
                    && ss->staticEval + 250 + 150 * lmrDepth <= alpha) {
                    skipQuiets = true;
                }
            }

            // See pruning: prune all the moves that have a SEE score that is lower than our threshold
            if (    depth <= 8
                && !SEE(pos, move, see_margin[std::min(lmrDepth, 63)][isQuiet]))
                continue;
        }

        int extension = 0;
        // Limit Extensions to try and curb search explosions
        if (ss->ply < td->RootDepth * 2) {
            // Singular Extensions
            if (   !rootNode
                &&  depth >= 7
                &&  move == ttMove
                && !excludedMove
                && (ttBound & HFLOWER)
                &&  abs(ttScore) < MATE_FOUND
                &&  tte.depth >= depth - 3) {
                const int singularBeta = ttScore - depth;
                const int singularDepth = (depth - 1) / 2;

                ss->excludedMove = ttMove;
                int singularScore = Negamax<false>(singularBeta - 1, singularBeta, singularDepth, cutNode, td, ss);
                ss->excludedMove = NOMOVE;

                if (singularScore < singularBeta) {
                    extension = 1;
                    // Avoid search explosion by limiting the number of double extensions
                    if (   !pvNode
                        &&  singularScore < singularBeta - 17
                        &&  ss->doubleExtensions <= 11) {
                        extension = 2 + (!isTactical(ttMove) && singularScore < singularBeta - 100);
                        ss->doubleExtensions = (ss - 1)->doubleExtensions + 1;
                        depth += depth < 10;
                    }
                }
                else if (singularBeta >= beta)
                    return singularBeta;

                // If we didn't successfully extend and our TT score is above beta reduce the search depth
                else if (ttScore >= beta)
                    extension = -2;

                // If we are expecting a fail-high both based on search states from previous plies and based on TT bound
                // but our TT move is not singular and our TT score is failing low, reduce the search depth
                else if (cutNode)
                    extension = -1;
            }
        }
        // we adjust the search depth based on potential extensions
        int newDepth = depth - 1 + extension;
        // Speculative prefetch of the TT entry
        TTPrefetch(keyAfter(pos, move));
        ss->move = move;

        // Play the move
        MakeMove(move, pos);
        ss->contHistEntry = &sd->contHist[PieceTo(move)];
        // Add any played move to the matching list
        AddMove(move, isQuiet ? &quietMoves : &noisyMoves);

        // increment nodes count
        info->nodes++;
        const uint64_t nodesBeforeSearch = info->nodes;
        // Conditions to consider LMR. Calculate how much we should reduce the search depth.
        if (totalMoves > 1 + pvNode && depth >= 3 && (isQuiet || !ttPv)) {

            // Get base reduction value
            int depthReduction = reductions[isQuiet][std::min(depth, 63)][std::min(totalMoves, 63)];

            // Fuck
            if (cutNode)
                depthReduction += 2;

            // Reduce more if we are not improving
            if (!improving)
                depthReduction += 1;

            // Reduce less if the move is a refutation
            if (move == mp.killer || move == mp.counter)
                depthReduction -= 1;

            // Reduce less if we have been on the PV
            if (ttPv)
                depthReduction -= 1 + cutNode;

            // Decrease the reduction for moves that give check
            if (pos->checkers)
                depthReduction -= 1;

            // Decrease the reduction for moves that have a good history score and increase it for moves with a bad score
            depthReduction -= moveHistory / 16384;

            // adjust the reduction so that we can't drop into Qsearch and to prevent extensions
            depthReduction = std::clamp(depthReduction, 0, newDepth - 1);

            int reducedDepth = newDepth - depthReduction;
            // search current move with reduced depth:
            score = -Negamax<false>(-alpha - 1, -alpha, reducedDepth, true, td, ss + 1);

            // if we failed high on a reduced node we'll search with a reduced window and full depth
            if (score > alpha && newDepth > reducedDepth) {
                // Based on the value returned by our reduced search see if we should search deeper or shallower, 
                // this is an exact yoink of what SF does and frankly i don't care lmao
                const bool doDeeperSearch = score > (bestScore + 53 + 2 * newDepth);
                const bool doShallowerSearch = score < (bestScore + newDepth);
                newDepth += doDeeperSearch - doShallowerSearch;
                if (newDepth > reducedDepth)
                    score = -Negamax<false>(-alpha - 1, -alpha, newDepth, !cutNode, td, ss + 1);

                int bonus = score > alpha ? history_bonus(depth)
                                          : -history_bonus(depth);
                updateCHScore(ss, move, bonus);
            }
        }
        // If we skipped LMR and this isn't the first move of the node we'll search with a reduced window and full depth
        else if (!pvNode || totalMoves > 1) {
            score = -Negamax<false>(-alpha - 1, -alpha, newDepth, !cutNode, td, ss + 1);
        }

        // PV Search: Search the first move and every move that beat alpha with full depth and a full window
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
                    // If the move that caused the beta cutoff is quiet we have a killer move
                    if (isQuiet) {
                        ss->searchKiller = bestMove;

                        // Save counterMoves
                        if (ss->ply >= 1)
                            sd->counterMoves[FromTo((ss - 1)->move)] = move;
                    }
                    // Update the history heuristics based on the new best move
                    UpdateHistories(pos, sd, ss, depth + (eval <= alpha), bestMove, &quietMoves, &noisyMoves);

                    // node (move) fails high
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
    // Set the TT bound based on whether we failed high or raised alpha
    int bound = bestScore >= beta ? HFLOWER : alpha != old_alpha ? HFEXACT : HFUPPER;

    if (!excludedMove)
        StoreTTEntry(pos->posKey, MoveToTT(bestMove), ScoreToTT(bestScore, ss->ply), ss->staticEval, bound, depth, pvNode, ttPv);

    return bestScore;
}

// Quiescence search to avoid the horizon effect
template <bool pvNode>
int Quiescence(int alpha, int beta, ThreadData* td, SearchStack* ss) {
    Position* pos = &td->pos;
    SearchData* sd = &td->sd;
    SearchInfo* info = &td->info;
    const bool inCheck = pos->checkers;
    // tte is an TT entry, it will store the values fetched from the TT
    TTEntry tte;
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
    if (alpha < 0 && hasGameCycle(pos,ss->ply))
    {
        alpha = 0;
        if (alpha >= beta)
            return alpha;
    }

    // ttHit is true if and only if we find something in the TT
    const bool ttHit = ProbeTTEntry(pos->GetPoskey(), &tte);
    const int ttScore = ttHit ? ScoreFromTT(tte.score, ss->ply) : SCORE_NONE;
    const int ttMove = ttHit ? MoveFromTT(pos, tte.move) : NOMOVE;
    const uint8_t ttBound = ttHit ? BoundFromTT(tte.ageBoundPV) : uint8_t(HFNONE);
    // If we found a value in the TT for this position, we can return it (pv nodes are excluded)
    if (   !pvNode
        &&  ttScore != SCORE_NONE
        && (   (ttBound == HFUPPER && ttScore <= alpha)
            || (ttBound == HFLOWER && ttScore >= beta)
            ||  ttBound == HFEXACT))
        return ttScore;

    const bool ttPv = pvNode || (ttHit && FormerPV(tte.ageBoundPV));

    if (inCheck) {
        ss->staticEval = SCORE_NONE;
        bestScore = -MAXSCORE;
    }
    // If we have a ttHit with a valid eval use that
    else if (ttHit) {

        // If the value in the TT is valid we use that, otherwise we call the static evaluation function
        ss->staticEval = bestScore = tte.eval != SCORE_NONE ? tte.eval : EvalPosition(pos);

        // We can also use the TT score as a more accurate form of eval
        if (    ttScore != SCORE_NONE
            && (   (ttBound == HFUPPER && ttScore < bestScore)
                || (ttBound == HFLOWER && ttScore > bestScore)
                ||  ttBound == HFEXACT))
            bestScore = ttScore;
    }
    // If we don't have any useful info in the TT just call Evalpos
    else {
        bestScore = ss->staticEval = EvalPosition(pos);
        // Save the eval into the TT
        StoreTTEntry(pos->posKey, NOMOVE, SCORE_NONE, ss->staticEval, HFNONE, 0, pvNode, ttPv);
    }

    // Stand pat
    if (bestScore >= beta)
        return bestScore;

    // Adjust alpha based on eval
    alpha = std::max(alpha, bestScore);

    Movepicker mp;
    // If we aren't in check we generate just the captures, otherwise we generate all the moves
    InitMP(&mp, pos, sd, ss, ttMove, QSEARCH);

    int bestmove = NOMOVE;
    int move;
    int totalMoves = 0;

    // loop over moves within the movelist
    while ((move = NextMove(&mp, !inCheck || bestScore > -MATE_FOUND)) != NOMOVE) {

        if (!IsLegal(pos, move))
            continue;

        totalMoves++;

        // Futility pruning. If static eval is far below alpha, only search moves that win material.
        if (    bestScore > -MATE_FOUND
            && !inCheck
            &&  BoardHasNonPawns(pos, pos->side)) {
            const int futilityBase = ss->staticEval + 192;
            if (futilityBase <= alpha && !SEE(pos, move, 1)) {
                bestScore = std::max(futilityBase, bestScore);
                continue;
            }
        }
        // Speculative prefetch of the TT entry
        TTPrefetch(keyAfter(pos, move));
        ss->move = move;
        // Play the move
        MakeMove(move, pos);
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
            // Update  what the best score is
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

    StoreTTEntry(pos->posKey, MoveToTT(bestmove), ScoreToTT(bestScore, ss->ply), ss->staticEval, bound, 0, pvNode, ttPv);

    return bestScore;
}
