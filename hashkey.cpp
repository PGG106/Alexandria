
#include "init.h"
#include <cassert>





Bitboard GeneratePosKey(const S_Board* pos) {
	int sq = 0;
	Bitboard finalkey = 0;
	int piece = WP;
	for (sq = 0;sq < 64;++sq) {
		piece = pos->pieces[sq];
		if (piece != EMPTY) {
			assert(piece >= WP && piece <= BK);
			finalkey ^= PieceKeys[piece][sq];
		}
	}// for every square, if the square is valid and there's a piece on it, check if the piece is valid, use the piece as part of the finalkey
	if (pos->side == BLACK) {
		finalkey ^= SideKey;

	} //if the player is white add a random value
	if (pos->enPas != no_sq) {
		assert(pos->enPas >= 0 && pos->enPas < 64);
		finalkey ^= enpassant_keys[pos->enPas];
	} //add to the key if enpassant was played
	assert(pos->castleperm >= 0 && pos->castleperm <= 15);
	finalkey ^= CastleKeys[pos->castleperm]; // add to the key the status of the castling permissions
	return finalkey;
}