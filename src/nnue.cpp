#include "nnue.h"
#include "position.h"
#include "simd.h"
#include "threats.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#ifdef NNZ_PROFILE
#include <array>
#include <cstdlib>
#include <iostream>
#include <mutex>
#endif
#include "attack.h"
#include "incbin/incbin.h"
#include "io.h"
#include <fstream>

// Macro to embed the default efficiently updatable neural network (NNUE) file
// data in the engine binary (using incbin.h, by Dale Weiler).
// This macro invocation will declare the following three variables
//     const unsigned char        gEVALData[];  // a pointer to the embedded data
//     const unsigned char *const gEVALEnd;     // a marker to the end
//     const unsigned int         gEVALSize;    // the size of the embedded file
// Note that this does not work in Microsoft Visual Studio.
#if !defined(_MSC_VER)
INCBIN(EVAL, EVALFILE);
#else
const unsigned char gEVALData[1] = {};
const unsigned char *const gEVALEnd = &gEVALData[1];
const unsigned int gEVALSize = 1;
#endif

const Network *net;
NNZTable nnzTable;

#ifdef NNZ_PROFILE
namespace
{
#ifndef NNZ_PROFILE_SAMPLES
#define NNZ_PROFILE_SAMPLES 100000
#endif

    constexpr uint64_t NnzProfileSamples = NNZ_PROFILE_SAMPLES;
    constexpr std::size_t NnzProfileMaskBytes = (L1_SIZE / 2) / 8;
    static_assert((L1_SIZE / 2) % 8 == 0);

    const char *nnzProfilePath()
    {
        const char *path = std::getenv("NNZ_PROFILE_OUTPUT");
        return path != nullptr ? path : "nnz-masks.bin";
    }

    std::ofstream nnzProfile{nnzProfilePath(), std::ios::binary};
    std::mutex nnzProfileMutex;
    uint64_t nnzProfileSamples = 0;

    void recordNnzProfile(const uint8_t *output)
    {
        std::lock_guard lock{nnzProfileMutex};
        if (nnzProfileSamples >= NnzProfileSamples)
            return;
        if (!nnzProfile)
        {
            std::cerr << "Error: Could not write NNZ profile to " << nnzProfilePath() << '\n';
            std::abort();
        }

        std::array<uint8_t, NnzProfileMaskBytes> mask = {};
        for (int lane = 0; lane < L1_SIZE / 2; ++lane)
            mask[lane / 8] |= static_cast<uint8_t>((output[lane] != 0) << (lane % 8));

        nnzProfile.write(reinterpret_cast<const char *>(mask.data()), mask.size());
        ++nnzProfileSamples;
    }
}
#endif

QuantisedNetwork quantisedNet;
Network permutedNet;

uint16_t threatPairIndices[5][64][64];
int32_t threatIndexBases[2][12][12][2];

void NNUE::init()
{
    net = reinterpret_cast<const Network *>(gEVALData);

    for (int attackerType = PAWN; attackerType <= QUEEN; ++attackerType)
    {
        for (int square = 0; square < 64; ++square)
            for (int destination = 0; destination < 64; ++destination)
                threatPairIndices[attackerType][square][destination] = 0xFFFF;

        int pairCount = 0;
        for (int square = 0; square < 64; ++square)
        {
            if (attackerType == PAWN)
            {
                const int rank = square / 8;
                const int file = square % 8;
                if (rank == 0 || rank == 7)
                    continue;

                for (int destination = 0; destination < 64; ++destination)
                    if (std::abs(destination / 8 - rank) == 1
                        && std::abs(destination % 8 - file) == 1)
                        threatPairIndices[attackerType][square][destination] =
                            static_cast<uint16_t>((rank - 1) * 14 + 2 * file
                                                  + (destination % 8 > file) - 1);
            }
            else
            {
                Bitboard attacks = pieceAttacks(attackerType, square, 0ULL);
                while (attacks)
                {
                    const Square destination = popLsb(attacks);
                    threatPairIndices[attackerType][square][destination] = static_cast<uint16_t>(pairCount++);
                }
            }
        }
    }

    for (int perspective = WHITE; perspective <= BLACK; ++perspective)
        for (int attacker = WP; attacker <= BK; ++attacker)
            for (int victim = WP; victim <= BK; ++victim)
                for (int destinationAfterSource = 0; destinationAfterSource <= 1; ++destinationAfterSource)
                {
                    const int attackerType = PieceType[attacker];
                    const int targetType = PieceType[victim];
                    const int map = attackerType == KING ? -1 : TargetMap[attackerType][targetType];
                    if (map < 0
                        || (attackerType != PAWN && targetType == attackerType && destinationAfterSource))
                    {
                        threatIndexBases[perspective][attacker][victim][destinationAfterSource] = -1;
                        continue;
                    }

                    const int targetBlock = (Color[victim] != perspective) * ThreatTargetCount[attackerType] + map;
                    threatIndexBases[perspective][attacker][victim][destinationAfterSource] =
                        (Color[attacker] != perspective) * THREAT_INPUTS_PER_SIDE
                        + ThreatOffset[attackerType]
                        + targetBlock * ThreatCount[attackerType];
                }
}

// get the threat features for the given position
ThreatFeatures NNUE::getThreatFeatures(const Position *pos)
{
    ThreatFeatures result;
    const Bitboard occ = pos->Occupancy(BOTH);
    const bool flip[2] = {
        get_file[KingSQ(pos, WHITE)] > 3,
        get_file[KingSQ(pos, BLACK)] > 3};

    for (int piece = WP; piece <= BK; ++piece)
    {
        const int attackerType = PieceType[piece];
        if (attackerType == KING)
            continue;

        const int attackerColor = Color[piece];
        Bitboard attackingPieces = pos->state().bitboards[piece];

        while (attackingPieces)
        {
            const Square from = popLsb(attackingPieces);
            // TODO: Finally create a uniform interface for this mess
            Bitboard targets = attackerType == PAWN
                                   ? getPawnAttacks(from, attackerColor)
                                   : pieceAttacks(attackerType, from, occ);

            targets &= occ;
            while (targets)
            {
                const Square to = popLsb(targets);
                const int victim = pos->PieceOn(to);
                const int targetType = PieceType[victim];

                if (TargetMap[attackerType][targetType] < 0)
                    continue;

                for (int perspective = WHITE; perspective <= BLACK; ++perspective)
                {
                    const int index = getThreatIndex(piece, victim, from, to, perspective, flip[perspective]);
                    if (index < 0)
                        continue;
                    result.indices[perspective][result.count[perspective]++] = index;
                }
            }
        }
    }

    return result;
}

void NNUE::recordPieceAddedThreats(const Position *pos, const int piece, const int square,
                                   ThreatAccumulator &accumulator)
{
    const Bitboard *bitboards = pos->state().bitboards;
    const Bitboard occupancy = pos->Occupancy(BOTH);
    const int pieceType = PieceType[piece];

    ThreatDelta *created = accumulator.added;
    int &createdCount = accumulator.addedCount;
    ThreatDelta *blocked = accumulator.removed;
    int &blockedCount = accumulator.removedCount;

    // Kings don't generate threats so we can skip them
    if (pieceType != KING)
    {
        // Add all the new threats created by the changed piece.
        Bitboard attacked = (pieceType == PAWN
                                 ? getPawnAttacks(square, Color[piece])
                                 : pieceAttacks(pieceType, square, occupancy))
                            & occupancy;

        while (attacked)
        {
            const int to = popLsb(attacked);
            created[createdCount++] = {
                static_cast<uint8_t>(piece),
                static_cast<uint8_t>(square),
                static_cast<uint8_t>(pos->PieceOn(to)),
                static_cast<uint8_t>(to)};
        }

        // Non slider threats against the changed piece. (we don't have to care about discovered or blocked threats)
        Bitboard attackers = (getPawnAttacks(square, BLACK) & bitboards[WP])
                           | (getPawnAttacks(square, WHITE) & bitboards[BP])
                           | (getKnightAttacks(square) & (bitboards[WN] | bitboards[BN]));

        while (attackers)
        {
            const int from = popLsb(attackers);
            created[createdCount++] = {
                static_cast<uint8_t>(pos->PieceOn(from)),
                static_cast<uint8_t>(from),
                static_cast<uint8_t>(piece),
                static_cast<uint8_t>(square)};
        }
    }

    const Bitboard squareBit = 1ULL << square;
    const Bitboard queens = bitboards[WQ] | bitboards[BQ];
    const Bitboard bishops = bitboards[WB] | bitboards[BB];
    const Bitboard rooks = bitboards[WR] | bitboards[BR];

    // Find every bishop or queen that reaches the empty focus square.
    Bitboard diagonal = getBishopAttacks(square, occupancy)
                      & (bishops | queens);
                      
    while (diagonal)
    {
        const int from = popLsb(diagonal);
        const int slider = pos->PieceOn(from);

        // record the threat from the slider to the added piece
        if (pieceType != KING)
            created[createdCount++] = {
                static_cast<uint8_t>(slider),
                static_cast<uint8_t>(from),
                static_cast<uint8_t>(piece),
                static_cast<uint8_t>(square)};

        // figure out what pieces lie behind the piece we added on the same diagonal
        const Bitboard behind = getBishopAttacks(from, occupancy)
                              & ~getBishopAttacks(from, occupancy | squareBit)
                              & occupancy;
        // those attacks are not blocked by the newly added piece
        if (behind)
        {
            const int to = GetLsbIndex(behind);
            blocked[blockedCount++] = {
                static_cast<uint8_t>(slider),
                static_cast<uint8_t>(from),
                static_cast<uint8_t>(pos->PieceOn(to)),
                static_cast<uint8_t>(to)};
        }
    }

    // Repeat the same incoming/blocking calculation for rooks and queens.
    Bitboard orthogonal = getRookAttacks(square, occupancy)
                        & (rooks | queens);
    while (orthogonal)
    {
        const int from = popLsb(orthogonal);
        const int slider = pos->PieceOn(from);

        if (pieceType != KING)
            created[createdCount++] = {
                static_cast<uint8_t>(slider),
                static_cast<uint8_t>(from),
                static_cast<uint8_t>(piece),
                static_cast<uint8_t>(square)};

        const Bitboard behind = getRookAttacks(from, occupancy)
                              & ~getRookAttacks(from, occupancy | squareBit)
                              & occupancy;
        if (behind)
        {
            const int to = GetLsbIndex(behind);
            blocked[blockedCount++] = {
                static_cast<uint8_t>(slider),
                static_cast<uint8_t>(from),
                static_cast<uint8_t>(pos->PieceOn(to)),
                static_cast<uint8_t>(to)};
        }
    }
}

void NNUE::recordPieceRemovedThreats(const Position *pos, const int piece, const int square,
                                     ThreatAccumulator &accumulator)
{
    const Bitboard *bitboards = pos->state().bitboards;
    const Bitboard occupancy = pos->Occupancy(BOTH);
    const int pieceType = PieceType[piece];

    ThreatDelta *destroyed = accumulator.removed;
    int &destroyedCount = accumulator.removedCount;
    ThreatDelta *uncovered = accumulator.added;
    int &uncoveredCount = accumulator.addedCount;

    // Kings don't generate threats so we can skip them.
    if (pieceType != KING)
    {
        // Remove all threats that were made by the removed piece.
        Bitboard attacked = (pieceType == PAWN
                                 ? getPawnAttacks(square, Color[piece])
                                 : pieceAttacks(pieceType, square, occupancy))
                            & occupancy;

        while (attacked)
        {
            const int to = popLsb(attacked);
            destroyed[destroyedCount++] = {
                static_cast<uint8_t>(piece),
                static_cast<uint8_t>(square),
                static_cast<uint8_t>(pos->PieceOn(to)),
                static_cast<uint8_t>(to)};
        }

        // Remove pawn and knight threats against the removed piece.
        Bitboard attackers = (getPawnAttacks(square, BLACK) & bitboards[WP])
                           | (getPawnAttacks(square, WHITE) & bitboards[BP])
                           | (getKnightAttacks(square) & (bitboards[WN] | bitboards[BN]));

        while (attackers)
        {
            const int from = popLsb(attackers);
            destroyed[destroyedCount++] = {
                static_cast<uint8_t>(pos->PieceOn(from)),
                static_cast<uint8_t>(from),
                static_cast<uint8_t>(piece),
                static_cast<uint8_t>(square)};
        }
    }

    const Bitboard squareBit = 1ULL << square;
    const Bitboard queens = bitboards[WQ] | bitboards[BQ];

    // Find every bishop or queen that reached the removed piece.
    Bitboard diagonal = getBishopAttacks(square, occupancy)
                      & (bitboards[WB] | bitboards[BB] | queens);
    while (diagonal)
    {
        const int from = popLsb(diagonal);
        const int slider = pos->PieceOn(from);

        if (pieceType != KING)
            destroyed[destroyedCount++] = {
                static_cast<uint8_t>(slider),
                static_cast<uint8_t>(from),
                static_cast<uint8_t>(piece),
                static_cast<uint8_t>(square)};

        // Removing the piece uncovers the slider's threat against the occupied
        // square behind it.
        const Bitboard behind = getBishopAttacks(from, occupancy)
                              & ~getBishopAttacks(from, occupancy | squareBit)
                              & occupancy;
        if (behind)
        {
            const int to = GetLsbIndex(behind);
            uncovered[uncoveredCount++] = {
                static_cast<uint8_t>(slider),
                static_cast<uint8_t>(from),
                static_cast<uint8_t>(pos->PieceOn(to)),
                static_cast<uint8_t>(to)};
        }
    }

    // Repeat the same destroyed/uncovered calculation for rooks and queens.
    Bitboard orthogonal = getRookAttacks(square, occupancy)
                        & (bitboards[WR] | bitboards[BR] | queens);
    while (orthogonal)
    {
        const int from = popLsb(orthogonal);
        const int slider = pos->PieceOn(from);

        if (pieceType != KING)
            destroyed[destroyedCount++] = {
                static_cast<uint8_t>(slider),
                static_cast<uint8_t>(from),
                static_cast<uint8_t>(piece),
                static_cast<uint8_t>(square)};

        const Bitboard behind = getRookAttacks(from, occupancy)
                              & ~getRookAttacks(from, occupancy | squareBit)
                              & occupancy;
        if (behind)
        {
            const int to = GetLsbIndex(behind);
            uncovered[uncoveredCount++] = {
                static_cast<uint8_t>(slider),
                static_cast<uint8_t>(from),
                static_cast<uint8_t>(pos->PieceOn(to)),
                static_cast<uint8_t>(to)};
        }
    }
}

void NNUE::updateThreatAccumulators(Position *pos)
{
    const int head = pos->history.head;

    for (int side = WHITE; side <= BLACK; ++side)
    {
        if (pos->threatAccumulators[head].computed[side])
            continue;

        int ply = head;

        // look for the most recent ply where the threat accumulator for this side was either computed or needs a refresh
        while (ply > 0
               && !pos->threatAccumulators[ply].computed[side]
               && !pos->threatAccumulators[ply].needsRefresh[side])
            --ply;

        // if we can't work from a previously computed threat accumulator, refresh it from scratch at the ply that
        // needs it, so every later position in this subtree can update from it instead of refreshing again
        if (!pos->threatAccumulators[ply].computed[side])
        {
            ThreatAccumulator &accumulator = pos->threatAccumulators[ply];
            pos->history.head = ply;
            const ThreatFeatures threats = getThreatFeatures(pos);
            pos->history.head = head;

            if (threats.count[side] == 0)
            {
                accumulator.values[side].fill(0);
            }
            else
            {
                size_t offset = size_t(threats.indices[side][0]) * L1_SIZE;
                for (int j = 0; j < L1_SIZE; ++j)
                    accumulator.values[side][j] = net->ThreatWeights[offset + j];

                for (int i = 1; i < threats.count[side]; ++i)
                {
                    offset = size_t(threats.indices[side][i]) * L1_SIZE;
                    for (int j = 0; j < L1_SIZE; ++j)
                        accumulator.values[side][j] += net->ThreatWeights[offset + j];
                }
            }

            accumulator.computed[side] = true;
        }

        const bool flip = get_file[KingSQ(pos, side)] > 3;
        // apply incremental updates to the threat accumulator for this side
        for (++ply; ply <= head; ++ply)
        {
            const ThreatAccumulator &parent = pos->threatAccumulators[ply - 1];
            ThreatAccumulator &accumulator = pos->threatAccumulators[ply];
            accumulator.values[side] = parent.values[side];

            for (int i = 0; i < accumulator.addedCount; ++i)
            {
                const ThreatDelta &delta = accumulator.added[i];
                const int index = getThreatIndex(delta.attacker, delta.victim, delta.from, delta.to, side, flip);
                if (index >= 0)
                {
                    const size_t offset = size_t(index) * L1_SIZE;
                    for (int j = 0; j < L1_SIZE; ++j)
                        accumulator.values[side][j] += net->ThreatWeights[offset + j];
                }
            }

            for (int i = 0; i < accumulator.removedCount; ++i)
            {
                const ThreatDelta &delta = accumulator.removed[i];
                const int index = getThreatIndex(delta.attacker, delta.victim, delta.from, delta.to, side, flip);
                if (index >= 0)
                {
                    const size_t offset = size_t(index) * L1_SIZE;
                    for (int j = 0; j < L1_SIZE; ++j)
                        accumulator.values[side][j] -= net->ThreatWeights[offset + j];
                }
            }

            accumulator.computed[side] = true;
        }
    }
}

// does FT activate for one pov at a time
void NNUE::povActivateAffine(Position *pos, NNUE::FinnyTable *FinnyPointer, const int side,
                             const PovAccumulator &threatValues, uint16_t *base,
                             uint16_t *nnzIndices, int &nnzCount, uint8_t *output)
{
    const int kingSq = KingSQ(pos, side);
    const bool flip = get_file[kingSq] > 3;
    const int kingBucket = getBucket(kingSq, side);
    FinnyTableEntry &cachedEntry = (*FinnyPointer)[side][kingBucket][flip];

    size_t add[32], remove[32]; // Max add or remove is 32 unless illegal position
    size_t addCnt = 0, removeCnt = 0;

    for (int piece = WP; piece <= BK; piece++)
    {
        Bitboard added = pos->state().bitboards[piece] & ~cachedEntry.occupancies[piece];
        Bitboard removed = cachedEntry.occupancies[piece] & ~pos->state().bitboards[piece];
        while (added)
        {
            int square = popLsb(added);
            add[addCnt++] = getPsqtIndex(piece, square, side, kingBucket, flip);
        }

        while (removed)
        {
            int square = popLsb(removed);
            remove[removeCnt++] = getPsqtIndex(piece, square, side, kingBucket, flip);
        }

        cachedEntry.occupancies[piece] = pos->state().bitboards[piece];
    }

    NNUE::PovAccumulator &accumCache = cachedEntry.accumCache;

#if defined(USE_SIMD)

    // 8 measured to be optimal on both avx512 and avx2
    constexpr int NUM_REGI = 8;
    static_assert(NUM_REGI % 2 == 0 && (L1_SIZE / 2) % (NUM_REGI * FT_CHUNK_SIZE) == 0);

    const vepi16 Zero = vec_zero_epi16();
    const vepi16 One = vec_set1_epi16(FT_QUANT);
    const v128i LookupIncr = vec128_set1_epi16(8);

    const size_t minCnt = std::min(addCnt, removeCnt);

    v128i baseVec = vec128_loadu_epi16(reinterpret_cast<const v128i *>(base));
    for (int b = 0; b < L1_SIZE / 2; b += NUM_REGI * FT_CHUNK_SIZE)
    {

        vepi16 *accPtr0 = reinterpret_cast<vepi16 *>(&accumCache[b]);
        vepi16 *accPtr1 = reinterpret_cast<vepi16 *>(&accumCache[b + L1_SIZE / 2]);

        vepi16 acc0[NUM_REGI], acc1[NUM_REGI];
        for (int j = 0; j < NUM_REGI; ++j)
        {
            acc0[j] = accPtr0[j];
            acc1[j] = accPtr1[j];
        }

        for (size_t i = 0; i < minCnt; ++i)
        {
            const vepi16 *add0 = reinterpret_cast<const vepi16 *>(&net->FTWeights[add[i] + b]);
            const vepi16 *add1 = reinterpret_cast<const vepi16 *>(&net->FTWeights[add[i] + b + L1_SIZE / 2]);
            const vepi16 *rem0 = reinterpret_cast<const vepi16 *>(&net->FTWeights[remove[i] + b]);
            const vepi16 *rem1 = reinterpret_cast<const vepi16 *>(&net->FTWeights[remove[i] + b + L1_SIZE / 2]);

            for (int j = 0; j < NUM_REGI; ++j)
            {
                acc0[j] = vec_add_epi16(acc0[j], vec_sub_epi16(add0[j], rem0[j]));
                acc1[j] = vec_add_epi16(acc1[j], vec_sub_epi16(add1[j], rem1[j]));
            }
        }

        for (size_t i = minCnt; i < addCnt; ++i)
        {
            const vepi16 *wgt0 = reinterpret_cast<const vepi16 *>(&net->FTWeights[add[i] + b]);
            const vepi16 *wgt1 = reinterpret_cast<const vepi16 *>(&net->FTWeights[add[i] + b + L1_SIZE / 2]);

            for (int j = 0; j < NUM_REGI; ++j)
            {
                acc0[j] = vec_add_epi16(acc0[j], wgt0[j]);
                acc1[j] = vec_add_epi16(acc1[j], wgt1[j]);
            }
        }

        for (size_t i = minCnt; i < removeCnt; ++i)
        {
            const vepi16 *wgt0 = reinterpret_cast<const vepi16 *>(&net->FTWeights[remove[i] + b]);
            const vepi16 *wgt1 = reinterpret_cast<const vepi16 *>(&net->FTWeights[remove[i] + b + L1_SIZE / 2]);

            for (int j = 0; j < NUM_REGI; ++j)
            {
                acc0[j] = vec_sub_epi16(acc0[j], wgt0[j]);
                acc1[j] = vec_sub_epi16(acc1[j], wgt1[j]);
            }
        }

        for (int j = 0; j < NUM_REGI; ++j)
        {
            accPtr0[j] = acc0[j];
            accPtr1[j] = acc1[j];
        }

        const vepi16 *threat0 = reinterpret_cast<const vepi16 *>(&threatValues[b]);
        const vepi16 *threat1 = reinterpret_cast<const vepi16 *>(&threatValues[b + L1_SIZE / 2]);
        for (int j = 0; j < NUM_REGI; ++j)
        {
            acc0[j] = vec_add_epi16(acc0[j], threat0[j]);
            acc1[j] = vec_add_epi16(acc1[j], threat1[j]);
        }

        for (int i = 0; i < NUM_REGI; i += 2)
        {
            vepi16 input0a = acc0[i + 0];
            vepi16 input0b = acc0[i + 1];
            vepi16 input1a = acc1[i + 0];
            vepi16 input1b = acc1[i + 1];

            // Comments stolen from SF (since I was the original author of this anyways):
            // What we want to do is multiply inputs in a pairwise manner (after clipping), and then shift right by FT_SHIFT. Instead, we
            // shift left by (16 - FT_SHIFT), and use mulhi, stripping the bottom 16 bits, effectively shifting right by 16, resulting in a net shift
            // of FT_SHIFT bits. We use mulhi because it maintains the sign of the multiplication (unlike mullo), allowing us to make use
            // of packus to clip 2 of the inputs, resulting in a save of 2 "vec_max_epi16" calls.
            const vepi16 clipped0a = vec_min_epi16(vec_max_epi16(input0a, Zero), One);
            const vepi16 clipped0b = vec_min_epi16(vec_max_epi16(input0b, Zero), One);
            const vepi16 clipped1a = vec_min_epi16(input1a, One);
            const vepi16 clipped1b = vec_min_epi16(input1b, One);

            const vepi16 producta = vec_mulhi_epi16(vec_slli_epi16(clipped0a, 16 - FT_SHIFT), clipped1a);
            const vepi16 productb = vec_mulhi_epi16(vec_slli_epi16(clipped0b, 16 - FT_SHIFT), clipped1b);

            const vepi8 product = vec_packus_epi16(producta, productb);
            vec_store_epi(reinterpret_cast<vepi8 *>(&output[b + i * FT_CHUNK_SIZE]), product);
            // store all non zero indices to transform L1 sparsely
            // start ny creating a mask masking all the non 0 elements in our product vector (actually 4 8bit elements
            // creating a 32 bit element at a time, which will be non 0 if at least 1 8 bit element is.
            const uint16_t nnzMask = vec_nnz_mask(product);
            // check number of elements inside the actual register / 8 since we are working on a per bit basis
            for (int lookup = 0; lookup < int(sizeof(vepi32) / sizeof(uint32_t)) / 8; ++lookup)
            {
                // 0-255 mask index for the table
                uint8_t maskSlice = (nnzMask >> (8 * lookup)) & 0xFF;
                // look up from a precaculated table how many bits are set to 1 and what the indexes are
                NNZEntry nnzEntry = nnzTable.table[maskSlice];
                // get ready to store in in nnzIndices by getting the appropriate pointer to it
                v128i *nnzStore = reinterpret_cast<v128i *>(&nnzIndices[nnzCount]);
                // add entry indices to our non-zero indices list
                const v128i indices = vec128_loadu_epi16(reinterpret_cast<const v128i *>(nnzEntry.indices));
                // add base address to indexes and store them
                vec128_storeu_epi16(nnzStore, vec128_add_epi16(baseVec, indices));

                // increment count of total non 0 elements
                nnzCount += nnzEntry.count;
                // update base value for the next cycle iteration
                baseVec = vec128_add_epi16(baseVec, LookupIncr);
            }
        }
    }
    vec128_storeu_epi16(reinterpret_cast<v128i *>(base), baseVec);

#else
    const size_t minCnt = std::min(addCnt, removeCnt);

    for (size_t i = 0; i < minCnt; ++i)
    {
        const auto added = add[i];
        const auto removed = remove[i];
        for (int j = 0; j < L1_SIZE; ++j)
        {
            accumCache[j] += net->FTWeights[added + j] - net->FTWeights[removed + j];
        }
    }

    for (size_t i = minCnt; i < addCnt; ++i)
    {
        const auto added = add[i];
        for (int j = 0; j < L1_SIZE; ++j)
        {
            accumCache[j] += net->FTWeights[added + j];
        }
    }

    for (size_t i = minCnt; i < removeCnt; ++i)
    {
        const auto removed = remove[i];
        for (int j = 0; j < L1_SIZE; ++j)
        {
            accumCache[j] -= net->FTWeights[removed + j];
        }
    }

    for (int i = 0; i < L1_SIZE / 2; ++i)
    {
        const int32_t value0 = accumCache[i] + threatValues[i];
        const int32_t value1 = accumCache[i + L1_SIZE / 2] + threatValues[i + L1_SIZE / 2];

        const int16_t clipped0 = static_cast<int16_t>(std::clamp<int32_t>(value0, 0, FT_QUANT));
        const int16_t clipped1 = static_cast<int16_t>(std::clamp<int32_t>(value1, 0, FT_QUANT));
        output[i] = static_cast<uint8_t>(clipped0 * clipped1 >> FT_SHIFT);
    }
#endif

#ifdef NNZ_PROFILE
    recordNnzProfile(output);
#endif
}

void NNUE::propagateL1(const uint8_t *inputs, [[maybe_unused]] uint16_t *nnzIndices, [[maybe_unused]] int nnzCount, const int8_t *weights, const float *biases, float *output)
{
#if defined(USE_SIMD)
    vepi32 sums[L2_SIZE / L2_CHUNK_SIZE] = {};
    const int32_t *inputs32 = reinterpret_cast<const int32_t *>(inputs);

    // We read in the inputs in chunks of 4 (as dpbusd horizontally sums by 4).
    // Then, each chunk of 4 is multiplied by the L1 weights. (The weights are pre-permuted to allow us to do this)
    // We also unroll by 2 to save a madd every 2 multiplications (in the non VNNI case).
    // Note that we sacrificed some quantisation accuracy to do this, as the additional accuracy had no elo gain.
    int i = 0;
    for (; i < nnzCount - 1; i += 2)
    {
        const uint16_t indexa = nnzIndices[i + 0];
        const uint16_t indexb = nnzIndices[i + 1];
        const vepi32 input32a = vec_set1_epi32(inputs32[indexa]);
        const vepi32 input32b = vec_set1_epi32(inputs32[indexb]);
        const vepi8 *weighta = reinterpret_cast<const vepi8 *>(&weights[indexa * L1_CHUNK_PER_32 * L2_SIZE]);
        const vepi8 *weightb = reinterpret_cast<const vepi8 *>(&weights[indexb * L1_CHUNK_PER_32 * L2_SIZE]);
        for (int j = 0; j < L2_SIZE / L2_CHUNK_SIZE; ++j)
            sums[j] = vec_dpbusdx2_epi32(sums[j], input32a, weighta[j], input32b, weightb[j]);
    }

    for (; i < nnzCount; ++i)
    {
        const uint16_t index = nnzIndices[i];
        const vepi32 input32 = vec_set1_epi32(inputs32[index]);
        const vepi8 *weight = reinterpret_cast<const vepi8 *>(&weights[index * L1_CHUNK_PER_32 * L2_SIZE]);
        for (int j = 0; j < L2_SIZE / L2_CHUNK_SIZE; ++j)
            sums[j] = vec_dpbusd_epi32(sums[j], input32, weight[j]);
    }

    // We divide by the ONE value to proceed into the later layers, which is carried out in floats.
    // A nice trick by ciekce: instead of dividing, and then adding the L1 bias, we multiply by its reciprocal,
    // and then add the bias, which allows us to use FMA.
    for (i = 0; i < L2_SIZE / L2_CHUNK_SIZE; ++i)
    {
        // Convert into floats, and activate L1
        const vps32 biasVec = vec_load_ps(&biases[i * L2_CHUNK_SIZE]);
        const vps32 sumMul = vec_set1_ps(L1_MUL);
        const vps32 sumPs = vec_mul_add_ps(vec_cvtepi32_ps(sums[i]), sumMul, biasVec);

        const vps32 Zero = vec_zero_ps();
        const vps32 One = vec_set1_ps(1.0f);
        // linear
        const vps32 clipped = vec_min_ps(vec_max_ps(sumPs, Zero), One);
        // squared
        const vps32 squared = vec_mul_ps(sumPs, sumPs);
        const vps32 squared_clipped = vec_min_ps(vec_max_ps(squared, Zero), One);
        // it's storing time
        vec_store_ps(&output[i * L2_CHUNK_SIZE], clipped);
        vec_store_ps(&output[L2_SIZE + i * L2_CHUNK_SIZE], squared_clipped);
    }
#else
    int sums[L2_SIZE] = {};
    for (int i = 0; i < L1_SIZE; ++i)
    {
        for (int j = 0; j < L2_SIZE; ++j)
        {
            sums[j] += static_cast<int32_t>(inputs[i] * weights[j * L1_SIZE + i]);
        }
    }

    for (int i = 0; i < L2_SIZE; ++i)
    {
        // Convert into floats and activate L1
        const float z = float(sums[i]) * L1_MUL + biases[i];
        // Dual activation: produce 2 L1 outputs for each input by applying different activations
        const float squared = std::clamp(z * z, 0.0f, 1.0f);
        const float linear = std::clamp(z, 0.0f, 1.0f);
        output[i] = linear;
        output[i + L2_SIZE] = squared;
    }
#endif
}

void NNUE::propagateL2(const float *inputs, const float *weights, const float *biases, float *output)
{
    // For each input, multiply by all the L2 weights
#if defined(USE_SIMD)
    vps32 sumVecs[L3_SIZE / L3_CHUNK_SIZE];

    for (int i = 0; i < L3_SIZE / L3_CHUNK_SIZE; ++i)
        sumVecs[i] = vec_load_ps(&biases[i * L3_CHUNK_SIZE]);

    for (int i = 0; i < EFFECTIVE_L2_SIZE; ++i)
    {
        const vps32 inputVec = vec_set1_ps(inputs[i]);
        const vps32 *weight = reinterpret_cast<const vps32 *>(&weights[i * L3_SIZE]);
        for (int j = 0; j < L3_SIZE / L3_CHUNK_SIZE; ++j)
            sumVecs[j] = vec_mul_add_ps(inputVec, weight[j], sumVecs[j]);
    }

    // Activate L2
    for (int i = 0; i < L3_SIZE / L3_CHUNK_SIZE; ++i)
    {
        const vps32 Zero = vec_zero_ps();
        const vps32 One = vec_set1_ps(1.0f);
        const vps32 clipped = vec_min_ps(vec_max_ps(sumVecs[i], Zero), One);
        const vps32 squared = vec_mul_ps(clipped, clipped);
        vec_store_ps(&output[i * L3_CHUNK_SIZE], squared);
    }
#else
    float sums[L3_SIZE];

    for (int i = 0; i < L3_SIZE; ++i)
        sums[i] = biases[i];

    // Affine transform for L2
    for (int i = 0; i < EFFECTIVE_L2_SIZE; ++i)
    {
        const float *weight = &weights[i * L3_SIZE];
        for (int out = 0; out < L3_SIZE; ++out)
        {
            sums[out] += inputs[i] * weight[out];
        }
    }

    // Activate L2
    for (int i = 0; i < L3_SIZE; ++i)
    {
        const float clipped = std::clamp(sums[i], 0.0f, 1.0f);
        const float squared = clipped * clipped;
        output[i] = squared;
    }
#endif
}

void NNUE::propagateL3(const float *inputs, const float *weights, const float bias, float &output)
{
    constexpr int avx512chunk = 512 / 32;
#if defined(USE_SIMD)
    constexpr int numSums = avx512chunk / (sizeof(vps32) / sizeof(float));
    vps32 sumVecs[numSums] = {};
    // Affine transform for L3
    for (int i = 0; i < L3_SIZE / L3_CHUNK_SIZE; ++i)
    {
        const vps32 weightVec = vec_load_ps(&weights[i * L3_CHUNK_SIZE]);
        const vps32 inputsVec = vec_load_ps(&inputs[i * L3_CHUNK_SIZE]);
        sumVecs[i % numSums] = vec_mul_add_ps(inputsVec, weightVec, sumVecs[i % numSums]);
    }
    output = vec_reduce_add_ps(sumVecs) + bias;
#else
    constexpr int numSums = avx512chunk;
    float sums[numSums] = {};

    // Affine transform for L3
    for (int i = 0; i < L3_SIZE; ++i)
    {
        sums[i % numSums] += inputs[i] * weights[i];
    }
    output = reduce_add(sums, numSums) + bias;
#endif
}

void NNUE::activateAffine(Position *pos, NNUE::FinnyTable *FinnyPointer,
                          const ThreatAccumulator &threatAccumulator,
                          [[maybe_unused]] uint16_t *base, [[maybe_unused]] uint16_t *nnzIndices,
                          [[maybe_unused]] int &nnzCount, uint8_t *output)
{
    povActivateAffine(pos, FinnyPointer, pos->side, threatAccumulator.values[pos->side],
                      base, nnzIndices, nnzCount, output);
    povActivateAffine(pos, FinnyPointer, pos->side ^ 1, threatAccumulator.values[pos->side ^ 1],
                      base, nnzIndices, nnzCount, &output[L1_SIZE / 2]);
}

int NNUE::output(Position *pos, NNUE::FinnyTable *FinnyPointer)
{
    int nnzCount = 0;
    uint16_t base[8] = {}; // replaces v128i base
    alignas(64) uint16_t nnzIndices[L1_SIZE / L1_CHUNK_PER_32];

    const int pieceCount = pos->PieceCount();
    const int outputBucket = std::min((63 - pieceCount) * (32 - pieceCount) / 225, 7);
    alignas(64) uint8_t FTOutputs[L1_SIZE];
    alignas(64) float L1Outputs[EFFECTIVE_L2_SIZE];
    alignas(64) float L2Outputs[L3_SIZE];
    float L3Output;

    updateThreatAccumulators(pos);

    // does FT activation for both accumulators
    activateAffine(pos, FinnyPointer, pos->threatAccumulator(), base, nnzIndices, nnzCount, FTOutputs);

    propagateL1(FTOutputs, nnzIndices, nnzCount, net->L1Weights[outputBucket], net->L1Biases[outputBucket], L1Outputs);

    propagateL2(L1Outputs, net->L2Weights[outputBucket], net->L2Biases[outputBucket], L2Outputs);

    propagateL3(L2Outputs, net->L3Weights[outputBucket], net->L3Biases[outputBucket], L3Output);

    return L3Output * NET_SCALE;
}

size_t NNUE::getPsqtIndex(const int piece, const int square, const int side, const int bucket, const bool flip)
{
    constexpr std::size_t COLOR_STRIDE = 64 * 6;
    constexpr std::size_t PIECE_STRIDE = 64;
    const int piecetype = GetPieceType(piece);
    const int pieceColor = Color[piece] * (!MERGE_KING_PLANES || piecetype == KING);
    const int pieceColorPov = pieceColor ^ side;

    // Get the final indexes of the updates, accounting for hm
    auto squarePov = square ^ (0b111'000 * !side) ^ (0b000'111 * flip);
    auto idx = bucket * NUM_INPUTS + pieceColorPov * COLOR_STRIDE + piecetype * PIECE_STRIDE + squarePov;
    return idx * L1_SIZE;
}

int NNUE::getThreatIndex(int attacker, int victim, Square from,
                         Square to, int perspective, bool flip)
{
    const int squareXor = (perspective == WHITE ? 56 : 0) ^ (flip ? 7 : 0);
    const int source = from ^ squareXor;
    const int destination = to ^ squareXor;
    const int base = threatIndexBases[perspective][attacker][victim][destination > source];
    if (base < 0)
        return -1;

    const uint16_t pairIndex = threatPairIndices[PieceType[attacker]][source][destination];
    return pairIndex == 0xFFFF ? -1 : base + pairIndex;
}
