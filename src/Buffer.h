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
	    Buffer(uint64_t i);
	    void attachDie(Die *d);
	    void attachChannel(Channel *c);
	    void sendToDie(ChannelPacket *busPacket);
	    void sendToController(ChannelPacket *busPacket);

	    bool sendPiece(SenderType t, uint type, uint64_t die, uint64_t plane);
	    bool isFull(SenderType t, uint64_t die);
	    
	    void update(void);

	    void processInData(uint64_t die);
	    void processOutData(uint64_t die);

	    Channel *channel;
	    std::vector<Die *> dies;

	    uint64_t id;

        private:
	    class BufferPacket{
	        public:
		// type of packet
		uint type;
		// how many bits are outstanding for this page
		uint64_t number;
		// plane that this page is for
		uint64_t plane;

		BufferPacket(){
		    type = 0;
		    number = 0;
		    plane = 0;
		}
	    };
	    
	    uint64_t* cyclesLeft;	    
	    uint64_t* outDataLeft;
	    uint64_t* critData; // burst on which the critical line will be done
	    uint64_t* inDataLeft;
	    bool* waiting;
	    bool* sending;

	    uint64_t sendingDie;
	    uint64_t sendingPlane;

	    uint64_t* outDataSize;
	    std::vector<std::list<BufferPacket *> >  outData;
	    uint64_t* inDataSize;
	    std::vector<std::list<BufferPacket *> > inData;
    };
} 

#endif
