#ifndef NVPCMLOGGER_H
#define NVPCMLOGGER_H

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

	void save_epoch(uint64_t cycle, uint epoch);

	// State
	// Power Stuff
	// This is computed per package
	std::vector<double> vpp_idle_energy;
	std::vector<double> vpp_access_energy;

	class EpochEntry
	{
	public:
	    uint64_t cycle;
	    uint64_t epoch;

	    uint64_t num_accesses;
	    uint64_t num_reads;
	    uint64_t num_writes;

	    uint64_t num_unmapped;
	    uint64_t num_mapped;

	    uint64_t num_read_unmapped;
	    uint64_t num_read_mapped;
	    uint64_t num_write_unmapped;
	    uint64_t num_write_mapped;
		
	    uint64_t average_latency;
	    uint64_t average_read_latency;
	    uint64_t average_write_latency;
	    uint64_t average_queue_latency;

	    uint64_t ftl_queue_length;
	    std::vector<uint64_t> ctrl_queue_length;

	    std::vector<double> idle_energy;
	    std::vector<double> access_energy;

	    std::vector<double> vpp_idle_energy;
	    std::vector<double> vpp_access_energy;

	    EpochEntry()
	    {
		num_accesses = 0;
		num_reads = 0;
		num_writes = 0;

		num_unmapped = 0;
		num_mapped = 0;

		num_read_unmapped = 0;
		num_read_mapped = 0;
		num_write_unmapped = 0;
		num_write_mapped = 0;
		
		average_latency = 0;
		average_read_latency = 0;
		average_write_latency = 0;
		average_queue_latency = 0;
		
		ftl_queue_length = 0;
		ctrl_queue_length = std::vector<uint64_t>(NUM_PACKAGES, 0);

		idle_energy = std::vector<double>(NUM_PACKAGES, 0.0); 
		access_energy = std::vector<double>(NUM_PACKAGES, 0.0); 

		vpp_idle_energy = std::vector<double>(NUM_PACKAGES, 0.0); 
		vpp_access_energy = std::vector<double>(NUM_PACKAGES, 0.0); 
	    }
	};

	// Store system snapshot from last epoch to compute this epoch
	EpochEntry last_epoch;

	// Store the data from each epoch for printing at the end of the simulation
	std::list<EpochEntry> epoch_queue;
    };
}

#endif
