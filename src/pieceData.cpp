#include "pieceData.h"
#include "types.h"
#include "eval.h"
#include <map>
#include <string>

// convert ASCII character pieces to encoded constants

std::map<char, int> char_pieces = {
    {'P', WP}, {'N', WN}, {'B', WB}, {'R', WR}, {'Q', WQ}, {'K', WK}, {'p', BP},
    {'n', BN}, {'b', BB}, {'r', BR}, {'q', BQ}, {'k', BK}
};

//Map promoted piece to the corresponding ASCII character
std::map<int, char> promoted_pieces = {{WQ, 'q'}, {WR, 'r'}, {WB, 'b'},
                                       {WN, 'n'}, {BQ, 'q'}, {BR, 'r'},
                                       {BB, 'b'}, {BN, 'n'}};


