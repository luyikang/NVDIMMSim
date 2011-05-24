//Channel.cpp
//class file for channel class
#include "Channel.h"
#include "ChannelPacket.h"
#include "Controller.h"

using namespace NVDSim;

Channel::Channel(void){
	sender = -1;
	busy = 0;
	cyclesLeft = 0;
	beatsLeft = 0;
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
	
	if(t == CONTROLLER){
	    cyclesLeft = DEVICE_CYCLE / CYCLE_TIME;
	    beatsLeft = DEVICE_WIDTH / CHANNEL_WIDTH;
	}else{
	    cyclesLeft = CHANNEL_CYCLE / CYCLE_TIME;
	    beatsLeft = DEVICE_WIDTH / CHANNEL_WIDTH;
	}
}

int Channel::releaseChannel(SenderType t, uint s){
        
        // these should be zero anyway but clear then just to be safe
        cyclesLeft = 0;
	beatsLeft = 0;
    
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

void Channel::sendPiece(SenderType t){
        if (t ==  CONTROLLER){
	    beatsLeft--;
	    if(beatsLeft == 0)
	    {
		busy = 1;
	    }
        }else{
	    busy = 2;
        }
}

int Channel::notBusy(void){
    if(busy > 0)
	return 0;
    return 1;
}

void Channel::update(void){
    if(busy == 1){
	cyclesLeft--;
	if(cyclesLeft <= 0){
	    busy = 0;
	}
    }else if(busy == 2){
	cyclesLeft--;
	if(cyclesLeft <= 0){
	    beatsLeft--;
	    cyclesLeft = CHANNEL_CYCLE / CYCLE_TIME;
	}
	if(beatsLeft <= 0){
	    busy = 0;
	}
    }
}
