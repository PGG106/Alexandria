#include "time_manager.h"

#include "search.h"
#include "uci.h"
#include <algorithm>
#include <cfloat>
#include <cmath>

int optimum(S_Board *pos, S_SearchINFO *info, int time, int inc) {

  if (time != -1) {

    info->timeset = TRUE;

    if (info->movestogo != -1) {
      time /= info->movestogo;
      time -= 50;
      return (info->starttime + (time + inc / 2));

    } else {

      return (info->starttime + (time / 20 + inc / 2));
    }
  }

  return (info->starttime + (time + inc / 2));
}
