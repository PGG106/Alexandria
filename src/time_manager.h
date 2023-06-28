#pragma once

struct S_SearchINFO;
struct S_ThreadData;

void Optimum(S_SearchINFO* info, int time, int inc);
bool StopEarly(const S_SearchINFO* info);
bool TimeOver(const S_SearchINFO* info);
bool NodesOver(const S_SearchINFO* info);
void ScaleTm(S_ThreadData* td);
