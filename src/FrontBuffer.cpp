/*********************************************************************************
*  Copyright (c) 2011-2012, Paul Tschirhart
*                             Peter Enns
*                             Jim Stevens
*                             Ishwar Bhati
*                             Mu-Tien Chang
*                             Bruce Jacob
*                             University of Maryland 
*                             pkt3c [at] umd [dot] edu
*  All rights reserved.
*  
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions are met:
*  
*     * Redistributions of source code must retain the above copyright notice,
*        this list of conditions and the following disclaimer.
*  
*     * Redistributions in binary form must reproduce the above copyright notice,
*        this list of conditions and the following disclaimer in the documentation
*        and/or other materials provided with the distribution.
*  
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
*  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
*  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
*  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
*  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
*  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
*  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
*  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
*  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*********************************************************************************/

// FrontBuffer.cpp
// class file for the front side buffer for nvdimm

#include "FrontBuffer.h"
#include "Ftl.h"
#include "NVDIMM.h"

using namespace NVDSim;

FrontBuffer::FrontBuffer(NVDIMM* parent){
    parentNVDIMM = parent;
    sender = -1;
    
    // transaction queues, separate command queue not needed because commands
    // always accompany requests
    requests = queue<FlashTransaction *>();
    responses = queue<FlashTransaction *>();

    // usage of buffer space
    requestsSize = 0;
    responsesSize = 0;
    
    // transaction currently being serviced by each potential channel
    requestTrans = NULL;
    responseTrans = NULL;
    commandTrans = NULL;
    
    // queue of transactions that have been finished by command or data channels but
    // still waiting on the other channel in a split channel setup
    pendingData = vector<FlashTransaction *>();
    pendingCommand = vector<FlashTransaction *>();

    // channel progress counters
    requestCyclesLeft = 0;
    responseCyclesLeft = 0;
    commandCyclesLeft = 0;
}

// allows us to push a transaction into the FTL
void FrontBuffer::attachFTL(Ftl *f){
    ftl = f;
}

// place the transaction in the appropriate queue
bool FrontBuffer::addTransaction(FlashTransaction *transaction){
    switch (transaction->transactionType)
    {
    case DATA_READ: 
	if(requestsSize <= (REQUEST_BUFFER_SIZE - COMMAND_LENGTH))
	{
	    requests.push(transaction);
	    requestsSize += COMMAND_LENGTH;
	    return true;
	}
	else
	{
	    return false;
	}
    case DATA_WRITE:
	if(requestsSize <= (REQUEST_BUFFER_SIZE - (COMMAND_LENGTH + (NV_PAGE_SIZE*BITS_PER_KB))))
	{
	    requests.push(transaction);
	    requestsSize += (COMMAND_LENGTH + (NV_PAGE_SIZE*BITS_PER_KB));
	    return true;
	}
	else
	{
	    return false;
	}
    case RETURN_DATA:
	if(responsesSize <= (RESPONSE_BUFFER_SIZE - (NV_PAGE_SIZE*BITS_PER_KB)))
	{
	    responses.push(transaction);
	    responsesSize += (NV_PAGE_SIZE*BITS_PER_KB);
	    return true;
	}
	else
	{
	    return false;
	}
    default:
	ERROR("Illegal transaction added \n");
	return false;
    }
}

void FrontBuffer::update(void){

    // always check the response channel status cause we will always have a
    // response channel
    // channel already doing something, keep doing it
    if(responseTrans != NULL)
    {
	updateResponse();
    }
    // if half duplex case, responses get strict priority over requests
    else if(!responses.empty())
    {
	responseTrans = responses.front();
	responses.pop();
	responsesSize = subtract_params(responsesSize, (NV_PAGE_SIZE*BITS_PER_KB));
	updateResponse();
    }
    // half duplex case, requests also use the response channel
    // we need to do this even if there is a command channel because there
    // may be write data to send (read cases are handled by the updateResponse function)
    else if(!ENABLE_REQUEST_CHANNEL && !requests.empty())
    {
	responseTrans = requests.front();
	// pop only if we aren't going to need this trans for the command channel too
	if(!ENABLE_COMMAND_CHANNEL)
	{
	    requests.pop();
	    requestsSize = subtract_params(requestsSize, (COMMAND_LENGTH + (NV_PAGE_SIZE*BITS_PER_KB)));
	}
	else
	{
	    requestsSize = subtract_params(requestsSize, (NV_PAGE_SIZE*BITS_PER_KB));
	}
	updateResponse();
    }

    // full duplex case, requests use dedicated request channel
    if(ENABLE_REQUEST_CHANNEL)
    {
	// channel already doing something, keep doing it
	if(requestTrans != NULL)
	{
	    updateRequest();
	}
	// again we need to do this even if there is a command channel for the 
	// write data (reads will be handled differently in the updateRequest function)
	else if(!requests.empty())
	{
	    requestTrans = requests.front();
	    // pop only if we aren't going to need this trans for the command channel too
	    if(!ENABLE_COMMAND_CHANNEL)
	    {
		requests.pop();
		requestsSize = subtract_params(requestsSize, (COMMAND_LENGTH + (NV_PAGE_SIZE*BITS_PER_KB)));
	    }
	    else
	    {
		requestsSize = subtract_params(requestsSize, (NV_PAGE_SIZE*BITS_PER_KB));
	    }
	    updateRequest();
	}
    }

    // separate command channel
    if(ENABLE_COMMAND_CHANNEL)
    {
	// channel already doing something, keep doing it
	if(commandTrans != NULL)
	{
	    updateCommand();
	}
	else if(!requests.empty())
	{
	    commandTrans = requests.front();
	    requests.pop();
	    requestsSize = subtract_params(requestsSize, COMMAND_LENGTH);
	    updateCommand();
	}
    }
}

void FrontBuffer::updateResponse(void){
    // first time calling this for this command
    // need to figure out how many cycles we need to move the transaction to the FTL or Hybrid controller
    if(responseCyclesLeft == 0)
    {
	if(responseTrans->transactionType == RETURN_DATA)
	{
	    // number of channel cycles to go equals the total number of data bits divided by the bits 
	    // moved per channel cycle
	    responseCyclesLeft = divide_params((NV_PAGE_SIZE*BITS_PER_KB), CHANNEL_WIDTH);
	}
	// no request channel to handle request data and commands so we need to handle those cases
	else if(!ENABLE_REQUEST_CHANNEL)
	{
	    // function handles determining the right number of cycles to delay by
	    responseCyclesLeft = setDataCycles(responseTrans, CHANNEL_WIDTH);
	}
	else
	{
	    // sanity check
	    ERROR("unidentified command on response channel");
	}
    }

    // we're updating so data has moved
    // even if we're calling this for the time
    responseCyclesLeft--;

    // we're done here
    if(responseCyclesLeft <= 0)
    {
	// just to make sure we precisely tigger the first if statement
	responseCyclesLeft = 0;

	if(responseTrans->transactionType == RETURN_DATA)
	{
	    // data has been moved, do the callback to hybrid
	    sendToHybrid(*responseTrans);
	    responseTrans = NULL;
	}
	else{
	// *** TODO this code is also replicated between the response and request update functions and needs to be pulled out ******************************************
	    if(ENABLE_COMMAND_CHANNEL)
	    {
		// see if the command had already been sent over the command channel
		vector<FlashTransaction*>::iterator it;
		for (it = pendingData.begin(); it != pendingData.end(); it++)
		{
		    // if the pointers point ot the same thing then the command channel has sent this
		    // transactions command already
		    if((*it) == responseTrans)
		    {
			// command has already been sent, now data is done so send to FTL
			sendToFTL(responseTrans);
			responseTrans = NULL;
			
			// clear pending entry
			pendingData.erase(it);
			
			// iterators now invalid due to reordering, have to break
			// should only be one entry that matches anyway
			break;
		    }
		}
		
		// didn't find anything, still have a transaction
		// need to wait for command channel to send command
		// so push onto the pendingCommand vector
		if(responseTrans != NULL)
		{
		    pendingCommand.push_back(responseTrans);
		    
		    // we can use the response channel for other things though
		    // while waiting on the command channel
		    responseTrans = NULL;
		}
	    }
	    else
	    {
		// no command channel to wait on so if we're done then just send to the FTL
		sendToFTL(responseTrans);
		responseTrans = NULL;
	    }
	}
    }
}

void FrontBuffer::updateRequest(void){
    // first time calling this for this command
    // need to figure out how many cycles we need to move the transaction to the FTL
    if(requestCyclesLeft == 0)
    {
	// function handles determining the right number of cycles to delay by
	requestCyclesLeft = setDataCycles(requestTrans, REQUEST_CHANNEL_WIDTH);
    }
    
    // we're updating so data has moved
    // even if we're calling this for the time
    requestCyclesLeft--;

    // we're done here
    if(requestCyclesLeft <= 0)
    {
	if(ENABLE_COMMAND_CHANNEL)
	{
	    // see if the command had already been sent over the command channel
	    vector<FlashTransaction*>::iterator it;
	    for (it = pendingData.begin(); it != pendingData.end(); it++)
	    {
		// if the pointers point ot the same thing then the command channel has sent this
		// transactions command already
		if((*it) == requestTrans)
		{
		    // command has already been sent, now data is done so send to FTL
		    sendToFTL(requestTrans);
		    requestTrans = NULL;
		    
		    // clear pending entry
		    pendingData.erase(it);
		    
		    // iterators now invalid due to reordering, have to break
		    // should only be one entry that matches anyway
		    break;
		}
	    }
	    
	    // didn't find anything, still have a transaction
	    // need to wait for command channel to send command
	    // so push onto the pendingCommand vector
	    if(requestTrans != NULL)
	    {
		pendingCommand.push_back(requestTrans);
		
		// we can use the response channel for other things though
		// while waiting on the command channel
		requestTrans = NULL;
	    }
	}
	else
	{
	    // no command channel to wait on so if we're done then just send to the FTL
	    sendToFTL(requestTrans);
	    responseTrans = NULL;
	}
    }
}



void FrontBuffer::updateCommand(void){
    // first time calling this for this command
    // need to figure out how many cycles we need to move the command to the FTL
    if(commandCyclesLeft == 0)
    {
	commandCyclesLeft = divide_params(COMMAND_LENGTH, COMMAND_CHANNEL_WIDTH);
    }

    // we're updating so command bits have moved
    // even if we're calling this for the time
    commandCyclesLeft--;

    // we're done here
    if(commandCyclesLeft <= 0)
    {
	// see if the command had already been sent over the command channel
	vector<FlashTransaction*>::iterator it;
	for (it = pendingCommand.begin(); it != pendingCommand.end(); it++)
	{
	    // if the pointers point ot the same thing then the command channel has sent this
	    // transactions command already
	    if((*it) == commandTrans)
	    {
		// command has already been sent, now data is done so send to FTL
		sendToFTL(commandTrans);
		commandTrans = NULL;
		
		// clear pending entry
		pendingCommand.erase(it);
		
		// iterators now invalid due to reordering, have to break
		// should only be one entry that matches anyway
		break;
	    }
	}
	
	// didn't find anything, still have a transaction
	// need to wait for data channel to send the data
	// so push onto the pendingData vector
	if(commandTrans != NULL)
	{
	    pendingData.push_back(commandTrans);
	    
	    // we can use the command channel for other things though
	    // while waiting on the request channel
	    commandTrans = NULL;
	}
    }
}

uint64_t FrontBuffer::setDataCycles(FlashTransaction *transaction, uint64_t width){
    if(transaction->transactionType == DATA_READ)
    {
	if(!ENABLE_COMMAND_CHANNEL)
	{
	    // number of channel cycles to go equals the total number of command bits
	    // divided by the bits moved per channel cycle
	    return divide_params(COMMAND_LENGTH, width);
	}
	// command channel is handling the read command so do nothing
	// set to one so that the decrement doesn't break things
	else
	{
	    return 1;
	}
    }
    else if(transaction->transactionType == DATA_WRITE)
    {
	if(!ENABLE_COMMAND_CHANNEL)
	{
	    // number of channel cycles to go equals the total number of data bits plus the number of command bits
	    // divided by the bits moved per channel cycle
	    return divide_params(((NV_PAGE_SIZE*BITS_PER_KB)+COMMAND_LENGTH), width);
	}
	else
	{
	    // number of channel cycles to go equals the total number of data bits divided by the bits 
	    // moved per channel cycle
	    return divide_params((NV_PAGE_SIZE*BITS_PER_KB), width);
	}
    }
    ERROR("Something bad happened in setDataCycles");
    abort();
}

// we are done, these functions handle the finish cases
// done with outgoing data to dies, so add this transaction to the FTL cause the data has been sent
void FrontBuffer::sendToFTL(FlashTransaction *transaction){
    ftl->addTransaction(*transaction);
}

// done with returning data from dies, so initiate a callback 
void FrontBuffer::sendToHybrid(const FlashTransaction &transaction){
    if(parentNVDIMM->ReturnReadData!=NULL){
	(*parentNVDIMM->ReturnReadData)(parentNVDIMM->systemID, transaction.address, currentClockCycle, true);
    }
    parentNVDIMM->numReads++;
}
