#ifndef __JHSYSTEMLIB
#define __JHSYSTEMLIB

#include "../inttype.h"

#ifdef _WIN32
#define NOMINMAX
#include<Windows.h>
#else
#include <signal.h>
#endif
#include <cstring> // for memcpy/memset
#include<math.h>
#include <algorithm>


#ifndef _WIN32 // temporary replacement for _ADDRESSOF, replace with boost
template< class T >
T* tmp_addressof(T& arg) {
    return reinterpret_cast<T*>(
               &const_cast<char&>(
                  reinterpret_cast<const volatile char&>(arg)));
}
#endif
/*
typedef unsigned long long 	uint64_t;
typedef signed long long	int64_t;

typedef unsigned int 	uint32_t;
typedef signed int 		int32_t;

typedef unsigned short 	uint16_t;
typedef signed short 	int16_t;

typedef unsigned char 	uint8_t;
typedef signed char 	int8_t;*/

#define JHCALLBACK	__fastcall


void* _ex1_malloc(int size);
void* _ex1_realloc(void* old, int size);
void _ex1_free(void* p);

void _ex2_initialize();

void* _ex2_malloc(int size, char* file, int32_t line);
void* _ex2_realloc(void* old, int size, char* file, int32_t line);
void _ex2_free(void* p, char* file, int32_t line);
void _ex2_analyzeMemoryLog();

// memory validator
//#define malloc(x) _ex1_malloc(x)
//#define realloc(x,y) _ex1_realloc(x,y)
//#define free(x) _ex1_free(x)

// memory logger
//#define MEMORY_LOGGER_ACTIVE			1
//#define MEMORY_LOGGER_ANALYZE_ACTIVE	1

#ifdef MEMORY_LOGGER_ACTIVE
#define malloc(x) _ex2_malloc(x,__FILE__,__LINE__)
#define realloc(x,y) _ex2_realloc(x,y,__FILE__,__LINE__)
#define free(x) _ex2_free(x,__FILE__,__LINE__)
#endif

/*#include".\streamWrapper.h"
#include".\fastString.h"
#include".\hashTable.h"
#include".\fastSorter.h"
#include".\fileMgr.h"
#include".\sData.h"
#include".\bmp.h"
#include".\tgaLib.h"
#include".\fMath.h"
#include".\packetBuffer.h"
#include".\msgQueue.h"
#include".\simpleList.h"
#include".\customBuffer.h"*/

#include"streamWrapper.h"
#include"fastString.h"
#include"hashTable.h"
#ifdef _WIN32
#include"fMath.h"
#include"sData.h"
#endif
#include"fastSorter.h"
#include"fileMgr.h"
#include"bmp.h"
#include"tgaLib.h"
#include"msgQueue.h"
#include"packetBuffer.h"
#include"simpleList.h"
#include"customBuffer.h"


/* error */
#define assertFatal(condition, message, errorCode) if( condition ) { OutputDebugString(message);  ExitProcess(errorCode); } 


#endif

