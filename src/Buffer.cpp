//Buffer.cpp
//Buffer functions

#include "Buffer.h"

using namespace std;
using namespace NVDSim;

Buffer::Buffer(uint64_t i){

    id = i;

    outData = vector<list <BufferPacket *> >(DIES_PER_PACKAGE, list<BufferPacket *>());
    inData = vector<list <BufferPacket *> >(DIES_PER_PACKAGE, list<BufferPacket *>());

    outDataSize = new uint64_t [DIES_PER_PACKAGE];
    inDataSize = new uint64_t [DIES_PER_PACKAGE];
    cyclesLeft = new uint64_t [DIES_PER_PACKAGE];
    outDataLeft = new uint64_t [DIES_PER_PACKAGE];
    critData = new uint64_t [DIES_PER_PACKAGE];
    inDataLeft = new uint64_t [DIES_PER_PACKAGE];
    waiting =  new bool [DIES_PER_PACKAGE];

    for(uint i = 0; i < DIES_PER_PACKAGE; i++){
	outDataSize[i] = 0;
	inDataSize[i] = 0;
	cyclesLeft[i] = 0;
	outDataLeft[i] = 0;
	critData[i] = 0;
	inDataLeft[i] = 0;
	waiting[i] = false;
   }

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

bool Buffer::sendPiece(SenderType t, uint type, uint64_t die, uint64_t plane){
    if(t == CONTROLLER)
    {
	if(IN_BUFFER_SIZE == 0 || inDataSize[die] <= (IN_BUFFER_SIZE-(CHANNEL_WIDTH)))
	{
	    if(!inData[die].empty() && inData[die].back()->type == type && inData[die].back()->plane == plane &&
	       type == 5 && inData[die].back()->number < (NV_PAGE_SIZE*8192))
	    {
		inData[die].back()->number = inData[die].back()->number + CHANNEL_WIDTH;
		inDataSize[die] = inDataSize[die] + CHANNEL_WIDTH;
	    }
	    else if(!inData[die].empty() && inData[die].back()->type == type && inData[die].back()->plane == plane && 
		    type != 5 && inData[die].back()->number < COMMAND_LENGTH)
	    {
		inData[die].back()->number = inData[die].back()->number + CHANNEL_WIDTH;
		inDataSize[die] = inDataSize[die] + CHANNEL_WIDTH;
	    }
	    else
	    {	
		BufferPacket* myPacket = new BufferPacket();
		myPacket->type = type;
		myPacket->number = CHANNEL_WIDTH;
		myPacket->plane = plane;
		inData[die].push_back(myPacket);
		inDataSize[die] = inDataSize[die] + CHANNEL_WIDTH;
	    }
	    return true;
	}
	else
	{
	    return false;
	}
    }
    else if(t == BUFFER)
    {
	if(OUT_BUFFER_SIZE == 0 || outDataSize[die] <= (OUT_BUFFER_SIZE-DEVICE_WIDTH))
	{
	    if(!outData[die].empty() && outData[die].back()->type == type && outData[die].back()->plane == plane &&
	       outData[die].back()->number < (NV_PAGE_SIZE*8192)){
		outData[die].back()->number = outData[die].back()->number + DEVICE_WIDTH;
		outDataSize[die] = outDataSize[die] + DEVICE_WIDTH;
		// if ths was the last piece of this packet, tell the die
		if( outData[die].back()->number >= (NV_PAGE_SIZE*8192))
		{
		    dies[die]->bufferLoaded();
		}
	    }else{
		BufferPacket* myPacket = new BufferPacket();
		myPacket->type = type;
		myPacket->number = DEVICE_WIDTH;
		myPacket->plane = plane;
		outData[die].push_back(myPacket);
		outDataSize[die] = outDataSize[die] + DEVICE_WIDTH;
	    }
	    return true;
	}
	else
	{
	    return false;
	}
    }
    return false;
}

bool Buffer::isFull(SenderType t, uint64_t die)
{
    if(t == CONTROLLER)
    {
	if(IN_BUFFER_SIZE == 0 || inDataSize[die] <= (IN_BUFFER_SIZE-(CHANNEL_WIDTH)))
	{
	    return false;
	}
	else
	{
	    return true;
	}
    }
    else if(t == BUFFER)
    {
	if(OUT_BUFFER_SIZE == 0 || outDataSize[die] <= (OUT_BUFFER_SIZE-DEVICE_WIDTH))
	{
	    return false;
	}
	else
	{
	    return true;
	}
    }
    return true;
}
	    
void Buffer::update(void){
    for(uint64_t i = 0; i < DIES_PER_PACKAGE; i++){
	// moving data into a die
	//==================================================================================
	// if we're not already busy writing stuff
	if(!inData[i].empty())
	{
	    // *NOTE* removed check for inDataLeft == inData which i think was to make sure this didn't get called when things where just empty
	    if(inData[i].front()->number >= DEVICE_WIDTH)
	    {
		//cout << "we have enough data to send \n";
		// if it is a command, set the number of beats if we've not set them yet
		if(inData[i].front()->type != 5)
		{
		    // first time we've dealt with this command so we need to set our values
		    if(inDataLeft[i] == 0 && waiting[i] != true)
		    {
			inDataLeft[i] = COMMAND_LENGTH;
			cyclesLeft[i] = divide_params(DEVICE_CYCLE,CYCLE_TIME);
			processInData(i);
		    }
		    // need to make sure either enough data has been transfered to the buffer to warrant
		    // sending out more data or all of the data for this particular packet has already
		    // been loaded into the buffer
		    else if(inData[i].front()->number >= ((COMMAND_LENGTH-inDataLeft[i])+DEVICE_WIDTH) ||
			    (inData[i].front()->number >= COMMAND_LENGTH))
		    {
			processInData(i);
		    }
		}
		// its not a command but it is the first time we've dealt with this data
		else if(inDataLeft[i] == 0 && waiting[i] != true)
		{
		    inDataLeft[i] = (NV_PAGE_SIZE*8192);
		    cyclesLeft[i] = divide_params(DEVICE_CYCLE,CYCLE_TIME);
		    processInData(i);
		}
		// its not a command and its not the first time we've seen it but we still need to make sure either
		// there is enough data to warrant sending out the data or all of the data for this particular packet has already
		// been loaded into the buffer
			
		else if (inData[i].front()->number >= (((NV_PAGE_SIZE*8192)-inDataLeft[i])+DEVICE_WIDTH) ||
			    (inData[i].front()->number >= (NV_PAGE_SIZE*8192)))
		{
		    processInData(i);
		}
	    }
	}	    
	    
	
	// moving data away from die
	//====================================================================================
	// first scan through to see if we have stuff to send if we're not busy
	if(!outData[i].empty())
	{
	    if(outData[i].front()->number >= CHANNEL_WIDTH)
	    {
		//cout << "buffer tried to get the channel \n";
		// then see if we have control of the channel
		if (channel->hasChannel(BUFFER, id) && sendingDie == i && sendingPlane == outData[i].front()->plane)
		{
		    if((outData[i].front()->number >= (((NV_PAGE_SIZE*8192)-outDataLeft[i])+CHANNEL_WIDTH)) ||
		       (outData[i].front()->number >= (NV_PAGE_SIZE*8192)))
		    {
			processOutData(i);
		    }
		}
		else if (channel->obtainChannel(id, BUFFER, NULL)){
		    outDataLeft[i] = (NV_PAGE_SIZE*8192);
		    sendingDie = i;
		    sendingPlane = outData[i].front()->plane;
		    processOutData(i);
		}
	    }
	}
    }
}

void Buffer::processInData(uint64_t die){

    // count down the time it takes for the device to latch the data
    if(cyclesLeft[die] > 0)
    {
	cyclesLeft[die]--;
    }

    // device has finished latching this particular piece of data
    if(cyclesLeft[die] == 0)
    {	    
	// do we have data to send
	if(inDataLeft[die] > 0)
	{
	    // set the device latching cycle for this next piece of data
	    cyclesLeft[die] = divide_params(DEVICE_CYCLE,CYCLE_TIME);
	    // subtract this chunk of data from the data we need to send to be done
	    if(inDataLeft[die] >= DEVICE_WIDTH)
	    {
		//cout << "sending data to die \n";
		inDataLeft[die] = inDataLeft[die] - DEVICE_WIDTH;
		inDataSize[die] = inDataSize[die] - DEVICE_WIDTH;
	    }
	    // if we only had a tiny amount left to send just set remaining count to zero
	    // to avoid negative numbers here which break things
	    else
	    {
		inDataSize[die] = inDataSize[die] - inDataLeft[die];
		inDataLeft[die] = 0;
	    }
	}
    }
    
    // we're done here
    if(inDataLeft[die] == 0)
    {
	if(!dies[die]->isDieBusy(inData[die].front()->plane))
	{   
	    channel->bufferDone(id, die, inData[die].front()->plane);
	    inData[die].pop_front();
	    waiting[die] = false;
	}
	else
	{
	    waiting[die] = true;
	}
    }
}

void Buffer::processOutData(uint64_t die){
    // deal with the critical line first stuff first
    if(critData[die] >= 512 && critData[die] < 512+CHANNEL_WIDTH && channel->notBusy())
    {
	dies[die]->critLineDone();
    }
    
    // got the channel and we have stuff to send so send it
    if(outDataLeft[die] > 0 && channel->notBusy()){
	channel->sendPiece(BUFFER,outData[die].front()->type,die,outData[die].front()->plane);
	
	if(outDataLeft[die] >= CHANNEL_WIDTH)
	{
	    outDataLeft[die] = outDataLeft[die] - CHANNEL_WIDTH;
	    outDataSize[die] = outDataSize[die] - CHANNEL_WIDTH;
	}
	else
	{
	    outDataSize[die] = outDataSize[die] - outDataLeft[die];
	    outDataLeft[die] = 0;
	}
	critData[die] = critData[die] + CHANNEL_WIDTH;
    }
    
    // we're done here
    if(outDataLeft[die] == 0 && channel->notBusy())
    {
	dies[die]->bufferDone(outData[die].front()->plane);
	channel->releaseChannel(BUFFER,id);
	critData[die] = 0;
	outData[die].pop_front();
    }
}
