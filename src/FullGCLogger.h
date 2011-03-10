#ifndef FULLGCLOGGER_H
#define FULLGCLOGGER_H

#include <iostream>
#include <fstream>

#include "FlashConfiguration.h"
#include "GCLogger.h"

namespace NVDSim
{
    class FullGCLogger: public GCLogger
    {
    public:
	FullGCLogger(Controller *c);
	
	// operations
	void gcread();
	void gcwrite();

	void gcread_latency(uint64_t cycles);
	void gcwrite_latency(uint64_t cycles);
	
	void save();
	void print();

	void access_process(uint64_t addr, uint package, ChannelPacketType op, bool hit);
	void access_stop(uint64_t addr);
	
	// State
	uint64_t gcreads;
	uint64_t gcwrites;
		
	uint64_t average_gcread_latency;
	uint64_t average_gcwrite_latency;
    };
}

#endif
