#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <fstream>

#include "FlashConfiguration.h"
#include "ChannelPacket.h"

namespace NVDSim
{
    class Logger: public SimulatorObject
    {
    public:
	Logger(Controller *c);
	
	// operations
	void read();
	void write();

	void read_hit();
	void read_miss();
	void write_hit();
	void write_miss();

	void read_latency(uint64_t cycles);
	void write_latency(uint64_t cycles);

	double miss_rate();
	double read_miss_rate();
	double write_miss_rate();

	//Accessors for power data
	//Writing correct object oriented code up in this piece, what now?
	vector<double> getIdleEnergy(void);
	vector<double> getAccessEnergy(void);
	
	virtual void save(uint64_t cycle, uint epoch);
	virtual void print(uint64_t cycle);

	virtual void powerCallback(void); 

	virtual void update();
	
	void access_start(uint64_t addr);
	virtual void access_process(uint64_t addr, uint package, ChannelPacketType op, bool hit);
	virtual void access_stop(uint64_t addr);

	Controller *controller;
	
	// State
	std::ofstream savefile;

	uint64_t num_accesses;
	uint64_t num_reads;
	uint64_t num_writes;

	uint64_t num_misses;
	uint64_t num_hits;

	uint64_t num_read_misses;
	uint64_t num_read_hits;
	uint64_t num_write_misses;
	uint64_t num_write_hits;
		
	uint64_t average_latency;
	uint64_t average_read_latency;
	uint64_t average_write_latency;
	uint64_t average_queue_latency;

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
		ChannelPacketType op; // what operation is this?
		bool hit; // Is this a hit?
		AccessMapEntry()
		{
			start = 0;
			process = 0;
			stop = 0;
			read_op = false;
			hit = false;
		}
	};

	// Store access info while the access is being processed.
	unordered_map<uint64_t, AccessMapEntry> access_map;

	// Store the address and arrival time while access is waiting to be processed.
	// Must do this because duplicate addresses may arrive close together.
	list<pair <uint64_t, uint64_t>> access_queue;
    };
}

#endif
