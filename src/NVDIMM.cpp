//NVDIMM.cpp
//Class file for nonvolatile memory dimm system wrapper

#include "NVDIMM.h"
#include "Init.h"

using namespace NVDSim;
using namespace std;

uint BLOCKS_PER_PLANE;

NVDIMM::NVDIMM(uint id, string deviceFile, string sysFile, string pwd, string trc) :
	dev(deviceFile),
	sys(sysFile),
	cDirectory(pwd)
	{
	uint i, j;
	systemID = id;
	
	 if (cDirectory.length() > 0)
	 {
		 //ignore the pwd argument if the argument is an absolute path
		 if (dev[0] != '/')
		 {
		 dev = pwd + "/" + dev;
		 }
		 
		if (sys[0] != '/')
		 {
		 sys = pwd + "/" + sys;
		 }
	}
	Init::ReadIniFile(dev, false);
	//Init::ReadIniFile(sys, true);

	 if (!Init::CheckIfAllSet())
	 {
		 exit(-1);
	 }
	
	BLOCKS_PER_PLANE = (uint) VIRTUAL_BLOCKS_PER_PLANE * PBLOCKS_PER_VBLOCK;
	PRINT("\nDevice Information:\n");
	PRINT("Device Type: "<<DEVICE_TYPE);
	PRINT("Size (GB): "<<TOTAL_SIZE/(1024*1024));
	PRINT("Block Size: "<<BLOCK_SIZE);
	PRINT("Plane Size: "<<PLANE_SIZE);
	PRINT("Die Size: "<<DIE_SIZE);
	PRINT("Package Size: "<<PACKAGE_SIZE);
	PRINT("Total Size: "<<TOTAL_SIZE);
	PRINT("Packages/Channels: "<<NUM_PACKAGES);
	PRINT("Page size (KB): "<<NV_PAGE_SIZE);
	if(GARBAGE_COLLECT == 1)
	{
	  PRINT("Device is using garbage collection");
	}
	else
	{
	  PRINT("Device is not using garbage collection");
	}
	PRINT("\nTiming Info:\n");
	PRINT("Read time: "<<READ_TIME);
	PRINT("Write Time: "<<WRITE_TIME);
	PRINT("Erase time: "<<ERASE_TIME);
	PRINT("Channel latency for data: "<<DATA_TIME);
	PRINT("Channel latency for a command: "<<COMMAND_TIME);
	if(USE_EPOCHS == 1)
	{
	    PRINT("Device is using epoch data logging");
	}
	PRINT("Epoch Time: "<<EPOCH_TIME);
	PRINT("");

	if(GARBAGE_COLLECT == 0 && (DEVICE_TYPE.compare("NAND") == 0 || DEVICE_TYPE.compare("NOR") == 0))
	{
	  ERROR("Device is Flash and must use garbage collection");
	  exit(-1);
	}

	if(DEVICE_TYPE.compare("PCM") == 0)
	  {
	    if(ASYNC_READ_I == 0.0)
	      {
		WARNING("No asynchronous read current supplied, using 0.0");
	      }
	    else if(VPP_STANDBY_I == 0.0)
	       {
		PRINT("VPP standby current data missing for PCM, using 0.0");
	      }
	    else if(VPP_READ_I == 0.0)
	      {
		PRINT("VPP read current data missing for PCM, using 0.0");
	      }
	    else if(VPP_WRITE_I == 0.0)
	       {
		PRINT("VPP write current data missing for PCM, using 0.0");
	      }
	    else if(VPP_ERASE_I == 0.0)
	       {
		PRINT("VPP erase current data missing for PCM, using 0.0");
	      }
	    else if(VPP == 0.0)
	      {
		PRINT("VPP power data missing for PCM, using 0.0");
	      }
	  }

	controller= new Controller(this);
	packages= new vector<Package>();

	if (DIES_PER_PACKAGE > INT_MAX){
		ERROR("Too many dies.");
		exit(1);
	}

	// sanity checks
	

	for (i= 0; i < NUM_PACKAGES; i++){
		Package pack = {new Channel(), vector<Die *>()};
		//pack.channel= new Channel();
		pack.channel->attachController(controller);
		for (j= 0; j < DIES_PER_PACKAGE; j++){
			Die *die= new Die(this, j);
			die->attachToChannel(pack.channel);
			pack.channel->attachDie(die);
			pack.dies.push_back(die);
		}
		packages->push_back(pack);
	}
	controller->attachPackages(packages);

	if(DEVICE_TYPE.compare("PCM") == 0 && GARBAGE_COLLECT == 1 && FULL_LOGGING == 1)
	{
	  ftl = new PCMGCFtl(controller);
	  log = new PCMFullGCLogger(controller);
	}
	else if(DEVICE_TYPE.compare("PCM") == 0 && GARBAGE_COLLECT == 1)
	{
	  ftl = new PCMGCFtl(controller);
	  log = new PCMGCLogger(controller);
	}
	else if(DEVICE_TYPE.compare("PCM") == 0 && GARBAGE_COLLECT == 0)
	{
	  ftl = new PCMFtl(controller);
	  log = new PCMLogger(controller);
	}
	else if(GARBAGE_COLLECT == 1 && FULL_LOGGING == 1)
	{
	  ftl = new GCFtl(controller);
	  log = new FullGCLogger(controller);
	}
	else if(GARBAGE_COLLECT == 1)
	{
	  ftl = new GCFtl(controller);
	  log = new GCLogger(controller);
	}
	else
	{
	  ftl = new Ftl(controller);
	  log = new Logger(controller);
	}
	
	ReturnReadData= NULL;
	WriteDataDone= NULL;

	epoch_count = 0;
	epoch_cycles = 0;

	numReads= 0;
	numWrites= 0;
	numErases= 0;
	currentClockCycle= 0;
}

bool NVDIMM::add(FlashTransaction &trans){
	return ftl->addTransaction(trans);
}

bool NVDIMM::addTransaction(bool isWrite, uint64_t addr){
	TransactionType type = isWrite ? DATA_WRITE : DATA_READ;
	FlashTransaction trans = FlashTransaction(type, addr, NULL);
	return ftl->addTransaction(trans);
}

string NVDIMM::SetOutputFileName(string tracefilename){
	return "";
}

void NVDIMM::RegisterCallbacks(Callback_t *readCB, Callback_t *writeCB, Callback_v *Power){
	ReturnReadData = readCB;
	WriteDataDone = writeCB;
	ReturnPowerData = Power;
}

void NVDIMM::printStats(void){

       ftl->printStats(currentClockCycle, numReads, numWrites, numErases);
}

void NVDIMM::saveStats(void){
    ftl->saveStats(currentClockCycle, numReads, numWrites, numErases, epoch_count);
}

void NVDIMM::update(void){
	uint i, j;
	Package package;
	
	for (i= 0; i < packages->size(); i++){
		package= (*packages)[i];
		for (j= 0; j < package.dies.size() ; j++){
			package.dies[j]->update();
			package.dies[j]->step();
		}
	}
	
	ftl->update();
	ftl->step();
	controller->update();
	controller->step();

	step();

	//saving stats at the end of each epoch
	if(USE_EPOCHS)
	{
	    if(epoch_cycles >= EPOCH_TIME)
	    {
		logger->saveStats(currentClockCycle, epoch_count);
		epoch_count++;
		epoch_cycles = 0;		
	    }
	    else
	    {
		epoch_cycles++;
	    }
	}

	//cout << "NVDIMM successfully updated" << endl;
}

void NVDIMM::powerCallback(void){
    ftl->powerCallback();
}
