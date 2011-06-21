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
    beatsLeft = new uint *[DIES_PER_PACKAGE];
    reading_busy = new int *[DIES_PER_PACKAGE];
    writing_busy = new int *[DIES_PER_PACKAGE];

    for(uint i = 0; i < DIES_PER_PACKAGE; i++){
	cyclesLeft[i] = new uint [PLANES_PER_DIE];
	deviceWriting[i] = new uint[PLANES_PER_DIE];
	beatsLeft[i] = new uint [PLANES_PER_DIE];
	reading_busy[i] = new int [PLANES_PER_DIE];
	writing_busy[i] = new int [PLANES_PER_DIE];
	
	for(uint j = 0; j < PLANES_PER_DIE; j++){
	    cyclesLeft[i][j] = 0;
	    deviceWriting[i][j] = 0;
	    beatsLeft[i][j] = 0;
	    reading_busy[i][j] = 0;
	    writing_busy[i][j] = 0;
	}
   }
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
	if(!inPackets[die][plane].empty() && inPackets[die][plane].back()->type == type){
	    BufferPacket* myPacket = new BufferPacket();
	    myPacket->type = type;
	    myPacket->number = inPackets[die][plane].back()->number + 1;
	    inPackets[die][plane].push_back(myPacket);
	    cout << "got an in packet \n";
	    cout << "we now have " << inPackets[die][plane].size() << " packets \n";
	}else{
	    BufferPacket* myPacket = new BufferPacket();
	    myPacket->type = type;
	    inPackets[die][plane].push_back(myPacket);
	    cout << "got a first in packet \n";
	    cout << "we now have " << inPackets[die][plane].size() << " packets \n";
	    cout << "and the number of our new packet is " << inPackets[die][plane].back()->number << "\n";
	}
    }else if(t == BUFFER){
	if(!outPackets[die][plane].empty() && outPackets[die][plane].back()->type == type){
	    BufferPacket* myPacket = new BufferPacket();
	    myPacket->type = type;
	    myPacket->number = outPackets[die][plane].back()->number + 1;
	    outPackets[die][plane].push_back(myPacket);
	    cout << "got an out packet \n";
	    cout << "we now have " << inPackets[die][plane].size() << " packets \n";
	}else{
	    BufferPacket* myPacket = new BufferPacket();
	    myPacket->type = type;
	    outPackets[die][plane].push_back(myPacket);
	    cout << "got a first out packet \n";
	    cout << "we now have " << inPackets[die][plane].size() << " packets \n";
	}
    }
}
	    
void Buffer::update(void){
    for(uint i = 0; i < DIES_PER_PACKAGE; i++){
	for(uint j = 0; j < PLANES_PER_DIE; j++){
	    // moving data into a die
	    //==================================================================================
	    // if we're not already busy writing stuff
	    std::list<BufferPacket *>::iterator it;
	    if(deviceWriting[i][j] == 0)
	    {
		// scaning through the incoming queue to see if we've got a whole packet
		// starting with the oldest packets first		
		for(it = inPackets[i][j].begin(); it != inPackets[i][j].end(); it++)
		{
		    if((*it)->number > 0 && (*it)->number%divide_params(DEVICE_WIDTH,CHANNEL_WIDTH) == 0)
		    {
			writing_busy[i][j] = 1;
			break;
		    }
		}

		// clear out those packets from the incoming queue
		// and beging the process of writing them into the device
		if(writing_busy[i][j] == 1)
		{
		    // create a second iterator to mark the beginning of the erase range
		    std::list<BufferPacket *>::iterator it2;
		    it2 = it;
		    advance(it2, -divide_params(DEVICE_WIDTH,CHANNEL_WIDTH)+1);

		    // if it is a command, set the number of beats if we've not set them yet
		    if((*it)->type != 5 && beatsLeft[i][j] == 0)
		    {
			beatsLeft[i][j] = divide_params(COMMAND_LENGTH,DEVICE_WIDTH);
		    }
		    else if(beatsLeft[i][j] == 0)
		    {
			beatsLeft[i][j] = divide_params((NV_PAGE_SIZE*8192),DEVICE_WIDTH);
		    }
		
		    inPackets[i][j].erase(it2, it);
		    deviceWriting[i][j] = 1;
		    cyclesLeft[i][j] = divide_params(DEVICE_CYCLE,CYCLE_TIME);
		}
	    }
	    
	    // if we have a full device beat, write it into the device
	    if(deviceWriting[i][j] == 1)
	    {
		if(cyclesLeft[i][j] > 0)
		{
		    cyclesLeft[i][j]--;
		}
		if(cyclesLeft[i][j] <= 0)
		{
		    if(beatsLeft[i][j] > 0)
		    {
			beatsLeft[i][j]--;
			deviceWriting[i][j] = 0;
		    }
		}
	    }
	   
	    if(beatsLeft[i][j] == 0 && writing_busy[i][j] == 1)
	    {	    
		    channel->bufferDone(i,j);
		    writing_busy[i][j] = 0;
	    }
	    
	    // moving data away from die
	    //====================================================================================
	    // first scan through to see if we have stuff to send if we're not busy
	    if(reading_busy[i][j] == 0)
	    {
		// right now I'm going to wait until we have a full read complete to send the data		
		for(it3 = outPackets[i][j].begin(); it3 != outPackets[i][j].end(); it3++)
		{
		    if((*it3)->number%divide_params((NV_PAGE_SIZE*8192),CHANNEL_WIDTH) == 0)
		    {
			reading_busy[i][j] = 1;
			break;
		    }
		}
	    }

	    // clear out those packets from the outcoming queue
	    // and beging the process of sending them onto the channel
	    if(reading_busy[i][j] == 1)
	    {
		// then see if we have control of the channel
		if (channel->hasChannel(BUFFER, id)){
		    if(beatsLeft[i][j] > 0 && channel->notBusy()){
			channel->sendPiece(BUFFER,(*it3)->type,i,j);
			beatsLeft[i][j]--;
		    }
		}
		else if (channel->obtainChannel(id, BUFFER, NULL)){
		    beatsLeft[i][j] = divide_params((NV_PAGE_SIZE*8192),DEVICE_WIDTH);
		}

		if(beatsLeft[i][j] == 0)
		{
		    reading_busy[i][j] = 0;
		    dies[i][j].bufferDone();
		    channel->releaseChannel(BUFFER,id);
		    // create a second iterator to mark the beginning of the erase range
		    std::list<BufferPacket *>::iterator it2;
		    it2 = it3;
		    advance(it2, -divide_params((NV_PAGE_SIZE*8192),CHANNEL_WIDTH));		    
		    outPackets[i][j].erase(it2, it3);
		}
	    }
	}
    }
}
