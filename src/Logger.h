#ifndef NVLOGGER_H
#define NVLOGGER_H

#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <queue>
#include <list>

#include "SimObj.h"
#include "FlashConfiguration.h"
#include "ChannelPacket.h"

namespace NVDSim
{
    class Logger: public SimObj
    {
    public:
	Logger();
	
	// operations
	void read();
	void write();
	void mapped();
	void unmapped();

	void read_mapped();
	void read_unmapped();
	void write_mapped();
	void write_unmapped();

	void read_latency(uint64_t cycles);
	void write_latency(uint64_t cycles);
	void queue_latency(uint64_t cycles);

	double unmapped_rate();
	double read_unmapped_rate();
	double write_unmapped_rate();
	double calc_throughput(uint64_t cycles, uint64_t accesses);

	double divide(double num, double denom);

	void ftlQueueLength(uint64_t length);
	void ctrlQueueLength(std::vector<uint64_t> length);

	void ftlQueueReset();
	void ctrlQueueReset();

	//Accessor for power data
	//Writing correct object oriented code up in this piece, what now?
	virtual std::vector<std::vector<double>> getEnergyData(void);
	
	virtual void save(uint64_t cycle, uint epoch);
	virtual void print(uint64_t cycle);

	virtual void update();
	
	void access_start(uint64_t addr);
	virtual void access_process(uint64_t addr, uint64_t paddr, uint package, ChannelPacketType op);
	virtual void access_stop(uint64_t addr);

	virtual void save_epoch(uint64_t cycle, uint epoch);
	
	// State
	std::ofstream savefile;

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

	uint64_t max_ftl_queue_length;
	std::vector<uint64_t> max_ctrl_queue_length;

	std::unordered_map<uint64_t, uint64_t> writes_per_address;

	// Power Stuff
	// This is computed per package
	std::vector<double> idle_energy;
	std::vector<double> access_energy;


	class AccessMapEntry
	{
		public:
		uint64_t start; // Starting cycle of access
		uint64_t process; // Cycle when processing starts
		uint64_t stop; // Stopping cycle of access
		uint64_t pAddr; // Virtual address of access
		uint64_t package; // package for the power calculations
		ChannelPacketType op; // what operation is this?
		AccessMapEntry()
		{
			start = 0;
			process = 0;
			stop = 0;
			pAddr = 0;
			package = 0;
			op = READ;
		}
	};

	// Store access info while the access is being processed.
	std::unordered_map<uint64_t, AccessMapEntry> access_map;

	// Store the address and arrival time while access is waiting to be processed.
	// Must do this because duplicate addresses may arrive close together.
	std::list<std::pair <uint64_t, uint64_t>> access_queue;

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
	    
	    std::unordered_map<uint64_t, uint64_t> writes_per_address;

	    std::vector<double> idle_energy;
	    std::vector<double> access_energy;

	    EpochEntry()
	    {
		cycle = 0;
		epoch = 0;

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
	    }
	};

	// Store system snapshot from last epoch to compute this epoch
	EpochEntry last_epoch;

	// Store the data from each epoch for printing at the end of the simulation
	std::list<EpochEntry> epoch_queue;

	virtual void write_epoch(EpochEntry *e);
    };
}

#endif
