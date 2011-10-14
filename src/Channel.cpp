//Channel.cpp
//class file for channel class
#include "Channel.h"
#include "ChannelPacket.h"
#include "Controller.h"
#include "Buffer.h"

using namespace NVDSim;

Channel::Channel(void){
	sender = -1;
	busy = 0;

	cyclesLeft = 0;

	firstCheck = 0;
}

void Channel::attachBuffer(Buffer *b){
	buffer = b;
}

void Channel::attachController(Controller *c){
	controller = c;
}

int Channel::obtainChannel(uint s, SenderType t, ChannelPacket *p){
    if (sender != -1 || (t == CONTROLLER && buffer->dies[p->die]->isDieBusy(p->plane)) || busy == 1){
		return 0;
	}
	sType = t;
	sender = (int) s;
	
	return 1;
}

int Channel::releaseChannel(SenderType t, uint s){       
	if (t == sType && sender == (int) s){
		sender = -1;
		return 1;
	}
	return 0;
}

int Channel::hasChannel(SenderType t, uint s){
	if (t == sType && sender == (int) s)
		return 1;
	return 0;
}

void Channel::sendToBuffer(ChannelPacket *busPacket){
        buffer->sendToDie(busPacket);
}

void Channel::sendToController(ChannelPacket *busPacket){
        controller->receiveFromChannel(busPacket);
}

void Channel::sendPiece(SenderType t, uint type, uint die, uint plane){
        cyclesLeft = divide_params(CHANNEL_CYCLE,CYCLE_TIME);
	busy = 1;
	currentDie = die;
	currentPlane = plane;
	packetType = type;
}

int Channel::notBusy(void){
        if(busy == 1){
	    return 0;
	}else{
	    return 1;
	}
}

void Channel::update(void){
        if(cyclesLeft == 0 && busy == 1){
	    if(sType == CONTROLLER){
		bool success = false;
		success = buffer->sendPiece(CONTROLLER, packetType, currentDie, currentPlane);
		if(success == true)
		{
		    busy = 0;
		}
	    }
	    else
	    {
		busy = 0;
	    }
	}
	    
	if(cyclesLeft > 0){
	    cyclesLeft--;
	}
}

void Channel::bufferDone(uint die, uint plane){
    controller->bufferDone(die, plane);
}
