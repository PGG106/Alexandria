#pragma once

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
[[nodiscard]] Bitboard MaskPawnAttacks(int side, int square);

// generate knight attacks
[[nodiscard]] Bitboard MaskKnightAttacks(int square);

// generate king attacks
[[nodiscard]] Bitboard MaskKingAttacks(int square);

// mask bishop attacks
[[nodiscard]] Bitboard MaskBishopAttacks(int square);

// mask rook attacks
[[nodiscard]] Bitboard MaskRookAttacks(int square);

// generate bishop attacks on the fly
[[nodiscard]] Bitboard BishopAttacksOnTheFly(int square, Bitboard block);

// generate rook attacks on the fly
[[nodiscard]] Bitboard RookAttacksOnTheFly(int square, Bitboard block);

// pawn attacks table [side][square]
extern Bitboard pawn_attacks[2][64];

// knight attacks table [square]
extern Bitboard knight_attacks[64];

// king attacks table [square]
extern Bitboard king_attacks[64];

// bishop attack masks
extern Bitboard bishop_masks[64];

// rook attack masks
extern Bitboard rook_masks[64];

// bishop attacks table [square][occupancies]
extern Bitboard bishop_attacks[64][512];

// rook attacks rable [square][occupancies]
extern Bitboard rook_attacks[64][4096];

// set occupancies
[[nodiscard]] Bitboard SetOccupancy(int index, int bits_in_mask, Bitboard attack_mask);

[[nodiscard]] Bitboard GetPieceTypeNonPawnAttacksToSquare(int piecetype,  int pieceSquare, Bitboard occ);
