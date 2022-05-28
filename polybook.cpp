#include "Board.h"
#include "polykeys.h"
#include "stdlib.h"
#include <stdio.h>
#include "move.h"
#include<time.h>
#include "io.h"

typedef struct {
	Bitboard key;
	unsigned short move;
	unsigned short weight;
	unsigned int learn;
}S_POLY_BOOK_ENTRY;


long NumEntries = 0;


S_POLY_BOOK_ENTRY* entries;


const int PolyKindOfPiece[12] = {
	1,3,5,7,9,11,0,2,4,6,8,10
};

void InitPolyBook() {

	FILE* pFile = fopen("performance.bin", "rb");

	if (pFile == NULL) {
		printf("Book File Not Read\n");
	}
	else {
		fseek(pFile, 0, SEEK_END);
		long position = ftell(pFile);

		if (position < sizeof(S_POLY_BOOK_ENTRY)) {
			printf("No Entries Found");
			return;
		}

		NumEntries = position / sizeof(S_POLY_BOOK_ENTRY);
		printf("%ld Entries Found in file", NumEntries);

		entries = (S_POLY_BOOK_ENTRY*)malloc(NumEntries * sizeof(S_POLY_BOOK_ENTRY));

		rewind(pFile);

		size_t returnValue;

		returnValue = fread(entries, sizeof(S_POLY_BOOK_ENTRY), NumEntries, pFile);

		printf("fread() %zd Entries Read in from file\n", returnValue);
	}

}

void CleanPolyBook() {

	free(entries);


}



int HasPawnForCapture(const S_Board* pos) {
	int sqWithPawn = 0;
	int targetPce = (pos->side == WHITE) ? WP : BP;
	if (pos->enPas != no_sq) {
		if (pos->side == WHITE) {
			sqWithPawn = pos->enPas + 8;
		}
		else {
			sqWithPawn = pos->enPas - 8;
		}
		int file, rank;
		file = get_file[sqWithPawn];
		rank = get_rank[sqWithPawn];

		if ((rank != 7 && ((sqWithPawn + 1) <= 63))) {

			if (pos->pieces[sqWithPawn + 1] == targetPce) {
				return TRUE;
			}

		}


		if (rank != 0 && ((sqWithPawn - 1) >= 0))
		{

			if (pos->pieces[sqWithPawn - 1] == targetPce) {
				return TRUE;

			}

		}

		return FALSE;

	}
	return FALSE;
}


Bitboard PolyKeyFromBoard(S_Board* pos) {

	int square = 0, rank = 0, file = 0;

	Bitboard finalKey = 0;
	int piece = EMPTY;
	int polyPiece = 0;
	int offset = 0;


	for (square = 0;square < 64;++square) {
		piece = pos->pieces[square];

		if (piece != no_sq && piece != EMPTY) {
			polyPiece = PolyKindOfPiece[piece];
			rank = get_rank[square];
			file = get_file[square];
			finalKey ^= Random64Poly[(64 * polyPiece) + (8 * rank) + file];
		}


	}

	offset = 768;

	if (pos->castleperm & WKCA) finalKey ^= Random64Poly[offset + 0];
	if (pos->castleperm & WQCA) finalKey ^= Random64Poly[offset + 1];
	if (pos->castleperm & BKCA) finalKey ^= Random64Poly[offset + 2];
	if (pos->castleperm & BQCA) finalKey ^= Random64Poly[offset + 3];

	offset = 772;


	if (HasPawnForCapture(pos) == TRUE) {
		file = get_file[pos->enPas];
		finalKey ^= Random64Poly[offset + file];
	}


	if (pos->side == WHITE) {

		finalKey ^= Random64Poly[780];
	}


	return finalKey;


}

unsigned short endian_swap_u16(unsigned short x)
{
	x = (x >> 8) |
		(x << 8);
	return x;
}

unsigned int endian_swap_u32(unsigned int x)
{
	x = (x >> 24) |
		((x << 8) & 0x00FF0000) |
		((x >> 8) & 0x0000FF00) |
		(x << 24);
	return x;
}

Bitboard endian_swap_u64(Bitboard x)
{
	x = (x >> 56) |
		((x << 40) & 0x00FF000000000000) |
		((x << 24) & 0x0000FF0000000000) |
		((x << 8) & 0x000000FF00000000) |
		((x >> 8) & 0x00000000FF000000) |
		((x >> 24) & 0x0000000000FF0000) |
		((x >> 40) & 0x000000000000FF00) |
		(x << 56);
	return x;
}

int ConvertPolyMovetoInternalMove(unsigned short move, S_Board* pos) {

	int ff = (move >> 6) & 7;
	int fr = (move >> 9) & 7;
	int tf = (move >> 0) & 7;
	int tr = (move >> 3) & 7;
	int pp = (move >> 12) & 7;

	char moveString[6];

	if (pp == 0) {

		sprintf(moveString, "%c%c%c%c",
			FileChar[ff],
			RankChar[fr],
			FileChar[tf],
			RankChar[tr]
		);

	}
	else {
		char promChar = 'q';
		switch (pp) {
		case 1: promChar = 'n'; break;
		case 2: promChar = 'b'; break;
		case 3: promChar = 'r'; break;
		}

		sprintf(moveString, "%c%c%c%c%c",
			FileChar[ff],
			RankChar[fr],
			FileChar[tf],
			RankChar[tf],
			promChar);
	}

	return ParseMove(moveString, pos);

}




int GetBookMove(S_Board* pos) {

	S_POLY_BOOK_ENTRY* entry;
	unsigned short move;
	Bitboard PolyKey = PolyKeyFromBoard(pos);
	int bookMoves[32];
	int tempMove = NOMOVE;
	int count = 0;

	for (entry = entries;entry < entries + NumEntries;entry++) {

		if (PolyKey == endian_swap_u64(entry->key)) {

			move = endian_swap_u16(entry->move);
			tempMove = ConvertPolyMovetoInternalMove(move, pos);

			if (tempMove != NOMOVE) {
				bookMoves[count++] = tempMove;
				if (count > 32) {
					break;
				}
			}
		}

	}

	if (count != 0) {
		srand(time(0));
		int randMove = rand() % count;
		return bookMoves[randMove];
	}
	else {
		return NOMOVE;
	}

}

