#ifndef GCLOGGER_H
#define GCLOGGER_H

#include <iostream>
#include <fstream>

#include "FlashConfiguration.h"
#include "Logger.h"

namespace NVDSim
{
    class GCLogger: public Logger
    {
    public:
	GCLogger(Controller *c);
	
	// operations
	void erase();

	void erase_latency(uint64_t cycles);
	
	void save(uint64_t cycle, uint epoch);
	void print(uint64_t cycle);

	void powerCallback();

	void update();

	void access_process(uint64_t addr, uint package, ChannelPacketType op, bool hit);
	void access_stop(uint64_t addr);

	//Accessors for power data
	//Writing correct object oriented code up in this piece, what now?
	vector<double> getEraseEnergy(void);
	
	// State
	uint64_t num_erases;
		
	uint64_t average_erase_latency;

	// Power Stuff
	// This is computed per package
	std::vector<double> erase_energy;
    };
}

#endif
