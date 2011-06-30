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
			int notBusy(void);

			void update(void);

			void bufferDone(uint die, uint plane);
			
			Controller *controller;
		private:
			SenderType sType;
			uint packetType;
			int sender;
			int busy;
			int firstCheck;
			Buffer *buffer;

			uint cyclesLeft; //cycles per device or channel beat
			uint currentDie;
			uint currentPlane;
	};
}
#endif
