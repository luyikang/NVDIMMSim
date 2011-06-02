//Controller.cpp
//Class files for controller

#include "Controller.h"
#include "NVDIMM.h"

using namespace NVDSim;

Controller::Controller(NVDIMM* parent, Logger* l){
	parentNVDIMM = parent;
	log = l;

	channelXferCyclesLeft = vector<uint>(NUM_PACKAGES, 0);
	channelBeatsLeft = vector<uint>(NUM_PACKAGES, 0);

	channelQueues = vector<queue <ChannelPacket *> >(NUM_PACKAGES, queue<ChannelPacket *>());
	outgoingPackets = vector<ChannelPacket *>(NUM_PACKAGES, NULL);
	pendingPackets = vector<ChannelPacket *>(NUM_PACKAGES, NULL);

	currentClockCycle = 0;
}

void Controller::attachPackages(vector<Package> *packages){
	this->packages= packages;
}

void Controller::returnReadData(const FlashTransaction  &trans){
        log->access_stop(trans.address);
	if(parentNVDIMM->ReturnReadData!=NULL){
		(*parentNVDIMM->ReturnReadData)(parentNVDIMM->systemID, trans.address, currentClockCycle);
	}
	parentNVDIMM->numReads++;
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
		(*parentNVDIMM->ReturnPowerData)(parentNVDIMM->systemID, power_data, currentClockCycle);
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
		(*parentNVDIMM->ReturnPowerData)(parentNVDIMM->systemID, power_data, currentClockCycle);
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
		(*parentNVDIMM->ReturnPowerData)(parentNVDIMM->systemID, power_data, currentClockCycle);
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
		(*parentNVDIMM->ReturnPowerData)(parentNVDIMM->systemID, power_data, currentClockCycle);
  }
}

void Controller::receiveFromChannel(ChannelPacket *busPacket){
	if (busPacket->busPacketType == READ)
		returnTransaction.push_back(FlashTransaction(RETURN_DATA, busPacket->physicalAddress, busPacket->data));
	else
		returnTransaction.push_back(FlashTransaction(GC_DATA, busPacket->physicalAddress, busPacket->data));
	delete(busPacket);
}

bool Controller::addPacket(ChannelPacket *p){
    if (channelQueues[p->package].size() >= CTRL_QUEUE_LENGTH && CTRL_QUEUE_LENGTH != 0)
    {
	return false;
    }
    else
    {
	channelQueues[p->package].push(p);
	return true;
    }
}

void Controller::update(void){
	uint i;
		
        //Look through queues and send oldest packets to the appropriate channel
	for (i = 0; i < channelQueues.size(); i++){
		if (!channelQueues[i].empty() && outgoingPackets[i]==NULL){
			//if we can get the channel (channel contention not implemented yet)
			if ((*packages)[i].channel->obtainChannel(0, CONTROLLER, channelQueues[i].front())){
				outgoingPackets[i]= channelQueues[i].front();
				channelQueues[i].pop();
				switch (outgoingPackets[i]->busPacketType){
					case DATA:
					        channelXferCyclesLeft[i] = divide_params(CHANNEL_CYCLE,CYCLE_TIME); //system cycles per channel beat
					        channelBeatsLeft[i] = divide_params((NV_PAGE_SIZE*8192),CHANNEL_WIDTH); //channel pieces per page
						break;
					default:
					        channelXferCyclesLeft[i] = divide_params(CHANNEL_CYCLE,CYCLE_TIME);
					        channelBeatsLeft[i] = divide_params(COMMAND_LENGTH,CHANNEL_WIDTH);
						break;
				}
			}
		}
	}

	//Check for commands/data on a channel. If there is, see if it is done on channel
	for (i= 0; i < outgoingPackets.size(); i++){
		if (outgoingPackets[i] != NULL && (*packages)[outgoingPackets[i]->package].channel->hasChannel(CONTROLLER, 0)){
		        if (channelBeatsLeft[i] == 0 && (*packages)[outgoingPackets[i]->package].channel->notBusy() 
			    && pendingPackets[i] == NULL){
				(*packages)[outgoingPackets[i]->package].channel->releaseChannel(CONTROLLER, 0);
				cout << "the setting outgoing die is " << outgoingPackets[i]->die << " the setting outgoing plane is " << outgoingPackets[i]->plane << "\n";
				cout << "outgoing packet type is " << outgoingPackets[i]->busPacketType << "\n";
				pendingPackets[i] = outgoingPackets[i];
				cout << "the set pending die is " << pendingPackets[i]->die << " the set pending plane is " << pendingPackets[i]->plane << "\n";
				outgoingPackets[i] = NULL;
				cout << "the post pending die is " << pendingPackets[i]->die << " the post pending plane is " << pendingPackets[i]->plane << "\n";
			}
			if (channelXferCyclesLeft[i] <= 0 && channelBeatsLeft[i] > 0){
			        if(outgoingPackets[i]->busPacketType == DATA)
				{
				    (*packages)[outgoingPackets[i]->package].channel->sendPiece(CONTROLLER, 0, outgoingPackets[i]->plane, outgoingPackets[i]->die);
				    // cout << "the send piece outgoing die is " << outgoingPackets[i]->die << " the setting outgoing plane is " << outgoingPackets[i]->plane << "\n";
				}else{
				    cout << "sending the command \n";
				    (*packages)[outgoingPackets[i]->package].channel->sendPiece(CONTROLLER, 1, outgoingPackets[i]->plane, outgoingPackets[i]->die);
				}
			        channelBeatsLeft[i]--;
				channelXferCyclesLeft[i] = divide_params(CHANNEL_CYCLE,CYCLE_TIME);
			}
		        channelXferCyclesLeft[i]--;
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
    log->ctrlQueueLength(temp);
}

void Controller::writeToPackage(ChannelPacket *packet)
{
    (*packages)[packet->package].dies[packet->die]->writeToPlane(packet);
}

void Controller::channelDone(uint die, uint plane)
{
    cout << "got to the controller's channel done \n";
    cout << "the sent die is " << die << " the sent plane is " << plane << "\n";
    for (uint i = 0; i < pendingPackets.size(); i++){
	if (pendingPackets[i] != NULL){
	    cout << "the pending die is " << pendingPackets[i]->die << "the pending plane is " << pendingPackets[i]->plane << "\n";
	}
	if (pendingPackets[i] != NULL) && pendingPackets[i]->die == die && pendingPackets[i]->plane == plane){
	    cout << "okay so we're sending \n";
	    (*packages)[pendingPackets[i]->package].channel->sendToDie(pendingPackets[i]);
	    (*packages)[pendingPackets[i]->package].channel->acknowledge(die, plane);
	    pendingPackets[i] = NULL;
	}
    }
}
