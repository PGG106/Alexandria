#include "init.h"
#include "position.h"
#include "attack.h"
#include "cuckoo.h"
#include "magic.h"
#include "random.h"
#include "misc.h"
#include "search.h"
#include "ttable.h"
#include "threads.h"
#include "history.h"
#include <cstdint>
#include <cmath>
#ifdef _WIN32
#include <windows.h>
#endif

Bitboard FileBBMask[8];
Bitboard RankBBMask[8];

// pawn attacks table [side][square]
Bitboard pawn_attacks[2][64];

// knight attacks table [square]
Bitboard knight_attacks[64];

// king attacks table [square]
Bitboard king_attacks[64];

// bishop attack masks
Bitboard bishop_masks[64];

// rook attack masks
Bitboard rook_masks[64];

// bishop attacks table [square][pos->occupancies]
Bitboard bishop_attacks[64][512];

// rook attacks rable [square][pos->occupancies]
Bitboard rook_attacks[64][4096];

Bitboard SQUARES_BETWEEN_BB[64][64];

// Initialize the Zobrist keys
void initHashKeys() {
    for (int Typeindex = WP; Typeindex <= BK; ++Typeindex) {
        for (int squareIndex = 0; squareIndex < 64; ++squareIndex) {
            PieceKeys[Typeindex][squareIndex] = GetRandomU64Number();
        }
    }
    // loop over board squares
    for (int square = 0; square < 64; square++)
        // init random enpassant keys
        enpassant_keys[square] = GetRandomU64Number();

    // loop over castling keys
    for (int index = 0; index < 16; index++)
        // init castling keys
        CastleKeys[index] = GetRandomU64Number();

    // init random side key
    SideKey = GetRandomU64Number();
}

// init attack tables for all the piece types, indexable by square
void InitAttackTables() {
    for (int square = 0; square < 64; square++) {
        // init pawn attacks
        pawn_attacks[WHITE][square] = MaskPawnAttacks(WHITE, square);
        pawn_attacks[BLACK][square] = MaskPawnAttacks(BLACK, square);

        // init knight attacks
        knight_attacks[square] = MaskKnightAttacks(square);

        // init king attacks
        king_attacks[square] = MaskKingAttacks(square);

        bishop_masks[square] = MaskBishopAttacks(square);

        // init bishop mask
        Bitboard bishop_mask = bishop_masks[square];

        // get the relevant occupancy bit count
        int relevant_bits_count = CountBits(bishop_mask);

        // init occupancy indices
        int occupancy_indices = 1 << relevant_bits_count;

        // loop over occupancy indices
        for (int index = 0; index < occupancy_indices; index++) {
                // init current occupancy variation
                Bitboard occupancy = SetOccupancy(index, relevant_bits_count, bishop_mask);

                // init magic index
                uint64_t magic_index = (occupancy * bishop_magic_numbers[square]) >> (64 - bishop_relevant_bits);

                // init bishop attacks
                bishop_attacks[square][magic_index] = BishopAttacksOnTheFly(square, occupancy);
        }

        rook_masks[square] = MaskRookAttacks(square);

        // init rook mask
        Bitboard rook_mask = rook_masks[square];

        // init relevant occupancy bit count
        relevant_bits_count = CountBits(rook_mask);

        // init occupancy indices
        occupancy_indices = 1 << relevant_bits_count;

        // loop over occupancy indices
        for (int index = 0; index < occupancy_indices; index++) {
            // init current occupancy variation
            Bitboard occupancy = SetOccupancy(index, relevant_bits_count, rook_mask);

            // init magic index
            uint64_t magic_index = (occupancy * rook_magic_numbers[square]) >> (64 - rook_relevant_bits);

            // init rook attacks
            rook_attacks[square][magic_index] = RookAttacksOnTheFly(square, occupancy);
        }
    }
}

void initializeLookupTables() {
    // initialize squares between table
    Bitboard sqs;
    for (int sq1 = 0; sq1 < 64; ++sq1) {
        for (int sq2 = 0; sq2 < 64; ++sq2) {
            sqs = (1ULL << sq1) | (1ULL << sq2);
            if (get_file[sq1] == get_file[sq2] || get_rank[sq1] == get_rank[sq2])
                SQUARES_BETWEEN_BB[sq1][sq2] = GetRookAttacks(sq1, sqs) & GetRookAttacks(sq2, sqs);
            else if (get_diagonal[sq1] == get_diagonal[sq2] || get_antidiagonal(sq1) == get_antidiagonal(sq2))
                SQUARES_BETWEEN_BB[sq1][sq2] = GetBishopAttacks(sq1, sqs) & GetBishopAttacks(sq2, sqs);
        }
    }
}

// Precalculate the logarithms used in the reduction calculation
void InitReductions() {
    for (int depth = 0; depth < 64; ++depth) {
        // SEE Margins
        seeMargins[0][depth] = -double(tacticalSeeCoeff()) * std::pow(depth, double(tacticalSeePower()) / 100.0) / 100.0; // Tactical SEE margin
        seeMargins[1][depth] = -double(   quietSeeCoeff()) * std::pow(depth, double(   quietSeePower()) / 100.0) / 100.0; // Quiet SEE margin

        // FP Margins
        futilityMargins[0][depth] = fpNonImpMarginQuadratic() * depth * depth + fpNonImpMarginLinear() * depth + fpNonImpMarginConst();
        futilityMargins[1][depth] = fpImpMarginQuadratic()    * depth * depth + fpImpMarginLinear()    * depth + fpImpMarginConst();

        // LMP Margins
        lmpMargins[0][depth] = (double(lmpNonImpMarginConst()) + double(lmpNonImpMarginMult()) * std::pow(depth, double(lmpNonImpMarginPower()) / 100.0)) / 100.0;
        lmpMargins[1][depth] = (double(lmpImpMarginConst()   ) + double(lmpImpMarginMult()   ) * std::pow(depth, double(lmpImpMarginPower()   ) / 100.0)) / 100.0;

        // LMR Reductions
        for (int moves = 0; moves < 64; ++moves) {
            // Manually set reduction to 0 if depth or moves is 0 as log(0) is NaN
            if (depth == 0 || moves == 0) {
                lmrReductions[0][moves][depth] = 0;
                lmrReductions[1][moves][depth] = 0;
                continue;
            }
            lmrReductions[0][moves][depth] = double(tacticalLmrBase()) + double(tacticalLmrMult()) * std::log(depth) * std::log(moves); // Tactical LMR
            lmrReductions[1][moves][depth] = double(quietLmrBase()) + double(quietLmrMult()) * std::log(depth) * std::log(moves); // Quiet LMR
        }
    }
}

void initCuckoo(){
    // keep a total tally of the table entries to sanity check the init
    int count = 0;
    // Reset any previously initialized value
    keys.fill(0);
    cuckooMoves.fill(NOMOVE);

    for(int piece = WN; piece <= BK; piece++) {
        if (piece == BP) continue;
        for (int square0 = 0; square0 < 64; square0++) {
            for (int square1 = square0 + 1; square1 < 64; square1++) {

                // check if a piece of piecetype standing on square0 could attack square1
                const Bitboard possibleattackoverlapthing = GetPieceTypeNonPawnAttacksToSquare(GetPieceType(piece), square0, 0) & (1ULL << square1);
                if (possibleattackoverlapthing == 0ULL)
                    continue;
                Move move = encode_move(square0,square1,PAWN,Movetype::Quiet);
                ZobristKey key = PieceKeys[piece][square0] ^ PieceKeys[piece][square1] ^ SideKey;
                uint32_t slot = H1(key);
                while (true)
                {
                    std::swap(keys[slot], key);
                    std::swap(cuckooMoves[slot], move);

                    if (move == NOMOVE)
                        break;

                    slot = slot == H1(key) ? H2(key) : H1(key);
                }
                ++count;
            }
        }
    }
    assert(count == 3668);
}

void InitAll() {
    // Force windows to display colors
#ifdef _WIN64
    if (!tryhardmode) {
        HANDLE stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD flags;
        GetConsoleMode(stdoutHandle, &flags);
        SetConsoleMode(stdoutHandle, flags | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif
    // init attacks tables
    InitAttackTables();
    initializeLookupTables();
    initHashKeys();
    InitReductions();
    // Init TT
    InitTT(16);
    nnue.init("nn.net");
    initCuckoo();
}

void InitNewGame(ThreadData* td) {
    // Extract data structures from ThreadData
    Position* pos = &td->pos;
    SearchData* sd = &td->sd;
    SearchInfo* info = &td->info;
    PvTable* pvTable = &td->pvTable;

    CleanHistories(sd);

    // Clean the PV Table
    for (int index = 0; index < MAXDEPTH + 1; ++index) {
        pvTable->pvLength[index] = 0;
        for (int index2 = 0; index2 < MAXDEPTH + 1; ++index2) {
            pvTable->pvArray[index][index2] = NOMOVE;
        }
    }

    // Reset plies and search info
    info->starttime = GetTimeMs();
    info->stopped = 0;
    info->nodes = 0;
    info->seldepth = 0;
    // Clear TT
    ClearTT();

    // Empty threads and thread data
    StopHelperThreads();

    threads_data.clear();

    // delete played moves hashes
    pos->played_positions.clear();
    // call parse position function
    ParsePosition("position startpos", pos);
}
