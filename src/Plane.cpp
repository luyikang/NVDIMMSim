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

//Plane.cpp
//class file for plane object
//

#include "Plane.h"

using namespace NVDSim;
using namespace std;

Plane::Plane(void){
	dataReg= NULL;
	cacheReg= NULL;
	write_reg = 0;
	read_reg = 0;
}

void Plane::read(ChannelPacket *busPacket){
	if (blocks.find(busPacket->block) != blocks.end()){
	    busPacket->data= blocks[busPacket->block].read(busPacket->page);
	} else{
		DEBUG("Invalid read: Block "<<busPacket->block<<" hasn't been written to");
	}

	// Put this packet on either the data register or the cache register to send it back (it will 
	// eventually end up in Controller::receieveFromChannel).
	if(dataReg == NULL)
	{
	    dataReg = busPacket;
	    read_reg = 1;
	}
	else if(dataReg != NULL && cacheReg == NULL)
	{
	    cacheReg = busPacket;
	    read_reg = 2;
	}
	else
	{
	    // no more room in this inn, somebody fucked up and it was probably me
	    DEBUG("tried to add read data to plane "<<busPacket->plane<<" but both of its registers were full");
	}
}

void Plane::write(ChannelPacket *busPacket){
        // if this block has not been accessed yet, construct a new block and add it to the blocks map
	if (blocks.find(busPacket->block) == blocks.end())
		blocks[busPacket->block] = Block(busPacket->block);

	if(busPacket->busPacketType == FAST_WRITE)
	{
	    blocks[busPacket->block].write(busPacket->page, NULL);
	}
	// safety first...
	else if(write_reg == 1)
	{
	    if(dataReg != NULL)
	    {
		blocks[busPacket->block].write(busPacket->page, dataReg->data);
	    
		// The data packet is now done being used, so it can be deleted.
		delete dataReg;
		dataReg = NULL;
	    }
	    else
	    {
		DEBUG("Invalid write: dataReg was NULL when block "<<busPacket->block<<"was written to");
	    }
	}
	else if(write_reg == 2)
	{
	    if(cacheReg != NULL)
	    {
		blocks[busPacket->block].write(busPacket->page, cacheReg->data);
	    
		// The data packet is now done being used, so it can be deleted.
		delete cacheReg;
		cacheReg = NULL;
	    }
	    else
	    {
		DEBUG("Invalid write: dataReg was NULL when block "<<busPacket->block<<"was written to");
	    }
	}
	else
	{
	    DEBUG("Invalid write: dataReg and cacheReg were NULL when block "<<busPacket->block<<"was written to");
	}
}

// should only ever erase blocks
void Plane::erase(ChannelPacket *busPacket){
	if (blocks.find(busPacket->block) != blocks.end()){
		blocks[busPacket->block].erase();
		blocks.erase(busPacket->block);
	}
}


void Plane::storeInData(ChannelPacket *busPacket){
    if(dataReg == NULL && cacheReg == NULL)
    {
	dataReg= busPacket;
	write_reg = 1;
    }
    else if(dataReg != NULL && cacheReg == NULL)
    {
	cacheReg = busPacket;
	if(read_reg == 1)
	{
	    write_reg = 2; // the data in the data reg is for a read, so the the cache reg will be used by the next write
	}
	else
	{
	    write_reg = 1; // if there was data in the data reg then its going to be used by the next write
	}
    }
    else if(dataReg == NULL && cacheReg != NULL)
    {
	dataReg= busPacket;
	if(read_reg == 2)
	{
	    write_reg = 1; // the data in the cache reg is for a read so the next write should use the data reg
	}
	else
	{
	    write_reg = 2; // if there was data in the cache reg then its going to be used by the next write
	}
    }
    else
    {
	// no more room in this inn, somebody fucked up and it was probably me
	ERROR("tried to add write data to plane "<<busPacket->plane<<" but both of its registers were full");
    }

}

ChannelPacket *Plane::readFromData(void){
    ChannelPacket *temp =  NULL; // so we can free the register
    if(read_reg == 1)
    {
	// read no longer needs this register
	read_reg = 0;
	temp = dataReg;
	dataReg = NULL;
	return temp;
    }
    else if(read_reg == 2)
    {
	// read no longer needs this register
	read_reg = 0;
	temp = cacheReg;
	cacheReg = NULL;
	return temp;
    }
    else
    {
	ERROR("tried to read from a plane but there was no read data on either of its registers");
	abort();
    }

}
