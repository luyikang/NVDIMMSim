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
	deviceWriting = 0;

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
	cout << t << " has the channel \n";
	return 1;
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

//TODO: Need to check the type of piece to see if its a command or data
void Channel::sendPiece(SenderType t, uint type){
        if(busy == 1){
	    beatsDone++;
	}else if(busy == 2){
	    beatsLeft += divide_params(DEVICE_WIDTH,CHANNEL_WIDTH);
	}else{
	    if(t == CONTROLLER){
		if(type == 0){
		    beatsLeft = divide_params((NV_PAGE_SIZE*8192),DEVICE_WIDTH);
		}else{
		    beatsLeft = divide_params(COMMAND_LENGTH,DEVICE_WIDTH);
		}
		beatsDone = 1;
	        busy = 1;
	    }else{
		cyclesLeft = divide_params(CHANNEL_CYCLE,CYCLE_TIME);
		beatsLeft = divide_params(DEVICE_WIDTH,CHANNEL_WIDTH);
		busy = 2;
	    }
	}
}

int Channel::notBusy(void){
    if(busy > 0){
	return 0;
    }else{
	return 1;
    }
}

void Channel::update(void){
    //cout << "updating the channel \n";
    if(busy == 1){
	if(beatsDone >= divide_params(DEVICE_WIDTH,CHANNEL_WIDTH) && deviceWriting == 0)
	{
	    cyclesLeft = divide_params(DEVICE_CYCLE,CYCLE_TIME);
	    deviceWriting = 1;
	    beatsDone -= divide_params(DEVICE_WIDTH,CHANNEL_WIDTH);
	}
	if(deviceWriting == 1)
	{
	    cyclesLeft--;
	    if(cyclesLeft <= 0)
	    {
		deviceWriting = 0;
		beatsLeft--;
	    }
	}
	if(beatsLeft <= 0){
	    busy = 0;
	}
    }else if(busy == 2){
	cyclesLeft--;
	if(cyclesLeft <= 0 && beatsLeft > 0){
	    beatsLeft--;
	    cyclesLeft = divide_params(CHANNEL_CYCLE,CYCLE_TIME);
	}
	if(beatsLeft <= 0){
	    busy = 0;
	}
    }
}
