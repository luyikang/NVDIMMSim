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
	    
	    void update(void);
        private:
	    std::vector<Die *> dies;
	    
	    uint** cyclesLeft;
	    uint** beatsLeft;
	    
	    uint** deviceWriting;
	    uint** writePending;
	    uint** packetType;
    };
} 

#endif
