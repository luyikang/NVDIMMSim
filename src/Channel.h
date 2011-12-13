#ifndef NVCHANNEL_H
#define NVCHANNEL_H
//Channel.h
//header file for the Package class

#include "FlashConfiguration.h"
#include "ChannelPacket.h"
#include "Util.h"

namespace NVDSim{
	enum SenderType{
		CONTROLLER,
		BUFFER
	};

	class Controller;
	class Buffer;
	class Channel{
		public:
			Channel(void);
			void attachBuffer(Buffer *b);
			void attachController(Controller *c);
			int obtainChannel(uint s, SenderType t, ChannelPacket *p);
			int releaseChannel(SenderType t, uint s);
			int hasChannel(SenderType t, uint s);		
			void sendToBuffer(ChannelPacket *busPacket);
			void sendToController(ChannelPacket *busPacket);
			void sendPiece(SenderType t, uint type, uint die, uint plane);
			bool isBufferFull(SenderType t, uint die);
			int notBusy(void);

			void update(void);

			void bufferDone(uint64_t package, uint64_t die, uint64_t plane);
			
			Controller *controller;
		private:
			SenderType sType;
			uint packetType;
			int sender;
			int busy;
			int firstCheck;
			Buffer *buffer;

			uint currentDie;
			uint currentPlane;
	};
}
#endif
