//Buffer.cpp
//Buffer functions

#include "Buffer.h"

using namespace std;
using namespace NVDSim;

Buffer::Buffer(void){

    outPackets = vector<vector<queue <BufferPacket *> > >(DIES_PER_PACKAGE, vector<queue <BufferPacket *> >(PLANES_PER_DIE, queue<BufferPacket *>()));
    inPackets = vector<vector<queue <BufferPacket *> > >(DIES_PER_PACKAGE, vector<queue <BufferPacket *> >(PLANES_PER_DIE, queue<BufferPacket *>()));

    cyclesLeft = new uint *[DIES_PER_PACKAGE];
    deviceWriting = new uint *[DIES_PER_PACKAGE];

    for(uint i = 0; i < DIES_PER_PACKAGE; i++){
	cyclesLeft[i] = new uint [PLANES_PER_DIE];
	deviceWriting[i] = new uint[PLANES_PER_DIE];
   }
}

Buffer::attachDie(Die *d){
    dies.push_back(d)
}

Buffer::channelDone(uint Plane){
    
}

Buffer::sendPiece(SenderType t, uint type, uint die, uint plane){
    if(t == CONTROLLER){
	if(!inPackets.empty()){
	    if(inPackets.back()->type == type){
	    }else if(inPackets.back()->number == divide_params((NV_PAGE_SIZE*8192),DEVICE_WIDTH) ||
		     inPackets.back()->number == divide_params(COMMAND_LENGTH,DEVICE_WIDTH)){
		
	    }else{
		
	    }
	}else{
	    BufferPacket* myPacket = new BufferPacket();
	    myPacket->type = type;
	    inPackets.push(myPacket);
	}
    }else if(t == DIE){
    }
}
	    
Buffer::void update(void){
    for(uint i = 0; i < DIES_PER_PACKAGE; i++){
	for(uint j = 0; j < PLANES_PER_DIE; j++){
	    // moving data into a die
	    // this implementation would require 4kB of SRAM for each die attached to the buffer
	    // see if we have a full device beats worth of data that we can write into the device
	    if(beatsLeft[i][j][0]%divide_params(DEVICE_WIDTH,CHANNEL_WIDTH) == 0 && 
		beatsLefts[i][j][0] > 0 && deviceWriting[i][j] == 0 && packetType[i][j] == 0)
	    {
		    cyclesLeft[i][j] = divide_params(DEVICE_CYCLE,CYCLE_TIME);
		    deviceWriting[i][j] = 1;
	    }
	    // if we have a full device beat, write it into the device
	    if(deviceWriting[i][j] == 1)
	    {
		if(cyclesLeft[i][j] > 0){
		    cyclesLeft[i][j]--;
		}
		if(cyclesLeft[i][j] <= 0)
		{
		    deviceWriting[i][j] = 0;
		    if(beatsLeft[i][j][0] > 0){
			beatsLeft[i][j][0]--;
		    }
		}	
	    }

	    
	    
	    // sending data back from the die
	    if(beatsLeft[i][j][1] > 0)
	    {
	    }
	    
	}
    }
}
