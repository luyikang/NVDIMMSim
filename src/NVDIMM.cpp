//NVDIMM.cpp
//Class file for nonvolatile memory dimm system wrapper

#include "NVDIMM.h"
#include "Init.h"

using namespace std;

uint64_t BLOCKS_PER_PLANE;

namespace NVDSim
{
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
	
	BLOCKS_PER_PLANE = (uint64_t) VIRTUAL_BLOCKS_PER_PLANE * PBLOCKS_PER_VBLOCK;
	if(LOGGING == 1)
	{
	    PRINT("Logs are being generated");
	}else{
	    PRINT("Logs are not being generated");
	}
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
	}else{
	  PRINT("Device is not using garbage collection");
	}
	if(BUFFERED == 1)
	{
	  PRINT("Memory is using a buffer between the channel and dies");
	}else{
	  PRINT("Memory channels are directly connected to dies");
	}
	PRINT("\nTiming Info:\n");
	PRINT("Read time: "<<READ_TIME);
	PRINT("Write Time: "<<WRITE_TIME);
	PRINT("Erase time: "<<ERASE_TIME);
	PRINT("Channel latency for data: "<<CHANNEL_CYCLE);
	PRINT("Channel width for data: "<<CHANNEL_WIDTH);
	PRINT("Device latency for data: "<<DEVICE_CYCLE);
	PRINT("Device width for data: "<<DEVICE_WIDTH)
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
		
	if(DEVICE_TYPE.compare("PCM") == 0 && GARBAGE_COLLECT == 1)
	{
	  log = new PCMGCLogger();
	  controller= new Controller(this, log);
	  ftl = new GCFtl(controller, log, this);
	}
	else if(DEVICE_TYPE.compare("PCM") == 0 && GARBAGE_COLLECT == 0)
	{
	  log = new PCMLogger();
	  controller= new Controller(this, log);
	  ftl = new Ftl(controller, log, this);
	}
	else if(GARBAGE_COLLECT == 1)
	{
	  log = new GCLogger();
	  controller= new Controller(this, log);
	  ftl = new GCFtl(controller, log, this);
	}
	else
	{
	  log = new Logger();
	  controller= new Controller(this, log);
	  ftl = new Ftl(controller, log, this);
	}
	packages= new vector<Package>();

	if (DIES_PER_PACKAGE > INT_MAX){
		ERROR("Too many dies.");
		exit(1);
	}

	// sanity checks
	

	for (i= 0; i < NUM_PACKAGES; i++){
	    Package pack = {new Channel(), new Buffer(i), vector<Die *>()};
		//pack.channel= new Channel();
		pack.channel->attachController(controller);
		pack.channel->attachBuffer(pack.buffer);
		pack.buffer->attachChannel(pack.channel);
		for (j= 0; j < DIES_PER_PACKAGE; j++){
		        Die *die= new Die(this, log, j);
			die->attachToBuffer(pack.buffer);
			pack.buffer->attachDie(die);
			pack.dies.push_back(die);
		}
		packages->push_back(pack);
	}
	controller->attachPackages(packages);
	
	ReturnReadData= NULL;
	WriteDataDone= NULL;

	epoch_count = 0;
	epoch_cycles = 0;

	numReads= 0;
	numWrites= 0;
	numErases= 0;
	currentClockCycle= 0;

	ftl->loadNVState();
	}

// static allocator for the library interface
NVDIMM *getNVDIMMInstance(uint id, string deviceFile, string sysFile, string pwd, string trc)
{
    return new NVDIMM(id, deviceFile, sysFile, pwd, trc);
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
	UnmappedReadDone = NULL;
	CriticalLineDone = NULL;
	WriteDataDone = writeCB;
	ReturnPowerData = Power;
}

void NVDIMM::RegisterCallbacks(Callback_t *readCB, Callback_t *unmappedCB, Callback_t *critLineCB, 
		       Callback_t *writeCB, Callback_v *Power)
{
    	ReturnReadData = readCB;
	UnmappedReadDone = unmappedCB;
	CriticalLineDone = critLineCB;
	WriteDataDone = writeCB;
	ReturnPowerData = Power;
}

void NVDIMM::printStats(void){
       if(LOGGING == true)
       {
	   log->print(currentClockCycle);
       }
}

void NVDIMM::saveStats(void){
       if(LOGGING == true)
       {
	   log->save(currentClockCycle, epoch_count);
       }
       ftl->saveNVState();
}

void NVDIMM::update(void){
	uint i, j;
	Package package;
	
	for (i= 0; i < packages->size(); i++){
		package= (*packages)[i];
		package.channel->update();
		package.buffer->update();
		for (j= 0; j < package.dies.size() ; j++){
			package.dies[j]->update();
			package.dies[j]->step();
		}
	}
	
	ftl->update();
	ftl->step();
	controller->update();
	controller->step();
	if(LOGGING == true)
	{
	    log->update();
	}

	step();

	//saving stats at the end of each epoch
	if(USE_EPOCHS)
	{
	    ftl->sendQueueLength();
	    controller->sendQueueLength();
	    if(epoch_cycles >= EPOCH_TIME)
	    {
		if(LOGGING == true)
		{
		    log->save_epoch(currentClockCycle, epoch_count);
		    log->ftlQueueReset();
		    log->ctrlQueueReset();
		}
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

//If either of these methods are called it is because HybridSim called them
//therefore the appropriate system setting should be set
void NVDIMM::saveNVState(string filename){
    ENABLE_NV_SAVE = 1;
    NV_SAVE_FILE = filename;
    cout << "got to save state in nvdimm \n";
    cout << "save file was" << NV_SAVE_FILE << "\n";
    ftl->saveNVState();
}

void NVDIMM::loadNVState(string filename){
    ENABLE_NV_RESTORE = 1;
    NV_RESTORE_FILE = filename;
    ftl->loadNVState();
}

void NVDIMM::queuesNotFull(void)
{
    ftl->queuesNotFull();
}

void NVDIMM::GCReadDone(uint64_t vAddr)
{
    ftl->GCReadDone(vAddr);
}
}
