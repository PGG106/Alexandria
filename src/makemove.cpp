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
    pop_bit(pos->state().bitboards[piece], from);
    pop_bit(pos->state().occupancies[color], from);
    pos->state().pieces[from] = EMPTY;
    HashKey(pos->state().posKey, PieceKeys[piece][from]);
    if(GetPieceType(piece) == PAWN)
        HashKey(pos->state().pawnKey, PieceKeys[piece][from]);
    else if(Color[piece] == WHITE)
        HashKey(pos->state().whiteNonPawnKey, PieceKeys[piece][from]);
    else
        HashKey(pos->state().blackNonPawnKey, PieceKeys[piece][from]);
}

void AddPiece(const int piece, const int to, Position* pos) {
    assert(piece != EMPTY);
    const int color = Color[piece];
    set_bit(pos->state().bitboards[piece], to);
    set_bit(pos->state().occupancies[color], to);
    pos->state().pieces[to] = piece;
    HashKey(pos->state().posKey, PieceKeys[piece][to]);
    if(GetPieceType(piece) == PAWN)
        HashKey(pos->state().pawnKey, PieceKeys[piece][to]);
    else if(Color[piece] == WHITE)
        HashKey(pos->state().whiteNonPawnKey, PieceKeys[piece][to]);
    else
        HashKey(pos->state().blackNonPawnKey, PieceKeys[piece][to]);
}

void MovePiece(const int piece, const int from, const int to, Position* pos) {
    ClearPiece(piece, from, pos);
    AddPiece(piece, to, pos);
}

void UpdateCastlingPerms(Position* pos, int source_square, int target_square) {
    // Xor the old castling key from the zobrist key
    HashKey(pos->state().posKey, CastleKeys[pos->getCastlingPerm()]);
    pos->state().castlePerm &= pos->castlingRightsMask[source_square];
    pos->state().castlePerm &= pos->castlingRightsMask[target_square];
    // Xor the new one
    HashKey(pos->state().posKey, CastleKeys[pos->getCastlingPerm()]);
}

inline void resetEpSquare(Position* pos) {
    if (pos->getEpSquare() != no_sq) {
        HashKey(pos->state().posKey, enpassant_keys[pos->getEpSquare()]);
        pos->state().enPas = no_sq;
    }
}

template void MakeMove<true, true>(const Move move, Position* pos, std::vector<ZobristKey>& keyHistory,
                             DirtyPieces* dirtyPieces);
template void MakeMove<true, false>(const Move move, Position* pos, std::vector<ZobristKey>& keyHistory,
                             DirtyPieces* dirtyPieces);
template void MakeMove<false, false>(const Move move, Position* pos, std::vector<ZobristKey>& keyHistory,
                              DirtyPieces* dirtyPieces);

// make move on chess board
template <bool UPDATE, bool TRACK_DIRTY>
void MakeMove(const Move move, Position* pos, std::vector<ZobristKey>& keyHistory, DirtyPieces* dirtyPieces) {
    if constexpr (UPDATE) {
        pos->history.push(pos->state());
    }

    // Store position key in the array of searched position
    keyHistory.emplace_back(pos->getPoskey());

    // parse move flag
    const bool capture = isCapture(move);
    const bool doublePush = isDP(move);
    const bool enpass = isEnpassant(move);
    const bool castling = isCastle(move);
    const bool promotion = isPromo(move);

    // parse move
    const Square sourceSquare = From(move);
    const Square targetSquare = To(move);
    const int piece = Piece(move);

    // increment fifty move rule counter
    pos->state().fiftyMove++;
    pos->state().plyFromNull++;
    pos->state().hisPly++;

    if (castling) {
        const bool kingSide = GetMovetype(move) == static_cast<int>(Movetype::KSCastle);
        const int castleRight = pos->side == WHITE ? kingSide ? WKCA : WQCA : kingSide ? BKCA : BQCA;
        const Square rookSourceSquare = pos->getCastlingRookSquare(castleRight);
        const Square rookTargetSquare = pos->side == WHITE ? kingSide ? f1 : d1 : kingSide ? f8 : d8;
        const int rook = GetPiece(ROOK, pos->side);

        if constexpr (TRACK_DIRTY) {
            dirtyPieces->removedCount = 0;
            dirtyPieces->addedCount = 0;
            dirtyPieces->remove(sourceSquare, piece);
            dirtyPieces->remove(rookSourceSquare, rook);
            dirtyPieces->add(targetSquare, piece);
            dirtyPieces->add(rookTargetSquare, rook);
        }

        // Move both pieces after clearing their original squares, which handles overlapping Chess960 paths.
        ClearPiece(piece, sourceSquare, pos);
        ClearPiece(rook, rookSourceSquare, pos);
        AddPiece(piece, targetSquare, pos);
        AddPiece(rook, rookTargetSquare, pos);
        resetEpSquare(pos);
        UpdateCastlingPerms(pos, sourceSquare, targetSquare);
    }
    else if (doublePush) {
        if constexpr (TRACK_DIRTY) {
            dirtyPieces->removedCount = 0;
            dirtyPieces->addedCount = 0;
            dirtyPieces->remove(sourceSquare, piece);
            dirtyPieces->add(targetSquare, piece);
        }

        pos->state().fiftyMove = 0;

        MovePiece(piece, sourceSquare, targetSquare, pos);

        resetEpSquare(pos);

        // Add new ep square
        const int SOUTH = pos->side == WHITE ? 8 : -8;
        int epSquareCandidate = targetSquare + SOUTH;
        if (!(getPawnAttacks(epSquareCandidate, pos->side) & pos->getPieceColorBB(PAWN, pos->side ^ 1)))
            epSquareCandidate = no_sq;
        pos->state().enPas = epSquareCandidate;
        if (pos->getEpSquare() != no_sq)
            HashKey(pos->state().posKey, enpassant_keys[pos->getEpSquare()]);
    }
    else if (enpass) {
        pos->state().fiftyMove = 0;

        const int SOUTH = pos->side == WHITE ? 8 : -8;
        const int pieceCap = GetPiece(PAWN, pos->side ^ 1);
        const Square capturedPieceLocation = static_cast<Square>(targetSquare + SOUTH);

        if constexpr (TRACK_DIRTY) {
            dirtyPieces->removedCount = 0;
            dirtyPieces->addedCount = 0;
            dirtyPieces->remove(sourceSquare, piece);
            dirtyPieces->remove(capturedPieceLocation, pieceCap);
            dirtyPieces->add(targetSquare, piece);
        }

        ClearPiece(pieceCap, capturedPieceLocation, pos);

        // Remove the piece fom the square it moved from
        ClearPiece(piece, sourceSquare, pos);
        // Set the piece to the destination square
        AddPiece(piece, targetSquare, pos);

        // Reset EP square
        assert(pos->getEpSquare() != no_sq);
        HashKey(pos->state().posKey, enpassant_keys[pos->getEpSquare()]);
        pos->state().enPas = no_sq;
    }
    else if (promotion) {
        pos->state().fiftyMove = 0;

        const int promotedPiece = GetPiece(getPromotedPiecetype(move), pos->side);

        if constexpr (TRACK_DIRTY) {
            dirtyPieces->removedCount = 0;
            dirtyPieces->addedCount = 0;
            dirtyPieces->remove(sourceSquare, piece);
            if (capture)
                dirtyPieces->remove(targetSquare, pos->PieceOn(targetSquare));
            dirtyPieces->add(targetSquare, promotedPiece);
        }

        // Remove the piece fom the square it moved from
        ClearPiece(piece, sourceSquare, pos);

        if (capture) {
            const int pieceCap = pos->PieceOn(targetSquare);
            assert(pieceCap != EMPTY);
            assert(GetPieceType(pieceCap) != KING);
            ClearPiece(pieceCap, targetSquare, pos);
        }
        // Set the piece to the destination square, if it was a promotion we directly set the promoted piece
        AddPiece(promotedPiece, targetSquare, pos);

        resetEpSquare(pos);

        UpdateCastlingPerms(pos, sourceSquare, targetSquare);
    }
    else if (!capture) {
        if constexpr (TRACK_DIRTY) {
            dirtyPieces->removedCount = 0;
            dirtyPieces->addedCount = 0;
            dirtyPieces->remove(sourceSquare, piece);
            dirtyPieces->add(targetSquare, piece);
        }

        // if a pawn was moved or a capture was played reset the 50 move rule counter
        if (GetPieceType(piece) == PAWN)
            pos->state().fiftyMove = 0;

        MovePiece(piece, sourceSquare, targetSquare, pos);

        resetEpSquare(pos);

        UpdateCastlingPerms(pos, sourceSquare, targetSquare);
    }
    else {
        pos->state().fiftyMove = 0;

        const int pieceCap = pos->PieceOn(targetSquare);
        assert(pieceCap != EMPTY);
        assert(GetPieceType(pieceCap) != KING);

        if constexpr (TRACK_DIRTY) {
            dirtyPieces->removedCount = 0;
            dirtyPieces->addedCount = 0;
            dirtyPieces->remove(sourceSquare, piece);
            dirtyPieces->remove(targetSquare, pieceCap);
            dirtyPieces->add(targetSquare, piece);
        }

        ClearPiece(pieceCap, targetSquare, pos);

        MovePiece(piece, sourceSquare, targetSquare, pos);

        resetEpSquare(pos);

        UpdateCastlingPerms(pos, sourceSquare, targetSquare);
    }

    pos->ChangeSide();
    // Xor the new side into the key
    HashKey(pos->state().posKey, SideKey);
    // Update pinmasks and checkers
    UpdatePinsAndCheckers(pos);

    // Make sure a freshly generated zobrist key matches the one we are incrementally updating
    assert(pos->getPoskey() == GeneratePosKey(pos));
    assert(pos->state().pawnKey == GeneratePawnKey(pos));
}

void UnmakeMove(Position* pos, std::vector<ZobristKey>& keyHistory) {
    pos->history.pop();
    pos->ChangeSide();
    keyHistory.pop_back();
}

// MakeNullMove handles the playing of a null move (a move that doesn't move any piece)
void MakeNullMove(Position* pos, std::vector<ZobristKey>& keyHistory) {
    pos->history.push(pos->state());
    // Store position key in the array of searched position
    keyHistory.emplace_back(pos->getPoskey());
    resetEpSquare(pos);
    pos->ChangeSide();
    HashKey(pos->state().posKey, SideKey);

    pos->state().hisPly++;
    pos->state().fiftyMove++;
    pos->state().plyFromNull = 0;

    // we haven't moved any piece, so pins stay unchanged, as far as checkers for the opponent's side:
    // if the opponent was in check before we could play this move we would've won, so they weren't
    // that can't change after a nullmove, therefore do not update pins and checkers
}

// Take back a null move
void TakeNullMove(Position* pos,  std::vector<ZobristKey>& keyHistory) {
    pos->history.pop();
    pos->ChangeSide();
    keyHistory.pop_back();
}
