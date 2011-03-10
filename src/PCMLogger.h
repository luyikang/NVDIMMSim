#ifndef PCMLOGGER_H
#define PCMLOGGER_H

#include <iostream>
#include <fstream>

#include "FlashConfiguration.h"
#include "Logger.h"

namespace NVDSim
{
    class PCMLogger: public Logger
    {
    public:
	PCMLogger(Controller *c);
	
	void save();
	void print();

	void powerCallback();

	void update();

	void access_process(uint64_t addr, uint package, ChannelPacketType op, bool hit);
	void access_stop(uint64_t addr);

	//Accessors for power data
	//Writing correct object oriented code up in this piece, what now?
	vector<double> getVppIdleEnergy(void);
	vector<double> getVppAccessEnergy(void);

	// State
	// Power Stuff
	// This is computed per package
	std::vector<double> vpp_idle_energy;
	std::vector<double> vpp_access_energy;
    };
}

#endif
