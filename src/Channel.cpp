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
	cout << t << " has the channel now \n";
	cout << "obtained channel on clock cycle " << dies[0]->currentClockCycle << "\n";
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
    //cout << "we're sending to die \n";
	dies[busPacket->die]->receiveFromChannel(busPacket);
}

void Channel::sendToController(ChannelPacket *busPacket){
        cout << "sent virtual address was " << busPacket->virtualAddress << "\n";
        controller->receiveFromChannel(busPacket);
}

//TODO: Need to check the type of piece to see if its a command or data
void Channel::sendPiece(SenderType t, uint type, uint die, uint plane){
        if(t == CONTROLLER){
	    if(type == 5 && beatsLeft[die][plane][0] > 0){
		    beatsDone[die][plane][0]++;
	    }else if(type == 5){
		    beatsLeft[die][plane][0] = divide_params((NV_PAGE_SIZE*8192),DEVICE_WIDTH);
		    beatsDone[die][plane][0] = 1;
		    packetType[die][plane] = 0;
		    busy = 1;
	    }else if(beatsLeft[die][plane][1] > 0){
		    beatsDone[die][plane][1]++;
	    }else{
		    beatsLeft[die][plane][1] = divide_params(COMMAND_LENGTH,DEVICE_WIDTH);
		    beatsDone[die][plane][1] = 1;
		    busy = 1;
	    }
	}else if(t == DIE){
	    //cout << "die reading has the channel \n";
	    if(beatsDone[die][plane][3] > 0){
		beatsLeft[die][plane][3] += divide_params(DEVICE_WIDTH,CHANNEL_WIDTH);
		//cout << "Die now has " << beatsLeft[die][plane][0] << "beats left\n";
		beatsDone[die][plane][3]++;
	    }else{
		cyclesLeft[die][plane][3] = divide_params(CHANNEL_CYCLE,CYCLE_TIME);
		cout << "Die has " << cyclesLeft[die][plane] << "cycles left\n";
		beatsLeft[die][plane][3] = divide_params(DEVICE_WIDTH,CHANNEL_WIDTH);
		beatsDone[die][plane][3] = 1;
		busy = 1;
	    }
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
	    // see if we have a full device beats worth of data that we can write into the device
	    if(beatsDone[i][j][1]%divide_params(DEVICE_WIDTH,CHANNEL_WIDTH) == 0 && 
		beatsDone[i][j][1] > 0 && deviceWriting[i][j] == 0 && packetType[i][j] == 1)
	        {
		    cyclesLeft[i][j][1] = divide_params(DEVICE_CYCLE,CYCLE_TIME);
		    deviceWriting[i][j] = 1;
		}else if(beatsDone[i][j][0]%divide_params(DEVICE_WIDTH,CHANNEL_WIDTH) == 0 && 
		   beatsDone[i][j][0] > 0 && deviceWriting[i][j] == 0 && packetType[i][j] == 0)
		{
		    cyclesLeft[i][j][0] = divide_params(DEVICE_CYCLE,CYCLE_TIME);
		    deviceWriting[i][j] = 1;
		}
		// if we have a full device beat, write it into the device
	    if(deviceWriting[i][j] == 1)
	    {
		if(cyclesLeft[i][j][packetType[i][j]] > 0){
		    cyclesLeft[i][j][packetType[i][j]]--;
		}
		if(cyclesLeft[i][j][packetType[i][j]] <= 0)
		{
		    deviceWriting[i][j] = 0;
		    if(beatsLeft[i][j][packetType[i][j]]){
			beatsLeft[i][j][packetType[i][j]]--;
		    }
		}	
	    }
	    // see if we've moved an entire pages worth of data, if so, set the busy back to not busy

	    if(busy == 1 && writePending[i][j] == 0 &&
	       (beatsDone[i][j][0] == divide_params((NV_PAGE_SIZE*8192),CHANNEL_WIDTH) || 
	        beatsDone[i][j][1] == divide_params(COMMAND_LENGTH,CHANNEL_WIDTH)))
	        {
	           cout << "current clock cycle is " << dies[0]->currentClockCycle << "\n";
		   cout << "beatsDone is " << beatsDone[i][j][packetType[i][j]] << "it should be " << divide_params((NV_PAGE_SIZE*8192),CHANNEL_WIDTH) << "\n";
		   cout << "beatsLeft is " << beatsLeft[i][j][packetType[i][j]] << "\n";
		   cout << "busy is " << busy << "\n";
		   writePending[i][j] = 1;
		   busy = 0;
	        }
		    
	    if(beatsLeft[i][j][packetType[i][j]] <= 0 && writePending[i][j] == 1){
	       //cout << "shouldn't see this more than a few times \n";
	       //cout << "beatsDone is " << beatsDone[i][j][packetType[i][j]] << "\n";
	       //cout << "channel done for packet of type " << packetType[i][j] << "\n";
	       controller->channelDone(i,j);
	    }	    

	    if(cyclesLeft[i][j][3] > 0){
	        //cout << "Die has " << cyclesLeft[i][j] << " cycles left in update \n";
	        //cout << "Die has " << beatsLeft[i][j][0] << " beats left\n";
	        cyclesLeft[i][j][3]--;
	    }
	    if(cyclesLeft[i][j][3] <= 0 && beatsLeft[i][j][3] > 0){
	        //cout << "Die has " << beatsLeft[i][j][0] << " beats left\n";
	        beatsLeft[i][j][3]--;
	        cyclesLeft[i][j][3] = divide_params(CHANNEL_CYCLE,CYCLE_TIME);
	    }
	    if(beatsLeft[i][j][3] <= 0 && beatsDone[i][j][3] == divide_params((NV_PAGE_SIZE*8192),DEVICE_WIDTH)){
		dies[i]->channelDone();
		beatsDone[i][j][3] = 0;
	        busy = 0;
	       //controller->channelDone(i,j);
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
