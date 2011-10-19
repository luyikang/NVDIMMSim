//Controller.cpp
//Class files for controller

#include "Controller.h"
#include "NVDIMM.h"

using namespace NVDSim;

Controller::Controller(NVDIMM* parent, Logger* l){
	parentNVDIMM = parent;
	log = l;

	channelBeatsLeft = vector<uint>(NUM_PACKAGES, 0);

	channelQueues = vector<list <ChannelPacket *> >(NUM_PACKAGES, list<ChannelPacket *>());
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
    if (!((channelQueues[p->package].size() + 1 < CTRL_WRITE_QUEUE_LENGTH) || (CTRL_WRITE_QUEUE_LENGTH == 0)))
	return false;
    else
	return true;
}

bool Controller::addPacket(ChannelPacket *p){
    // If there is not room in the command queue for this packet, then return false.
    // If CTRL_QUEUE_LENGTH is 0, then infinite queues are allowed.
    switch (p->busPacketType)
    {
	case READ:
        case GC_READ:
        case ERASE:
	    if (!((channelQueues[p->package].size() < CTRL_READ_QUEUE_LENGTH) || (CTRL_READ_QUEUE_LENGTH == 0)))
		return false;
	    break;
        case WRITE:
        case GC_WRITE:
        case DATA:
	    if (!((channelQueues[p->package].size() < CTRL_WRITE_QUEUE_LENGTH) || (CTRL_WRITE_QUEUE_LENGTH == 0)))
		return false;
            break;
	default:
	    ERROR("Illegal busPacketType " << p->busPacketType << " in Controller::receiveFromChannel\n");
	    break;
    }

    channelQueues[p->package].push_back(p);
    
    if(LOGGING && QUEUE_EVENT_LOG)
    {
	switch (p->busPacketType)
	{
	case READ:
        case GC_READ:
        case ERASE:
	    log->log_ctrl_queue_event(false, p->package, &channelQueues[p->package]);
	    break;
        case WRITE:
        case GC_WRITE:
        case DATA:
	    log->log_ctrl_queue_event(true, p->package, &channelQueues[p->package]);
            break;
	default:
	    ERROR("Illegal busPacketType " << p->busPacketType << " in Controller::receiveFromChannel\n");
	    break;
	}
    }
    return true;
}

void Controller::update(void){
	//Use the buffer code for the NVDIMMS to calculate the actual transfer time
	if(BUFFERED == true)
	{
		uint64_t i;	
		//Look through queues and send oldest packets to the appropriate channel
		for (i = 0; i < channelQueues.size(); i++){
			if (!channelQueues[i].empty() && outgoingPackets[i]==NULL){
			    //if we can get the channel
			    if ((*packages)[i].channel->obtainChannel(0, CONTROLLER, channelQueues[i].front())){
				outgoingPackets[i]= channelQueues[i].front();
				if(LOGGING && QUEUE_EVENT_LOG)
				{
				    switch (channelQueues[i].front()->busPacketType)
				    {
				    case READ:
				    case GC_READ:
				    case ERASE:
					log->log_ctrl_queue_event(false, channelQueues[i].front()->package, &channelQueues[i]);
					break;
				    case WRITE:
				    case GC_WRITE:
				    case DATA:
					log->log_ctrl_queue_event(true, channelQueues[i].front()->package, &channelQueues[i]);
					break;
				    }
				}
				channelQueues[i].pop_front();
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
	
		
		//Check for commands/data on a channel. If there is, see if it is done on channel
		for (i= 0; i < outgoingPackets.size(); i++){
		    if (outgoingPackets[i] != NULL){
			if(paused[outgoingPackets[i]->package] == true && 
			   !(*packages)[outgoingPackets[i]->package].channel->isBufferFull(CONTROLLER, outgoingPackets[i]->die))
			{
			    if ((*packages)[outgoingPackets[i]->package].channel->obtainChannel(0, CONTROLLER, outgoingPackets[i])){
				paused[outgoingPackets[i]->package] = false;
				//cout << "paused is now " << paused[outgoingPackets[i]->package] << "\n";
			    }
			}
			if ((*packages)[outgoingPackets[i]->package].channel->hasChannel(CONTROLLER, 0) && paused[outgoingPackets[i]->package] == false){
			    if (channelBeatsLeft[i] == 0){
				//if(outgoingPackets[i]->package == 15)
				//{
				    //cout << "finished a packet in the controller \n";
				    //cout << "type was " << outgoingPackets[i]->busPacketType << " package was " << outgoingPackets[i]->package << "\n";
				//}
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
	    
	    //Look through queues and send oldest packets to the appropriate channel
	    for (i = 0; i < channelQueues.size(); i++){
		if (!channelQueues[i].empty() && outgoingPackets[i]==NULL){
		    //if we can get the channel
		    if ((*packages)[i].channel->obtainChannel(0, CONTROLLER, channelQueues[i].front())){
			outgoingPackets[i] = channelQueues[i].front();
			if(LOGGING && QUEUE_EVENT_LOG)
			{
			    switch (channelQueues[i].front()->busPacketType)
			    {
			    case READ:
			    case GC_READ:
			    case ERASE:
				log->log_ctrl_queue_event(false, channelQueues[i].front()->package, &channelQueues[i]);
				break;
			    case WRITE:
			    case GC_WRITE:
			    case DATA:
				log->log_ctrl_queue_event(true, channelQueues[i].front()->package, &channelQueues[i]);
				break;
			    }
			}
			channelQueues[i].pop_front();
			parentNVDIMM->queuesNotFull();
			switch (outgoingPackets[i]->busPacketType){
			case DATA:
			    // Note: NV_PAGE_SIZE is multiplied by 8192 since the parameter is given in KB and this is how many bits
			    // are in 1 KB (1024 * 8).
			    channelBeatsLeft[i] = (divide_params((NV_PAGE_SIZE*8192),DEVICE_WIDTH) * DEVICE_CYCLE) / CYCLE_TIME;
			    break;
			default:
			    channelBeatsLeft[i] = (divide_params(COMMAND_LENGTH,DEVICE_WIDTH) * DEVICE_CYCLE) / CYCLE_TIME;
			    break;
			}
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
	vector<uint64_t> temp = vector<uint64_t>(channelQueues.size(),0);
	for(uint i = 0; i < channelQueues.size(); i++)
	{
		temp[i] = channelQueues[i].size();
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
    cout << "called buffer done for package " << package << "\n";
	for (uint i = 0; i < pendingPackets.size(); i++){
		std::list<ChannelPacket *>::iterator it;
		for(it = pendingPackets[i].begin(); it != pendingPackets[i].end(); it++){
		    if ((*it) != NULL && (*it)->package == package && (*it)->die == die && (*it)->plane == plane){
			cout << "packet type was " << (*it)->busPacketType << "\n";
				(*packages)[(*it)->package].channel->sendToBuffer((*it));
				pendingPackets[i].erase(it);
				break;
		    }
		}
	}
}

void Controller::releaseChannel(uint64_t package)
{
    
}
