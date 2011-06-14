#ifndef NVBUFFER_H
#define NVBUFFER_H
// Buffer.h
// Header file for the buffer class

#include "SimObj.h"
#include "FlashConfiguration.h"
#include "ChannelPacket.h"
#include "Channel.h"
#include "Die.h"

namespace NVDSim{
    class Buffer : public SimObj{
        public:
	    Buffer(void);
	    attachDie(Die *d);
	    channelDone(uint Plane);

	    bool notBusy(void);
	    
	    void update(void);

	    void acknowledge(uint die, uint plane);
        private:
	    std::vector<Die *> dies;
	    
	    uint** cyclesLeft;	    
	    uint** deviceWriting;

	    std::vector<std::vector<std::queue<BufferPacket *> > > outPackets;
	    std::vector<std::vector<std::queue<BufferPacket *> > > inPackets;

	    class BufferPacket{
		// type of packet that this is part of
		uint type;
		// what part of the packet this is
		uint number;
		
		BufferPacket(){
		    type = 0;
		    number = 0;
		}
	    };
    };
} 

#endif
