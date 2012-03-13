/*********************************************************************************
*  Copyright (c) 2011-2012, Paul Tschirhart
*                             Peter Enns
*                             Jim Stevens
*                             Ishwar Bhati
*                             Mu-Tien Chang
*                             Bruce Jacob
*                             University of Maryland 
*                             pkt3c [at] umd [dot] edu
*  All rights reserved.
*  
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions are met:
*  
*     * Redistributions of source code must retain the above copyright notice,
*        this list of conditions and the following disclaimer.
*  
*     * Redistributions in binary form must reproduce the above copyright notice,
*        this list of conditions and the following disclaimer in the documentation
*        and/or other materials provided with the distribution.
*  
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
*  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
*  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
*  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
*  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
*  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
*  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
*  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
*  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*********************************************************************************/

//Die.cpp
//class file for die class

#include "Die.h"
#include "Channel.h"
#include "Controller.h"
#include "NVDIMM.h"

using namespace NVDSim;
using namespace std;

Die::Die(NVDIMM *parent, Logger *l, uint64_t idNum){
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

	critBeat = ((divide_params((NV_PAGE_SIZE*8192),DEVICE_WIDTH)-divide_params((uint)512,DEVICE_WIDTH)) * DEVICE_CYCLE) / CYCLE_TIME; // cache line is 64 bytes
}

void Die::attachToBuffer(Buffer *buff){
	buffer = buff;
}

void Die::receiveFromBuffer(ChannelPacket *busPacket){
	if (busPacket->busPacketType == DATA){
		planes[busPacket->plane].storeInData(busPacket);
	} else if (currentCommands[busPacket->plane] == NULL) {
		currentCommands[busPacket->plane] = busPacket;
		if (LOGGING)
		{
			// Tell the logger the access has now been processed.		        
			log->access_process(busPacket->virtualAddress, busPacket->physicalAddress, busPacket->package, busPacket->busPacketType);		
		}
		switch (busPacket->busPacketType){
			case READ:
			case GC_READ:
				controlCyclesLeft[busPacket->plane]= READ_TIME;
				cout << "physical address is " << busPacket->physicalAddress << "\n";
				// log the new state of this plane
				if(LOGGING && PLANE_STATE_LOG)
				{
				    if(busPacket->busPacketType == READ)
				    {
					log->log_plane_state(busPacket->virtualAddress, busPacket->package, busPacket->die, busPacket->plane, READING);
				    }
				    else if(busPacket->busPacketType == GC_READ)
				    {
					log->log_plane_state(busPacket->virtualAddress, busPacket->package, busPacket->die, busPacket->plane, GC_READING);
				    }
				}
				break;
			case WRITE:
			case GC_WRITE:
			        if((DEVICE_TYPE.compare("PCM") == 0 || DEVICE_TYPE.compare("P8P") == 0) && GARBAGE_COLLECT == 0)
				{
					controlCyclesLeft[busPacket->plane]= ERASE_TIME;
				}
				else
				{
					controlCyclesLeft[busPacket->plane]= WRITE_TIME;
				}
				// log the new state of this plane
				if(LOGGING && PLANE_STATE_LOG)
				{
				    if(busPacket->busPacketType == WRITE)
				    {
					log->log_plane_state(busPacket->virtualAddress, busPacket->package, busPacket->die, busPacket->plane, WRITING);
				    }
				    else if(busPacket->busPacketType == GC_WRITE)
				    {
					log->log_plane_state(busPacket->virtualAddress, busPacket->package, busPacket->die, busPacket->plane, GC_WRITING);
				    }
				}
				break;
			case ERASE:
			        controlCyclesLeft[busPacket->plane]= ERASE_TIME;

				// log the new state of this plane
				if(LOGGING && PLANE_STATE_LOG)
				{
				    log->log_plane_state(busPacket->virtualAddress, busPacket->package, busPacket->die, busPacket->plane, ERASING);
				}
				break;
			default:
				break;			
		}
	} else{
		ERROR("Die is busy");
		exit(1);
	}
}

int Die::isDieBusy(uint64_t plane){
	if (currentCommands[plane] == NULL && sending == false){
		return 0;
	}
	return 1;
}

void Die::update(void){
	uint64_t i;
	ChannelPacket *currentCommand;

	for (i = 0 ; i < PLANES_PER_DIE ; i++){
		currentCommand = currentCommands[i];
		if (currentCommand != NULL){
			if (controlCyclesLeft[i] == 0){

				// Process each command based on the packet type.
				switch (currentCommand->busPacketType){
					case READ:	
						planes[currentCommand->plane].read(currentCommand);
						returnDataPackets.push(planes[currentCommand->plane].readFromData());
						break;
					case GC_READ:
					        planes[currentCommand->plane].read(currentCommand);
						returnDataPackets.push(planes[currentCommand->plane].readFromData());
						parentNVDIMM->GCReadDone(currentCommand->virtualAddress);
						break;
					case WRITE:				     
						planes[currentCommand->plane].write(currentCommand);
						parentNVDIMM->numWrites++;					
						//call write callback
						if (parentNVDIMM->WriteDataDone != NULL){
						    (*parentNVDIMM->WriteDataDone)(parentNVDIMM->systemID, currentCommand->virtualAddress, currentClockCycle,true);
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
					case DATA:
						// Nothing to do.
					default:
						break;
				}

				ChannelPacketType bpt = currentCommand->busPacketType;
				if ((bpt == WRITE) || (bpt == GC_WRITE) || (bpt == ERASE))
				{
					// For everything but READ/GC_READ, and DATA, the access is done at this point.
					// Note: for READ/GC_READ, this is handled in Controller::receiveFromChannel().
					// For DATA, this is handled as part of the WRITE in Plane.

					// Tell the logger the access is done.
					if (LOGGING)
					{
					    log->access_stop(currentCommand->virtualAddress, currentCommand->physicalAddress);
					    if(PLANE_STATE_LOG)
					    {
						log->log_plane_state(currentCommand->virtualAddress, currentCommand->package, currentCommand->die, currentCommand->plane, IDLE);
					    }
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

	if (!returnDataPackets.empty())
	{
	    if( BUFFERED == true)
	    {
		if(deviceBeatsLeft == 0 && sending == false){
		    dataCyclesLeft = divide_params(DEVICE_CYCLE,CYCLE_TIME);
		    deviceBeatsLeft = divide_params((NV_PAGE_SIZE*8192),DEVICE_WIDTH);
		    sending = true;
		}

		if(dataCyclesLeft == 0 && deviceBeatsLeft > 0){
		    bool success = false;
		    success = buffer->sendPiece(BUFFER, 0, id, returnDataPackets.front()->plane);
		    if(success == true)
		    {
			deviceBeatsLeft--;
			dataCyclesLeft = divide_params(DEVICE_CYCLE,CYCLE_TIME);
		    }
		}
		
		if(dataCyclesLeft > 0 && deviceBeatsLeft > 0){
		    dataCyclesLeft--;
		}
	    }else{
		if(buffer->channel->hasChannel(BUFFER, id)){
		    if(dataCyclesLeft == 0){
			buffer->channel->sendToController(returnDataPackets.front());
			buffer->channel->releaseChannel(BUFFER, id);
			if(LOGGING && PLANE_STATE_LOG)
			{
			    log->log_plane_state(returnDataPackets.front()->virtualAddress, returnDataPackets.front()->package, returnDataPackets.front()->die, returnDataPackets.front()->plane, IDLE);
			}
			returnDataPackets.pop();
		    }
		    if(CRIT_LINE_FIRST && dataCyclesLeft == critBeat)
		    {
			buffer->channel->controller->returnCritLine(returnDataPackets.front());
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

void Die::bufferDone(uint64_t plane)
{
    //sanity check
    if(pendingDataPackets.front()->plane == plane)
    {
	buffer->sendToController(pendingDataPackets.front());
	pendingDataPackets.pop();
    }
    else
    {
	ERROR("Tried to complete a pending data packet for the wrong plane, things got out of order somehow");
    }
}

void Die::bufferLoaded()
{
    pendingDataPackets.push(returnDataPackets.front());
    if(LOGGING && PLANE_STATE_LOG)
    {
	log->log_plane_state(returnDataPackets.front()->virtualAddress, returnDataPackets.front()->package, returnDataPackets.front()->die, returnDataPackets.front()->plane, IDLE);
    }
    returnDataPackets.pop();	
    sending = false;
}

void Die::critLineDone()
{
    if(CRIT_LINE_FIRST)
    {
	buffer->channel->controller->returnCritLine(returnDataPackets.front());
    }
}

void Die::writeToPlane(ChannelPacket *packet)
{
	ChannelPacket *temp = new ChannelPacket(DATA, packet->virtualAddress, packet->physicalAddress, packet->page, packet->block, packet->plane, packet->die, packet->package, NULL);
	planes[packet->plane].storeInData(temp);
	planes[packet->plane].write(packet);
}
