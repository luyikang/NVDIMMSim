//Ftl.cpp
//class file for ftl
//
#include "Ftl.h"
#include "ChannelPacket.h"
#include "NVDIMM.h"
#include <cmath>

using namespace NVDSim;
using namespace std;

Ftl::Ftl(Controller *c, Logger *l, NVDIMM *p){
	int numBlocks = NUM_PACKAGES * DIES_PER_PACKAGE * PLANES_PER_DIE * BLOCKS_PER_PLANE;

	/*offset = log2(NV_PAGE_SIZE);
	pageBitWidth = log2(PAGES_PER_BLOCK);
	blockBitWidth = log2(BLOCKS_PER_PLANE);
	planeBitWidth = log2(PLANES_PER_DIE);
	dieBitWidth = log2(DIES_PER_PACKAGE);
	packageBitWidth = log2(NUM_PACKAGES);
	*/
	channel = 0;
	die = 0;
	plane = 0;
	lookupCounter = 0;

	busy = 0;

	addressMap = std::unordered_map<uint64_t, uint64_t>();

        used = vector<vector<bool>>(numBlocks, vector<bool>(PAGES_PER_BLOCK, false));
	
	transactionQueue = list<FlashTransaction>();

	used_page_count = 0;
	gc_status = 0;
	panic_mode = 0;

	controller = c;

	parent = p;

	log = l;
}

ChannelPacket *Ftl::translate(ChannelPacketType type, uint64_t vAddr, uint64_t pAddr){

	uint package, die, plane, block, page;
	//uint64_t tempA, tempB, physicalAddress = pAddr;
	uint64_t physicalAddress = pAddr;

	if (physicalAddress > TOTAL_SIZE - 1 || physicalAddress < 0){
		ERROR("Inavlid address in Ftl: "<<physicalAddress);
		exit(1);
	}

	//offset = log2(NV_PAGE_SIZE);
	//physicalAddress = physicalAddress >> offset;
	physicalAddress /= NV_PAGE_SIZE;
	page = physicalAddress % PAGES_PER_BLOCK;
	physicalAddress /= PAGES_PER_BLOCK;
	block = physicalAddress % BLOCKS_PER_PLANE;
	physicalAddress /= BLOCKS_PER_PLANE;
	plane = physicalAddress % PLANES_PER_DIE;
	physicalAddress /= PLANES_PER_DIE;
	die = physicalAddress % DIES_PER_PACKAGE;
	physicalAddress /= DIES_PER_PACKAGE;
	package = physicalAddress % NUM_PACKAGES;

	/*tempA = physicalAddress;
	physicalAddress = physicalAddress >> pageBitWidth;
	tempB = physicalAddress << pageBitWidth;
	page = tempA ^ tempB;

	tempA = physicalAddress;
	physicalAddress = physicalAddress >> blockBitWidth;
	tempB = physicalAddress << blockBitWidth;
	block = tempA ^ tempB;

	tempA = physicalAddress;
	physicalAddress = physicalAddress >> planeBitWidth;
	tempB = physicalAddress << planeBitWidth;
	plane = tempA ^ tempB;

	tempA = physicalAddress;
	physicalAddress = physicalAddress >> dieBitWidth;
	tempB = physicalAddress << dieBitWidth;
	die = tempA ^ tempB;
	
	tempA = physicalAddress;
	physicalAddress = physicalAddress >> packageBitWidth;
	tempB = physicalAddress << packageBitWidth;
	package = tempA ^ tempB;
*/
	return new ChannelPacket(type, vAddr, pAddr, page, block, plane, die, package, NULL);
}

bool Ftl::addTransaction(FlashTransaction &t){

    if(transactionQueue.size() >= FTL_QUEUE_LENGTH && FTL_QUEUE_LENGTH != 0)
    {
	return false;
    }
    else
    {
	transactionQueue.push_back(t);

	// Start the logging for this access.
	log->access_start(t.address);

	return true;
    }
}

void Ftl::addGcTransaction(FlashTransaction &t){ 
	transactionQueue.push_back(t);

	// Start the logging for this access.
	log->access_start(t.address);
}

void Ftl::update(void){
	uint64_t block, page, start;
	bool result;
	if (busy) {
		if (lookupCounter == 0){
			uint64_t vAddr = currentTransaction.address, pAddr;
			bool done = false;
			ChannelPacket *commandPacket, *dataPacket;
			
			switch (currentTransaction.transactionType){
				case DATA_READ:
					if (addressMap.find(vAddr) == addressMap.end()){
					        //update the logger
					        log->access_process(vAddr, 0, READ);
						log->read_unmapped();
						
						//miss, nothing to read so return garbage
						controller->returnReadData(FlashTransaction(RETURN_DATA, vAddr, (void *)0xdeadbeef));
					} else {					       
						commandPacket = Ftl::translate(READ, vAddr, addressMap[vAddr]);
						
						//update the logger
						log->read_mapped();
						//send the read to the controller
						result = controller->addPacket(commandPacket);
					}
					break;
				case DATA_WRITE:
				        if (addressMap.find(vAddr) != addressMap.end()){
					    // we're going to write this data somewhere else for wear-leveling purposes however we will probably 
					    // want to reuse this block for something at some later time so mark it as unused because it is
					    used[addressMap[vAddr] / BLOCK_SIZE][(addressMap[vAddr] / NV_PAGE_SIZE) % PAGES_PER_BLOCK] = false;
					    log->write_mapped();
					}
					else
					{
					    log->write_unmapped();
					}
					//look for first free physical page starting at the write pointer
	                                start = BLOCKS_PER_PLANE * (plane + PLANES_PER_DIE * (die + NUM_PACKAGES * channel));

					for (block = start ; block < TOTAL_SIZE / BLOCK_SIZE && !done; block++){
					  for (page = 0 ; page < PAGES_PER_BLOCK  && !done ; page++){
						if (!used[block][page]){
						        pAddr = (block * BLOCK_SIZE + page * NV_PAGE_SIZE);
							used[block][page] = true;
							used_page_count++;
							done = true;
						}
					  }
					}

					//if we didn't find a free page after scanning til the end, check the beginning
				        if (!done){
					  for (block = 0 ; block < start / BLOCK_SIZE && !done ; block++){
					      for (page = 0 ; page < PAGES_PER_BLOCK && !done ; page++){
						if (!used[block][page]){
							pAddr = (block * BLOCK_SIZE + page * NV_PAGE_SIZE);
					  		used[block][page] = true;
							used_page_count++;
							done = true;		 
					        }
					      }
					  }								
					}

					if (!done){
						//bad news
						ERROR("FLASH DIMM IS COMPLETELY FULL - If you see this, something has gone horribly wrong.");
						exit(9001);
					} else {
						addressMap[vAddr] = pAddr;
					}
					//send write to controller
					dataPacket = Ftl::translate(DATA, vAddr, pAddr);
					commandPacket = Ftl::translate(WRITE, vAddr, pAddr);
					controller->addPacket(dataPacket);
					result = controller->addPacket(commandPacket);
					//update "write pointer"
					channel = (channel + 1) % NUM_PACKAGES;
					if (channel == 0){
						die = (die + 1) % DIES_PER_PACKAGE;
						if (die == 0)
							plane = (plane + 1) % PLANES_PER_DIE;
					}
					break;
				case BLOCK_ERASE:
				        ERROR("Called Block erase on memory which does not need this");
					break;					
				default:
					break;
			}
			if(result == true)
			{
			    transactionQueue.pop_front();
			    busy = 0;
			}
		} //if lookupCounter is not 0
		else
			lookupCounter--;
	} // if busy
	else {
		// Not currently busy.
		if (!transactionQueue.empty()) {
			busy = 1;
			currentTransaction = transactionQueue.front();
			lookupCounter = LOOKUP_TIME;
		}
	}
}

uint64_t Ftl::get_ptr(void) {
    // Return a pointer to the current plane.
    return NV_PAGE_SIZE * PAGES_PER_BLOCK * BLOCKS_PER_PLANE * 
	   (plane + PLANES_PER_DIE * (die + NUM_PACKAGES * channel));
}

void Ftl::powerCallback(void) 
{
    vector<vector<double> > temp = log->getEnergyData();
    if(temp.size() == 2)
    {
	controller->returnPowerData(temp[0], temp[1]);
    }
    else if(temp.size() == 3)
    {
	controller->returnPowerData(temp[0], temp[1], temp[2]);
    }
    else if(temp.size() == 4)
    {
	controller->returnPowerData(temp[0], temp[1], temp[2], temp[3]);
    }
    else if(temp.size() == 6)
    {
	controller->returnPowerData(temp[0], temp[1], temp[2], temp[3], temp[4], temp[5]);
    }
}

void Ftl::sendQueueLength(void)
{
    log->ftlQueueLength(transactionQueue.size());
}

void Ftl::saveNVState(void)
{
    if(ENABLE_NV_SAVE)
    {
	ofstream save_file;
	save_file.open("../NVDIMMSim/src/"+NVDIMM_SAVE_FILE, ios_base::out | ios_base::trunc);
	if(!save_file)
	{
	    cout << "ERROR: Could not open NVDIMM state save file: " << NVDIMM_SAVE_FILE << "\n";
	    abort();
	}
	
	cout << "NVDIMM is saving the used table and address map \n";

	// save the used table
	save_file << "Used \n";
	for(int i = 0; i < used.size(); i++)
	{
	    for(int j = 0; j < used[i].size(); j++)
	    {
		save_file << used[i][j] << " ";
	    }
	    save_file << "\n";
	}
	
	// save the address map
	save_file << "Address Map \n";
	std::unordered_map<unit64_t, uint64_t>::iterator it;
	for (it = addressMap.begin(); it != addressMap.end(); it++)
	{
	    save_file << (*it).first << " " << (*it).second << " \n";
	}

	save_file.close();
    }
}

void Ftl::loadNVState(void)
{
    if(ENABLE_NV_RESTORE)
    {
	ifstream restore_file;
	restore_file.open("../NVDIMMSim/src/"+NVDIMM_RESTORE_FILE);
	if(!restore_file)
	{
	    cout << "ERROR: Could not open NVDIMM restore file: " << NVDIMM_RESTORE_FILE << "\n";
	    abort();
	}

	cout << "NVDIMM is restoring the system from file \n";

	// restore the data
	int doing_used = 0;
	int doing_addresses = 0;
	while(!restore_file.eof());
	{
	    std::string temp;
	    restore_file >> temp;
	    if(temp.strcmp("Used ") == 0)
	    {
		doing_used = 1;
	    }

	    // restore used data
	    if(doing_used == 1)
	    {
	    }
	}
	
    }
}



