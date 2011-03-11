#include "FullGCLogger.h"

using namespace NVDSim;
using namespace std;

FullGCLogger::FullGCLogger()
  : GCLogger()
{
    	gcreads = 0;
	gcwrites = 0;

	average_gcread_latency = 0;
	average_gcwrite_latency = 0;
}

// Using virtual addresses here right now
void FullGCLogger::access_process(uint64_t addr, uint package, ChannelPacketType op)
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
		cerr << "ERROR: NVLogger.access_process() called with address not in the access_queue. address=0x" << hex << addr << "\n" << dec;
		abort();
	}

	if (access_map.count(addr) != 0)
	{
		cerr << "ERROR: NVLogger.access_process() called with address already in access_map. address=0x" << hex << addr << "\n" << dec;
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
	else if (op == ERASE)
	{
	    //update access energy figures
	    erase_energy[package] += (ERASE_I - STANDBY_I) * ERASE_TIME/2;
	    this->erase();
	}
	else if (op == GC_READ)
	{
	    //update access energy figures
	    access_energy[package] += (READ_I - STANDBY_I) * READ_TIME/2;
	    this->gcread();
	}
	else if (op == GC_WRITE)
	{
	    //update access energy figures
	    access_energy[package] += (WRITE_I - STANDBY_I) * WRITE_TIME/2;
	    this->gcwrite();
	}
}

void FullGCLogger::access_stop(uint64_t addr)
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
	else if (a.op == WRITE)
	        this->write_latency(a.stop - a.start);
	else if (a.op == ERASE)
		this->erase_latency(a.stop - a.start);
	else if (a.op == GC_READ)
		this->gcread_latency(a.stop - a.start);
	else if (a.op == GC_WRITE)
	        this->gcwrite_latency(a.stop - a.start);
		
	access_map.erase(addr);
}

void FullGCLogger::gcread()
{
	gcreads += 1;
}

void FullGCLogger::gcwrite()
{
	gcwrites += 1;
}

void FullGCLogger::gcread_latency(uint64_t cycles)
{
	// Need to calculate a running average of latency.
}

void FullGCLogger::gcwrite_latency(uint64_t cycles)
{
	// Need to calculate a running average of latency.
}

void FullGCLogger::save(uint64_t cycle, uint epoch) {
        // Power stuff
	// Total power used
	vector<double> total_energy = vector<double>(NUM_PACKAGES, 0.0); 
	
        // Average power used
	vector<double> ave_idle_power = vector<double>(NUM_PACKAGES, 0.0);
	vector<double> ave_access_power = vector<double>(NUM_PACKAGES, 0.0);
	vector<double> ave_erase_power = vector<double>(NUM_PACKAGES, 0.0);
	vector<double> average_power = vector<double>(NUM_PACKAGES, 0.0);

	for(uint i = 0; i < NUM_PACKAGES; i++)
	{
	  total_energy[i] = (idle_energy[i] + access_energy[i] + erase_energy[i]) * VCC;
	  ave_idle_power[i] = (idle_energy[i] * VCC) / cycle;
	  ave_access_power[i] = (access_energy[i] * VCC) / cycle;
	  ave_erase_power[i] = (erase_energy[i] * VCC) / cycle;	  
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
	savefile<<"Erases completed: "<<num_erases<<"\n";
	savefile<<"GC Reads completed: "<<gcreads<<"\n";
	savefile<<"GC Writes completed: "<<gcwrites<<"\n";
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
	    savefile<<"Accumulated Erase Energy: "<<(erase_energy[i] * VCC * (CYCLE_TIME * 0.000000001))<<"mJ\n";
	    
	    savefile<<"Total Energy: "<<(total_energy[i] * (CYCLE_TIME * 0.000000001))<<"mJ\n\n";
	 
	    savefile<<"Average Idle Power: "<<ave_idle_power[i]<<"mW\n";
	    savefile<<"Average Access Power: "<<ave_access_power[i]<<"mW\n";
	    savefile<<"Average Erase Power: "<<ave_erase_power[i]<<"mW\n";

	    savefile<<"Average Power: "<<average_power[i]<<"mW\n\n";
	}

	savefile.close();
}

void FullGCLogger::print(uint64_t cycle) {
	// Power stuff
	// Total power used
	vector<double> total_energy = vector<double>(NUM_PACKAGES, 0.0); 
	
        // Average power used
	vector<double> ave_idle_power = vector<double>(NUM_PACKAGES, 0.0);
	vector<double> ave_access_power = vector<double>(NUM_PACKAGES, 0.0);
	vector<double> ave_erase_power = vector<double>(NUM_PACKAGES, 0.0);
	vector<double> average_power = vector<double>(NUM_PACKAGES, 0.0);

	for(uint i = 0; i < NUM_PACKAGES; i++)
	{
	  total_energy[i] = (idle_energy[i] + access_energy[i] + erase_energy[i]) * VCC;
	  ave_idle_power[i] = (idle_energy[i] * VCC) / cycle;
	  ave_access_power[i] = (access_energy[i] * VCC) / cycle;
	  ave_erase_power[i] = (erase_energy[i] * VCC) / cycle;	  
	  average_power[i] = total_energy[i] / cycle;
	}

	cout<<"Reads completed: "<<num_reads<<"\n";
	cout<<"Writes completed: "<<num_writes<<"\n";
	cout<<"Erases completed: "<<num_erases<<"\n";

	cout<<"\nPower Data: \n";
	cout<<"========================\n";

	for(uint i = 0; i < NUM_PACKAGES; i++)
	{
	    cout<<"Package: "<<i<<"\n";
	    cout<<"Accumulated Idle Energy: "<<(idle_energy[i] * VCC * (CYCLE_TIME * 0.000000001))<<"mJ\n";
	    cout<<"Accumulated Access Energy: "<<(access_energy[i] * VCC * (CYCLE_TIME * 0.000000001))<<"mJ\n";
	    cout<<"Accumulated Erase Energy: "<<(erase_energy[i] * VCC * (CYCLE_TIME * 0.000000001))<<"mJ\n";
	    
	    cout<<"Total Energy: "<<(total_energy[i] * (CYCLE_TIME * 0.000000001))<<"mJ\n\n";
	 
	    cout<<"Average Idle Power: "<<ave_idle_power[i]<<"mW\n";
	    cout<<"Average Access Power: "<<ave_access_power[i]<<"mW\n";
	    cout<<"Average Erase Power: "<<ave_erase_power[i]<<"mW\n";

	    cout<<"Average Power: "<<average_power[i]<<"mW\n\n";
	}
}
