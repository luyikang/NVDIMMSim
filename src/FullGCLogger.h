#ifndef NVFULLGCLOGGER_H
#define NVFULLGCLOGGER_H

#include <iostream>
#include <fstream>

#include "FlashConfiguration.h"
#include "GCLogger.h"

namespace NVDSim
{
    class FullGCLogger: public GCLogger
    {
    public:
	FullGCLogger();
	
	// operations
	void gcread();
	void gcwrite();

	void gcread_latency(uint64_t cycles);
	void gcwrite_latency(uint64_t cycles);
	
	void save(uint64_t cycle, uint epoch);
	void print(uint64_t cycle);

	void access_process(uint64_t addr, uint package, ChannelPacketType op);
	void access_stop(uint64_t addr);
	
	// State
	uint64_t gcreads;
	uint64_t gcwrites;
		
	uint64_t average_gcread_latency;
	uint64_t average_gcwrite_latency;
    };
}

#endif
