#ifndef NVDIE_H
#define NVDIE_H
//Die.h
//header file for the Die class

#include "SimObj.h"
#include "FlashConfiguration.h"
#include "ChannelPacket.h"
#include "Plane.h"
#include "Logger.h"
#include "Util.h"

namespace NVDSim{

	class Buffer;
	class Controller;
	class NVDIMM;
	class Ftl;
	class Die : public SimObj{
		public:
	                Die(NVDIMM *parent, Logger *l, uint id);
			void attachToBuffer(Buffer *buff);
			void receiveFromBuffer(ChannelPacket *busPacket);
			int isDieBusy(uint plane);
			void update(void);
			void channelDone(void);
			void bufferDone(void);
			void critLineDone(void);

			// for fast forwarding
			void writeToPlane(ChannelPacket *packet);

		private:
			uint id;
			NVDIMM *parentNVDIMM;
			Buffer *buffer;
			Logger *log;
			bool sending;
			uint dataCyclesLeft; //cycles per device beat
			uint deviceBeatsLeft; //device beats per page
			uint critBeat; //device beat when first cache line will have been sent, used for crit line first
			std::queue<ChannelPacket *> returnDataPackets;
			std::vector<Plane> planes;
			std::vector<ChannelPacket *> currentCommands;
			uint *controlCyclesLeft;
	};
}
#endif
