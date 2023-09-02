#pragma once

#include <map>

// convert ASCII character pieces to encoded constants
extern std::map<char, int> char_pieces;

// promoted pieces
extern std::map<int, char> promoted_pieces;

// MVV LVA [attacker][victim]
constexpr int mvv_lva[6][6] = {
    105, 205, 305, 405, 505, 605,
    104, 204, 304, 404, 504, 604,  
    103, 203, 303, 403, 503, 603,  
    102, 202, 302, 402, 502, 602,  
    101, 201, 301, 401, 501, 601,  
    100, 200, 300, 400, 500, 600
};
