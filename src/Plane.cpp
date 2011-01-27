//Plane.cpp
//class file for plane object
//

#include "Plane.h"

using namespace NVDSim;
using namespace std;

Plane::Plane(void){
	dataReg= NULL;
	cacheReg= NULL;
}

void Plane::read(ChannelPacket *busPacket){
	if (blocks.find(busPacket->block) != blocks.end()){
	    busPacket->data= blocks[busPacket->block].read(busPacket->size, busPacket->word, busPacket->page);
	} else{
		DEBUG("Invalid read: Block "<<busPacket->block<<" hasn't been written to");
	}

	busPacket->busPacketType= DATA;
	dataReg= busPacket;
}

void Plane::write(ChannelPacket *busPacket){
        // if this block has not been accessed yet, construct a new block and add it to the blocks map
	if (blocks.find(busPacket->block) == blocks.end())
		blocks[busPacket->block] = Block(busPacket->block);

        blocks[busPacket->block].write(busPacket->size, busPacket->word, busPacket->page, dataReg->data);
}

// should only ever erase blocks
void Plane::erase(ChannelPacket *busPacket){
	if (blocks.find(busPacket->block) != blocks.end()){
		blocks[busPacket->block].erase();
		blocks.erase(busPacket->block);
	}
}


void Plane::storeInData(ChannelPacket *busPacket){
	dataReg= busPacket;
}

ChannelPacket *Plane::readFromData(void){
	return dataReg;
}
