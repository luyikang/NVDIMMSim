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

void Ftl::addFfTransaction(FlashTransaction &t){ 
	transactionQueue.push_back(t);
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
	cout << "got here \n";
	cout << NVDIMM_SAVE_FILE << "\n";
	ofstream save_file;
	save_file.open(NVDIMM_SAVE_FILE, ios_base::out | ios_base::trunc);
	if(!save_file)
	{
	    cout << "ERROR: Could not open NVDIMM state save file: " << NVDIMM_SAVE_FILE << "\n";
	    abort();
	}
	
	cout << "NVDIMM is saving the used table and address map \n";

	// save the address map
	save_file << "AddressMap \n";
	std::unordered_map<uint64_t, uint64_t>::iterator it;
	for (it = addressMap.begin(); it != addressMap.end(); it++)
	{
	    save_file << (*it).first << " " << (*it).second << " \n";
	}

	// save the used table
	save_file << "Used \n";
	for(uint i = 0; i < used.size(); i++)
	{
	    for(uint j = 0; j < used[i].size(); j++)
	    {
		save_file << used[i][j] << " ";
	    }
	    save_file << "\n";
	}

	save_file.close();
    }
}

void Ftl::loadNVState(void)
{
    if(ENABLE_NV_RESTORE)
    {
	ifstream restore_file;
	restore_file.open(NVDIMM_RESTORE_FILE);
	if(!restore_file)
	{
	    cout << "ERROR: Could not open NVDIMM restore file: " << NVDIMM_RESTORE_FILE << "\n";
	    abort();
	}

	cout << "NVDIMM is restoring the system from file \n";

	// restore the data
	uint doing_used = 0;
	uint doing_addresses = 0;
	uint row = 0;
	uint column = 0;
	uint first = 0;
	uint key = 0;
	uint64_t pAddr, vAddr = 0;

	std::unordered_map<uint64_t,uint64_t> tempMap;

	std::string temp;
	
	while(!restore_file.eof())
	{ 
	    restore_file >> temp;

	    // restore used data
	    // have the row check cause eof sux
	    if(doing_used == 1 && row < used.size())
	    {
		used[row][column] = convert_uint64_t(temp);

		// this page was used need to issue fake write
		if(temp.compare("1") == 0)
		{
		    pAddr = (row * BLOCK_SIZE + column * NV_PAGE_SIZE);
		    vAddr = tempMap[pAddr];
		    ChannelPacket *tempPacket = Ftl::translate(WRITE, vAddr, pAddr);
		    controller->writeToPackage(tempPacket);
		    // TODO: translate physical address then use the NVDIMM parent to issue a write directly to the appropriate block
		}

		column++;
		if(column >= PAGES_PER_BLOCK)
		{
		    row++;
		    column = 0;
		}
	    }
	    
	    if(temp.compare("Used") == 0)
	    {
		doing_used = 1;
		doing_addresses = 0;
	    }

	    // restore address map data
	    if(doing_addresses == 1)
	    {
		if(first == 0)
		{
		    first = 1;
		    key = convert_uint64_t(temp);
		}
		else
		{
		    addressMap[key] = convert_uint64_t(temp);
		    tempMap[convert_uint64_t(temp)] = key;
		    first = 0;
		}
	    }

	    if(temp.compare("AddressMap") == 0)
	    {
		doing_addresses = 1;
	    }
	}
	
    }
}



