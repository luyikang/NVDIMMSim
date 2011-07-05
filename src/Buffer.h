#ifndef NVBUFFER_H
#define NVBUFFER_H
// Buffer.h
// Header file for the buffer class

#include "SimObj.h"
#include "FlashConfiguration.h"
#include "ChannelPacket.h"
#include "Die.h"
#include "Channel.h"

namespace NVDSim{
    class Buffer : public SimObj{
        public:
	    Buffer(uint id);
	    void attachDie(Die *d);
	    void attachChannel(Channel *c);
	    void sendToDie(ChannelPacket *busPacket);
	    void sendToController(ChannelPacket *busPacket);

	    void sendPiece(SenderType t, uint type, uint die, uint plane);
	    
	    void update(void);

	    Channel *channel;
	    std::vector<Die *> dies;

        private:
	    class BufferPacket{
	        public:
		// type of packet that this is part of
		uint type;
		// what part of the packet this is
		uint number;
		
		BufferPacket(){
		    type = 0;
		    number = 0;
		}
	    };
	    uint id;

	    int** writing_busy;
	    
	    uint** cyclesLeft;	    
	    uint** deviceWriting;
	    uint** outDataLeft;
	    uint** inDataLeft;

	    uint sendingDie;
	    uint sendingPlane;

	    std::vector<std::vector<std::list<BufferPacket *> > > outData;
	    std::vector<std::vector<std::list<BufferPacket *> > > inData;

	    int count;
    };
} 

#endif
