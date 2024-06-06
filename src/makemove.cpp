#include "bitboard.h"
#include "makemove.h"
#include "ttable.h"
#include "position.h"
#include "init.h"

template void ClearPiece<false>(const int piece, const int to, Position* pos);
template void ClearPiece<true>(const int piece, const int to, Position* pos);

// Remove a piece from a square, the UPDATE params determines whether we want to update the NNUE weights or not
template <bool UPDATE = true>
void ClearPiece(const int piece, const int from, Position* pos) {
    if constexpr(UPDATE)
        pos->AccumulatorTop().AppendSubIndex(nnue.GetIndex(piece, from));
    const int color = Color[piece];
    pop_bit(pos->bitboards[piece], from);
    pop_bit(pos->occupancies[color], from);
    pos->pieces[from] = EMPTY;
    HashKey(pos, PieceKeys[piece][from]);
}

template void AddPiece<false>(const int piece, const int to, Position* pos);
template void AddPiece<true>(const int piece, const int to, Position* pos);

// Add a piece to a square, the UPDATE params determines whether we want to update the NNUE weights or not
template <bool UPDATE = true>
void AddPiece(const int piece, const int to, Position* pos) {
    if constexpr(UPDATE)
        pos->AccumulatorTop().AppendAddIndex(nnue.GetIndex(piece, to));
    const int color = Color[piece];
    set_bit(pos->bitboards[piece], to);
    set_bit(pos->occupancies[color], to);
    pos->pieces[to] = piece;
    HashKey(pos, PieceKeys[piece][to]);
}

// Move a piece from the [to] square to the [from] square, the UPDATE params determines whether we want to update the NNUE weights or not
template <bool UPDATE = true>
void MovePiece(const int piece, const int from, const int to, Position* pos) {
    ClearPiece<UPDATE>(piece, from, pos);
    AddPiece<UPDATE>(piece, to, pos);
}

void UpdateCastlingPerms(Position* pos, int source_square, int target_square) {
    // Xor the old castling key from the zobrist key
    HashKey(pos, CastleKeys[pos->GetCastlingPerm()]);
    // update castling rights
    pos->castleperm &= castling_rights[source_square];
    pos->castleperm &= castling_rights[target_square];
    // Xor the new one
    HashKey(pos, CastleKeys[pos->GetCastlingPerm()]);
}

void inline HashKey(Position* pos, ZobristKey key) {
    pos->posKey ^= key;
}

template <bool UPDATE>
void MakeCastle(const Move move, Position* pos) {
    // parse move
    const int sourceSquare = From(move);
    const int targetSquare = To(move);
    const int piece = Piece(move);
    // Remove the piece fom the square it moved from
    ClearPiece<UPDATE>(piece, sourceSquare, pos);
    // Set the piece to the destination square, if it was a promotion we directly set the promoted piece
    AddPiece<UPDATE>(piece, targetSquare, pos);

    // Reset EP square
    if (GetEpSquare(pos) != no_sq){
        HashKey(pos, enpassant_keys[GetEpSquare(pos)]);
        pos->enPas = no_sq;
    }

    // move the rook
    switch (targetSquare) {
        // white castles king side
        case (g1):
            // move H rook
            MovePiece<UPDATE>(WR, h1, f1, pos);
            break;

            // white castles queen side
        case (c1):
            // move A rook
            MovePiece<UPDATE>(WR, a1, d1, pos);
            break;

            // black castles king side
        case (g8):
            // move H rook
            MovePiece<UPDATE>(BR, h8, f8, pos);
            break;

            // black castles queen side
        case (c8):
            // move A rook
            MovePiece<UPDATE>(BR, a8, d8, pos);
            break;
    }
    UpdateCastlingPerms(pos, sourceSquare, targetSquare);
}

template <bool UPDATE>
void MakeEp(const Move move, Position* pos) {
    pos->fiftyMove = 0;

    // parse move
    const int sourceSquare = From(move);
    const int targetSquare = To(move);
    const int piece = Piece(move);
    const int SOUTH = pos->side == WHITE ? 8 : -8;

    const int pieceCap = GetPiece(PAWN, pos->side ^ 1);
    pos->history[pos->historyStackHead].capture = pieceCap;
    const int capturedPieceLocation = targetSquare + SOUTH;
    ClearPiece<UPDATE>(pieceCap, capturedPieceLocation, pos);

    // Remove the piece fom the square it moved from
    ClearPiece<UPDATE>(piece, sourceSquare, pos);
    // Set the piece to the destination square
    AddPiece<UPDATE>(piece, targetSquare, pos);

    // Reset EP square
    assert(GetEpSquare(pos) != no_sq);
    HashKey(pos, enpassant_keys[GetEpSquare(pos)]);
    pos->enPas = no_sq;
}

template <bool UPDATE>
void MakePromo(const Move move, Position* pos) {
    pos->fiftyMove = 0;

    // parse move
    const int sourceSquare = From(move);
    const int targetSquare = To(move);
    const int piece = Piece(move);
    const int promotedPiece = GetPiece(getPromotedPiecetype(move), pos->side);

    // Remove the piece fom the square it moved from
    ClearPiece<UPDATE>(piece, sourceSquare, pos);
    // Set the piece to the destination square, if it was a promotion we directly set the promoted piece
    AddPiece<UPDATE>(promotedPiece , targetSquare, pos);

    // Reset EP square
    if (GetEpSquare(pos) != no_sq){
        HashKey(pos, enpassant_keys[GetEpSquare(pos)]);
        // reset enpassant square
        pos->enPas = no_sq;
    }

    UpdateCastlingPerms(pos, sourceSquare, targetSquare);
}

template <bool UPDATE>
void MakePromocapture(const Move move, Position* pos) {
    pos->fiftyMove = 0;

    // parse move
    const int sourceSquare = From(move);
    const int targetSquare = To(move);
    const int piece = Piece(move);
    const int promotedPiece = GetPiece(getPromotedPiecetype(move), pos->side);

    const int pieceCap = pos->PieceOn(targetSquare);
    assert(pieceCap != EMPTY);
    assert(GetPieceType(pieceCap) != KING);
    ClearPiece<UPDATE>(pieceCap, targetSquare, pos);

    pos->history[pos->historyStackHead].capture = pieceCap;

    // Remove the piece fom the square it moved from
    ClearPiece<UPDATE>(piece, sourceSquare, pos);
    // Set the piece to the destination square, if it was a promotion we directly set the promoted piece
    AddPiece<UPDATE>(promotedPiece , targetSquare, pos);

    // Reset EP square
    if (GetEpSquare(pos) != no_sq){
        HashKey(pos, enpassant_keys[GetEpSquare(pos)]);
        // reset enpassant square
        pos->enPas = no_sq;
    }

    UpdateCastlingPerms(pos, sourceSquare, targetSquare);
}

template <bool UPDATE>
void MakeQuiet(const Move move, Position* pos) {
    // parse move
    const int sourceSquare = From(move);
    const int targetSquare = To(move);
    const int piece = Piece(move);

    // if a pawn was moved or a capture was played reset the 50 move rule counter
    if (GetPieceType(piece) == PAWN)
        pos->fiftyMove = 0;

    MovePiece<UPDATE>(piece,sourceSquare,targetSquare,pos);

    // Reset EP square
    if (GetEpSquare(pos) != no_sq){
        HashKey(pos, enpassant_keys[GetEpSquare(pos)]);
        // reset enpassant square
        pos->enPas = no_sq;
    }

    UpdateCastlingPerms(pos, sourceSquare, targetSquare);
}

template <bool UPDATE>
void MakeCapture(const Move move, Position* pos) {
    // parse move
    const int sourceSquare = From(move);
    const int targetSquare = To(move);
    const int piece = Piece(move);

    pos->fiftyMove = 0;

    const int pieceCap = pos->PieceOn(targetSquare);
    assert(pieceCap != EMPTY);
    assert(GetPieceType(pieceCap) != KING);
    ClearPiece<UPDATE>(pieceCap, targetSquare, pos);
    pos->history[pos->historyStackHead].capture = pieceCap;

    MovePiece<UPDATE>(piece, sourceSquare, targetSquare, pos);

    // Reset EP square
    if (GetEpSquare(pos) != no_sq){
        HashKey(pos, enpassant_keys[GetEpSquare(pos)]);
        // reset enpassant square
        pos->enPas = no_sq;
    }

    UpdateCastlingPerms(pos, sourceSquare, targetSquare);
}

template <bool UPDATE>
void MakeDP(const Move move, Position* pos)
{   pos->fiftyMove = 0;

    // parse move
    const int sourceSquare = From(move);
    const int targetSquare = To(move);
    const int piece = Piece(move);

    // Remove the piece fom the square it moved from
    ClearPiece<UPDATE>(piece, sourceSquare, pos);
    // Set the piece to the destination square, if it was a promotion we directly set the promoted piece
    AddPiece<UPDATE>(piece, targetSquare, pos);
    // Reset EP square
    if (GetEpSquare(pos) != no_sq) {
        HashKey(pos, enpassant_keys[GetEpSquare(pos)]);
        // reset enpassant square
        pos->enPas = no_sq;
    }
    // Add new ep square
    const int SOUTH = pos->side == WHITE ? 8 : -8;
    pos->enPas = targetSquare + SOUTH;
    HashKey(pos, enpassant_keys[GetEpSquare(pos)]);
}

template void MakeMove<true>(const Move move, Position* pos);
template void MakeMove<false>(const Move move, Position* pos);

// make move on chess board
template <bool UPDATE>
void MakeMove(const Move move, Position* pos) {
    if constexpr (UPDATE){
        saveBoardState(pos);
        pos->accumStackHead++;
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
    pos->fiftyMove++;
    pos->plyFromNull++;
    pos->hisPly++;

    if(castling){
        MakeCastle<UPDATE>(move,pos);
    }
    else if(doublePush){
        MakeDP<UPDATE>(move,pos);
    }
    else if(enpass){
        MakeEp<UPDATE>(move,pos);
    }
    else if(promotion && capture){
        MakePromocapture<UPDATE>(move,pos);
    }
    else if(promotion){
        MakePromo<UPDATE>(move,pos);
    }
    else if(!capture){
        MakeQuiet<UPDATE>(move,pos);
    }
    else {
        MakeCapture<UPDATE>(move, pos);
    }

    if constexpr (UPDATE)
        pos->historyStackHead++;

    // change side
    pos->ChangeSide();
    // Xor the new side into the key
    HashKey(pos, SideKey);
    // Update pinmasks and checkers
    UpdatePinsAndCheckers(pos, pos->side);
    // If we are in check get the squares between the checking piece and the king
    if (pos->checkers) {
        const int kingSquare = KingSQ(pos, pos->side);
        const int pieceLocation = GetLsbIndex(pos->checkers);
        pos->checkMask = (1ULL << pieceLocation) | RayBetween(pieceLocation, kingSquare);
    }
    else
        pos->checkMask = fullCheckmask;
    // Make sure a freshly generated zobrist key matches the one we are incrementally updating
    assert(pos->posKey == GeneratePosKey(pos));
}

void UnmakeMove(const Move move, Position* pos) {
    // quiet moves

    pos->hisPly--;
    pos->historyStackHead--;

    restorePreviousBoardState(pos);

    pos->AccumulatorTop().NNUEAdd.clear();
    pos->AccumulatorTop().NNUESub.clear();

    // parse move
    const int sourceSquare = From(move);
    const int targetSquare = To(move);
    // parse move flag
    const bool capture = isCapture(move);
    const bool enpass = isEnpassant(move);
    const bool castling = isCastle(move);
    const bool promotion = isPromo(move);

    pos->accumStackHead--;

    const int piece = promotion ? GetPiece(getPromotedPiecetype(move), pos->side ^ 1) : Piece(move);

    // clear the piece on the target square
    ClearPiece<false>(piece, targetSquare, pos);
    // no matter what add back the piece that was originally moved, ignoring any promotion
    AddPiece<false>(Piece(move), sourceSquare, pos);

    // handle captures
    if (capture) {
        const int SOUTH = pos->side == WHITE ? -8 : 8;
        // Retrieve the captured piece we have to restore
        const int piececap = pos->history[pos->historyStackHead].capture;
        const int capturedPieceLocation = enpass ? targetSquare + SOUTH : targetSquare;
        AddPiece<false>(piececap, capturedPieceLocation, pos);
    }

    // handle castling moves
    if (castling) {
        // switch target square
        switch (targetSquare) {
            // white castles king side
        case (g1):
            // move H rook
            MovePiece<false>(WR, f1, h1, pos);
            break;

            // white castles queen side
        case (c1):
            // move A rook
            MovePiece<false>(WR, d1, a1, pos);
            break;

            // black castles king side
        case (g8):
            // move H rook
            MovePiece<false>(BR, f8, h8, pos);
            break;

            // black castles queen side
        case (c8):
            // move A rook
            MovePiece<false>(BR, d8, a8, pos);
            break;
        }
    }

    // change side
    pos->ChangeSide();

    // restore zobrist key (done at the end to avoid overwriting the value while
    // moving pieces back to their place)
    pos->posKey = pos->played_positions.back();
    pos->played_positions.pop_back();
}

// MakeNullMove handles the playing of a null move (a move that doesn't move any piece)
void MakeNullMove(Position* pos) {
    saveBoardState(pos);
    // Store position key in the array of searched position
    pos->played_positions.emplace_back(pos->posKey);
    // Update the zobrist key asap so we can prefetch
    if (GetEpSquare(pos) != no_sq) {
        HashKey(pos, enpassant_keys[GetEpSquare(pos)]);
        pos->enPas = no_sq;
    }
    pos->ChangeSide();
    HashKey(pos, SideKey);
    TTPrefetch(pos->GetPoskey());

    pos->hisPly++;
    pos->historyStackHead++;
    pos->fiftyMove++;
    pos->plyFromNull = 0;

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
