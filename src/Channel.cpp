//Channel.cpp
//class file for channel class
#include "Channel.h"
#include "ChannelPacket.h"
#include "Controller.h"

using namespace NVDSim;

Channel::Channel(void){
	sender = -1;
	busy = 0;

	cyclesLeft = new uint **[DIES_PER_PACKAGE];
	beatsLeft = new uint **[DIES_PER_PACKAGE];
	beatsDone = new uint **[DIES_PER_PACKAGE];
	deviceWriting = new uint *[DIES_PER_PACKAGE];
	writePending = new uint *[DIES_PER_PACKAGE];
	packetType = new uint *[DIES_PER_PACKAGE];

	for(uint i = 0; i < DIES_PER_PACKAGE; i++){
	    cyclesLeft[i] = new uint *[PLANES_PER_DIE];
	    beatsLeft[i] = new uint *[PLANES_PER_DIE];
	    beatsDone[i] = new uint *[PLANES_PER_DIE];
	    deviceWriting[i] = new uint[PLANES_PER_DIE];
	    writePending[i] = new uint[PLANES_PER_DIE];
	    packetType[i] = new uint[PLANES_PER_DIE];

	    for(uint j = 0; j < PLANES_PER_DIE; j++){
		cyclesLeft[i][j] = new uint[3];
		beatsLeft[i][j] = new uint[3];
		beatsDone[i][j] = new uint[3];
		packetType[i][j] = 0;
		for(uint k = 0; k < 3; k++){
		    cyclesLeft[i][j][k] = 0;
		    beatsLeft[i][j][k] = 0;
		    beatsDone[i][j][k] = 0;
		}
	    }
	}

	firstCheck = 0;
}

void Channel::attachDie(Die *d){
	dies.push_back(d);
}

void Channel::attachController(Controller *c){
	controller= c;
}

int Channel::obtainChannel(uint s, SenderType t, ChannelPacket *p){
	if (sender != -1 || (t == CONTROLLER && dies[p->die]->isDieBusy(p->plane))){
		return 0;
	}
	type = t;
	sender = (int) s;
	return 1;
}

int Channel::releaseChannel(SenderType t, uint s){   
        // these should be zero anyway but clear then just to be safe    
	if (t == type && sender == (int) s){
		sender = -1;
		return 1;
	}
	return 0;
}

int Channel::hasChannel(SenderType t, uint s){
	if (t == type && sender == (int) s)
		return 1;
	return 0;
}

void Channel::sendToDie(ChannelPacket *busPacket){
	dies[busPacket->die]->receiveFromChannel(busPacket);
}

void Channel::sendToController(ChannelPacket *busPacket){
        controller->receiveFromChannel(busPacket);
}

void Channel::sendPiece(SenderType t, uint type, uint die, uint plane){
	if(beatsLeft[die][plane] > 0){
		    beatsLeft[die][plane]++;
	}else{
		    beatsLeft[die][plane] = 1;
		    packetType = type;
		    busy = 1;
	}
}

int Channel::notBusy(void){
    if(busy == 1){
	return 0;
    }else{
	return 1;
    }
}

void Channel::update(void){
    for(uint i = 0; i < DIES_PER_PACKAGE; i++){
	for(uint j = 0; j < PLANES_PER_DIE; j++){   
	    if(beatsLeft[i][j][1] <= 0){
	        beatsLeft[i][j][1]--;
	    }

	    if(beatsLeft[i][j][3] <= 0 && beatsDone[i][j][3] == divide_params((NV_PAGE_SIZE*8192),DEVICE_WIDTH)){
		dies[i]->channelDone();
		beatsDone[i][j][3] = 0;
	        busy = 0;
	    }
	}
    }
}

void Channel::acknowledge(uint die, uint plane){
    writePending[die][plane] = 0;
    if(packetType[die][plane] == 0)
    {
	beatsDone[die][plane][0] = 0;
	cyclesLeft[die][plane][0] = 0;
	beatsLeft[die][plane][0] = 0;
	packetType[die][plane] = 1;
    }
    else if(packetType[die][plane] == 1)
    {
	beatsDone[die][plane][1] = 0;
	cyclesLeft[die][plane][1] = 0;
	beatsLeft[die][plane][1] = 0;
    }
}
