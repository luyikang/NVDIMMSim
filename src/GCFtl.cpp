//GCFtl.cpp
//class file for GCftl
//
#include "GCFtl.h"
#include "ChannelPacket.h"
#include "NVDIMM.h"
#include <cmath>

using namespace NVDSim;
using namespace std;

GCFtl::GCFtl(Controller *c, Logger *l, NVDIMM *p) 
    : Ftl(c, l, p)
{	
        int numBlocks = NUM_PACKAGES * DIES_PER_PACKAGE * PLANES_PER_DIE * BLOCKS_PER_PLANE;

	used_page_count = 0;
	gc_status = 0;
	panic_mode = 0;

	dirty = vector<vector<bool>>(numBlocks, vector<bool>(PAGES_PER_BLOCK, false));

	// set up internal pointer to make sure that the gc isn't always erasing the same block
	erase_pointer = 0;
}

bool GCFtl::addTransaction(FlashTransaction &t){
        if(transactionQueue.size() >= FTL_QUEUE_LENGTH && FTL_QUEUE_LENGTH != 0)
	{
	    return false;
	}
	else
	{
	    if (!panic_mode)
	    {
		transactionQueue.push_back(t);
		if(LOGGING == true)
		{
		    // Start the logging for this access.
		    log->access_start(t.address);
		}
		return true;
	    }
	    
	    return false;
	}
	
}

void GCFtl::addGcTransaction(FlashTransaction &t){ 
	transactionQueue.push_front(t);
	queues_full = false;

	if(LOGGING == true)
	{
	    // Start the logging for this access.
	    log->access_start(t.address);
	}
}

void GCFtl::update(void){
        uint64_t start;
	uint i;

	if (busy) {
	    if (lookupCounter <= 0 && !queues_full){
		        currentTransaction = transactionQueue.front();
			uint64_t vAddr = currentTransaction.address, pAddr;
			bool done = false;
			bool result = false;
			ChannelPacket *commandPacket;
			
			switch (currentTransaction.transactionType){
				case DATA_READ:
					handle_read(false);
					break;

				case DATA_WRITE:
					handle_write(false);
					break;

				case GC_DATA_READ:
					handle_read(true);
					break;

				case GC_DATA_WRITE:
					handle_write(true);
					break;

				case BLOCK_ERASE:
					commandPacket = Ftl::translate(ERASE, vAddr, vAddr);//note: vAddr is actually the pAddr in this case with the way garbage collection is written
					result = controller->addPacket(commandPacket);
					if(result == true)
					{
					    for (i = 0 ; i < PAGES_PER_BLOCK ; i++){
						dirty[vAddr / BLOCK_SIZE][i] = false;
						if (used[vAddr / BLOCK_SIZE][i]){
							used[vAddr / BLOCK_SIZE][i] = false;
							used_page_count--;
						}
					    }
					    transactionQueue.pop_front();
					    busy = 0;
					}
					else
					{
					    delete commandPacket;
					    queues_full = true;
					}
					break;		

				default:
					ERROR("Transaction in Ftl that isn't a read or write... What?");
					exit(1);
					break;
			}
		} 
		else if(lookupCounter > 0)
		{
			lookupCounter--;
		}
	} // if busy
	else {
		// Not currently busy.
		if (!transactionQueue.empty()) {
			busy = 1;
			lookupCounter = LOOKUP_TIME;
		}
		// Check to see if GC needs to run.
		else {
			// Check to see if GC needs to run.
			if (checkGC() && !gc_status) {
				// Run the GC.
				start_erase = parent->numErases;
				gc_status = 1;
				runGC();
			}
		}
	}

	if (gc_status){
		if (!panic_mode && parent->numErases == start_erase + 1)
			gc_status = 0;
		if (panic_mode && parent->numErases == start_erase + PLANES_PER_DIE){
			panic_mode = 0;
			gc_status = 0;
		}
	}

	if (!gc_status && (float)used_page_count >= (float)(FORCE_GC_THRESHOLD * (VIRTUAL_TOTAL_SIZE / NV_PAGE_SIZE))){
		start_erase = parent->numErases;
		gc_status = 1;
		panic_mode = 1;
		//cout << (float)(FORCE_GC_THRESHOLD * (VIRTUAL_TOTAL_SIZE / NV_PAGE_SIZE)) << "\n";
		//cout << (float)used_page_count << "\n";
		for (i = 0 ; i < PLANES_PER_DIE ; i++)
		{
			runGC();
		}
	}

	//place power callbacks to hybrid_system
#if Verbose_Power_Callback
	  controller->returnPowerData(idle_energy, access_energy, erase_energy);
#endif

}

void GCFtl::write_used_handler(uint64_t vAddr)
{
	dirty[addressMap[vAddr] / BLOCK_SIZE][(addressMap[vAddr] / NV_PAGE_SIZE) % PAGES_PER_BLOCK] = true;

	//cout << "USING GCFTL's WRITE_USED_HANDLER!!!\n";
	//cout << "Block " << addressMap[vAddr] / BLOCK_SIZE << " Page " << addressMap[vAddr] / NV_PAGE_SIZE << " is now dirty \n";
}

bool GCFtl::checkGC(void){
	// Return true if more than 70% of blocks are in use and false otherwise.
        if ((float)used_page_count > ((float)IDLE_GC_THRESHOLD * (VIRTUAL_TOTAL_SIZE / NV_PAGE_SIZE)))
		return true;
	return false;
}


void GCFtl::runGC() {
  uint64_t block, page, count, dirty_block=0, dirty_count=0, pAddr, vAddr, tmpAddr;
	FlashTransaction trans;

	// Get the dirtiest block (assumes the flash keeps track of this with an online algorithm).
	for (block = erase_pointer; block < TOTAL_SIZE / BLOCK_SIZE; block++) {
	  count = 0;
	  for (page = 0; page < PAGES_PER_BLOCK; page++) {
		if (dirty[block][page] == true) {
			count++;
		}
	  }
	  if (count > dirty_count) {
	      	dirty_count = count;
	       	dirty_block = block;
	  }
	}
	erase_pointer = (dirty_block + 1) % (TOTAL_SIZE / BLOCK_SIZE);

	// All used pages in the dirty block, they must be moved elsewhere.
	for (page = 0; page < PAGES_PER_BLOCK; page++) {
	  if (used[dirty_block][page] == true && dirty[dirty_block][page] == false) {
	    	// Compute the physical address to move.
		pAddr = (dirty_block * BLOCK_SIZE + page * NV_PAGE_SIZE);

		// Do a reverse lookup for the virtual page address.
		// This is slow, but the alternative is maintaining a full reverse lookup map.
		// Another alternative may be to make new FlashTransaction commands for physical address read/write.
		bool found = false;
		for (std::unordered_map<uint64_t, uint64_t>::iterator it = addressMap.begin(); it != addressMap.end(); it++) {
			tmpAddr = (*it).second;
			if (tmpAddr == pAddr) {
				vAddr = (*it).first;
				found = true;
				break;
			}
		}
		assert(found);

		ChannelPacket *dataPacket = Ftl::translate(DATA, vAddr, pAddr);
		// Schedule a read and a write.
		trans = FlashTransaction(GC_DATA_READ, vAddr, NULL);
		addGcTransaction(trans);
		trans = FlashTransaction(GC_DATA_WRITE, vAddr, NULL);
		addGcTransaction(trans);
	  }
   }

   // Schedule the BLOCK_ERASE command.
   // Note: The address field is just the block number, not an actual byte address.
   trans = FlashTransaction(BLOCK_ERASE, dirty_block * BLOCK_SIZE, NULL); 
   addGcTransaction(trans);
}

void GCFtl::saveNVState(void)
{
     if(ENABLE_NV_SAVE)
    {
	ofstream save_file;
	save_file.open(NV_SAVE_FILE, ios_base::out | ios_base::trunc);
	if(!save_file)
	{
	    cout << "ERROR: Could not open NVDIMM state save file: " << NV_SAVE_FILE << "\n";
	    abort();
	}
	
	cout << "NVDIMM is saving the used table, dirty table and address map \n";

	// save the address map
	save_file << "AddressMap \n";
	std::unordered_map<uint64_t, uint64_t>::iterator it;
	for (it = addressMap.begin(); it != addressMap.end(); it++)
	{
	    save_file << (*it).first << " " << (*it).second << " \n";
	}

        // save the dirty table
	save_file << "Dirty \n";
	for(uint i = 0; i < dirty.size(); i++)
	{
	    for(uint j = 0; j < dirty[i].size(); j++)
	    {
		save_file << dirty[i][j] << " ";
	    }
	    save_file << "\n";
	}

	// save the used table
	save_file << "Used";
	for(uint i = 0; i < used.size(); i++)
	{
	    save_file << "\n";
	    for(uint j = 0; j < used[i].size()-1; j++)
	    {
		save_file << used[i][j] << " ";
	    }
	    save_file << used[i][used[i].size()-1];
	}

	save_file.close();
    }
}

void GCFtl::loadNVState(void)
{
    if(ENABLE_NV_RESTORE && !loaded)
    {
	ifstream restore_file;
	restore_file.open(NV_RESTORE_FILE);
	if(!restore_file)
	{
	    cout << "ERROR: Could not open NVDIMM restore file: " << NV_RESTORE_FILE << "\n";
	    abort();
	}

	cout << "NVDIMM is restoring the system from file " << NV_RESTORE_FILE <<"\n";

	// restore the data
	uint doing_used = 0;
	uint doing_dirty = 0;
	uint doing_addresses = 0;
	uint row = 0;
	uint column = 0;
	uint first = 0;
	uint key = 0;
	uint64_t pAddr = 0;
	uint64_t vAddr = 0;

	std::unordered_map<uint64_t,uint64_t> tempMap;

	std::string temp;
	
	while(!restore_file.eof())
	{ 
	    restore_file >> temp;

	    // restore used data
	    if(doing_used == 1)
	    {
		used[row][column] = convert_uint64_t(temp);

                // this page was used need to issue fake write
		if(temp.compare("1") == 0 && dirty[row][column] != 1)
		{
		    pAddr = (row * BLOCK_SIZE + column * NV_PAGE_SIZE);
		    vAddr = tempMap[pAddr];
		    ChannelPacket *tempPacket = Ftl::translate(WRITE, vAddr, pAddr);
		    controller->writeToPackage(tempPacket);

		    used_page_count++;
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
		doing_dirty = 0;
		
		row = 0;
		column = 0;
	    }

	    // restore dirty data
	    if(doing_dirty == 1)
	    {
		dirty[row][column] = convert_uint64_t(temp);
		column++;
		if(column >= PAGES_PER_BLOCK)
		{
		    row++;
		    column = 0;
		}
		}

	    if(temp.compare("Dirty") == 0)
	    {
		doing_used = 0;
		doing_dirty = 1;
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

	restore_file.close();
	loaded = true;
    }
}
