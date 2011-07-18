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

	sending = false;

	planes= vector<Plane>(PLANES_PER_DIE, Plane());

	currentCommands= vector<ChannelPacket *>(PLANES_PER_DIE, NULL);

	dataCyclesLeft= 0;
	deviceBeatsLeft= 0;
	controlCyclesLeft= new uint[PLANES_PER_DIE];

	currentClockCycle= 0;
}

void Die::attachToBuffer(Buffer *buff){
	buffer = buff;
}

void Die::receiveFromBuffer(ChannelPacket *busPacket){
	if (busPacket->busPacketType == DATA){
		planes[busPacket->plane].storeInData(busPacket);
	} else if (currentCommands[busPacket->plane] == NULL) {
		currentCommands[busPacket->plane] = busPacket;
		switch (busPacket->busPacketType){
			case READ:
				if(DEVICE_TYPE.compare("PCM") == 0)
				{
					controlCyclesLeft[busPacket->plane]= READ_TIME * ((NV_PAGE_SIZE*8192) / 8);			
				}
				else
				{
					controlCyclesLeft[busPacket->plane]= READ_TIME;
				}
				break;
			case GC_READ:
				if(DEVICE_TYPE.compare("PCM") == 0)
				{
					controlCyclesLeft[busPacket->plane]= READ_TIME * ((NV_PAGE_SIZE*8192) / 8);
				}
				else
				{
					controlCyclesLeft[busPacket->plane]= READ_TIME;
				}
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
				break;
			default:
				break;


			if (LOGGING)
			{
				// Tell the logger the access has now been processed.
				log->access_process(busPacket->virtualAddress, busPacket->physicalAddress, busPacket->package, busPacket->busPacketType);
			}
				
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

				// Process each command based on the packet type.
				switch (currentCommand->busPacketType){
					case GC_READ:
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
					case GC_WRITE:
						planes[currentCommand->plane].write(currentCommand);
						parentNVDIMM->numWrites++;
						break;
					case ERASE:
						planes[currentCommand->plane].erase(currentCommand);
						parentNVDIMM->numErases++;
						break;
					default:
						break;
				}

				if (currentCommand->busPacketType != READ)
				{
					// For everything but READ, the access is done at this point.
					// Note: for DATA_READ, this is handled in Controller::receiveFromChannel().

					// Tell the logger the access is done.
					if(LOGGING == true)
					{
						log->access_stop(currentCommand->virtualAddress);
					}

					// Delete the memory allocated for the current command to prevent memory leaks.
					delete currentCommand;
				}

				//sim output
				currentCommands[i]= NULL;
			}
			controlCyclesLeft[i]--;
		}
	}

	if (!returnDataPackets.empty()){
		if( BUFFERED == true)
		{
			if(dataCyclesLeft == 0 && deviceBeatsLeft > 0){
				deviceBeatsLeft--;
				buffer->sendPiece(BUFFER, 0, id, returnDataPackets.front()->plane);
				dataCyclesLeft = divide_params(DEVICE_CYCLE,CYCLE_TIME);
			}

			if(dataCyclesLeft > 0){
				dataCyclesLeft--;
			}

			if(deviceBeatsLeft == 0 && sending == false){
				dataCyclesLeft = divide_params(DEVICE_CYCLE,CYCLE_TIME);
				deviceBeatsLeft = divide_params((NV_PAGE_SIZE*8192),DEVICE_WIDTH);
				sending = true;
			}
		}else{
			if(buffer->channel->hasChannel(BUFFER, id)){
				if(dataCyclesLeft == 0){
					buffer->channel->sendToController(returnDataPackets.front());
					buffer->channel->releaseChannel(BUFFER, id);
					returnDataPackets.pop();
				}

				dataCyclesLeft--;
			}else{
				if(buffer->channel->obtainChannel(id, BUFFER, NULL))
				{
					dataCyclesLeft = (divide_params((NV_PAGE_SIZE*8192),DEVICE_WIDTH) * DEVICE_CYCLE) / CYCLE_TIME;
				}
			}
		}
	}
}

void Die::bufferDone()
{
	buffer->sendToController(returnDataPackets.front());
	returnDataPackets.pop();			
	sending = false;
}

void Die::writeToPlane(ChannelPacket *packet)
{
	ChannelPacket *temp = new ChannelPacket(DATA, packet->virtualAddress, packet->physicalAddress, packet->page, packet->block, packet->plane, packet->die, packet->package, NULL);
	planes[packet->plane].storeInData(temp);
	planes[packet->plane].write(packet);
}
