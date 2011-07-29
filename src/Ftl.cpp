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
	
	loaded = false;
	queues_full = false;

	// Counter to keep track of succesful writes.
	write_counter = 0;

	// Counter to keep track of cycles we spend waiting on erases
	// if we wait longer than the length of an erase, we've probably deadlocked
	deadlock_counter = 0;

	// the maximum amount of time we can wait before we're sure we've deadlocked
	// time it takes to read all of the pages in a block
	deadlock_time = PAGES_PER_BLOCK * (READ_TIME + ((divide_params((NV_PAGE_SIZE*8192),DEVICE_WIDTH) * DEVICE_CYCLE) / CYCLE_TIME) +
					   ((divide_params(COMMAND_LENGTH,DEVICE_WIDTH) * DEVICE_CYCLE) / CYCLE_TIME));
	// plus the time it takes to write all of the pages in a block
	deadlock_time += PAGES_PER_BLOCK * (WRITE_TIME + ((divide_params((NV_PAGE_SIZE*8192),DEVICE_WIDTH) * DEVICE_CYCLE) / CYCLE_TIME) +
					   ((divide_params(COMMAND_LENGTH,DEVICE_WIDTH) * DEVICE_CYCLE) / CYCLE_TIME));
	// plus the time it takes to erase the block
	deadlock_time += ERASE_TIME + ((divide_params(COMMAND_LENGTH,DEVICE_WIDTH) * DEVICE_CYCLE) / CYCLE_TIME);
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

		if(LOGGING == true)
		{
			// Start the logging for this access.
			log->access_start(t.address);
		}
		return true;
	}
}

void Ftl::addFfTransaction(FlashTransaction &t){ 
	transactionQueue.push_back(t);
}

void Ftl::update(void){
	if (busy) {
	    if (lookupCounter <= 0 && !queues_full){

			switch (currentTransaction.transactionType){
				case DATA_READ:
					handle_read(false);
					break;

				case DATA_WRITE: 
					handle_write(false);
					break;

				default:
					ERROR("FTL received an illegal transaction type:" << currentTransaction.transactionType);
					break;
			}
		} //if lookupCounter is not 0
		else if(lookupCounter > 0)
		{
			lookupCounter--;
		}
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

void Ftl::handle_read(bool gc)
{
	ChannelPacket *commandPacket;
	uint64_t vAddr = currentTransaction.address;

	// Check to see if the vAddr exists in the address map.
	if (addressMap.find(vAddr) == addressMap.end())
	{
		if (gc)
		{
			ERROR("GC tried to move data that wasn't there.");
			exit(1);
		}

		// If not, then this is an unmapped read.
		// We return a fake result immediately.
		// In the future, this could be an error message if we want.
		if(LOGGING)
		{
			// Update the logger
			log->read_unmapped();

			// access_process for this read is called here since this ends now.
			log->access_process(vAddr, vAddr, 0, READ);

			// stop_process for this read is called here since this ends now.
			log->access_stop(vAddr, vAddr);
		}

		// Miss, nothing to read so return garbage.
		controller->returnReadData(FlashTransaction(RETURN_DATA, vAddr, (void *)0xdeadbeef));
		transactionQueue.pop_front();
		busy = 0;
	} 
	else 
	{					       
		ChannelPacketType read_type;
		if (gc)
			read_type = GC_READ;
		else
			read_type = READ;
		commandPacket = Ftl::translate(read_type, vAddr, addressMap[vAddr]);

		//send the read to the controller
		bool result = controller->addPacket(commandPacket);
		if(result)
		{
			if(LOGGING && !gc)
			{
				// Update the logger (but not for GC_READ).
				log->read_mapped();
			}
			transactionQueue.pop_front();
			busy = 0;
		}
		else
		{
			// Delete the packet if it is not being used to prevent memory leaks.
			delete commandPacket;
			queues_full = true;
		}
	}
}

void Ftl::write_used_handler(uint64_t vAddr)
{
		// we're going to write this data somewhere else for wear-leveling purposes however we will probably 
		// want to reuse this block for something at some later time so mark it as unused because it is
		used[addressMap[vAddr] / BLOCK_SIZE][(addressMap[vAddr] / NV_PAGE_SIZE) % PAGES_PER_BLOCK] = false;

		//cout << "USING FTL's WRITE_USED_HANDLER!!!\n";
}


void Ftl::handle_write(bool gc)
{
	uint64_t start;
	uint64_t vAddr = currentTransaction.address, pAddr;
	ChannelPacket *commandPacket, *dataPacket;
	bool done = false;

	// Mapped is used to indicate to the logger that a write was mapped or unmapped.
	bool mapped = false;

	if (addressMap.find(vAddr) != addressMap.end())
	{
		write_used_handler(vAddr);

		mapped = true;
	}

	//look for first free physical page starting at the write pointer
	start = BLOCKS_PER_PLANE * (plane + PLANES_PER_DIE * (die + DIES_PER_PACKAGE * channel));
	uint64_t block, page, tmp_block, tmp_page;

	// Search from the current write pointer to the end of the flash for a free page.
	for (block = start ; block < TOTAL_SIZE / BLOCK_SIZE && !done; block++)
	{
		for (page = 0 ; page < PAGES_PER_BLOCK  && !done ; page++)
		{
			if (!used[block][page])
			{
				tmp_block = block;
				tmp_page = page;
				pAddr = (block * BLOCK_SIZE + page * NV_PAGE_SIZE);
				done = true;
			}
		}
	}
	block = tmp_block;
	page = tmp_page;
	//cout << "outside: " << block << "/" << page << "\n";
	//attemptWrite(start, &vAddr, &pAddr, &done);


	// If we didn't find a free page after scanning to the end. Scan from the beginning
	// to the write pointer
	if (!done)
	{
		//attemptWrite(0, &vAddr, &pAddr, &done);							

		for (block = 0 ; block < start / BLOCK_SIZE && !done; block++)
		{
			for (page = 0 ; page < PAGES_PER_BLOCK  && !done; page++)
			{
				if (!used[block][page])
				{
					tmp_block = block;
					tmp_page = page;
					pAddr = (block * BLOCK_SIZE + page * NV_PAGE_SIZE);
					done = true;
				}
			}
		}
		block = tmp_block;
		page = tmp_page;
	}

	if (!done)
	{
	    deadlock_counter++;
	    if(deadlock_counter == deadlock_time)
	    {
		//bad news
		cout << deadlock_time;
		ERROR("FLASH DIMM IS COMPLETELY FULL AND DEADLOCKED - If you see this, something has gone horribly wrong.");
		exit(9001);
	    }
	} 
	else 
	{
		// We've found a used page. Now we need to try to add the transaction to the Controller queue.
	       
	        // first things first, we're no longer in danger of dead locking so reset the counter
	        deadlock_counter = 0;

		//send write to controller
		
		ChannelPacketType write_type;
		if (gc)
			write_type = GC_WRITE;
		else
			write_type = WRITE;
		dataPacket = Ftl::translate(DATA, vAddr, pAddr);
		commandPacket = Ftl::translate(write_type, vAddr, pAddr);

		// Check to see if there is enough room for both packets in the queue (need two open spots).
		bool queue_open = controller->checkQueueWrite(dataPacket);

		if (!queue_open)
		{
			// These packets are not being used. Since they were dynamically allocated, we must delete them to prevent
			// memory leaks.
			delete dataPacket;
			delete commandPacket;
			queues_full = true;
		}

		if (queue_open)
		{
			// Add the packets to the controller queue.
			// Do not need to check the return values for these since checkQueueWrite() was called.
			controller->addPacket(dataPacket);
			controller->addPacket(commandPacket);

			// Successfully added transaction to the controller queue.

			//update "write pointer"
			channel = (channel + 1) % NUM_PACKAGES;
			if (channel == 0){
				die = (die + 1) % DIES_PER_PACKAGE;
				if (die == 0)
					plane = (plane + 1) % PLANES_PER_DIE;
			}

			// Set the used bit for this page to true.
			used[block][page] = true;
			used_page_count++;

			// Pop the transaction from the transaction queue.
			transactionQueue.pop_front();

			// The FTL is no longer busy.
			busy = 0;

			// Update the write counter.
			write_counter++;
			//cout << "WRITE COUNTER IS " << write_counter << "\n";


			if (LOGGING && !gc)
			{
				if (mapped)
					log->write_mapped();
				else
					log->write_unmapped();
			}

			// Update the address map.
			addressMap[vAddr] = pAddr;
			//cout << "Added " << hex << vAddr << " -> " << pAddr << dec << " to addressMap on cycle " << 100 << " " << currentClockCycle << "\n";
		}
	}
}


void Ftl::attemptWrite(uint64_t start, uint64_t *vAddr, uint64_t *pAddr, bool *done){
	uint64_t block, page;

	for (block = start ; block < TOTAL_SIZE / BLOCK_SIZE && !*done; block++){
		for (page = 0 ; page < PAGES_PER_BLOCK  && !*done ; page++){
			if (!used[block][page]){
				*pAddr = (block * BLOCK_SIZE + page * NV_PAGE_SIZE);
				*done = true;
			}
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
	if(LOGGING == true)
	{
		log->ftlQueueLength(transactionQueue.size());
	}
}

void Ftl::saveNVState(void)
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

		cout << "NVDIMM is saving the used table and address map \n";
		cout << "save file is" << NV_SAVE_FILE << "\n";

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
			save_file << "\n";
			for(uint j = 0; j < used[i].size()-1; j++)
			{
				save_file << used[i][j] << " ";
			}
			save_file << used[i][used[i].size()];
		}

		save_file.close();
	}
}

void Ftl::loadNVState(void)
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
			if(doing_used == 1)
			{
				used[row][column] = convert_uint64_t(temp);

				// this page was used need to issue fake write
				if(temp.compare("1") == 0)
				{
					pAddr = (row * BLOCK_SIZE + column * NV_PAGE_SIZE);
					vAddr = tempMap[pAddr];
					ChannelPacket *tempPacket = Ftl::translate(WRITE, vAddr, pAddr);
					controller->writeToPackage(tempPacket);
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

		restore_file.close();
		loaded = true;
	}
}

void Ftl::queuesNotFull(void)
{
    queues_full = false;
}
