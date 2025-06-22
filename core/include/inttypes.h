#pragma once

// minimal inttypes.h to provide formatting macros for integers

#ifdef UNDER_TEST
#include "/usr/include/inttypes.h"
#else

#include <limits.h>

// this is a total assumption, but should hopefully hold up (fingers crossed!)
#if __SIZEOF_POINTER__ == 2
#define __PREFIX_32 "l" // regular int will most likely be 16 bits here, it'll be strange if it's not
#define __PREFIX_64 "ll"
#define __PREFIX_POINTER
#elif __SIZEOF_POINTER__ == 4
#define __PREFIX_32
#define __PREFIX_64 "ll"
#define __PREFIX_POINTER // this is assuming 32 bit int, which *should* always be the case
#elif __SIZEOF_POINTER__ == 8
#define __PREFIX_32
#define __PREFIX_64 "l"
#define __PREFIX_POINTER "l"
#else
#error unsupported pointer size
#endif

#if defined(LLONG_MAX) || defined (ULLONG_MAX)
#define __PREFIX_MAX "ll"
#else
#define __PREFIX_MAX "l"
#endif

// printf signed int macros

#define PRId8 "d"
#define PRId16 "d"
#define PRId32 __PREFIX_32 "d"
#define PRId64 __PREFIX_64 "d"

// these are all a total guess (this is how musl does it so maybe they're on to something), but it probably doesn't matter
#define PRIdLEAST8 "d"
#define PRIdLEAST16 "d"
#define PRIdLEAST32 __PREFIX_32 "d"
#define PRIdLEAST64 __PREFIX_64 "d"

#define PRIdFAST8 "d"
#define PRIdFAST16 "d"
#define PRIdFAST32 __PREFIX_32 "d"
#define PRIdFAST64 __PREFIX_64 "d"

#define PRIdMAX __PREFIX_MAX "d"
#define PRIdPTR __PREFIX_POINTER "d"

#define PRIi8 "i"
#define PRIi16 "i"
#define PRIi32 __PREFIX_32 "i"
#define PRIi64 __PREFIX_64 "i"

#define PRIiLEAST8 "i"
#define PRIiLEAST16 "i"
#define PRIiLEAST32 __PREFIX_32 "i"
#define PRIiLEAST64 __PREFIX_64 "i"

#define PRIiFAST8 "i"
#define PRIiFAST16 "i"
#define PRIiFAST32 __PREFIX_32 "i"
#define PRIiFAST64 __PREFIX_64 "i"

#define PRIiMAX __PREFIX_MAX "i"
#define PRIiPTR __PREFIX_POINTER "i"

// printf unsigned int macros

#define PRIb8 "b"
#define PRIb16 "b"
#define PRIb32 __PREFIX_32 "b"
#define PRIb64 __PREFIX_64 "b"

#define PRIbLEAST8 "b"
#define PRIbLEAST16 "b"
#define PRIbLEAST32 __PREFIX_32 "b"
#define PRIbLEAST64 __PREFIX_64 "b"

#define PRIbFAST8 "b"
#define PRIbFAST16 "b"
#define PRIbFAST32 __PREFIX_32 "b"
#define PRIbFAST64 __PREFIX_64 "b"

#define PRIbMAX __PREFIX_MAX "b"
#define PRIbPTR __PREFIX_POINTER "b"

#define PRIo8 "o"
#define PRIo16 "o"
#define PRIo32 __PREFIX_32 "o"
#define PRIo64 __PREFIX_64 "o"

#define PRIoLEAST8 "o"
#define PRIoLEAST16 "o"
#define PRIoLEAST32 __PREFIX_32 "o"
#define PRIoLEAST64 __PREFIX_64 "o"

#define PRIoFAST8 "o"
#define PRIoFAST16 "o"
#define PRIoFAST32 __PREFIX_32 "o"
#define PRIoFAST64 __PREFIX_64 "o"

#define PRIoMAX __PREFIX_MAX "o"
#define PRIoPTR __PREFIX_POINTER "o"

#define PRIu8 "u"
#define PRIu16 "u"
#define PRIu32 __PREFIX_32 "u"
#define PRIu64 __PREFIX_64 "u"

#define PRIuLEAST8 "u"
#define PRIuLEAST16 "u"
#define PRIuLEAST32 __PREFIX_32 "u"
#define PRIuLEAST64 __PREFIX_64 "u"

#define PRIuFAST8 "u"
#define PRIuFAST16 "u"
#define PRIuFAST32 __PREFIX_32 "u"
#define PRIuFAST64 __PREFIX_64 "u"

#define PRIuMAX __PREFIX_MAX "u"
#define PRIuPTR __PREFIX_POINTER "u"

#define PRIx8 "x"
#define PRIx16 "x"
#define PRIx32 __PREFIX_32 "x"
#define PRIx64 __PREFIX_64 "x"

#define PRIxLEAST8 "x"
#define PRIxLEAST16 "x"
#define PRIxLEAST32 __PREFIX_32 "x"
#define PRIxLEAST64 __PREFIX_64 "x"

#define PRIxFAST8 "x"
#define PRIxFAST16 "x"
#define PRIxFAST32 __PREFIX_32 "x"
#define PRIxFAST64 __PREFIX_64 "x"

#define PRIxMAX __PREFIX_MAX "x"
#define PRIxPTR __PREFIX_POINTER "x"

#define PRIX8 "X"
#define PRIX16 "X"
#define PRIX32 __PREFIX_32 "X"
#define PRIX64 __PREFIX_64 "X"

#define PRIXLEAST8 "X"
#define PRIXLEAST16 "X"
#define PRIXLEAST32 __PREFIX_32 "X"
#define PRIXLEAST64 __PREFIX_64 "X"

#define PRIXFAST8 "X"
#define PRIXFAST16 "X"
#define PRIXFAST32 __PREFIX_32 "X"
#define PRIXFAST64 __PREFIX_64 "X"

#define PRIXMAX __PREFIX_MAX "X"
#define PRIXPTR __PREFIX_POINTER "X"

#define PRIB8 "B"
#define PRIB16 "B"
#define PRIB32 __PREFIX_32 "B"
#define PRIB64 __PREFIX_64 "B"

#define PRIBLEAST8 "B"
#define PRIBLEAST16 "B"
#define PRIBLEAST32 __PREFIX_32 "B"
#define PRIBLEAST64 __PREFIX_64 "B"

#define PRIBFAST8 "B"
#define PRIBFAST16 "B"
#define PRIBFAST32 __PREFIX_32 "B"
#define PRIBFAST64 __PREFIX_64 "B"

#define PRIBMAX __PREFIX_MAX "B"
#define PRIBPTR __PREFIX_POINTER "B"

// scanf signed integer macros

#define SCNd8 "hhd"
#define SCNd16 "hd"
#define SCNd32 __PREFIX_32 "d"
#define SCNd64 __PREFIX_64 "d"

#define SCNdLEAST8 "hhd"
#define SCNdLEAST16 "hd"
#define SCNdLEAST32 __PREFIX_32 "d"
#define SCNdLEAST64 __PREFIX_64 "d"

#define SCNdFAST8 "hhd"
#define SCNdFAST16 "d"
#define SCNdFAST32 __PREFIX_32 "d"
#define SCNdFAST64 __PREFIX_64 "d"

#define SCNdMAX __PREFIX_MAX "d"
#define SCNdPTR __PREFIX_POINTER "d"

#define SCNi8 "hhi"
#define SCNi16 "hi"
#define SCNi32 __PREFIX_32 "i"
#define SCNi64 __PREFIX_64 "i"

#define SCNiLEAST8 "hhi"
#define SCNiLEAST16 "hi"
#define SCNiLEAST32 __PREFIX_32 "i"
#define SCNiLEAST64 __PREFIX_64 "i"

#define SCNiFAST8 "hhi"
#define SCNiFAST16 "i"
#define SCNiFAST32 __PREFIX_32 "i"
#define SCNiFAST64 __PREFIX_64 "i"

#define SCNiMAX __PREFIX_MAX "i"
#define SCNiPTR __PREFIX_POINTER "i"

// scanf unsigned integer macros

#define SCNb8 "hhb"
#define SCNb16 "hb"
#define SCNb32 __PREFIX_32 "b"
#define SCNb64 __PREFIX_64 "b"

#define SCNbLEAST8 "hhb"
#define SCNbLEAST16 "hb"
#define SCNbLEAST32 __PREFIX_32 "b"
#define SCNbLEAST64 __PREFIX_64 "b"

#define SCNbFAST8 "hhb"
#define SCNbFAST16 "b"
#define SCNbFAST32 __PREFIX_32 "b"
#define SCNbFAST64 __PREFIX_64 "b"

#define SCNbMAX __PREFIX_MAX "b"
#define SCNbPTR __PREFIX_POINTER "b"

#define SCNo8 "hho"
#define SCNo16 "ho"
#define SCNo32 __PREFIX_32 "o"
#define SCNo64 __PREFIX_64 "o"

#define SCNoLEAST8 "hho"
#define SCNoLEAST16 "ho"
#define SCNoLEAST32 __PREFIX_32 "o"
#define SCNoLEAST64 __PREFIX_64 "o"

#define SCNoFAST8 "hho"
#define SCNoFAST16 "o"
#define SCNoFAST32 __PREFIX_32 "o"
#define SCNoFAST64 __PREFIX_64 "o"

#define SCNoMAX __PREFIX_MAX "o"
#define SCNoPTR __PREFIX_POINTER "o"

#define SCNu8 "hhu"
#define SCNu16 "hu"
#define SCNu32 __PREFIX_32 "u"
#define SCNu64 __PREFIX_64 "u"

#define SCNuLEAST8 "hhu"
#define SCNuLEAST16 "hu"
#define SCNuLEAST32 __PREFIX_32 "u"
#define SCNuLEAST64 __PREFIX_64 "u"

#define SCNuFAST8 "hhu"
#define SCNuFAST16 "u"
#define SCNuFAST32 __PREFIX_32 "u"
#define SCNuFAST64 __PREFIX_64 "u"

#define SCNuMAX __PREFIX_MAX "u"
#define SCNuPTR __PREFIX_POINTER "u"

#define SCNx8 "hhx"
#define SCNx16 "hx"
#define SCNx32 __PREFIX_32 "x"
#define SCNx64 __PREFIX_64 "x"

#define SCNxLEAST8 "hhx"
#define SCNxLEAST16 "hx"
#define SCNxLEAST32 __PREFIX_32 "x"
#define SCNxLEAST64 __PREFIX_64 "x"

#define SCNxFAST8 "hhx"
#define SCNxFAST16 "x"
#define SCNxFAST32 __PREFIX_32 "x"
#define SCNxFAST64 __PREFIX_64 "x"

#define SCNxMAX __PREFIX_MAX "x"
#define SCNxPTR __PREFIX_POINTER "x"

#endif