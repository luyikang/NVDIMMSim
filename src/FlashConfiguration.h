#ifndef FLASHCONF_H
#define FLASHCONF_H
//SysemConfiguration.h
//Configuration values, headers, and macros for the whole system
//
#include <iostream>
#include <cstdlib>
#include <string>

#include <vector>
#include <unordered_map>
#include <queue>
#include <list>
#include <stdint.h>
#include <limits.h>

//sloppily reusing #defines from dramsim
#ifndef ERROR
#define ERROR(str) std::cerr<<"[ERROR ("<<__FILE__<<":"<<__LINE__<<")]: "<<str<<std::endl;
#endif

#ifndef WARNING
#define WARNING(str) std::cout<<"[WARNING ("<<__FILE__<<":"<<__LINE__<<")]: "<<str<<std::endl;
#endif

#ifdef DEBUG_BUILD
	#ifndef DEBUG
	#define DEBUG(str) std::cout<< str <<std::endl;
	#endif
	#ifndef DEBUGN
	#define DEBUGN(str) std::cout<< str;
	#endif
#else
	#ifndef DEBUG
	#define DEBUG(str) ;
	#endif
	#ifndef DEBUGN
	#define DEBUGN(str) ;
	#endif
#endif

#ifndef NO_OUTPUT
	#ifndef PRINT
	#define PRINT(str)  if(OUTPUT) { std::cerr <<str<<std::endl; }
	#endif
	#ifndef PRINTN
	#define PRINTN(str) if(OUTPUT) { std::cerr <<str; }
	#endif
#else
	#undef DEBUG
	#undef DEBUGN
	#define DEBUG(str) ;
	#define DEBUGN(str) ;
	#ifndef PRINT
	#define PRINT(str) ;
	#endif
	#ifndef PRINTN
	#define PRINTN(str) ;
	#endif
#endif

extern std::string DEVICE_TYPE;
extern uint NUM_PACKAGES;
extern uint DIES_PER_PACKAGE;
extern uint PLANES_PER_DIE;
extern uint BLOCKS_PER_PLANE;
extern uint PAGES_PER_BLOCK;
extern uint NV_PAGE_SIZE;
extern uint WORDS_PER_PAGE;
extern uint READ_SIZE;
extern uint WRITE_SIZE;

// everything larger than Blocks are stored in kilobytes, below that they're stored as bytes
#define NV_WORD_SIZE ((NV_PAGE_SIZE*1024) / WORDS_PER_PAGE)
#define BLOCK_SIZE (NV_PAGE_SIZE * PAGES_PER_BLOCK)
#define PLANE_SIZE (NV_PAGE_SIZE * BLOCKS_PER_PLANE * PAGES_PER_BLOCK)
#define DIE_SIZE (NV_PAGE_SIZE * PLANES_PER_DIE * BLOCKS_PER_PLANE * PAGES_PER_BLOCK)
#define PACKAGE_SIZE (NV_PAGE_SIZE * DIES_PER_PACKAGE * PLANES_PER_DIE * BLOCKS_PER_PLANE * PAGES_PER_BLOCK)
#define TOTAL_SIZE (NV_PAGE_SIZE * NUM_PACKAGES * DIES_PER_PACKAGE * PLANES_PER_DIE * BLOCKS_PER_PLANE * PAGES_PER_BLOCK)

extern uint READ_TIME;
extern uint WRITE_TIME;
extern uint ERASE_TIME;
extern uint DATA_TIME;
extern uint COMMAND_TIME;
extern uint LOOKUP_TIME;

extern uint OUTPUT;

namespace NVDSim{
	typedef void (*returnCallBack_t)(uint id, uint64_t addr, uint64_t clockcycle);
}
#endif
