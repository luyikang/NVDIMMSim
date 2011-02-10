//NVDIMM.cpp
//Class file for nonvolatile memory dimm system wrapper

#include "NVDIMM.h"
#include "Init.h"

using namespace NVDSim;
using namespace std;

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
	PRINT("");

	if(GARBAGE_COLLECT == 0 && (DEVICE_TYPE == "NAND" || DEVICE_TYPE == "NOR"))
	{
	  ERROR("Device is Flash and must use garbage collection");
	  exit(-1);
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

	ftl = new Ftl(controller);
	
	ReturnReadData= NULL;
	WriteDataDone= NULL;

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

	cout<<"Reads completed: "<<numReads<<endl;
	cout<<"Writes completed: "<<numWrites<<endl;
	cout<<"Erases completed: "<<numErases<<endl;

	// Power stuff
	// Total power used
	vector<double> total_power = vector<double>(NUM_PACKAGES, 0.0);

	// Energy values from the ftl
	vector<double> idle_energy = ftl->getIdleEnergy();
	vector<double> access_energy = ftl->getAccessEnergy();
	vector<double> erase_energy = ftl->getEraseEnergy();
	
	// Average power used
	vector<double> ave_idle_power = vector<double>(NUM_PACKAGES, 0.0);
	vector<double> ave_access_power = vector<double>(NUM_PACKAGES, 0.0);	
	vector<double> ave_erase_power = vector<double>(NUM_PACKAGES, 0.0);
	vector<double> average_power = vector<double>(NUM_PACKAGES, 0.0);

	for(uint i = 0; i < NUM_PACKAGES; i++)
	{
	  if(GARBAGE_COLLECT == 1)
	  {
	    total_power[i] = (idle_energy[i] + access_energy[i] + erase_energy[i]) * VCC;
	  }
	  else
	  {
	    total_power[i] = (idle_energy[i] + access_energy[i]) * VCC;
	  }
	  ave_idle_power[i] = (idle_energy[i] * VCC) / currentClockCycle;
	  ave_access_power[i] = (access_energy[i] * VCC) / currentClockCycle;
	  ave_erase_power[i] = (erase_energy[i] * VCC) / currentClockCycle;
	  average_power[i] = total_power[i] / currentClockCycle;
	}

	cout<<endl;
	cout<<"Power Data: "<<endl;
	cout<<"========================"<<endl;

	for(uint i = 0; i < NUM_PACKAGES; i++)
	{
	    cout<<"Package: "<<i<<endl;
	    cout<<"Accumulated Idle Power: "<<(idle_energy[i] * VCC)<<"mW"<<endl;
	    cout<<"Accumulated Access Power: "<<(access_energy[i] * VCC)<<"mW"<<endl;
	    if( GARBAGE_COLLECT == 1)
	    {
	      cout<<"Accumulated Erase Power: "<<(erase_energy[i] * VCC)<<"mW"<<endl;
	    }
	    cout<<"Total Power: "<<total_power[i]<<"mW"<<endl;
	    cout<<endl;
	    cout<<"Average Idle Power: "<<ave_idle_power[i]<<"mW"<<endl;
	    cout<<"Average Access Power: "<<ave_access_power[i]<<"mW"<<endl;
	    if( GARBAGE_COLLECT == 1)
	    {
	      cout<<"Average Erase Power: "<<ave_erase_power[i]<<"mW"<<endl;
	    }
	    cout<<"Average Power: "<<average_power[i]<<"mW"<<endl;
	    cout<<endl;
	}
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

	//cout << "NVDIMM successfully updated" << endl;
}

void NVDIMM::powerCallback(void){
  if( GARBAGE_COLLECT == 1)
  {
    controller->returnPowerData(ftl->getIdleEnergy(), ftl->getAccessEnergy(), ftl->getEraseEnergy());
  }
  controller->returnPowerData(ftl->getIdleEnergy(), ftl->getAccessEnergy());
}
