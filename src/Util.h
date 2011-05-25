#ifndef NVDIMM_UTIL_H
#define NVDIMM_UTIL_H

#include <string>
#include <iostream>
#include <sstream>

using namespace std;
   
// Utilites used by NVDIMM
uint64_t convert_uint64_t(string value);

uint divide_params(uint num, uint denom);
uint divide_params(float num, float denom);

#endif
