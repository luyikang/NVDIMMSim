//Controller.cpp
//Class files for controller

#include "Controller.h"
#include "NVDIMM.h"

using namespace NVDSim;

Controller::Controller(NVDIMM* parent, Logger* l){
	parentNVDIMM = parent;
	log = l;

	channelXferCyclesLeft = vector<uint>(NUM_PACKAGES, 0);

	channelQueues = vector<queue <ChannelPacket *> >(NUM_PACKAGES, queue<ChannelPacket *>());
	outgoingPackets = vector<ChannelPacket *>(NUM_PACKAGES, NULL);

	currentClockCycle = 0;
}

void Controller::attachPackages(vector<Package> *packages){
	this->packages= packages;
}

void Controller::returnReadData(const FlashTransaction  &trans){
    log->access_stop(trans.address, trans.address);
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
		returnTransaction.push_back(FlashTransaction(RETURN_DATA, busPacket->virtualAddress, busPacket->data));
	else
		returnTransaction.push_back(FlashTransaction(GC_DATA, busPacket->virtualAddress, busPacket->data));
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
	
	//Check for commands/data on a channel. If there is, see if it is done on channel
	for (i= 0; i < outgoingPackets.size(); i++){
		if (outgoingPackets[i] != NULL && (*packages)[outgoingPackets[i]->package].channel->hasChannel(CONTROLLER, 0)){

			channelXferCyclesLeft[i]--;
			if (channelXferCyclesLeft[i] == 0){
				(*packages)[outgoingPackets[i]->package].channel->sendToDie(outgoingPackets[i]);
				(*packages)[outgoingPackets[i]->package].channel->releaseChannel(CONTROLLER, 0);
				outgoingPackets[i]= NULL;
			}
		}
	}
	
	//Look through queues and send oldest packets to the appropriate channel
	for (i= 0; i < channelQueues.size(); i++){
		if (!channelQueues[i].empty() && outgoingPackets[i]==NULL){
			//if we can get the channel (channel contention not implemented yet)
			if ((*packages)[i].channel->obtainChannel(0, CONTROLLER, channelQueues[i].front())){
				outgoingPackets[i]= channelQueues[i].front();
				channelQueues[i].pop();
				switch (outgoingPackets[i]->busPacketType){
					case DATA:
						channelXferCyclesLeft[i]= DATA_TIME;
						break;
					default:
						channelXferCyclesLeft[i]= COMMAND_TIME;
						break;
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
    log->ctrlQueueLength(temp);
}
