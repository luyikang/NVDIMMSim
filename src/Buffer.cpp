//Buffer.cpp
//Buffer functions

#include "Buffer.h"

using namespace std;
using namespace NVDSim;

Buffer::Buffer(void){
    cyclesLeft = new uint **[DIES_PER_PACKAGE];
    beatsLeft = new uint **[DIES_PER_PACKAGE];

    deviceWriting = new uint *[DIES_PER_PACKAGE];
    writePending = new uint *[DIES_PER_PACKAGE];
    packetType = new uint *[DIES_PER_PACKAGE];

    for(uint i = 0; i < DIES_PER_PACKAGE; i++){
	cyclesLeft[i] = new uint *[PLANES_PER_DIE];
	beatsLeft[i] = new uint *[PLANES_PER_DIE];
	deviceWriting[i] = new uint[PLANES_PER_DIE];
	writePending[i] = new uint[PLANES_PER_DIE];
	packetType[i] = new uint[PLANES_PER_DIE];

        for(uint j = 0; j < PLANES_PER_DIE; j++){
	    cyclesLeft[i][j] = new uint[2];
	    beatsLeft[i][j] = new uint[2];
	    packetType[i][j] = 0;

	    for(uint k = 0; k < 2; k++){
		cyclesLeft[i][j][k] = 0;
		beatsLeft[i][j][k] = 0;
	    }
	}
   }
}

Buffer::attachDie(Die *d){
    dies.push_back(d)
}

Buffer::channelDone(uint Plane){
    
}

Buffer::sendPiece(SenderType t, uint type, uint die, uint plane){
    if(beatsLeft[i][j][type] > 0){
	beatsLeft[i][j][type]++;
    }else{
	beatsLeft[i][j][type] = 1;
    }
    packetType[i][j] = type;
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
