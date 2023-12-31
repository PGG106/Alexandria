#include "board.h"
#include "attack.h"
#include "magic.h"
#include "random.h"
#include "stdint.h"
#include <cmath>
#include "misc.h"
#include "search.h"
#include "ttable.h"
#include "threads.h"
#include "history.h"
#ifdef _WIN32
#include <windows.h>
#endif

Bitboard PieceKeys[12][64];
Bitboard enpassant_keys[64];
Bitboard SideKey;
Bitboard CastleKeys[16];

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

int reductions[2][MAXDEPTH][MAXPLY];
int lmp_margin[MAXDEPTH][2];
int see_margin[MAXDEPTH][2];

// Initialize the Zobrist keys
void initHashKeys() {
    for (int Typeindex = WP; Typeindex <= BK; ++Typeindex) {
        for (int Numberindex = 0; Numberindex < 64; ++Numberindex) {
            PieceKeys[Typeindex][Numberindex] = GetRandomBitboardNumber();
        }
    }
    // loop over board squares
    for (int square = 0; square < Board_sq_num; square++)
        // init random enpassant keys
        enpassant_keys[square] = GetRandomBitboardNumber();

    // loop over castling keys
    for (int index = 0; index < 16; index++)
        // init castling keys
        CastleKeys[index] = GetRandomBitboardNumber();

    // init random side key
    SideKey = GetRandomBitboardNumber();
}

// init leaper pieces attacks
void InitLeapersAttacks() {
    // loop over 64 board squares
    for (int square = 0; square < Board_sq_num; square++) {
        // init pawn attacks
        pawn_attacks[WHITE][square] = MaskPawnAttacks(WHITE, square);
        pawn_attacks[BLACK][square] = MaskPawnAttacks(BLACK, square);

        // init knight attacks
        knight_attacks[square] = MaskKnightAttacks(square);

        // init king attacks
        king_attacks[square] = MaskKingAttacks(square);
    }
}

// init slider piece's attack tables
void InitSlidersAttacks() {
    // Init bishop attacks
    for (int square = 0; square < Board_sq_num; square++) {
        bishop_masks[square] = MaskBishopAttacks(square);

        // init current mask
        Bitboard bishop_mask = bishop_masks[square];

        // init relevant occupancy bit count
        int relevant_bits_count = CountBits(bishop_mask);

        // init occupancy indicies
        int occupancy_indicies = (1 << relevant_bits_count);

        // loop over occupancy indicies
        for (int index = 0; index < occupancy_indicies; index++) {
                // init current occupancy variation
                Bitboard occupancy =
                    SetOccupancy(index, relevant_bits_count, bishop_mask);

                // init magic index
                uint64_t magic_index = (occupancy * bishop_magic_numbers[square]) >>
                    (64 - bishop_relevant_bits[square]);

                // init bishop attacks
                bishop_attacks[square][magic_index] =
                    BishopAttacksOnTheFly(square, occupancy);
        }
    }
    // Init rook attacks
    for (int square = 0; square < Board_sq_num; square++) {
        // init rook masks
        rook_masks[square] = MaskRookAttacks(square);

        // init current mask
        Bitboard rook_mask = rook_masks[square];

        // init relevant occupancy bit count
        int relevant_bits_count = CountBits(rook_mask);

        // init occupancy indicies
        int occupancy_indicies = (1 << relevant_bits_count);

        // loop over occupancy indicies
        for (int index = 0; index < occupancy_indicies; index++) {
            // init current occupancy variation
            Bitboard occupancy =
                SetOccupancy(index, relevant_bits_count, rook_mask);

            // init magic index
            uint64_t magic_index = (occupancy * rook_magic_numbers[square]) >>
                (64 - rook_relevant_bits[square]);

            // init rook attacks
            rook_attacks[square][magic_index] =
                RookAttacksOnTheFly(square, occupancy);
        }
    }
}

void initializeLookupTables() {
    // initialize squares between table
    Bitboard sqs;
    for (int sq1 = 0; sq1 <= 63; ++sq1) {
        for (int sq2 = 0; sq2 <= 63; ++sq2) {
            sqs = (1ULL << sq1) | (1ULL << sq2);
            if (get_file[sq1] == get_file[sq2] || (get_rank[sq1] == get_rank[sq2]))
                SQUARES_BETWEEN_BB[sq1][sq2] = GetRookAttacks(sq1, sqs) & GetRookAttacks(sq2, sqs);
            else if ((get_diagonal[sq1] == get_diagonal[sq2]) ||
                (get_antidiagonal(sq1) == get_antidiagonal(sq2)))
                SQUARES_BETWEEN_BB[sq1][sq2] =
                GetBishopAttacks(sq1, sqs) & GetBishopAttacks(sq2, sqs);
            else
                SQUARES_BETWEEN_BB[sq1][sq2] = 0;
        }
    }
}



// PreCalculate the logarithms used in the reduction calculation
void InitReductions() {
    // Avoid log(0) because it's bad
    reductions[0][0][0] = 0;
    reductions[1][0][0] = 0;

    for (int i = 1; i < MAXDEPTH; i++) {
        for (int j = 1; j < MAXDEPTH; j++) {
            reductions[0][i][j] = -0.25 + log(i) * log(j) / 2.25;
            reductions[1][i][j] = +1.00 + log(i) * log(j) / 2.00;
        }
    }

    for (int depth = 0; depth < MAXDEPTH; depth++) {
        lmp_margin[depth][0] = 1.5 + 0.5 * std::pow(depth, 2.0); // Not improving
        lmp_margin[depth][1] = 3.0 + 1.0 * std::pow(depth, 2.0); // improving

        see_margin[depth][1] = -80 * depth; // Quiet moves
        see_margin[depth][0] = -30 * depth * depth; // Non quiets

    }
}

void InitAll() {
    setvbuf(stdout, NULL, _IONBF, 0);
    // Force windows to display colors
#ifdef _WIN64
    HANDLE stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD flags;
    GetConsoleMode(stdoutHandle, &flags);
    SetConsoleMode(stdoutHandle, flags | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
    // init leaper pieces attacks
    InitLeapersAttacks();
    // init slider pieces attacks
    InitSlidersAttacks();
    initializeLookupTables();
    initHashKeys();
    InitReductions();
    // Init TT
    InitHashTable(HashTable, 16);
    nnue.init("nn.net");
}

void InitNewGame(S_ThreadData* td) {
    // Extract data structures from ThreadData
    S_Board* pos = &td->pos;
    Search_data* ss = &td->ss;
    S_SearchINFO* info = &td->info;
    PvTable* pvTable = &td->pvTable;

    CleanHistories(ss);

    // Clean the Pv array
    for (int index = 0; index < MAXDEPTH + 1; ++index) {
        pvTable->pvLength[index] = 0;
        for (int index2 = 0; index2 < MAXDEPTH + 1; ++index2) {
            pvTable->pvArray[index][index2] = NOMOVE;
        }
    }

    // Clean the Counter moves array
    for (int index = 0; index < Board_sq_num; ++index) {
        for (int index2 = 0; index2 < Board_sq_num; ++index2) {
            ss->CounterMoves[index][index2] = NOMOVE;
        }
    }

    // Reset plies and search info
    info->starttime = GetTimeMs();
    info->stopped = 0;
    info->nodes = 0;
    info->seldepth = 0;
    // Reset hash table
    ClearHashTable(HashTable);

    // Empty threads and thread data
    StopHelperThreads();

    threads_data.clear();

    // delete played moves hashes
    pos->played_positions.clear();
    // Empty the accumulator stack
    pos->accumulatorStack.clear();
    // call parse position function
    ParsePosition("position startpos", pos);
}
