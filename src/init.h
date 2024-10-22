#pragma once

#include <array>
#include "types.h"

struct Position;
struct ThreadData;

// generate 64-bit pseudo legal numbers
[[nodiscard]] constexpr uint64_t GetRandomU64Number(uint64_t num){
	 // XOR shift algorithm
    num ^= num << 13;
    num ^= num >> 7;
    num ^= num << 17;

    // return random number
    return num;
}

constexpr auto genPieceKeys(){
	std::array< std::array<Bitboard, 64>, 12> PK;
	uint64_t n = 6379633040001738036; // pseudo random number state
	
	for (int Typeindex = WP; Typeindex <= BK; ++Typeindex) {
       for (int squareIndex = 0; squareIndex < 64; ++squareIndex) {
            n = PK[Typeindex][squareIndex] = GetRandomU64Number(n);
        }
    }
    return PK;
}

constexpr std::array< std::array<Bitboard, 64>, 12> PieceKeys = genPieceKeys();

constexpr auto genEPKeys(){
	std::array< Bitboard, 64> EPKeys;
	uint64_t n = PieceKeys[11][63];
	
   // loop over board squares
   for (int square = 0; square < 64; square++)
        n = EPKeys[square] = GetRandomU64Number(n); // init random enpassant keys
   return EPKeys;
}

constexpr std::array<Bitboard, 64> enpassant_keys = genEPKeys();

constexpr auto genCastlingKeys(){
	std::array< Bitboard, 16> CastleKeys;
	uint64_t n = enpassant_keys[63];
		
   // loop over castling keys
   for (int index = 0; index < 16; index++)
      n = CastleKeys[index] = GetRandomU64Number(n); // init castling keys
   return CastleKeys;
}

constexpr std::array<Bitboard, 16> CastleKeys = genCastlingKeys();

constexpr Bitboard SideKey = GetRandomU64Number(CastleKeys[15]);

void InitNewGame(ThreadData* td);

// init slider piece's attack tables
void InitAttackTables();

void InitAll();
