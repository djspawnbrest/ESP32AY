#define PGM_READ_UNALIGNED 0

#include "AudioGeneratorXM.h"

#pragma GCC optimize ("O3")
#pragma GCC optimize ("unroll-loops")

// #define DEBUG_LHEADER
// #define DEBUG_LPATTERN
// #define DEBUG_SAMPLE_TIMING

#ifndef min
#define min(X,Y)((X)<(Y)?(X):(Y))
#endif

#ifndef max
#define max(X,Y)((X)>(Y)?(X):(Y))
#endif

#define MSN(x)(((x)&0xf0)>>4)
#define LSN(x)((x)&0x0f)

