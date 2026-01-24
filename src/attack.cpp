#include <algorithm>
#include "attack.h"
#include "bitboard.h"

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
        int square = popLsb(attack_mask);
        // make sure occupancy is on board
        if (index & (1 << count))
            // populate occupancy map
            occupancy |= (1ULL << square);
    }
    // return occupancy map
    return occupancy;
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
#if defined (USE_PEXT)
            // init magic index
            const uint64_t attack_index = _pext_u64(occupancy, bishop_mask);
#else
           const uint64_t magic_index = (occupancy * bishop_magic_numbers[square]) >> bishop_shift;
#endif
            // init bishop attacks
            bishop_attacks[square][attack_index] = BishopAttacksOnTheFly(square, occupancy);
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

#if defined (USE_PEXT)
            const uint64_t attack_index = _pext_u64(occupancy, rook_mask);
#else
            const uint64_t attack_index = (occupancy * rook_magic_numbers[square]) >> rook_shift;
#endif
            // init rook attacks
            rook_attacks[square][attack_index] = RookAttacksOnTheFly(square, occupancy);
        }
    }
}
