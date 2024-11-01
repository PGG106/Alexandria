#pragma once

#include "position.h"
#include "attack.h"
#include "cuckoo.h"
#include "misc.h"
#include "search.h"
#include "ttable.h"
#include "threads.h"
#include "history.h"

#include <array>
#include <cstdint>
#include <cmath>
#ifdef _WIN32
#include <windows.h>
#endif

struct Position;
struct ThreadData;

using std::array;

// generate 64-bit pseudo legal numbers
[[nodiscard]] constexpr uint64_t GetRandomU64Number(uint64_t num) {
    // XOR shift algorithm
    num ^= num << 13;
    num ^= num >> 7;
    num ^= num << 17;

    // return random number
    return num;
}

constexpr auto PieceKeys = []() {
    array<array<Bitboard, 64>, 12> PK;
    uint64_t n = 6379633040001738036; // pseudo random number state

    for (int Typeindex = WP; Typeindex <= BK; ++Typeindex) {
        for (int squareIndex = 0; squareIndex < 64; ++squareIndex) {
            PK[Typeindex][squareIndex] = n = GetRandomU64Number(n);
        }
    }
    return PK;
} ();

constexpr auto enpassant_keys = [](){
    array<Bitboard, 64> EPKeys;
	  uint64_t n = PieceKeys[11][63];

    // loop over board squares
    for (int square = 0; square < 64; square++)
        EPKeys[square] = n = GetRandomU64Number(n); // init random enpassant keys
    return EPKeys;
} ();

constexpr auto CastleKeys = []() {
    array< Bitboard, 16> CK;
	  uint64_t n = enpassant_keys[63];

    // loop over castling keys
    for (int index = 0; index < 16; index++)
        CK[index] = n = GetRandomU64Number(n); // init castling keys
    return CK;
} ();

constexpr Bitboard SideKey = GetRandomU64Number(CastleKeys[15]);

constexpr auto SQUARES_BETWEEN_BB = []() {
    array<array<Bitboard, 64>, 64> SQ_BTWN {};
    // initialize squares between table
    Bitboard sqs;
    for (int sq1 = 0; sq1 < 64; ++sq1) {
        for (int sq2 = 0; sq2 < 64; ++sq2) {
            sqs = (1ULL << sq1) | (1ULL << sq2);
            if (get_file[sq1] == get_file[sq2] || get_rank[sq1] == get_rank[sq2])
                SQ_BTWN[sq1][sq2] = GetRookAttacks(sq1, sqs) & GetRookAttacks(sq2, sqs);
            else if (get_diagonal[sq1] == get_diagonal[sq2] || get_antidiagonal(sq1) == get_antidiagonal(sq2))
                SQ_BTWN[sq1][sq2] = GetBishopAttacks(sq1, sqs) & GetBishopAttacks(sq2, sqs);
        }
    }
    return SQ_BTWN;
} ();

inline void initCuckoo(){
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

// PreCalculate the logarithms used in the reduction calculation
inline void InitReductions() {
    // Avoid log(0) because it's bad
    reductions[0][0][0] = 0;
    reductions[1][0][0] = 0;

    for (int i = 1; i < 64; i++) {
        for (int j = 1; j < 64; j++) {
            reductions[0][i][j] = -0.25 + log(i) * log(j) / 2.25;
            reductions[1][i][j] = +1.00 + log(i) * log(j) / 2.00;
        }
    }

    for (int depth = 0; depth < 64; depth++) {
        lmp_margin[depth][0] = 1.5 + 0.5 * std::pow(depth, 2.0); // Not improving
        lmp_margin[depth][1] = 3.0 + 1.0 * std::pow(depth, 2.0); // improving

        see_margin[depth][1] = -80.0 * std::pow(depth, 1.0); // Quiet moves
        see_margin[depth][0] = -30.0 * std::pow(depth, 2.0); // Non quiets

    }
}

inline void InitNewGame(ThreadData* td) {
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

    std::memset(sd->counterMoves, NOMOVE, sizeof(sd->counterMoves));

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

inline void InitAll() {
    // Force windows to display colors
#ifdef _WIN64
    if (!tryhardmode) {
        HANDLE stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD flags;
        GetConsoleMode(stdoutHandle, &flags);
        SetConsoleMode(stdoutHandle, flags | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif
    InitReductions();
    // Init TT
    InitTT(16);
    nnue.init("nn.net");
    initCuckoo();
}
