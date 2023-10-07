#pragma once

#include <map>

// convert ASCII character pieces to encoded constants
extern std::map<char, int> char_pieces;

// promoted pieces
extern std::map<int, char> promoted_pieces;

// MVV LVA [attacker][victim]
constexpr int mvv_lva[6][6] = {
    100005, 200005, 300005, 400005, 500005, 600005,
    100004, 200004, 300004, 400004, 500004, 600004,  
    100003, 200003, 300003, 400003, 500003, 600003,  
    100002, 200002, 300002, 400002, 500002, 600002,  
    100001, 200001, 300001, 400001, 500001, 600001,  
    100000, 200000, 300000, 400000, 500000, 600000
};
