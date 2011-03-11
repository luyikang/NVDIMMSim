#ifndef PCMLOGGER_H
#define PCMLOGGER_H

#include <iostream>
#include <fstream>
#include <vector>

#include "FlashConfiguration.h"
#include "Logger.h"

namespace NVDSim
{
    class PCMLogger: public Logger
    {
    public:
	PCMLogger();
	
	void save(uint64_t cycle, uint epoch);
	void print(uint64_t cycle);

	void update();

	void access_process(uint64_t addr, uint package, ChannelPacketType op);
	void access_stop(uint64_t addr);

	//Accessors for power data
	//Writing correct object oriented code up in this piece, what now?
	std::vector<std::vector<double>> getEnergyData(void);

	// State
	// Power Stuff
	// This is computed per package
	std::vector<double> vpp_idle_energy;
	std::vector<double> vpp_access_energy;
    };
}

#endif
