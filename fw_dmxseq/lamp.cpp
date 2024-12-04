// DMX512 lamps

#include "lamp.h"

Lamp* Lamp::lamps[MAX_LAMPS] = {0};
int Lamp::period_ms = 10; // compute period
int* Lamp::groups[MAX_GROUPS] = {0};
