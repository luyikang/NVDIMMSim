//Controller.cpp
//Class files for controller

#include "Controller.h"
#include "NVDIMM.h"

using namespace NVDSim;

Controller::Controller(NVDIMM* parent, Logger* l){
	parentNVDIMM = parent;
	log = l;

	channelBeatsLeft = vector<uint>(NUM_PACKAGES, 0);

	readQueues = vector<list <ChannelPacket *> >(NUM_PACKAGES, list<ChannelPacket *>());
	writeQueues = vector<list <ChannelPacket *> >(NUM_PACKAGES, list<ChannelPacket *>());
	outgoingPackets = vector<ChannelPacket *>(NUM_PACKAGES, 0);

	pendingPackets = vector<list <ChannelPacket *> >(NUM_PACKAGES, list<ChannelPacket *>());

	paused = new bool [NUM_PACKAGES];
	for(uint64_t i = 0; i < NUM_PACKAGES; i++)
	{
	    paused[i] = false;
	}

	currentClockCycle = 0;
}

void Controller::attachPackages(vector<Package> *packages){
	this->packages= packages;
}

void Controller::returnReadData(const FlashTransaction  &trans){
	if(parentNVDIMM->ReturnReadData!=NULL){
	    (*parentNVDIMM->ReturnReadData)(parentNVDIMM->systemID, trans.address, currentClockCycle, true);
	}
	parentNVDIMM->numReads++;
}

void Controller::returnUnmappedData(const FlashTransaction  &trans){
	if(parentNVDIMM->ReturnReadData!=NULL){
	    (*parentNVDIMM->ReturnReadData)(parentNVDIMM->systemID, trans.address, currentClockCycle, false);
	}
	parentNVDIMM->numReads++;
}

void Controller::returnCritLine(ChannelPacket *busPacket){
	if(parentNVDIMM->CriticalLineDone!=NULL){
	    (*parentNVDIMM->CriticalLineDone)(parentNVDIMM->systemID, busPacket->virtualAddress, currentClockCycle, true);
	}
}

void Controller::returnPowerData(vector<double> idle_energy, vector<double> access_energy, vector<double> erase_energy,
		vector<double> vpp_idle_energy, vector<double> vpp_access_energy, vector<double> vpp_erase_energy) {
	if(parentNVDIMM->ReturnPowerData!=NULL){
		vector<vector<double>> power_data = vector<vector<double>>(6, vector<double>(NUM_PACKAGES, 0.0));
		for(uint i = 0; i < NUM_PACKAGES; i++)
		{
			power_data[0][i] = idle_energy[i] * VCC;
			power_data[1][i] = access_energy[i] * VCC;
			power_data[2][i] = erase_energy[i] * VCC;
			power_data[3][i] = vpp_idle_energy[i] * VPP;
			power_data[4][i] = vpp_access_energy[i] * VPP;
			power_data[5][i] = vpp_erase_energy[i] * VPP;
		}
		(*parentNVDIMM->ReturnPowerData)(parentNVDIMM->systemID, power_data, currentClockCycle, false);
	}
}

void Controller::returnPowerData(vector<double> idle_energy, vector<double> access_energy, vector<double> vpp_idle_energy,
		vector<double> vpp_access_energy) {
	if(parentNVDIMM->ReturnPowerData!=NULL){
		vector<vector<double>> power_data = vector<vector<double>>(4, vector<double>(NUM_PACKAGES, 0.0));
		for(uint i = 0; i < NUM_PACKAGES; i++)
		{
			power_data[0][i] = idle_energy[i] * VCC;
			power_data[1][i] = access_energy[i] * VCC;
			power_data[2][i] = vpp_idle_energy[i] * VPP;
			power_data[3][i] = vpp_access_energy[i] * VPP;
		}
		(*parentNVDIMM->ReturnPowerData)(parentNVDIMM->systemID, power_data, currentClockCycle, false);
	}
}

void Controller::returnPowerData(vector<double> idle_energy, vector<double> access_energy, vector<double> erase_energy) {
	if(parentNVDIMM->ReturnPowerData!=NULL){
		vector<vector<double>> power_data = vector<vector<double>>(3, vector<double>(NUM_PACKAGES, 0.0));
		for(uint i = 0; i < NUM_PACKAGES; i++)
		{
			power_data[0][i] = idle_energy[i] * VCC;
			power_data[1][i] = access_energy[i] * VCC;
			power_data[2][i] = erase_energy[i] * VCC;
		}
		(*parentNVDIMM->ReturnPowerData)(parentNVDIMM->systemID, power_data, currentClockCycle, false);
	}
}

void Controller::returnPowerData(vector<double> idle_energy, vector<double> access_energy) {
	if(parentNVDIMM->ReturnPowerData!=NULL){
		vector<vector<double>> power_data = vector<vector<double>>(2, vector<double>(NUM_PACKAGES, 0.0));
		for(uint i = 0; i < NUM_PACKAGES; i++)
		{
			power_data[0][i] = idle_energy[i] * VCC;
			power_data[1][i] = access_energy[i] * VCC;
		}
		(*parentNVDIMM->ReturnPowerData)(parentNVDIMM->systemID, power_data, currentClockCycle, false);
	}
}

void Controller::receiveFromChannel(ChannelPacket *busPacket){
	// READ is now done. Log it and call delete
	if(LOGGING == true)
	{
		log->access_stop(busPacket->virtualAddress, busPacket->physicalAddress);
	}

	// Put in the returnTransaction queue 
	switch (busPacket->busPacketType)
	{
		case READ:
			returnTransaction.push_back(FlashTransaction(RETURN_DATA, busPacket->virtualAddress, busPacket->data));
			break;
		case GC_READ:
			// Nothing to do.
			break;
		default:
			ERROR("Illegal busPacketType " << busPacket->busPacketType << " in Controller::receiveFromChannel\n");
			break;
	}

	// Delete the ChannelPacket since READ is done. This must be done to prevent memory leaks.
	delete(busPacket);
}

// this is only called on a write as the name suggests
bool Controller::checkQueueWrite(ChannelPacket *p)
{
    if(CTRL_SCHEDULE)
    {
	if ((writeQueues[p->package].size() + 1 < CTRL_WRITE_QUEUE_LENGTH) || (CTRL_WRITE_QUEUE_LENGTH == 0))
	    return true;
	else
	    return false;
    }
    else
    {
	if ((readQueues[p->package].size() + 1 < CTRL_READ_QUEUE_LENGTH) || (CTRL_READ_QUEUE_LENGTH == 0))
	    return true;
	else
	    return false;
    }
}

bool Controller::addPacket(ChannelPacket *p){
    if(CTRL_SCHEDULE)
    {
	// If there is not room in the command queue for this packet, then return false.
	// If CTRL_QUEUE_LENGTH is 0, then infinite queues are allowed.
	switch (p->busPacketType)
	{
	case READ:
        case ERASE:
	    if ((readQueues[p->package].size() < CTRL_READ_QUEUE_LENGTH) || (CTRL_READ_QUEUE_LENGTH == 0))
		readQueues[p->package].push_back(p);
	    else	
	        return false;
	    break;
        case WRITE:
        case DATA:
	     if ((writeQueues[p->package].size() < CTRL_WRITE_QUEUE_LENGTH) || (CTRL_WRITE_QUEUE_LENGTH == 0))
	     {
		 // search the write queue to check if this write overwrites some other write
		 // this should really only happen if we're doing in place writing though (no gc)
		 if(!GARBAGE_COLLECT)
		 {
		     list<ChannelPacket *>::iterator it;
		     for (it = writeQueues[p->package].begin(); it != writeQueues[p->package].end(); it++)
		     {
			 if((*it)->virtualAddress == p->virtualAddress && (*it)->busPacketType == p->busPacketType)
			 {
			     if(LOGGING)
			     {		
				 // access_process for that write is called here since its over now.
				 log->access_process((*it)->virtualAddress, (*it)->physicalAddress, (*it)->package, WRITE);
				 
				 // stop_process for that write is called here since its over now.
				 log->access_stop((*it)->virtualAddress, (*it)->physicalAddress);
			     }
			     //call write callback
			     if (parentNVDIMM->WriteDataDone != NULL){
				 (*parentNVDIMM->WriteDataDone)(parentNVDIMM->systemID, (*it)->virtualAddress, currentClockCycle,true);
			     }
			     writeQueues[(*it)->package].erase(it, it++);
			     break;
			 }
		     }   
		 }
		 writeQueues[p->package].push_back(p);
		 break;
	     }
	     else
	     {
		 return false;
	     }
	case GC_READ:
	case GC_WRITE:
	    // Try to push the gc stuff to the front of the read queue in order to give them priority
	    if ((readQueues[p->package].size() < CTRL_READ_QUEUE_LENGTH) || (CTRL_READ_QUEUE_LENGTH == 0))
		readQueues[p->package].push_front(p);	
	    else
		return false;
	    break;
	default:
	    ERROR("Illegal busPacketType " << p->busPacketType << " in Controller::receiveFromChannel\n");
	    break;
	}
    
	if(LOGGING && QUEUE_EVENT_LOG)
	{
	    switch (p->busPacketType)
	    {
	    case READ:
	    case GC_READ:
	    case GC_WRITE:
	    case ERASE:
		log->log_ctrl_queue_event(false, p->package, &readQueues[p->package]);
		break;
	    case WRITE:
	    case DATA:
		log->log_ctrl_queue_event(true, p->package, &writeQueues[p->package]);
		break;
	    default:
		ERROR("Illegal busPacketType " << p->busPacketType << " in Controller::receiveFromChannel\n");
		break;
	    }
	}
	return true;
    }
    // Not scheduling so everything goes to the read queue
    else
    {
	if ((readQueues[p->package].size() < CTRL_READ_QUEUE_LENGTH) || (CTRL_READ_QUEUE_LENGTH == 0))
	{
	    readQueues[p->package].push_back(p);
    
	    if(LOGGING && QUEUE_EVENT_LOG)
	    {
		log->log_ctrl_queue_event(false, p->package, &readQueues[p->package]);
	    }
	    return true;
	}
	else
	{
	    return false;
	}
    }
}

void Controller::update(void){
    // schedule the next operation for each die
    if(CTRL_SCHEDULE)
    {
	bool write_queue_handled = false;
	uint64_t i;	
	//loop through the channels to find a packet for each
	for (i = 0; i < NUM_PACKAGES; i++){
	    // do we need to issue a write
	    if((CTRL_WRITE_ON_QUEUE_SIZE == true && writeQueues[i].size() >= CTRL_WRITE_QUEUE_LIMIT) ||
	       (CTRL_WRITE_ON_QUEUE_SIZE == false && writeQueues[i].size() >= CTRL_WRITE_QUEUE_LENGTH-1))
	    {
		if (!writeQueues[i].empty() && outgoingPackets[i]==NULL){
		    //if we can get the channel
		    if ((*packages)[i].channel->obtainChannel(0, CONTROLLER, writeQueues[i].front())){
			outgoingPackets[i] = writeQueues[i].front();
			if(LOGGING && QUEUE_EVENT_LOG)
			{
			    log->log_ctrl_queue_event(true, writeQueues[i].front()->package, &writeQueues[i]);
			}
			writeQueues[i].pop_front();
			parentNVDIMM->queuesNotFull();
			
			switch (outgoingPackets[i]->busPacketType){
			case DATA:
			    // Note: NV_PAGE_SIZE is multiplied by 8192 since the parameter is given in KB and this is how many bits
			    // are in 1 KB (1024 * 8).
			    channelBeatsLeft[i] = divide_params((NV_PAGE_SIZE*8192),CHANNEL_WIDTH); 
			    break;
			default:
			    channelBeatsLeft[i] = divide_params(COMMAND_LENGTH,CHANNEL_WIDTH);
			    break;
			}
		    }
		}	
	    }
	    // if we don't have to issue a write check to see if there is a read to send
	    else if (!readQueues[i].empty() && outgoingPackets[i]==NULL){
		if(queue_access_counter == 0 && readQueues[i].front()->busPacketType != GC_READ && 
		   readQueues[i].front()->busPacketType != GC_WRITE && !writeQueues[i].empty())
		{
		    //see if this read can be satisfied by something in the write queue
		    list<ChannelPacket *>::iterator it;
		    for (it = writeQueues[i].begin(); it != writeQueues[i].end(); it++)
		    {
			if((*it)->virtualAddress == readQueues[i].front()->virtualAddress)
			{
			    if(LOGGING)
			    {		
				// access_process for the read we're satisfying  is called here since we're doing it here.
				log->access_process(readQueues[i].front()->virtualAddress, readQueues[i].front()->physicalAddress, 
						    readQueues[i].front()->package, READ);
			    }
			    queue_access_counter = QUEUE_ACCESS_TIME;
			    write_queue_handled = true;
			    break;
			}
		    }
		}
		else if(queue_access_counter > 0)
		{
		    queue_access_counter--;
		    write_queue_handled = true;
		    if(queue_access_counter == 0)
		    {
			if(LOGGING)
			{
			    // stop_process for this read is called here since this ends now.
			    log->access_stop(readQueues[i].front()->virtualAddress, readQueues[i].front()->virtualAddress);
			}

			returnReadData(FlashTransaction(RETURN_DATA, readQueues[i].front()->virtualAddress, readQueues[i].front()->data));
			readQueues[i].pop_front();
		    }
		}
		else if(!write_queue_handled)
		{
		    //if we can get the channel
		    if ((*packages)[i].channel->obtainChannel(0, CONTROLLER, readQueues[i].front())){
			outgoingPackets[i] = readQueues[i].front();
			if(LOGGING && QUEUE_EVENT_LOG)
			{
			    log->log_ctrl_queue_event(false, readQueues[i].front()->package, &readQueues[i]);
			}
			readQueues[i].pop_front();
			parentNVDIMM->queuesNotFull();
			
			channelBeatsLeft[i] = divide_params(COMMAND_LENGTH,CHANNEL_WIDTH);
		    }
		}
	    }
	    // if there are no reads to send see if we're allowed to send a write instead
	    else if (CTRL_IDLE_WRITE == true && !writeQueues[i].empty() && outgoingPackets[i]==NULL){
		//if we can get the channel
		if ((*packages)[i].channel->obtainChannel(0, CONTROLLER, writeQueues[i].front())){
		    outgoingPackets[i] = writeQueues[i].front();
		    if(LOGGING && QUEUE_EVENT_LOG)
		    {
			log->log_ctrl_queue_event(true, writeQueues[i].front()->package, &writeQueues[i]);
		    }
		    writeQueues[i].pop_front();
		    parentNVDIMM->queuesNotFull();
		    
		    switch (outgoingPackets[i]->busPacketType){
		    case DATA:
			// Note: NV_PAGE_SIZE is multiplied by 8192 since the parameter is given in KB and this is how many bits
			// are in 1 KB (1024 * 8).
			channelBeatsLeft[i] = divide_params((NV_PAGE_SIZE*8192),CHANNEL_WIDTH); 
			break;
		    default:
			channelBeatsLeft[i] = divide_params(COMMAND_LENGTH,CHANNEL_WIDTH);
			break;
		    }
		}
	    }
	}
    }
    // not scheduling so everything comes from the read queue
    else
    {
	uint64_t i;	
	//Look through queues and send oldest packets to the appropriate channel
	for (i = 0; i < NUM_PACKAGES; i++){
	    if (!readQueues[i].empty() && outgoingPackets[i]==NULL){
		//if we can get the channel
		if ((*packages)[i].channel->obtainChannel(0, CONTROLLER, readQueues[i].front())){
		    outgoingPackets[i] = readQueues[i].front();
		    if(LOGGING && QUEUE_EVENT_LOG)
		    {
			switch (readQueues[i].front()->busPacketType)
			{
			case READ:
			case GC_READ:
			case ERASE:
			    log->log_ctrl_queue_event(false, readQueues[i].front()->package, &readQueues[i]);
			    break;
			case WRITE:
			case GC_WRITE:
			case DATA:
			    log->log_ctrl_queue_event(true, readQueues[i].front()->package, &readQueues[i]);
			    break;
			}
		    }
		    readQueues[i].pop_front();
		    parentNVDIMM->queuesNotFull();
		    switch (outgoingPackets[i]->busPacketType){
		    case DATA:
			// Note: NV_PAGE_SIZE is multiplied by 8192 since the parameter is given in KB and this is how many bits
			// are in 1 KB (1024 * 8).
			channelBeatsLeft[i] = divide_params((NV_PAGE_SIZE*8192),CHANNEL_WIDTH); 
			break;
		    default:
			channelBeatsLeft[i] = divide_params(COMMAND_LENGTH,CHANNEL_WIDTH);
			break;
		    }
		}
	    }
	}
    }
	
    //Use the buffer code for the NVDIMMS to calculate the actual transfer time
    if(BUFFERED)
    {	
	uint64_t i;
	//Check for commands/data on a channel. If there is, see if it is done on channel
	for (i= 0; i < outgoingPackets.size(); i++){
	    if (outgoingPackets[i] != NULL){
		if(paused[outgoingPackets[i]->package] == true && 
		   !(*packages)[outgoingPackets[i]->package].channel->isBufferFull(CONTROLLER, outgoingPackets[i]->die))
		{
		    if ((*packages)[outgoingPackets[i]->package].channel->obtainChannel(0, CONTROLLER, outgoingPackets[i])){
			paused[outgoingPackets[i]->package] = false;
		    }
		}
		if ((*packages)[outgoingPackets[i]->package].channel->hasChannel(CONTROLLER, 0) && paused[outgoingPackets[i]->package] == false){
		    if (channelBeatsLeft[i] == 0){
			(*packages)[outgoingPackets[i]->package].channel->releaseChannel(CONTROLLER, 0);
			pendingPackets[i].push_back(outgoingPackets[i]);
			outgoingPackets[i] = NULL;
		    }else if ((*packages)[outgoingPackets[i]->package].channel->notBusy()){
			if(!(*packages)[outgoingPackets[i]->package].channel->isBufferFull(CONTROLLER, outgoingPackets[i]->die))
			{
			    (*packages)[outgoingPackets[i]->package].channel->sendPiece(CONTROLLER, outgoingPackets[i]->busPacketType, 
											outgoingPackets[i]->die, outgoingPackets[i]->plane);
			    channelBeatsLeft[i]--;
			}
			else
			{
			    (*packages)[outgoingPackets[i]->package].channel->releaseChannel(CONTROLLER, 0);
			    paused[outgoingPackets[i]->package] = true;
			}
		    }
		}
	    }
	}
	//Directly calculate the expected transfer time 
    }
    else
    {
	// BUFFERED NOT TRUE CASE...
	uint64_t i;
	//Check for commands/data on a channel. If there is, see if it is done on channel
	for (i= 0; i < outgoingPackets.size(); i++){
	    if (outgoingPackets[i] != NULL && (*packages)[outgoingPackets[i]->package].channel->hasChannel(CONTROLLER, 0)){
		channelBeatsLeft[i]--;
		if (channelBeatsLeft[i] == 0){
		    (*packages)[outgoingPackets[i]->package].channel->sendToBuffer(outgoingPackets[i]);
		    (*packages)[outgoingPackets[i]->package].channel->releaseChannel(CONTROLLER, 0);
		    outgoingPackets[i] = NULL;
		}
	    }
	}
    }

    //See if any read data is ready to return
    while (!returnTransaction.empty()){
	//call return callback
	returnReadData(returnTransaction.back());
	returnTransaction.pop_back();
    }
}

void Controller::sendQueueLength(void)
{
	vector<uint64_t> temp = vector<uint64_t>(writeQueues.size(),0);
	for(uint i = 0; i < writeQueues.size(); i++)
	{
		temp[i] = writeQueues[i].size();
	}
	if(LOGGING == true)
	{
		log->ctrlQueueLength(temp);
	}
}

void Controller::writeToPackage(ChannelPacket *packet)
{
	(*packages)[packet->package].dies[packet->die]->writeToPlane(packet);
}

void Controller::bufferDone(uint64_t package, uint64_t die, uint64_t plane)
{
	for (uint i = 0; i < pendingPackets.size(); i++){
		std::list<ChannelPacket *>::iterator it;
		for(it = pendingPackets[i].begin(); it != pendingPackets[i].end(); it++){
		    if ((*it) != NULL && (*it)->package == package && (*it)->die == die && (*it)->plane == plane){
				(*packages)[(*it)->package].channel->sendToBuffer((*it));
				pendingPackets[i].erase(it);
				break;
		    }
		}
	}
}
