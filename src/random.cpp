#include "random.h"

// pseudo random number state
unsigned int random_state = 1804289383;

// generate 32-bit pseudo legal numbers
unsigned int GetRandomU32Number() {
    // get current state
    unsigned int number = random_state;

    // XOR shift algorithm
    number ^= number << 13;
    number ^= number >> 17;
    number ^= number << 5;

    // update random number state
    random_state = number;

    // return random number
    return number;
}

// generate 64-bit pseudo legal numbers
Bitboard GetRandomBitboardNumber() {
    // define 4 random numbers
    Bitboard n1, n2, n3, n4;

    // init random numbers slicing 16 bits from MS1B side
    n1 = static_cast<Bitboard>((GetRandomU32Number())) & 0xFFFF;
    n2 = static_cast<Bitboard>((GetRandomU32Number())) & 0xFFFF;
    n3 = static_cast<Bitboard>((GetRandomU32Number())) & 0xFFFF;
    n4 = static_cast<Bitboard>((GetRandomU32Number())) & 0xFFFF;

    // return random number
    return n1 | (n2 << 16) | (n3 << 32) | (n4 << 48);
}

// generate magic number candidate
Bitboard GenerateMagicNumber() {
    return GetRandomBitboardNumber() & GetRandomBitboardNumber() &
        GetRandomBitboardNumber();
}
