//Buffer.cpp
//Buffer functions

#include "Buffer.h"

using namespace std;
using namespace NVDSim;

Buffer::Buffer(uint id){

    id = id;

    outPackets = vector<vector<list <BufferPacket *> > >(DIES_PER_PACKAGE, vector<list <BufferPacket *> >(PLANES_PER_DIE, list<BufferPacket *>()));
    inPackets = vector<vector<list <BufferPacket *> > >(DIES_PER_PACKAGE, vector<list <BufferPacket *> >(PLANES_PER_DIE, list<BufferPacket *>()));

    cyclesLeft = new uint *[DIES_PER_PACKAGE];
    deviceWriting = new uint *[DIES_PER_PACKAGE];
    outBeatsLeft = new uint *[DIES_PER_PACKAGE];
    inBeatsLeft = new uint *[DIES_PER_PACKAGE];
    writing_busy = new int *[DIES_PER_PACKAGE];

    for(uint i = 0; i < DIES_PER_PACKAGE; i++){
	cyclesLeft[i] = new uint [PLANES_PER_DIE];
	deviceWriting[i] = new uint[PLANES_PER_DIE];
	outBeatsLeft[i] = new uint [PLANES_PER_DIE];
	inBeatsLeft[i] = new uint [PLANES_PER_DIE];
	writing_busy[i] = new int [PLANES_PER_DIE];
	
	for(uint j = 0; j < PLANES_PER_DIE; j++){
	    cyclesLeft[i][j] = 0;
	    deviceWriting[i][j] = 0;
	    outBeatsLeft[i][j] = 0;
	    inBeatsLeft[i][j] = 0;
	    writing_busy[i][j] = 0;
	}
   }

    count = 0;

    sendingDie = 0;
    sendingPlane = 0;
}

void Buffer::attachDie(Die *d){
    dies.push_back(d);
}

void Buffer::attachChannel(Channel *c){
    channel = c;
}

void Buffer::sendToDie(ChannelPacket *busPacket){
    dies[busPacket->die]->receiveFromBuffer(busPacket);
}

void Buffer::sendToController(ChannelPacket *busPacket){
    channel->sendToController(busPacket);
}

void Buffer::sendPiece(SenderType t, uint type, uint die, uint plane){
    if(t == CONTROLLER){
	if(!inPackets[die][plane].empty() && inPackets[die][plane].back()->type == type && type == 5 &&
	   inPackets[die][plane].back()->number < (NV_PAGE_SIZE*8192)){
	    inPackets[die][plane].back()->number = inPackets[die][plane].back()->number + CHANNEL_WIDTH;
	    count++;
	}else if(!inPackets[die][plane].empty() && inPackets[die][plane].back()->type == type && type != 5 &&
	          inPackets[die][plane].back()->number < COMMAND_LENGTH){
	    inPackets[die][plane].back()->number = inPackets[die][plane].back()->number + CHANNEL_WIDTH;
	    count++;
	}else{
	    BufferPacket* myPacket = new BufferPacket();
	    myPacket->type = type;
	    myPacket->number = CHANNEL_WIDTH;
	    inPackets[die][plane].push_back(myPacket);
	    count++;
	}
    }else if(t == BUFFER){
	if(!outPackets[die][plane].empty() && outPackets[die][plane].back()->type == type &&
	   outPackets[die][plane].back()->number < (NV_PAGE_SIZE*8192)){
	    outPackets[die][plane].back()->number = outPackets[die][plane].back()->number + DEVICE_WIDTH;
	}else{
	    BufferPacket* myPacket = new BufferPacket();
	    myPacket->type = type;
	    myPacket->number = DEVICE_WIDTH;
	    outPackets[die][plane].push_back(myPacket);
	   
	}
    }
}
	    
void Buffer::update(void){
    for(uint i = 0; i < DIES_PER_PACKAGE; i++){
	for(uint j = 0; j < PLANES_PER_DIE; j++){
	    // moving data into a die
	    //==================================================================================
	    // if we're not already busy writing stuff
	    if(!inPackets[i][j].empty())
	    {
	        if(inPackets[i][j].front()->number >= DEVICE_WIDTH || 
		   (inBeatsLeft[i][j] < DEVICE_WIDTH && inPackets[i][j].front()->number == inBeatsLeft[i][j]))
	        {
			// clear out those packets from the incoming queue
			// and beging the process of writing them into the device

			// if it is a command, set the number of beats if we've not set them yet
			if(inPackets[i][j].front()->type != 5 && inBeatsLeft[i][j] == 0)
			{
			    inBeatsLeft[i][j] = COMMAND_LENGTH;
			    cyclesLeft[i][j] = divide_params(DEVICE_CYCLE,CYCLE_TIME);
			}
			else if(inBeatsLeft[i][j] == 0)
			{
			    inBeatsLeft[i][j] = (NV_PAGE_SIZE*8192);
			    cyclesLeft[i][j] = divide_params(DEVICE_CYCLE,CYCLE_TIME);
			}

			if(cyclesLeft[i][j] > 0)
			{
			    cyclesLeft[i][j]--;
			}
			if(cyclesLeft[i][j] == 0)
			{	    
			    if(inBeatsLeft[i][j] > 0)
			    {
				cyclesLeft[i][j] = divide_params(DEVICE_CYCLE,CYCLE_TIME);
				if(inBeatsLeft[i][j] >= DEVICE_WIDTH)
				{
				    inBeatsLeft[i][j] = inBeatsLeft[i][j] - DEVICE_WIDTH;
				}
				else
				{
				    inBeatsLeft[i][j] = 0;
				}
				inPackets[i][j].front()->number = inPackets[i][j].front()->number - DEVICE_WIDTH;
			    }
			}
			
			if(inBeatsLeft[i][j] == 0)
			{	    
			    channel->bufferDone(i,j);
			    inPackets[i][j].pop_front();
			}
		}
	    }	    
	    
	    
	    // moving data away from die
	    //====================================================================================
	    // first scan through to see if we have stuff to send if we're not busy
	    if(!outPackets[i][j].empty())
	    {
		if(outPackets[i][j].front()->number >= CHANNEL_WIDTH ||
		    (outBeatsLeft[i][j] < CHANNEL_WIDTH && outPackets[i][j].front()->number == outBeatsLeft[i][j]))
		{
		    // then see if we have control of the channel
		    if (channel->hasChannel(BUFFER, id) && sendingDie == i && sendingPlane == j){
			if(outBeatsLeft[i][j] > 0 && channel->notBusy()){
			    channel->sendPiece(BUFFER,outPackets[i][j].front()->type,i,j);
			    outPackets[i][j].front()->number = outPackets[i][j].front()->number - CHANNEL_WIDTH;
			    if(outBeatsLeft[i][j] >= CHANNEL_WIDTH)
			    {
				outBeatsLeft[i][j] = outBeatsLeft[i][j] - CHANNEL_WIDTH;
			    }
			    else
			    {
				outBeatsLeft[i][j] = 0;
			    }
			}

			if(outBeatsLeft[i][j] == 0)
			{
			    dies[i]->bufferDone();
			    channel->releaseChannel(BUFFER,id);
			    outPackets[i][j].pop_front();
			}
		    }		
		    else if (channel->obtainChannel(id, BUFFER, NULL)){
			outBeatsLeft[i][j] = (NV_PAGE_SIZE*8192);
			sendingDie = i;
			sendingPlane = j;
		    }
		}
	    }
	}
    }
}
