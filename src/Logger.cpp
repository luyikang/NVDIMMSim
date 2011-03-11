#include "Logger.h"

using namespace NVDSim;
using namespace std;

Logger::Logger()
{
    	num_accesses = 0;
	num_reads = 0;
	num_writes = 0;

	num_misses = 0;
	num_hits = 0;

	num_read_misses = 0;
	num_read_hits = 0;
	num_write_misses = 0;
	num_write_hits = 0;

	average_latency = 0;
	average_read_latency = 0;
	average_write_latency = 0;
	average_queue_latency = 0;

	idle_energy = vector<double>(NUM_PACKAGES, 0.0); 
	access_energy = vector<double>(NUM_PACKAGES, 0.0); 
}

void Logger::update()
{
    	//update idle energy
	//since this is already subtracted from the access energies we just do it every time
	for(uint i = 0; i < (NUM_PACKAGES); i++)
	{
	  idle_energy[i] += STANDBY_I;
	}

	this->step();
}

void Logger::access_start(uint64_t addr)
{
	access_queue.push_back(pair <uint64_t, uint64_t>(addr, currentClockCycle));
}

// Using virtual addresses here right now
void Logger::access_process(uint64_t addr, uint package, ChannelPacketType op)
{
        // Get entry off of the access_queue.
	uint64_t start_cycle = 0;
	bool found = false;
	list<pair <uint64_t, uint64_t>>::iterator it;
	for (it = access_queue.begin(); it != access_queue.end(); it++)
	{
		uint64_t cur_addr = (*it).first;
		uint64_t cur_cycle = (*it).second;

		if (cur_addr == addr)
		{
			start_cycle = cur_cycle;
			found = true;
			access_queue.erase(it);
			break;
		}
	}

	if (!found)
	{
		cerr << "ERROR: Logger.access_process() called with address not in the access_queue. address=0x" << hex << addr << "\n" << dec;
		abort();
	}

	if (access_map.count(addr) != 0)
	{
		cerr << "ERROR: Logger.access_process() called with address already in access_map. address=0x" << hex << addr << "\n" << dec;
		abort();
	}

	AccessMapEntry a;
	a.start = start_cycle;
	a.op = op;
	a.process = this->currentClockCycle;
	access_map[addr] = a;

	// Log cache event type.
	if (op == READ)
	{
	    //update access energy figures
	    access_energy[package] += (READ_I - STANDBY_I) * READ_TIME/2;
	    this->read();
	}
	else if (op == WRITE)
	{
	    //update access energy figures
	    access_energy[package] += (WRITE_I - STANDBY_I) * WRITE_TIME/2;
	    this->write();
	}
}

void Logger::access_stop(uint64_t addr)
{
	if (access_map.count(addr) == 0)
	{
		cerr << "ERROR: Logger.access_stop() called with address not in access_map. address=" << hex << addr << "\n" << dec;
		abort();
	}

	AccessMapEntry a = access_map[addr];
	a.stop = this->currentClockCycle;
	access_map[addr] = a;

	if (a.op == READ)
		this->read_latency(a.stop - a.start);
	else
		this->write_latency(a.stop - a.start);
		
	access_map.erase(addr);
}

void Logger::read()
{
	num_accesses += 1;
	num_reads += 1;
}

void Logger::write()
{
	num_accesses += 1;
	num_writes += 1;
}

void Logger::hit()
{
	num_hits += 1;
}

void Logger::miss()
{
	num_misses += 1;
}

void Logger::read_hit()
{
	this->hit();
	num_read_hits += 1;
}

void Logger::read_miss()
{
	this->miss();
	num_read_misses += 1;
}

void Logger::write_hit()
{
	this->hit();
	num_write_hits += 1;
}

void Logger::write_miss()
{
	this->miss();
	num_write_misses += 1;
}

void Logger::read_latency(uint64_t cycles)
{
	// Need to calculate a running average of latency.
}

void Logger::write_latency(uint64_t cycles)
{
	// Need to calculate a running average of latency.
}

void Logger::queue_latency(uint64_t cycles)
{
	// Need to calculate a running average of latency.
}

double Logger::miss_rate()
{
	return (double)num_misses / num_accesses;
}

double Logger::read_miss_rate()
{
	return (double)num_read_misses / num_reads;
}

double Logger::write_miss_rate()
{
	return (double)num_write_misses / num_writes;
}

void Logger::save(uint64_t cycle, uint epoch) 
{
        // Power stuff
	// Total power used
	vector<double> total_energy = vector<double>(NUM_PACKAGES, 0.0);
	
        // Average power used
	vector<double> ave_idle_power = vector<double>(NUM_PACKAGES, 0.0);
	vector<double> ave_access_power = vector<double>(NUM_PACKAGES, 0.0);
	vector<double> average_power = vector<double>(NUM_PACKAGES, 0.0);

	for(uint i = 0; i < NUM_PACKAGES; i++)
	{
	    total_energy[i] = (idle_energy[i] + access_energy[i]) * VCC;
	    ave_idle_power[i] = (idle_energy[i] * VCC) / cycle;
	    ave_access_power[i] = (access_energy[i] * VCC) / cycle;
	    average_power[i] = total_energy[i] / cycle;
	}

	if(USE_EPOCHS && epoch > 0)
	{
	    savefile.open("NVDIMM.log", ios_base::out | ios_base::app);
	    savefile<<"\nData for Epoch "<<epoch<<"\n";
	    savefile<<"========================\n";
	    savefile<<"\nSimulation Data: \n";
	    savefile<<"========================\n";
	}
	else if(USE_EPOCHS)
	{
	    savefile.open("NVDIMM.log", ios_base::out | ios_base::trunc);
	    savefile<<"NVDIMM Log \n";
	    savefile<<"\nData for Epoch "<<epoch<<"\n";
	    savefile<<"========================\n";
	    savefile<<"\nSimulation Data: \n";
	    savefile<<"========================\n";
	}
	else
	{
	    savefile.open("NVDIMM.log", ios_base::out | ios_base::trunc);
	    savefile<<"NVDIMM Log \n";
	    savefile<<"\nSimulation Data: \n";
	    savefile<<"========================\n";
	}

	if (!savefile) 
	{
	    ERROR("Cannot open PowerStats.log");
	    exit(-1); 
	}

	savefile<<"Cycles Simulated: "<<cycle<<"\n";
	savefile<<"Accesses: "<<num_accesses<<"\n";
        savefile<<"Reads completed: "<<num_reads<<"\n";
	savefile<<"Writes completed: "<<num_writes<<"\n";
	savefile<<"Number of Misses: " <<num_misses<<"\n";
	savefile<<"Number of Hits: " <<num_hits<<"\n";
	savefile<<"Number of Read Misses: " <<num_read_misses<<"\n";
	savefile<<"Number of Read Hits: " <<num_read_hits<<"\n";
	savefile<<"Number of Write Misses: " <<num_write_misses<<"\n";
	savefile<<"Number of Write Hits: " <<num_write_hits<<"\n";
	savefile<<"Miss Rate: " <<miss_rate()<<"\n";
	savefile<<"Read Miss Rate: " <<read_miss_rate()<<"\n";
	savefile<<"Write Miss Rate: " <<write_miss_rate()<<"\n";

	savefile<<"\nPower Data: \n";
	savefile<<"========================\n";

	for(uint i = 0; i < NUM_PACKAGES; i++)
	{
	    savefile<<"Package: "<<i<<"\n";
	    savefile<<"Accumulated Idle Energy: "<<(idle_energy[i] * VCC * (CYCLE_TIME * 0.000000001))<<"mJ\n";
	    savefile<<"Accumulated Access Energy: "<<(access_energy[i] * VCC * (CYCLE_TIME * 0.000000001))<<"mJ\n";
	    savefile<<"Total Energy: "<<(total_energy[i] * (CYCLE_TIME * 0.000000001))<<"mJ\n\n";
	 
	    savefile<<"Average Idle Power: "<<ave_idle_power[i]<<"mW\n";
	    savefile<<"Average Access Power: "<<ave_access_power[i]<<"mW\n";
	    savefile<<"Average Power: "<<average_power[i]<<"mW\n\n";
	}

	savefile.close();
}

void Logger::print(uint64_t cycle) 
{
        // Power stuff
	// Total power used
	vector<double> total_energy = vector<double>(NUM_PACKAGES, 0.0);
	
        // Average power used
	vector<double> ave_idle_power = vector<double>(NUM_PACKAGES, 0.0);
	vector<double> ave_access_power = vector<double>(NUM_PACKAGES, 0.0);
	vector<double> average_power = vector<double>(NUM_PACKAGES, 0.0);

	for(uint i = 0; i < NUM_PACKAGES; i++)
	{
	    total_energy[i] = (idle_energy[i] + access_energy[i]) * VCC;
	    ave_idle_power[i] = (idle_energy[i] * VCC) / cycle;
	    ave_access_power[i] = (access_energy[i] * VCC) / cycle;
	    average_power[i] = total_energy[i] / cycle;
	}

	cout<<"Reads completed: "<<num_reads<<"\n";
	cout<<"Writes completed: "<<num_writes<<"\n";

	cout<<"\nPower Data: \n";
	cout<<"========================\n";

	for(uint i = 0; i < NUM_PACKAGES; i++)
	{
	    cout<<"Package: "<<i<<"\n";
	    cout<<"Accumulated Idle Energy: "<<(idle_energy[i] * VCC * (CYCLE_TIME * 0.000000001))<<"mJ\n";
	    cout<<"Accumulated Access Energy: "<<(access_energy[i] * VCC * (CYCLE_TIME * 0.000000001))<<"mJ\n";
	    cout<<"Total Energy: "<<(total_energy[i] * (CYCLE_TIME * 0.000000001))<<"mJ\n\n";
	 
	    cout<<"Average Idle Power: "<<ave_idle_power[i]<<"mW\n";
	    cout<<"Average Access Power: "<<ave_access_power[i]<<"mW\n";
	    cout<<"Average Power: "<<average_power[i]<<"mW\n\n";
	}
}

vector<vector<double> > Logger::getEnergyData(void)
{
    vector<vector<double> > temp = vector<vector<double> >(2, vector<double>(NUM_PACKAGES, 0.0));
    temp[0] = idle_energy;
    temp[1] = access_energy;
    return temp;
}
