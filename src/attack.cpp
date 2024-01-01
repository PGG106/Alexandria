#include "attack.h"
#include "magic.h"
#include <algorithm>

// generate pawn attacks
Bitboard MaskPawnAttacks(int side, int square) {
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
Bitboard MaskKnightAttacks(int square) {
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
Bitboard MaskKingAttacks(int square) {
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
Bitboard MaskBishopAttacks(int square) {
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
Bitboard MaskRookAttacks(int square) {
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
Bitboard BishopAttacksOnTheFly(int square, Bitboard block) {
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
Bitboard RookAttacksOnTheFly(int square, Bitboard block) {
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
Bitboard SetOccupancy(int index, int bits_in_mask, Bitboard attack_mask) {
    // occupancy map
    Bitboard occupancy = 0ULL;
    // loop over the range of bits within attack mask
    for (int count = 0; count < bits_in_mask; count++) {
        // get LSB index of attacks mask
        int square = GetLsbIndex(attack_mask);
        // pop LSB in attack map
        pop_lsb(attack_mask);
        // make sure occupancy is on board
        if (index & (1 << count))
            // populate occupancy map
            occupancy |= (1ULL << square);
    }
    // return occupancy map
    return occupancy;
}
