#include "bitboard.h"
#include "makemove.h"
#include "ttable.h"
#include "position.h"
#include "init.h"
#include "attack.h"

void inline HashKey(ZobristKey& originalKey , ZobristKey key) {
    originalKey ^= key;
}

// Remove a piece from a square
void ClearPiece(const int piece, const int from, Position* pos) {
    assert(piece != EMPTY);
    const int color = Color[piece];
    pop_bit(pos->bitboards[piece], from);
    pop_bit(pos->occupancies[color], from);
    pos->pieces[from] = EMPTY;
    HashKey(pos->posKey, PieceKeys[piece][from]);
    if(GetPieceType(piece) == PAWN)
        HashKey(pos->pawnKey, PieceKeys[piece][from]);
    else if(Color[piece] == WHITE)
        HashKey(pos->whiteNonPawnKey, PieceKeys[piece][from]);
    else
        HashKey(pos->blackNonPawnKey, PieceKeys[piece][from]);
}

void AddPiece(const int piece, const int to, Position* pos) {
    assert(piece != EMPTY);
    const int color = Color[piece];
    set_bit(pos->bitboards[piece], to);
    set_bit(pos->occupancies[color], to);
    pos->pieces[to] = piece;
    HashKey(pos->posKey, PieceKeys[piece][to]);
    if(GetPieceType(piece) == PAWN)
        HashKey(pos->pawnKey, PieceKeys[piece][to]);
    else if(Color[piece] == WHITE)
        HashKey(pos->whiteNonPawnKey, PieceKeys[piece][to]);
    else
        HashKey(pos->blackNonPawnKey, PieceKeys[piece][to]);
}

void MovePiece(const int piece, const int from, const int to, Position* pos) {
    ClearPiece(piece, from, pos);
    AddPiece(piece, to, pos);
}

void UpdateCastlingPerms(Position* pos, int source_square, int target_square) {
    // Xor the old castling key from the zobrist key
    HashKey(pos->posKey, CastleKeys[pos->getCastlingPerm()]);
    // update castling rights
    pos->state.castlePerm &= castling_rights[source_square];
    pos->state.castlePerm &= castling_rights[target_square];
    // Xor the new one
    HashKey(pos->posKey, CastleKeys[pos->getCastlingPerm()]);
}

inline void resetEpSquare(Position* pos) {
    if (pos->getEpSquare() != no_sq) {
        HashKey(pos->posKey, enpassant_keys[pos->getEpSquare()]);
        pos->state.enPas = no_sq;
    }
}

void MakeCastle(const Move move, Position* pos) {
    // parse move
    const int sourceSquare = From(move);
    const int targetSquare = To(move);
    const int piece = Piece(move);
    // Remove the piece fom the square it moved from
    ClearPiece(piece, sourceSquare, pos);
    // Set the piece to the destination square, if it was a promotion we directly set the promoted piece
    AddPiece(piece, targetSquare, pos);
    resetEpSquare(pos);

    // move the rook
    switch (targetSquare) {
        // white castles king side
        case (g1):
            // move H rook
            MovePiece(WR, h1, f1, pos);
            break;

            // white castles queen side
        case (c1):
            // move A rook
            MovePiece(WR, a1, d1, pos);
            break;

            // black castles king side
        case (g8):
            // move H rook
            MovePiece(BR, h8, f8, pos);
            break;

            // black castles queen side
        case (c8):
            // move A rook
            MovePiece(BR, a8, d8, pos);
            break;
    }
    UpdateCastlingPerms(pos, sourceSquare, targetSquare);
}

void MakeEp(const Move move, Position* pos) {
    pos->state.fiftyMove = 0;

    // parse move
    const int sourceSquare = From(move);
    const int targetSquare = To(move);
    const int piece = Piece(move);
    const int SOUTH = pos->side == WHITE ? 8 : -8;

    const int pieceCap = GetPiece(PAWN, pos->side ^ 1);
    pos->history[pos->historyStackHead].capture = pieceCap;
    const int capturedPieceLocation = targetSquare + SOUTH;
    ClearPiece(pieceCap, capturedPieceLocation, pos);

    // Remove the piece fom the square it moved from
    ClearPiece(piece, sourceSquare, pos);
    // Set the piece to the destination square
    AddPiece(piece, targetSquare, pos);

    // Reset EP square
    assert(pos->getEpSquare() != no_sq);
    HashKey(pos->posKey, enpassant_keys[pos->getEpSquare()]);
    pos->state.enPas = no_sq;
}

void MakePromo(const Move move, Position* pos) {
    pos->state.fiftyMove = 0;

    // parse move
    const int sourceSquare = From(move);
    const int targetSquare = To(move);
    const int piece = Piece(move);
    const int promotedPiece = GetPiece(getPromotedPiecetype(move), pos->side);

    // Remove the piece fom the square it moved from
    ClearPiece(piece, sourceSquare, pos);
    // Set the piece to the destination square, if it was a promotion we directly set the promoted piece
    AddPiece(promotedPiece , targetSquare, pos);

    resetEpSquare(pos);

    UpdateCastlingPerms(pos, sourceSquare, targetSquare);
}

void MakePromocapture(const Move move, Position* pos) {
    pos->state.fiftyMove = 0;

    // parse move
    const int sourceSquare = From(move);
    const int targetSquare = To(move);
    const int piece = Piece(move);
    const int promotedPiece = GetPiece(getPromotedPiecetype(move), pos->side);

    const int pieceCap = pos->PieceOn(targetSquare);
    assert(pieceCap != EMPTY);
    assert(GetPieceType(pieceCap) != KING);
    ClearPiece(pieceCap, targetSquare, pos);

    pos->history[pos->historyStackHead].capture = pieceCap;

    // Remove the piece fom the square it moved from
    ClearPiece(piece, sourceSquare, pos);
    // Set the piece to the destination square, if it was a promotion we directly set the promoted piece
    AddPiece(promotedPiece , targetSquare, pos);

    resetEpSquare(pos);

    UpdateCastlingPerms(pos, sourceSquare, targetSquare);
}

void MakeQuiet(const Move move, Position* pos) {
    // parse move
    const int sourceSquare = From(move);
    const int targetSquare = To(move);
    const int piece = Piece(move);

    // if a pawn was moved or a capture was played reset the 50 move rule counter
    if (GetPieceType(piece) == PAWN)
        pos->state.fiftyMove = 0;

    MovePiece(piece,sourceSquare,targetSquare,pos);

    resetEpSquare(pos);

    UpdateCastlingPerms(pos, sourceSquare, targetSquare);
}

void MakeCapture(const Move move, Position* pos) {
    // parse move
    const int sourceSquare = From(move);
    const int targetSquare = To(move);
    const int piece = Piece(move);

    pos->state.fiftyMove = 0;

    const int pieceCap = pos->PieceOn(targetSquare);
    assert(pieceCap != EMPTY);
    assert(GetPieceType(pieceCap) != KING);
    ClearPiece(pieceCap, targetSquare, pos);
    pos->history[pos->historyStackHead].capture = pieceCap;

    MovePiece(piece, sourceSquare, targetSquare, pos);

    resetEpSquare(pos);

    UpdateCastlingPerms(pos, sourceSquare, targetSquare);
}

void MakeDP(const Move move, Position* pos)
{   pos->state.fiftyMove = 0;

    // parse move
    const int sourceSquare = From(move);
    const int targetSquare = To(move);
    const int piece = Piece(move);

    MovePiece(piece,sourceSquare,targetSquare, pos);

    resetEpSquare(pos);

    // Add new ep square
    const int SOUTH = pos->side == WHITE ? 8 : -8;
    int epSquareCandidate = targetSquare + SOUTH;
    if(!(pawn_attacks[pos->side][epSquareCandidate] & pos->GetPieceColorBB(PAWN, pos->side ^ 1)))
        epSquareCandidate = no_sq;
    pos->state.enPas = epSquareCandidate;
    if(pos->getEpSquare() != no_sq)
    HashKey(pos->posKey, enpassant_keys[pos->getEpSquare()]);
}

template void MakeMove<true>(const Move move, Position* pos);
template void MakeMove<false>(const Move move, Position* pos);

bool shouldFlip(int from, int to) {
    const bool prevFlipped = get_file[from] > 3;
    const bool flipped = get_file[to] > 3;

    if (prevFlipped != flipped)
        return true;

    return false;
}

// make move on chess board
template <bool UPDATE>
void MakeMove(const Move move, Position* pos) {

    if constexpr (UPDATE) {
        saveBoardState(pos);
    }

    // Store position key in the array of searched position
    pos->played_positions.emplace_back(pos->posKey);

    // parse move flag
    const bool capture = isCapture(move);
    const bool doublePush = isDP(move);
    const bool enpass = isEnpassant(move);
    const bool castling = isCastle(move);
    const bool promotion = isPromo(move);
    // increment fifty move rule counter
    pos->state.fiftyMove++;
    pos->state.plyFromNull++;
    pos->hisPly++;

    if(castling){
        MakeCastle(move,pos);
    }
    else if(doublePush){
        MakeDP(move,pos);
    }
    else if(enpass){
        MakeEp(move,pos);
    }
    else if(promotion && capture){
        MakePromocapture(move,pos);
    }
    else if(promotion){
        MakePromo(move,pos);
    }
    else if(!capture){
        MakeQuiet(move,pos);
    }
    else {
        MakeCapture(move, pos);
    }

    if constexpr (UPDATE)
        pos->historyStackHead++;

    // change side
    pos->ChangeSide();
    // Xor the new side into the key
    HashKey(pos->posKey, SideKey);
    // Update pinmasks and checkers
    UpdatePinsAndCheckers(pos, pos->side);

    // Make sure a freshly generated zobrist key matches the one we are incrementally updating
    assert(pos->posKey == GeneratePosKey(pos));
    assert(pos->pawnKey == GeneratePawnKey(pos));
}

void UnmakeMove(const Move move, Position* pos) {
    // quiet moves
    pos->hisPly--;
    pos->historyStackHead--;

    restorePreviousBoardState(pos);

    // parse move
    const int sourceSquare = From(move);
    const int targetSquare = To(move);
    // parse move flag
    const bool capture = isCapture(move);
    const bool enpass = isEnpassant(move);
    const bool castling = isCastle(move);
    const bool promotion = isPromo(move);

    const int piece = promotion ? GetPiece(getPromotedPiecetype(move), pos->side ^ 1) : Piece(move);

    // clear the piece on the target square
    ClearPiece(piece, targetSquare, pos);
    // no matter what add back the piece that was originally moved, ignoring any promotion
    AddPiece(Piece(move), sourceSquare, pos);

    // handle captures
    if (capture) {
        const int SOUTH = pos->side == WHITE ? -8 : 8;
        // Retrieve the captured piece we have to restore
        const int piececap = pos->getCapturedPiece();
        const int capturedPieceLocation = enpass ? targetSquare + SOUTH : targetSquare;
        AddPiece(piececap, capturedPieceLocation, pos);
    }

    // handle castling moves
    if (castling) {
        // switch target square
        switch (targetSquare) {
            // white castles king side
        case (g1):
            // move H rook
            MovePiece(WR, f1, h1, pos);
            break;

            // white castles queen side
        case (c1):
            // move A rook
            MovePiece(WR, d1, a1, pos);
            break;

            // black castles king side
        case (g8):
            // move H rook
            MovePiece(BR, f8, h8, pos);
            break;

            // black castles queen side
        case (c8):
            // move A rook
            MovePiece(BR, d8, a8, pos);
            break;
        }
    }

    // change side
    pos->ChangeSide();

    // restore zobrist key (done at the end to avoid overwriting the value while moving pieces back to their place)
    // we don't need to do the same for the pawn key because the unmake function correctly restores it already
    pos->posKey = pos->played_positions.back();
    pos->played_positions.pop_back();
}

// MakeNullMove handles the playing of a null move (a move that doesn't move any piece)
void MakeNullMove(Position* pos) {
    saveBoardState(pos);
    // Store position key in the array of searched position
    pos->played_positions.emplace_back(pos->posKey);
    // Update the zobrist key asap so we can prefetch
    resetEpSquare(pos);
    pos->ChangeSide();
    HashKey(pos->posKey, SideKey);
    TTPrefetch(pos->getPoskey());

    pos->hisPly++;
    pos->historyStackHead++;
    pos->state.fiftyMove++;
    pos->state.plyFromNull = 0;

    // Update pinmasks and checkers
    UpdatePinsAndCheckers(pos, pos->side);
}

// Take back a null move
void TakeNullMove(Position* pos) {
    pos->hisPly--;
    pos->historyStackHead--;

    restorePreviousBoardState(pos);

    pos->ChangeSide();
    pos->posKey = pos->played_positions.back();
    pos->played_positions.pop_back();
}
