//Die.cpp
//class file for die class

#include "Die.h"
#include "Channel.h"
#include "Controller.h"
#include "NVDIMM.h"

using namespace NVDSim;
using namespace std;

Die::Die(NVDIMM *parent, uint idNum){
	id = idNum;
	parentNVDIMM= parent;

	planes= vector<Plane>(PLANES_PER_DIE, Plane());

	currentCommands= vector<ChannelPacket *>(PLANES_PER_DIE, NULL);

	dataCyclesLeft= 0;
	controlCyclesLeft= vector<uint>(PLANES_PER_DIE, 0);

	currentClockCycle= 0;

}

void Die::attachToChannel(Channel *chan){
	channel= chan;
}

void Die::receiveFromChannel(ChannelPacket *busPacket){
	 if (busPacket->busPacketType == DATA){
		 planes[busPacket->plane].storeInData(busPacket);
	 } else if (currentCommands[busPacket->plane] == NULL) {
		 currentCommands[busPacket->plane] = busPacket;
		 switch (busPacket->busPacketType){
			 case READ:
				 controlCyclesLeft[busPacket->plane]= READ_TIME;
				 break;
			 case WRITE:
				 controlCyclesLeft[busPacket->plane]= WRITE_TIME;
				 break;
			 case ERASE:
				 controlCyclesLeft[busPacket->plane]= ERASE_TIME;
				 break;
			 default:
				 break;
		 }
	 } else{
		 ERROR("Die is busy");
		 exit(1);
	 }
}

int Die::isDieBusy(uint plane){
	if (currentCommands[plane] == NULL){
		return 0;
	}
	return 1;
}

void Die::update(void){
	uint i;
	ChannelPacket *currentCommand;

	for (i = 0 ; i < PLANES_PER_DIE ; i++){
		currentCommand = currentCommands[i];
	 if (currentCommand != NULL){
		 if (controlCyclesLeft[i] == 0){
			 switch (currentCommand->busPacketType){
				 case READ:
					 planes[currentCommand->plane].read(currentCommand);
					 returnDataPackets.push(planes[currentCommand->plane].readFromData());
					 break;
				 case WRITE:
					 planes[currentCommand->plane].write(currentCommand);
					 parentNVDIMM->numWrites++;
					 //call write callback
					 if (parentNVDIMM->WriteDataDone != NULL){
						 (*parentNVDIMM->WriteDataDone)(parentNVDIMM->systemID, currentCommand->virtualAddress, currentClockCycle);

					 }
					 break;
				 case ERASE:
					 planes[currentCommand->plane].erase(currentCommand);
					 parentNVDIMM->numErases++;
					 break;
				 default:
					 break;
			 }
			 //sim output
			 //currentCommand->print(currentClockCycle);
			 currentCommands[i]= NULL;
		 }
		 controlCyclesLeft[i]--;
	 }
	}

	if (!returnDataPackets.empty()){
		if (channel->hasChannel(DIE, id)){
			if (dataCyclesLeft == 0){
				channel->sendToController(returnDataPackets.front());
				channel->releaseChannel(DIE, id);
				returnDataPackets.pop();
			}
			dataCyclesLeft--;
		} else
			if (channel->obtainChannel(id, DIE, NULL))
				dataCyclesLeft= DATA_TIME;
	}
}
