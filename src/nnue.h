#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <cassert>
#include "simd.h"
#include "types.h"

// Net arch: (768xINPUT_BUCKETS -> L1_SIZE)x2 -> 1xOUTPUT_BUCKETS
constexpr int NUM_INPUTS = 768;
constexpr int INPUT_BUCKETS = 4;
constexpr int L1_SIZE = 1536;
constexpr int OUTPUT_BUCKETS = 8;

constexpr int FT_QUANT = 362;
constexpr int L1_QUANT = 64;
constexpr int NET_SCALE = 400;

constexpr int buckets[64] = {
        0, 0, 1, 1, 1, 1, 0, 0,
        2, 2, 2, 2, 2, 2, 2, 2,
        3, 3, 3, 3, 3, 3, 3, 3,
        3, 3, 3, 3, 3, 3, 3, 3,
        3, 3, 3, 3, 3, 3, 3, 3,
        3, 3, 3, 3, 3, 3, 3, 3,
        3, 3, 3, 3, 3, 3, 3, 3,
        3, 3, 3, 3, 3, 3, 3, 3
};

[[nodiscard]] inline int getBucket(int kingSquare){
   return buckets[kingSquare];
}

#if defined(USE_SIMD)
constexpr int CHUNK_SIZE = sizeof(vepi16) / sizeof(int16_t);
#else
constexpr int CHUNK_SIZE = 1;
#endif

using NNUEIndices = std::array<std::size_t, 2>;

struct Network {
    int16_t FTWeights[INPUT_BUCKETS * NUM_INPUTS * L1_SIZE];
    int16_t FTBiases[L1_SIZE];
    int16_t L1Weights[L1_SIZE * 2 * OUTPUT_BUCKETS];
    int16_t L1Biases[OUTPUT_BUCKETS];
};

extern Network net;
struct Position;

class NNUE {
public:
    // per pov accumulator
    struct Pov_Accumulator{
            std::array<int16_t, L1_SIZE> values;
            int pov;
            std::vector<std::size_t> NNUEAdd = {};
            std::vector<std::size_t> NNUESub = {};
            bool needsRefresh = false;

            void accumulate(Position *pos);
            [[nodiscard]] int GetIndex(const int piece, const int square, const int kingSq , bool flip) const;
            void addSub(NNUE::Pov_Accumulator &prev_acc, std::size_t add, std::size_t sub);
            void addSubSub(NNUE::Pov_Accumulator &prev_acc, std::size_t add, std::size_t sub1, std::size_t sub2);
            void applyUpdate(Pov_Accumulator& previousPovAccumulator);
            void refresh(Position *pos);

            [[nodiscard]] bool isClean() const {
                return NNUEAdd.empty();
            }
    };
// final total accumulator that holds the 2 povs
    struct Accumulator {

        Accumulator(){
            this->perspective[WHITE].pov = WHITE;
            this->perspective[BLACK].pov = BLACK;
        }

        std::array<Pov_Accumulator, 2> perspective;

        void AppendAddIndex(int piece, int square, const int wkSq, const int bkSq, std::array<bool, 2> flip) {
            assert(this->perspective[WHITE].NNUEAdd.size() <= 1);
            assert(this->perspective[BLACK].NNUEAdd.size() <= 1);
            this->perspective[WHITE].NNUEAdd.emplace_back(perspective[WHITE].GetIndex(piece, square,wkSq, flip[WHITE]));
            this->perspective[BLACK].NNUEAdd.emplace_back(perspective[BLACK].GetIndex(piece, square, bkSq, flip[BLACK]));
        }

        void AppendSubIndex(int piece, int square, const int wkSq, const int bkSq, std::array<bool, 2> flip) {
            assert(this->perspective[WHITE].NNUESub.size() <= 1);
            assert(this->perspective[BLACK].NNUESub.size() <= 1);
            this->perspective[WHITE].NNUESub.emplace_back(perspective[WHITE].GetIndex(piece, square, wkSq, flip[WHITE]));
            this->perspective[BLACK].NNUESub.emplace_back(perspective[BLACK].GetIndex(piece, square, bkSq, flip[BLACK]));
        }

        void ClearAddIndex() {
            this->perspective[WHITE].NNUEAdd.clear();
            this->perspective[BLACK].NNUEAdd.clear();
        }

        void ClearSubIndex() {
            this->perspective[WHITE].NNUESub.clear();
            this->perspective[BLACK].NNUESub.clear();
        }
    };

    static void init(const char *file);
    static void accumulate(NNUE::Accumulator &board_accumulator, Position* pos);
    static void update(Accumulator *acc, Position* pos);
    [[nodiscard]] static int32_t ActivateFTAndAffineL1(const int16_t *us, const int16_t *them, const int16_t *weights, const int16_t bias);
    [[nodiscard]] static int32_t output(const NNUE::Accumulator &board_accumulator, const int stm, const int outputBucket);
};
