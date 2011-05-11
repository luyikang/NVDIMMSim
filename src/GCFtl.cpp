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
	dirty = vector<vector<bool>>(numBlocks, vector<bool>(PAGES_PER_BLOCK, false));
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
		// Start the logging for this access.
		log->access_start(t.address);
		return true;
	    }
	    
	    return false;
	}
	
}

void GCFtl::addGcTransaction(FlashTransaction &t){ 
	transactionQueue.push_back(t);

	// Start the logging for this access.
	log->access_start(t.address);
}

void GCFtl::update(void){
        uint64_t block, page, start;
	uint i;
	bool result = true; 
	bool wait = false;
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
					    dirty[addressMap[vAddr] / BLOCK_SIZE][(addressMap[vAddr] / NV_PAGE_SIZE) % PAGES_PER_BLOCK] = true;
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
						// TODO: Call GC
						//ERROR("No free pages? GC needs some work.");
						//exit(1);
						// Trust that the GC is running and wait
					        break;
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
			        case GC_DATA_READ:
				    	if (addressMap.find(vAddr) == addressMap.end()){
					        ERROR("GC tried to move data that wasn't there.");
						exit(1);
					} else {		
					        commandPacket = Ftl::translate(GC_READ, vAddr, addressMap[vAddr]);
						//send the read to the controller
						result = controller->addPacket(commandPacket);
					}
					break;
			        case GC_DATA_WRITE:
				        if (addressMap.find(vAddr) != addressMap.end()){
					    dirty[addressMap[vAddr] / BLOCK_SIZE][(addressMap[vAddr] / NV_PAGE_SIZE) % PAGES_PER_BLOCK] = true;
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
						// TODO: Call GC
						//ERROR("No free pages? GC needs some work.");
						//exit(1);
						// Trust that the GC is running and wait
					        wait = true;
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
				        for (i = 0 ; i < PAGES_PER_BLOCK ; i++){
						dirty[vAddr / BLOCK_SIZE][i] = false;
						if (used[vAddr / BLOCK_SIZE][i]){
							used[vAddr / BLOCK_SIZE][i] = false;
							used_page_count--;
						}
					}
					commandPacket = Ftl::translate(ERASE, 0, vAddr);//note: vAddr is actually the pAddr in this case with the way garbage collection is written
					result = controller->addPacket(commandPacket);
					break;		
				default:
					ERROR("Transaction in Ftl that isn't a read or write... What?");
					exit(1);
					break;
			}
			if( result == true && wait == false)
			{
			    transactionQueue.pop_front();
			    busy = 0;
			}
		} 
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
		for (i = 0 ; i < PLANES_PER_DIE ; i++)
			runGC();
	}

	//place power callbacks to hybrid_system
#if Verbose_Power_Callback
	  controller->returnPowerData(idle_energy, access_energy, erase_energy);
#endif

}

bool GCFtl::checkGC(void){
	// Return true if more than 70% of blocks are in use and false otherwise.
  if ((float)used_page_count > ((float)IDLE_GC_THRESHOLD * (VIRTUAL_TOTAL_SIZE / NV_PAGE_SIZE)))
		return true;
	return false;
}


void GCFtl::runGC(void) {
  uint64_t block, page, count, dirty_block=0, dirty_count=0, pAddr, vAddr, tmpAddr;
	FlashTransaction trans;

	// Get the dirtiest block (assumes the flash keeps track of this with an online algorithm).
	for (block = 0; block < TOTAL_SIZE / BLOCK_SIZE; block++) {
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
			

		// Schedule a read and a write.
		trans = FlashTransaction(GC_DATA_READ, vAddr, NULL);
		addGcTransaction(trans);
		trans = FlashTransaction(GC_DATA_WRITE, vAddr, NULL);
		addGcTransaction(trans);
	    } else if (dirty[dirty_block][page] == true){
		dirty[dirty_block][page] = false;
	    }	
	    if (used[dirty_block][page] == true){
		used_page_count--;
		used[dirty_block][page] = false;
	    }
   }

   // Schedule the BLOCK_ERASE command.
   // Note: The address field is just the block number, not an actual byte address.
   trans = FlashTransaction(BLOCK_ERASE, dirty_block * BLOCK_SIZE, NULL); 
   addGcTransaction(trans);

}

void GCFtl::saveNVState(void)
{
}

void GCFtl::loadNVState(void)
{
}
