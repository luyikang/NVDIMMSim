//Buffer.cpp
//Buffer functions

#include "Buffer.h"

using namespace std;
using namespace NVDSim;

Buffer::Buffer(uint id){

    id = id;

    outData = vector<vector<list <BufferPacket *> > >(DIES_PER_PACKAGE, vector<list <BufferPacket *> >(PLANES_PER_DIE, list<BufferPacket *>()));
    inData = vector<vector<list <BufferPacket *> > >(DIES_PER_PACKAGE, vector<list <BufferPacket *> >(PLANES_PER_DIE, list<BufferPacket *>()));

    cyclesLeft = new uint *[DIES_PER_PACKAGE];
    deviceWriting = new uint *[DIES_PER_PACKAGE];
    outDataLeft = new uint *[DIES_PER_PACKAGE];
    inDataLeft = new uint *[DIES_PER_PACKAGE];
    writing_busy = new int *[DIES_PER_PACKAGE];

    for(uint i = 0; i < DIES_PER_PACKAGE; i++){
	cyclesLeft[i] = new uint [PLANES_PER_DIE];
	deviceWriting[i] = new uint[PLANES_PER_DIE];
	outDataLeft[i] = new uint [PLANES_PER_DIE];
	inDataLeft[i] = new uint [PLANES_PER_DIE];
	writing_busy[i] = new int [PLANES_PER_DIE];
	
	for(uint j = 0; j < PLANES_PER_DIE; j++){
	    cyclesLeft[i][j] = 0;
	    deviceWriting[i][j] = 0;
	    outDataLeft[i][j] = 0;
	    inDataLeft[i][j] = 0;
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
	if(!inData[die][plane].empty() && inData[die][plane].back()->type == type && type == 5 &&
	   inData[die][plane].back()->number < (NV_PAGE_SIZE*8192)){
	    inData[die][plane].back()->number = inData[die][plane].back()->number + CHANNEL_WIDTH;
	    count++;
	}else if(!inData[die][plane].empty() && inData[die][plane].back()->type == type && type != 5 &&
	          inData[die][plane].back()->number < COMMAND_LENGTH){
	    inData[die][plane].back()->number = inData[die][plane].back()->number + CHANNEL_WIDTH;
	    count++;
	}else{
	    BufferPacket* myPacket = new BufferPacket();
	    myPacket->type = type;
	    myPacket->number = CHANNEL_WIDTH;
	    inData[die][plane].push_back(myPacket);
	    count++;
	}
    }else if(t == BUFFER){
	if(!outData[die][plane].empty() && outData[die][plane].back()->type == type &&
	   outData[die][plane].back()->number < (NV_PAGE_SIZE*8192)){
	    outData[die][plane].back()->number = outData[die][plane].back()->number + DEVICE_WIDTH;
	}else{
	    BufferPacket* myPacket = new BufferPacket();
	    myPacket->type = type;
	    myPacket->number = DEVICE_WIDTH;
	    outData[die][plane].push_back(myPacket);
	   
	}
    }
}
	    
void Buffer::update(void){
    for(uint i = 0; i < DIES_PER_PACKAGE; i++){
	for(uint j = 0; j < PLANES_PER_DIE; j++){
	    // moving data into a die
	    //==================================================================================
	    // if we're not already busy writing stuff
	    if(!inData[i][j].empty())
	    {
	        if(inData[i][j].front()->number >= DEVICE_WIDTH || 
		   (inDataLeft[i][j] < DEVICE_WIDTH && inData[i][j].front()->number == inDataLeft[i][j]))
	        {
			// clear out those packets from the incoming queue
			// and beging the process of writing them into the device

			// if it is a command, set the number of beats if we've not set them yet
			if(inData[i][j].front()->type != 5 && inDataLeft[i][j] == 0)
			{
			    inDataLeft[i][j] = COMMAND_LENGTH;
			    cyclesLeft[i][j] = divide_params(DEVICE_CYCLE,CYCLE_TIME);
			}
			else if(inDataLeft[i][j] == 0)
			{
			    inDataLeft[i][j] = (NV_PAGE_SIZE*8192);
			    cyclesLeft[i][j] = divide_params(DEVICE_CYCLE,CYCLE_TIME);
			}

			if(cyclesLeft[i][j] > 0)
			{
			    cyclesLeft[i][j]--;
			}
			if(cyclesLeft[i][j] == 0)
			{	    
			    if(inDataLeft[i][j] > 0)
			    {
				cyclesLeft[i][j] = divide_params(DEVICE_CYCLE,CYCLE_TIME);
				if(inDataLeft[i][j] >= DEVICE_WIDTH)
				{
				    inDataLeft[i][j] = inDataLeft[i][j] - DEVICE_WIDTH;
				}
				else
				{
				    inDataLeft[i][j] = 0;
				}
				inData[i][j].front()->number = inData[i][j].front()->number - DEVICE_WIDTH;
			    }
			}
			
			if(inDataLeft[i][j] == 0)
			{	    
			    channel->bufferDone(i,j);
			    inData[i][j].pop_front();
			}
		}
	    }	    
	    
	    
	    // moving data away from die
	    //====================================================================================
	    // first scan through to see if we have stuff to send if we're not busy
	    if(!outData[i][j].empty())
	    {
		if(outData[i][j].front()->number >= CHANNEL_WIDTH ||
		    (outDataLeft[i][j] < CHANNEL_WIDTH && outData[i][j].front()->number == outDataLeft[i][j]))
		{
		    // then see if we have control of the channel
		    if (channel->hasChannel(BUFFER, id) && sendingDie == i && sendingPlane == j){
			if(outDataLeft[i][j] > 0 && channel->notBusy()){
			    channel->sendPiece(BUFFER,outData[i][j].front()->type,i,j);
			    outData[i][j].front()->number = outData[i][j].front()->number - CHANNEL_WIDTH;
			    if(outDataLeft[i][j] >= CHANNEL_WIDTH)
			    {
				outDataLeft[i][j] = outDataLeft[i][j] - CHANNEL_WIDTH;
			    }
			    else
			    {
				outDataLeft[i][j] = 0;
			    }
			}

			if(outDataLeft[i][j] == 0)
			{
			    dies[i]->bufferDone();
			    channel->releaseChannel(BUFFER,id);
			    outData[i][j].pop_front();
			}
		    }		
		    else if (channel->obtainChannel(id, BUFFER, NULL)){
			outDataLeft[i][j] = (NV_PAGE_SIZE*8192);
			sendingDie = i;
			sendingPlane = j;
		    }
		}
	    }
	}
    }
}
