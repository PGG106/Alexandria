#include "hashkey.h"
#include <cassert>
#include "init.h"
#include "position.h"

// Generates zobrist key from scratch
Bitboard GeneratePosKey(const Position* pos) {
    Bitboard finalkey = 0;
    // for every square
    for (int sq = 0; sq < 64; ++sq) {
        // get piece on that square
        int piece = pos->pieces[sq];
        // if it's not empty add that piece to the zobrist key
        if (piece != EMPTY) {
            assert(piece >= WP && piece <= BK);
            finalkey ^= PieceKeys[piece][sq];
        }
    }
    // include side in the key
    if (pos->side == BLACK) {
        finalkey ^= SideKey;
    }
    // include the ep square in the key
    if (GetEpSquare(pos) != no_sq) {
        assert(pos->enPas >= 0 && pos->enPas < 64);
        finalkey ^= enpassant_keys[GetEpSquare(pos)];
    }
    assert(pos->castleperm >= 0 && pos->castleperm <= 15);
    // add to the key the status of the castling permissions
    finalkey ^= CastleKeys[pos->GetCastlingPerm()];
    return finalkey;
}
