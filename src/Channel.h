#ifndef NVCHANNEL_H
#define NVCHANNEL_H
//Channel.h
//header file for the Package class

#include "FlashConfiguration.h"
#include "Die.h"
#include "ChannelPacket.h"
#include "Util.h"

namespace NVDSim{
	enum SenderType{
		CONTROLLER,
		DIE
	};

	class Controller;
	class Channel{
		public:
			Channel(void);
			void attachDie(Die *d);
			void attachController(Controller *c);
			int obtainChannel(uint s, SenderType t, ChannelPacket *p);
			int releaseChannel(SenderType t, uint s);
			int hasChannel(SenderType t, uint s);
			void sendToDie(ChannelPacket *busPacket);
			void sendToController(ChannelPacket *busPacket);
			void sendPiece(SenderType t, uint type);
			int notBusy(void);

			void update(void);
			
			Controller *controller;
		private:
			SenderType type;
			int sender;
			int busy;
			int firstCheck;
			int deviceWriting;
			std::vector<Die *> dies;

			std::vector<uint> cyclesLeft; //cycles per device or channel beat
			std::vector<uint> beatsLeft; //beats per page
			std::vector<uint> beatsDone; //beats sent
	};
}
#endif
