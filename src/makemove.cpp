#include "bitboard.h"
#include "makemove.h"
#include "ttable.h"
#include "position.h"
#include "init.h"

// Remove a piece from a square
void ClearPiece(const int piece, const int from, Position* pos) {
    const int color = Color[piece];
    pop_bit(pos->bitboards[piece], from);
    pop_bit(pos->occupancies[color], from);
    pos->pieces[from] = EMPTY;
    HashKey(pos, PieceKeys[piece][from]);
}

// Add a piece to a square
void AddPiece(const int piece, const int to, Position* pos) {
    const int color = Color[piece];
    set_bit(pos->bitboards[piece], to);
    set_bit(pos->occupancies[color], to);
    pos->pieces[to] = piece;
    HashKey(pos, PieceKeys[piece][to]);
}

// Remove a piece from a square while also deactivating the nnue weights tied to the piece
void ClearPieceNNUE(const int piece, const int sq, Position* pos) {
    pos->NNUESub.emplace_back(nnue.GetIndex(piece, sq));
    ClearPiece(piece, sq, pos);
}

// Add a piece to a square while also activating the nnue weights tied to the piece
void AddPieceNNUE(const int piece, const int to, Position* pos) {
    pos->NNUEAdd.emplace_back(nnue.GetIndex(piece, to));
    AddPiece(piece, to, pos);
}

// Move a piece from square to to square from without updating the NNUE weights
void MovePiece(const int piece, const int from, const int to, Position* pos) {
    ClearPiece(piece, from, pos);
    AddPiece(piece, to, pos);
}

// Move a piece from the [to] square to the [from] square
void MovePieceNNUE(const int piece, const int from, const int to, Position* pos) {
    ClearPieceNNUE(piece, from, pos);
    AddPieceNNUE(piece, to, pos);
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

// make move on chess board
void MakeUCIMove(const int move, Position* pos) {
    const int sourceSquare = From(move);
    const int targetSquare = To(move);
    const int piece = Piece(move);
    const int promotedPiece = GetPiece(getPromotedPiecetype(move), pos->side);
    // parse move flag
    const bool capture = isCapture(move);
    const bool enpass = isEnpassant(move);
    // increment fifty move rule counter
    pos->fiftyMove++;
    pos->plyFromNull++;
    pos->hisPly++;

    // if a pawn was moved or a capture was played reset the 50 move rule counter
    if (GetPieceType(piece) == PAWN || capture)
        pos->fiftyMove = 0;

    const int SOUTH = pos->side == WHITE ? 8 : -8;
    // handling capture moves
    if (capture) {
        const int pieceCap = enpass ? GetPiece(PAWN, pos->side ^ 1) : pos->pieces[targetSquare];
        const int capturedPieceLocation = enpass ? targetSquare + SOUTH : targetSquare;
        assert(pieceCap != EMPTY);
        assert(GetPieceType(pieceCap) != KING);
        ClearPiece(pieceCap, capturedPieceLocation, pos);
    }

    // Remove the piece fom the square it moved from
    ClearPiece(piece, sourceSquare, pos);
    // Set the piece to the destination square, if it was a promotion we directly set the promoted piece
    AddPiece(isPromo(move) ? promotedPiece : piece, targetSquare, pos);

    // Reset EP square
    if (GetEpSquare(pos) != no_sq)
        HashKey(pos, enpassant_keys[GetEpSquare(pos)]);

    // reset enpassant square
    pos->enPas = no_sq;

    // handle double pawn push
    if (isDP(move)) {
        pos->enPas = targetSquare + SOUTH;
        // hash enpassant
        HashKey(pos, enpassant_keys[GetEpSquare(pos)]);
    }

    // handle castling moves
    if (isCastle(move)) {
        // switch target square
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
    }

    UpdateCastlingPerms(pos, sourceSquare, targetSquare);

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
}

// make move on chess board
void MakeMove(const int move, Position* pos) {
    saveBoardState(pos);
    // Store position key in the array of searched position
    pos->played_positions.emplace_back(pos->posKey);

    pos->accumStack[pos->accumStackHead] = pos->AccumulatorTop();
    pos->accumStackHead++;

    // parse move
    const int sourceSquare = From(move);
    const int targetSquare = To(move);
    const int piece = Piece(move);
    const int promotedPiece = GetPiece(getPromotedPiecetype(move), pos->side);
    // parse move flag
    const bool capture = isCapture(move);
    const bool enpass = isEnpassant(move);

    // increment fifty move rule counter
    pos->fiftyMove++;
    pos->plyFromNull++;
    pos->hisPly++;

    // if a pawn was moved or a capture was played reset the 50 move rule counter
    if (GetPieceType(piece) == PAWN || capture)
        pos->fiftyMove = 0;

    const int SOUTH = pos->side == WHITE ? 8 : -8;
    // handling capture moves
    if (capture) {
        const int pieceCap = enpass ? GetPiece(PAWN, pos->side ^ 1) : pos->pieces[targetSquare];
        const int capturedPieceLocation = enpass ? targetSquare + SOUTH : targetSquare;
        assert(pieceCap != EMPTY);
        assert(GetPieceType(pieceCap) != KING);
        ClearPieceNNUE(pieceCap, capturedPieceLocation, pos);

        pos->history[pos->historyStackHead].capture = pieceCap;
    }

    pos->historyStackHead++;
    // Remove the piece fom the square it moved from
    ClearPieceNNUE(piece, sourceSquare, pos);
    // Set the piece to the destination square, if it was a promotion we directly set the promoted piece
    AddPieceNNUE(isPromo(move) ? promotedPiece : piece, targetSquare, pos);

    // Reset EP square
    if (GetEpSquare(pos) != no_sq)
        HashKey(pos, enpassant_keys[GetEpSquare(pos)]);

    // reset enpassant square
    pos->enPas = no_sq;

    // handle double pawn push
    if (isDP(move)) {
        pos->enPas = targetSquare + SOUTH;
        // hash enpassant
        HashKey(pos, enpassant_keys[GetEpSquare(pos)]);
    }

    // handle castling moves
    if (isCastle(move)) {
        // switch target square
        switch (targetSquare) {
            // white castles king side
            case (g1):
                // move H rook
                MovePieceNNUE(WR, h1, f1, pos);
                break;

                // white castles queen side
            case (c1):
                // move A rook
                MovePieceNNUE(WR, a1, d1, pos);
                break;

                // black castles king side
            case (g8):
                // move H rook
                MovePieceNNUE(BR, h8, f8, pos);
                break;

                // black castles queen side
            case (c8):
                // move A rook
                MovePieceNNUE(BR, a8, d8, pos);
                break;
        }
    }
    UpdateCastlingPerms(pos, sourceSquare, targetSquare);

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
}

void UnmakeMove(const int move, Position* pos) {
    pos->hisPly--;
    pos->historyStackHead--;

    restorePreviousBoardState(pos);

    pos->NNUEAdd.clear();
    pos->NNUESub.clear();

    // parse move
    const int sourceSquare = From(move);
    const int targetSquare = To(move);
    const int piece = Piece(move);

    pos->accumStackHead--;

    // handle pawn promotions
    if (isPromo(move)) {
        const int promoted_piece = GetPiece(getPromotedPiecetype(move), pos->side ^ 1);
        ClearPiece(promoted_piece, targetSquare, pos);
    }

    // move piece
    MovePiece(piece, targetSquare, sourceSquare, pos);

    const int SOUTH = pos->side == WHITE ? -8 : 8;

    // handle captures
    if (isCapture(move)) {
        // Retrieve the captured piece we have to restore
        const int piececap = pos->history[pos->historyStackHead].capture;
        const int capturedPieceLocation = isEnpassant(move) ? targetSquare + SOUTH : targetSquare;
        AddPiece(piececap, capturedPieceLocation, pos);
    }

    // handle castling moves
    if (isCastle(move)) {
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
    if (GetEpSquare(pos) != no_sq)
        HashKey(pos, enpassant_keys[GetEpSquare(pos)]);
    pos->ChangeSide();
    HashKey(pos, SideKey);
    TTPrefetch(pos->GetPoskey());

    pos->hisPly++;
    pos->historyStackHead++;
    pos->fiftyMove++;
    pos->plyFromNull = 0;

    // reset enpassant square
    pos->enPas = no_sq;

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
