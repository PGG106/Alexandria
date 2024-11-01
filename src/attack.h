#pragma once

#include <array>
using std::array;
#include "types.h"
#include "position.h"

// not A file constant
constexpr Bitboard not_a_file = 18374403900871474942ULL;

// not H file constant
constexpr Bitboard not_h_file = 9187201950435737471ULL;

// not HG file constant
constexpr Bitboard not_hg_file = 4557430888798830399ULL;

// not AB file constant
constexpr Bitboard not_ab_file = 18229723555195321596ULL;

constexpr Bitboard rank_2_mask = 65280ULL;

constexpr Bitboard rank_7_mask = 71776119061217280ULL;

// bishop relevant occupancy bit count for every square on board
constexpr int bishop_relevant_bits = 9;

// rook relevant occupancy bit count for every square on board
constexpr int rook_relevant_bits = 12;

// rook magic numbers
constexpr Bitboard rook_magic_numbers[64] = {
    0x8a80104000800020ULL, 0x84020100804000ULL, 0x800a1000048020ULL,
    0xc4100020b1000200ULL, 0x400440002080420ULL, 0xa8004002a801200ULL,
    0x840140c80400100ULL, 0x10000820c412300ULL, 0x10800212400820ULL,
    0x8050190002800ULL, 0x1080800102000ULL, 0x41080080201001ULL,
    0x20820040800890aULL, 0x10800200008440ULL, 0x3200800418a0022ULL,
    0x250060600201100ULL, 0x4440002400860020ULL, 0x1004402800084000ULL,
    0x41404c0140004ULL, 0x5000400908001400ULL, 0x20841000830ULL,
    0x830a0101000500ULL, 0x14040a002804040ULL, 0x4400101008854220ULL,
    0xe008025220022600ULL, 0x440244008603000ULL, 0x8024004009000ULL,
    0x801009002100002ULL, 0x400200200010811ULL, 0x3204020044012400ULL,
    0x2100088200100ULL, 0x20800a004091041ULL, 0x210c224200241ULL,
    0x200a0c02040080ULL, 0x4d8028104c0800ULL, 0x813c0a0002900012ULL,
    0x8104200208020ULL, 0x240400a000a04080ULL, 0x802199100100042ULL,
    0x62c4c0020100280ULL, 0x20104280800820ULL, 0x20c8010080a80200ULL,
    0x1114084080464008ULL, 0x2000025430001805ULL, 0x1404c4a100110008ULL,
    0x8400012008ULL, 0x3045140080022010ULL, 0x8040028410080100ULL,
    0x220200310204820ULL, 0x200082244048202ULL, 0x90984c0208022ULL,
    0x8000110120040900ULL, 0x9000402400080084ULL, 0x2402100100038020ULL,
    0x98400600008028ULL, 0x111000040200cULL, 0x102402208108102ULL,
    0x440041482204101ULL, 0x4004402000040811ULL, 0x804a000810402002ULL,
    0x8000209020401ULL, 0x440341108009002ULL, 0x8825084204ULL,
    0x2084002112428402ULL };

// bishop magic numbers
constexpr Bitboard bishop_magic_numbers[64] = {
    0x80810410820200ULL, 0x2010520422401000ULL, 0x88a01411a0081800ULL,
    0x1001050002610001ULL, 0x9000908280000000ULL, 0x20080442a0000001ULL,
    0x221a80045080800ULL, 0x60200a404000ULL, 0x20100894408080ULL,
    0x800084021404602ULL, 0x40804100298014ULL, 0x5080201060400011ULL,
    0x49000620a0000000ULL, 0x8000001200300000ULL, 0x4000008241100060ULL,
    0x40920160200ULL, 0x42002000240090ULL, 0x484100420a804ULL,
    0x8000102000910ULL, 0x4880010a8100202ULL, 0x4018804040402ULL,
    0x202100108281120ULL, 0xc201162010101042ULL, 0x240088022010b80ULL,
    0x8301600c240814ULL, 0x28100e142050ULL, 0x20880000838110ULL,
    0x410800040204a0ULL, 0x2012002206008040ULL, 0x4402881900a008ULL,
    0x14a80004804c1080ULL, 0xa004814404800f02ULL, 0xc0180230101600ULL,
    0xc905200020080ULL, 0x60400080010404aULL, 0x40401080c0100ULL,
    0x20121010140040ULL, 0x500080000861ULL, 0x8202090241002020ULL,
    0x2008022008002108ULL, 0x200402401042000ULL, 0x2e03210042000ULL,
    0x110040080422400ULL, 0x908404c0584040c0ULL, 0x1000204202240408ULL,
    0x8002002200200200ULL, 0x2002008101081414ULL, 0x2080021098404ULL,
    0x60110080680000ULL, 0x1080048108420000ULL, 0x400184014100000ULL,
    0x8081a004012240ULL, 0x110080448182a0ULL, 0xa4002000604a4000ULL,
    0x4002811049020ULL, 0x24a0410a10220ULL, 0x808090089013000ULL,
    0xc80800400805800ULL, 0x1020100061618ULL, 0x1202820040501008ULL,
    0x413010050c100405ULL, 0x4248204042020ULL, 0x44004408280110ULL,
    0x6010220080600502ULL };

// generate pawn attacks
[[nodiscard]] constexpr Bitboard MaskPawnAttacks(int side, int square) {
    // result attacks bitboard
    Bitboard attacks = 0ULL;
    // piece bitboard
    Bitboard bitboard = 0ULL;
    // set piece on board
    set_bit(bitboard, square);
    // white pawns
    if (!side) {
        // generate pawn attacks
        if ((bitboard >> 7) & not_a_file)
            attacks |= (bitboard >> 7);
        if ((bitboard >> 9) & not_h_file)
            attacks |= (bitboard >> 9);
    }
    // black pawns
    else {
        // generate pawn attacks
        if ((bitboard << 7) & not_h_file)
            attacks |= (bitboard << 7);
        if ((bitboard << 9) & not_a_file)
            attacks |= (bitboard << 9);
    }
    // return attack map
    return attacks;
}

// generate knight attacks
[[nodiscard]] constexpr Bitboard MaskKnightAttacks(int square) {
    // result attacks bitboard
    Bitboard attacks = 0ULL;
    // piece bitboard
    Bitboard bitboard = 0ULL;
    // set piece on board
    set_bit(bitboard, square);
    // generate knight attacks
    if ((bitboard >> 17) & not_h_file)
        attacks |= (bitboard >> 17);
    if ((bitboard >> 15) & not_a_file)
        attacks |= (bitboard >> 15);
    if ((bitboard >> 10) & not_hg_file)
        attacks |= (bitboard >> 10);
    if ((bitboard >> 6) & not_ab_file)
        attacks |= (bitboard >> 6);
    if ((bitboard << 17) & not_a_file)
        attacks |= (bitboard << 17);
    if ((bitboard << 15) & not_h_file)
        attacks |= (bitboard << 15);
    if ((bitboard << 10) & not_ab_file)
        attacks |= (bitboard << 10);
    if ((bitboard << 6) & not_hg_file)
        attacks |= (bitboard << 6);

    // return attack map
    return attacks;
}

// generate king attacks
[[nodiscard]] constexpr Bitboard MaskKingAttacks(int square) {
    // result attacks bitboard
    Bitboard attacks = 0ULL;

    // piece bitboard
    Bitboard bitboard = 0ULL;

    // set piece on board
    set_bit(bitboard, square);

    // generate king attacks
    if (bitboard >> 8)
        attacks |= (bitboard >> 8);
    if ((bitboard >> 9) & not_h_file)
        attacks |= (bitboard >> 9);
    if ((bitboard >> 7) & not_a_file)
        attacks |= (bitboard >> 7);
    if ((bitboard >> 1) & not_h_file)
        attacks |= (bitboard >> 1);
    if (bitboard << 8)
        attacks |= (bitboard << 8);
    if ((bitboard << 9) & not_a_file)
        attacks |= (bitboard << 9);
    if ((bitboard << 7) & not_h_file)
        attacks |= (bitboard << 7);
    if ((bitboard << 1) & not_a_file)
        attacks |= (bitboard << 1);

    // return attack map
    return attacks;
}


// mask bishop attacks
[[nodiscard]] constexpr Bitboard MaskBishopAttacks(int square) {
    // result attacks bitboard
    Bitboard attacks = 0ULL;

    // init target rank & files
    int tr = square / 8;
    int tf = square % 8;

    // mask relevant bishop occupancy bits
    for (int r = tr + 1, f = tf + 1; r <= 6 && f <= 6; r++, f++)
        attacks |= (1ULL << (r * 8 + f));
    for (int r = tr - 1, f = tf + 1; r >= 1 && f <= 6; r--, f++)
        attacks |= (1ULL << (r * 8 + f));
    for (int r = tr + 1, f = tf - 1; r <= 6 && f >= 1; r++, f--)
        attacks |= (1ULL << (r * 8 + f));
    for (int r = tr - 1, f = tf - 1; r >= 1 && f >= 1; r--, f--)
        attacks |= (1ULL << (r * 8 + f));

    // return attack map
    return attacks;
}


// mask rook attacks
[[nodiscard]] constexpr Bitboard MaskRookAttacks(int square) {
    // result attacks bitboard
    Bitboard attacks = 0ULL;
    // init target rank & files
    int tr = square / 8;
    int tf = square % 8;
    // mask relevant rook occupancy bits
    for (int r = tr + 1; r <= 6; r++)
        attacks |= (1ULL << (r * 8 + tf));
    for (int r = tr - 1; r >= 1; r--)
        attacks |= (1ULL << (r * 8 + tf));
    for (int f = tf + 1; f <= 6; f++)
        attacks |= (1ULL << (tr * 8 + f));
    for (int f = tf - 1; f >= 1; f--)
        attacks |= (1ULL << (tr * 8 + f));
    // return attack map
    return attacks;
}

// generate bishop attacks on the fly
[[nodiscard]] Bitboard constexpr BishopAttacksOnTheFly(int square, Bitboard block) {
    // result attacks bitboard
    Bitboard attacks = 0ULL;
    // init target rank & files
    int tr = square / 8;
    int tf = square % 8;
    // generate bishop atacks
    for (int r = tr + 1, f = tf + 1; r <= 7 && f <= 7; r++, f++) {
        attacks |= (1ULL << (r * 8 + f));
        if ((1ULL << (r * 8 + f)) & block)
            break;
    }

    for (int r = tr - 1, f = tf + 1; r >= 0 && f <= 7; r--, f++) {
        attacks |= (1ULL << (r * 8 + f));
        if ((1ULL << (r * 8 + f)) & block)
            break;
    }

    for (int r = tr + 1, f = tf - 1; r <= 7 && f >= 0; r++, f--) {
        attacks |= (1ULL << (r * 8 + f));
        if ((1ULL << (r * 8 + f)) & block)
            break;
    }

    for (int r = tr - 1, f = tf - 1; r >= 0 && f >= 0; r--, f--) {
        attacks |= (1ULL << (r * 8 + f));
        if ((1ULL << (r * 8 + f)) & block)
            break;
    }
    // return attack map
    return attacks;
}

// generate rook attacks on the fly
[[nodiscard]] constexpr Bitboard RookAttacksOnTheFly(int square, Bitboard block) {
    // result attacks bitboard
    Bitboard attacks = 0ULL;
    // init target rank & files
    int tr = square / 8;
    int tf = square % 8;
    // generate rook attacks
    for (int r = tr + 1; r <= 7; r++) {
        attacks |= (1ULL << (r * 8 + tf));
        if ((1ULL << (r * 8 + tf)) & block)
            break;
    }

    for (int r = tr - 1; r >= 0; r--) {
        attacks |= (1ULL << (r * 8 + tf));
        if ((1ULL << (r * 8 + tf)) & block)
            break;
    }

    for (int f = tf + 1; f <= 7; f++) {
        attacks |= (1ULL << (tr * 8 + f));
        if ((1ULL << (tr * 8 + f)) & block)
            break;
    }

    for (int f = tf - 1; f >= 0; f--) {
        attacks |= (1ULL << (tr * 8 + f));
        if ((1ULL << (tr * 8 + f)) & block)
            break;
    }
    // return attack map
    return attacks;
}

// set occupancies
[[nodiscard]] constexpr Bitboard SetOccupancy(int index, int bits_in_mask, Bitboard attack_mask) {
    // occupancy map
    Bitboard occupancy = 0ULL;
    // loop over the range of bits within attack mask
    for (int count = 0; count < bits_in_mask; count++) {
        // get LSB index of attacks mask
        int square = popLsb(attack_mask);
        // make sure occupancy is on board
        if (index & (1 << count))
            // populate occupancy map
            occupancy |= (1ULL << square);
    }
    // return occupancy map
    return occupancy;
}

// pawn attacks table [side][square]
constexpr auto pawn_attacks = [](){
    array<array<Bitboard, 64>, 2> p_atk; 
    for (int square = 0; square < 64; square++) {
        p_atk[WHITE][square] = MaskPawnAttacks(WHITE, square);
        p_atk[BLACK][square] = MaskPawnAttacks(BLACK, square);
   }
   return p_atk;
} ();

// knight attacks table [square]
constexpr auto knight_attacks = [](){
    array<Bitboard, 64> n_atks; 
    for (int square = 0; square < 64; square++) {
        n_atks[square] = MaskKnightAttacks(square);
    }
    return n_atks;
} ();

// king attacks table [square]
constexpr auto king_attacks = []() {
    array<Bitboard, 64> k_atks;
    for (int square = 0; square < 64; square++) { 
        k_atks[square] = MaskKingAttacks(square);
    }
    return k_atks;
} ();

// bishop attack masks
constexpr auto bishop_masks = [](){
    array<Bitboard, 64> b_masks; 
    for (int square = 0; square < 64; square++) {
        b_masks[square] = MaskBishopAttacks(square);
    }
    return b_masks;
} ();

// rook attack masks
constexpr auto rook_masks = [](){
    array<Bitboard, 64> r_masks; 
    for (int square = 0; square < 64; square++) {
        r_masks[square] = MaskRookAttacks(square);
    }
    return r_masks;
} ();

constexpr auto bishop_attacks = [](){
    array<array<Bitboard, 512>, 64> b_atks {}; // bishop attacks table [square][pos->occupancies]
    for (int square = 0; square < 64; square++) {

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
                b_atks[square][magic_index] = BishopAttacksOnTheFly(square, occupancy);
        }
    }
    return b_atks;
} (); 

constexpr auto rook_attacks = []() {
    array<array<Bitboard, 4096>, 64> r_attacks {}; // rook attacks table [square][pos->occupancies]
    for (int square = 0; square < 64; square++) {
        // init rook mask
        Bitboard rook_mask = rook_masks[square];

        // init relevant occupancy bit count
        int relevant_bits_count = CountBits(rook_mask);

        // init occupancy indices
        int occupancy_indices = 1 << relevant_bits_count;

        // loop over occupancy indices
        for (int index = 0; index < occupancy_indices; index++) {
            // init current occupancy variation
            Bitboard occupancy = SetOccupancy(index, relevant_bits_count, rook_mask);

            // init magic index
            uint64_t magic_index = (occupancy * rook_magic_numbers[square]) >> (64 - rook_relevant_bits);

            // init rook attacks
            r_attacks[square][magic_index] = RookAttacksOnTheFly(square, occupancy);
        }
    }
    return r_attacks;
} ();

// get bishop attacks
[[nodiscard]] constexpr Bitboard GetBishopAttacks(const int square, Bitboard occupancy) {
    // get bishop attacks assuming current board occupancy
    occupancy &= bishop_masks[square];
    occupancy *= bishop_magic_numbers[square];
    occupancy >>= 64 - bishop_relevant_bits;

    // return bishop attacks
    return bishop_attacks[square][occupancy];
}

// get rook attacks
[[nodiscard]] constexpr Bitboard GetRookAttacks(const int square, Bitboard occupancy) {
    // get rook attacks assuming current board occupancy
    occupancy &= rook_masks[square];
    occupancy *= rook_magic_numbers[square];
    occupancy >>= 64 - rook_relevant_bits;

    // return rook attacks
    return rook_attacks[square][occupancy];
}

// get queen attacks
[[nodiscard]] constexpr Bitboard GetQueenAttacks(const int square, Bitboard occupancy) {
    return GetBishopAttacks(square, occupancy) | GetRookAttacks(square, occupancy);
}

// retrieves attacks for a generic piece (except pawns and kings because that requires actual work)
[[nodiscard]] constexpr Bitboard pieceAttacks(int piecetype,  int pieceSquare, Bitboard occ) {
    assert(piecetype > PAWN && piecetype < KING);
    switch (piecetype) {
        case KNIGHT:
            return knight_attacks[pieceSquare];
        case BISHOP:
            return GetBishopAttacks(pieceSquare, occ);
        case ROOK:
            return GetRookAttacks(pieceSquare, occ);
        case QUEEN:
            return GetQueenAttacks(pieceSquare, occ);
        default:
            __builtin_unreachable();
    }
}

[[nodiscard]] constexpr Bitboard GetPieceTypeNonPawnAttacksToSquare(int piecetype,  int pieceSquare, Bitboard occ) {
    assert(piecetype <= KING);
    if(piecetype == KNIGHT)
        return knight_attacks[pieceSquare];
    if(piecetype == BISHOP)
        return GetBishopAttacks(pieceSquare, occ);
    if(piecetype == ROOK)
        return GetRookAttacks(pieceSquare, occ);
    if(piecetype == QUEEN)
        return GetQueenAttacks(pieceSquare, occ);
    if(piecetype == KING)
        return king_attacks[pieceSquare];

    __builtin_unreachable();
}
