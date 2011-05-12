//Die.cpp
//class file for die class

#include "Die.h"
#include "Channel.h"
#include "Controller.h"
#include "NVDIMM.h"

using namespace NVDSim;
using namespace std;

Die::Die(NVDIMM *parent, Logger *l, uint idNum){
	id = idNum;
	parentNVDIMM = parent;
	log = l;

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
			     if(DEVICE_TYPE.compare("PCM") == 0)
			     {
				 controlCyclesLeft[busPacket->plane]= READ_TIME * (NV_PAGE_SIZE / 8);
			     }
			     else
			     {
				 controlCyclesLeft[busPacket->plane]= READ_TIME;
			     }
			     //update the logger
			     log->access_process(busPacket->virtualAddress, busPacket->package, READ);
			     break;
			 case GC_READ:
			     if(DEVICE_TYPE.compare("PCM") == 0)
			     {
				 controlCyclesLeft[busPacket->plane]= READ_TIME * (NV_PAGE_SIZE / 8);
			     }
			     else
			     {
				 controlCyclesLeft[busPacket->plane]= READ_TIME;
			     }
			     //update the logger
			     log->access_process(busPacket->virtualAddress, busPacket->package, GC_READ);
			     break;
			 case WRITE:
			     if(DEVICE_TYPE.compare("PCM") == 0 && GARBAGE_COLLECT == 0)
			     {
			         controlCyclesLeft[busPacket->plane]= ERASE_TIME;
			     }
			     else
			     {
				 controlCyclesLeft[busPacket->plane]= WRITE_TIME;
			     }
			     //update the logger
			     log->access_process(busPacket->virtualAddress, busPacket->package, WRITE);
			     break;
			 case GC_WRITE:
			     if(DEVICE_TYPE.compare("PCM") == 0 && GARBAGE_COLLECT == 0)
			     {
			         controlCyclesLeft[busPacket->plane]= ERASE_TIME;
			     }
			     else
			     {
				 controlCyclesLeft[busPacket->plane]= WRITE_TIME;
			     }
			     //update the logger
			     log->access_process(busPacket->virtualAddress, busPacket->package, GC_WRITE);
			     break;
		         case FF_WRITE:
			     controlCyclesLeft[busPacket->plane]= 1;
		             break;
			 case ERASE:
			     if(DEVICE_TYPE.compare("PCM") == 0)
			     {
				 controlCyclesLeft[busPacket->plane]= ERASE_TIME * (BLOCK_SIZE / NV_PAGE_SIZE);
			     }
			     else
			     {
				 controlCyclesLeft[busPacket->plane]= ERASE_TIME;
			     }
			     //update the logger
			     log->access_process(busPacket->virtualAddress, busPacket->package, ERASE);
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
			         case GC_READ:
				     log->access_stop(currentCommand->virtualAddress, currentCommand->physicalAddress);
				 case READ:
					 planes[currentCommand->plane].read(currentCommand);
					 returnDataPackets.push(planes[currentCommand->plane].readFromData());
					 break;
				 case WRITE:
					 planes[currentCommand->plane].write(currentCommand);
					 parentNVDIMM->numWrites++;
					 log->access_stop(currentCommand->virtualAddress, currentCommand->physicalAddress);
					 //call write callback
					 if (parentNVDIMM->WriteDataDone != NULL){
						 (*parentNVDIMM->WriteDataDone)(parentNVDIMM->systemID, currentCommand->virtualAddress, currentClockCycle);
					 }
					 break;
				 case GC_WRITE:
					 planes[currentCommand->plane].write(currentCommand);
					 parentNVDIMM->numWrites++;
					 log->access_stop(currentCommand->virtualAddress, currentCommand->physicalAddress);
					 break;
			         case FF_WRITE:
				         planes[currentCommand->plane].write(currentCommand);
					 parentNVDIMM->numWrites++;
					 cout << "isued command to page" << currentCommand->page << "\n";
				         break;
				 case ERASE:
					 planes[currentCommand->plane].erase(currentCommand);
					 parentNVDIMM->numErases++;
					 log->access_stop(currentCommand->virtualAddress, currentCommand->physicalAddress);
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
