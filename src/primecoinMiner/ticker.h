#ifndef TICKER_H
#define TICKER_H

#if (defined(__MACH__) && defined(__APPLE__))
#include <mach/mach_time.h>
#else
#include <time.h>
#endif

#include "inttype.h"

#ifdef _WIN32
#include <Windows.h>
#endif

time_t getTimeMilliseconds(void);

#endif // TICKER_H
