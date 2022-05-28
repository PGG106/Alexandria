#include "Board.h"
#include"PieceData.h"
#include <stdio.h>
#include "math.h"
#include <intrin.h>
#include "hashkey.h"
#include "string.h"

// convert squares to coordinates
const char* square_to_coordinates[] = {
    "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8",
    "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
    "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
    "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
    "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
    "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
    "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
    "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
};

char RankChar[] = "12345678";
char FileChar[] = "abcdefgh";


const int get_rank[64] =
{
    7, 7, 7, 7, 7, 7, 7, 7,
    6, 6, 6, 6, 6, 6, 6, 6,
    5, 5, 5, 5, 5, 5, 5, 5,
    4, 4, 4, 4, 4, 4, 4, 4,
    3, 3, 3, 3, 3, 3, 3, 3,
    2, 2, 2, 2, 2, 2, 2, 2,
    1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0
};


const int Color[12] =
{
    WHITE,WHITE,WHITE,WHITE,WHITE,WHITE,BLACK,BLACK,BLACK,BLACK,BLACK,BLACK
};



// extract rank from a square [square]
const int get_file[64] =
{
    0, 1, 2, 3, 4, 5, 6, 7,
    0, 1, 2, 3, 4, 5, 6, 7,
    0, 1, 2, 3, 4, 5, 6, 7,
    0, 1, 2, 3, 4, 5, 6, 7,
    0, 1, 2, 3, 4, 5, 6, 7,
    0, 1, 2, 3, 4, 5, 6, 7,
    0, 1, 2, 3, 4, 5, 6, 7,
    0, 1, 2, 3, 4, 5, 6, 7
};


// extract rank from a square [square]
const int get_diagonal[64] =
{
    14, 13, 12, 11, 10, 9, 8, 7,
    13, 12, 11, 10, 9, 8, 7, 6,
    12, 11, 10, 9, 8, 7, 6, 5,
    11, 10, 9, 8, 7, 6, 5, 4,
    10, 9, 8, 7, 6, 5, 4, 3,
    9, 8, 7, 6, 5, 4, 3, 2,
    8, 7, 6, 5, 4, 3, 2, 1,
    7, 6, 5, 4, 3, 2, 1, 0
};




// ASCII pieces
const char ascii_pieces[13] = "PNBRQKpnbrqk";


int fiftyMove = 0;

// castling rights update constants
const int castling_rights[64] = {
     7, 15, 15, 15,  3, 15, 15, 11,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    13, 15, 15, 15, 12, 15, 15, 14
};


int count_bits(Bitboard bitboard)
{
    return __popcnt64(bitboard);
}


#if defined(__GNUC__)  // GCC, Clang, ICC

inline Square lsb(Bitboard b) {
    assert(b);
    return Square(__builtin_ctzll(b));
}







#elif defined(_MSC_VER)  // MSVC

#ifdef _WIN64  // MSVC, WIN64

// get least significant 1st bit index
int get_ls1b_index(Bitboard bitboard)
{
    // make sure bitboard is not 0
    if (!bitboard)
        return(-100) ;

    unsigned long idx;
    _BitScanForward64(&idx, bitboard);
    return (int)idx;

}




#else  // MSVC, WIN32

int lsb(Bitboard b) {
    assert(b);
    unsigned long idx;

    if (b & 0xffffffff) {
        _BitScanForward(&idx, int32_t(b));
        return Square(idx);
    }
    else {
        _BitScanForward(&idx, int32_t(b >> 32));
        return Square(idx + 32);
    }
}


#endif


#else  // Compiler is neither GCC nor MSVC compatible

#error "Compiler not supported."

#endif






void ResetBoard(S_Board* pos) { // a function that resets every value stored in the position and on the board and sets the pieces to illegal squares
    int index = 0;

    for (index = 0; index < 64; ++index) {
        pos->pieces[index] = 14;
    }



    pos->side = BOTH;
    pos->enPas = no_sq;
    pos->fiftyMove = 0;
    pos->ply = 0;
    pos->hisPly = 0; //total number of halfmoves
    pos->castleperm = 0; //integer that represents the castling permission in his bits (1111) = all castlings allowed (0000) no castling allowed, (0101) only WKCA and BKCA allowed... 
    pos->posKey = 0ULL;
    pos->pinD = 0;
    pos->pinHV = 0;
    pos->checkMask = 18446744073709551615ULL;
    pos->checks = 0;




}


int  square_distance(int a, int b) {
    return fmax(abs(get_file[a] - get_file[b]), abs(get_rank[a] - get_rank[b]));
}

// parse FEN string
void parse_fen( char* fen, S_Board* pos)
{
    // reset board position (pos->pos->bitboards)
    memset(pos->bitboards, 0ULL, sizeof(pos->bitboards));

    // reset pos->occupancies (pos->pos->bitboards)
    memset(pos->occupancies, 0ULL, sizeof(pos->occupancies));


    ResetBoard(pos);



    // loop over board ranks
    for (int rank = 0; rank < 8; rank++)
    {
        // loop over board files
        for (int file = 0; file < 8; file++)
        {
            // init current square
            int square = rank * 8 + file;

            // match ascii pieces within FEN string
            if ((*fen >= 'a' && *fen <= 'z') || (*fen >= 'A' && *fen <= 'Z'))
            {
                // init piece type
                int piece = char_pieces[*fen];

                // set piece on corresponding bitboard
                set_bit(pos->bitboards[piece], square);
                pos->pieces[square] = piece;
                // increment pointer to FEN string
                fen++;
            }

            // match empty square numbers within FEN string
            if (*fen >= '0' && *fen <= '9')
            {
                // init offset (convert char 0 to int 0)
                int offset = *fen - '0';

                // define piece variable
                int piece = -1;

                // loop over all piece pos->pos->bitboards
                for (int bb_piece = WP; bb_piece <= BK; bb_piece++)
                {
                    // if there is a piece on current square
                    if (get_bit(pos->bitboards[bb_piece], square))
                        // get piece code
                        piece = bb_piece;
                }

                // on empty current square
                if (piece == -1)
                    // decrement file
                    file--;

                // adjust file counter
                file += offset;

                // increment pointer to FEN string
                fen++;
            }

            // match rank separator
            if (*fen == '/')
                // increment pointer to FEN string
                fen++;
        }
    }

    // got to parsing side to move (increment pointer to FEN string)
    fen++;

    // parse side to move

    (*fen == 'w') ? (pos->side = WHITE) : (pos->side = BLACK);

    // go to parsing castling rights
    fen += 2;

    // parse castling rights
    while (*fen != ' ')
    {
        switch (*fen)
        {
        case 'K': (pos->castleperm) |= WKCA; break;
        case 'Q': (pos->castleperm) |= WQCA; break;
        case 'k': (pos->castleperm) |= BKCA; break;
        case 'q': (pos->castleperm) |= BQCA; break;
        case '-': break;
        }

        // increment pointer to FEN string
        fen++;
    }

    // got to parsing enpassant square (increment pointer to FEN string)
    fen++;

    // parse enpassant square
    if (*fen != '-')
    {
        // parse enpassant file & rank
        int file = fen[0] - 'a';
        int rank = 8 - (fen[1] - '0');

        // init enpassant square
        pos->enPas = rank * 8 + file;
    }

    // no enpassant square
    else
        pos->enPas = no_sq;

    // loop over white pieces pos->pos->bitboards
    for (int piece = WP; piece <= WK; piece++)
        // populate white occupancy bitboard
        pos->occupancies[WHITE] |= pos->bitboards[piece];

    // loop over black pieces pos->pos->bitboards
    for (int piece = BP; piece <= BK; piece++)
        // populate white occupancy bitboard
        pos->occupancies[BLACK] |= pos->bitboards[piece];

    // init all pos->occupancies
    pos->occupancies[BOTH] |= pos->occupancies[WHITE];
    pos->occupancies[BOTH] |= pos->occupancies[BLACK];


    pos->posKey = GeneratePosKey(pos);

}

