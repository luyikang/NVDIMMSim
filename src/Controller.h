#ifndef CONTROLLER_H
#define CONTROLLER_H
//Controller.h
//header file for controller

#include "SimObj.h"
#include "FlashConfiguration.h"
#include "Die.h"
#include "Ftl.h"

#include "Channel.h"
#include "FlashTransaction.h"

namespace NVDSim{
	typedef struct {
		Channel *channel;
		std::vector<Die *> dies;
	} Package;

	class NVDIMM;
	class Controller : public SimObj{
		public:
			Controller(NVDIMM* parent);
			void attachPackages(vector<Package> *packages);
			void returnReadData(const FlashTransaction &trans);
			void returnIdlePower(vector<double> idle_energy);
			void returnAccessPower(vector<double> access_energy);
#if GC
			void returnErasePower(vector<double> erase_energy);
#endif

			void attachChannel(Channel *channel);
			void receiveFromChannel(ChannelPacket *busPacket);
			bool addPacket(ChannelPacket *p);
			void update(void);
			NVDIMM *parentNVDIMM;
		private:
			std::list<FlashTransaction> returnTransaction;
			std::vector<Package> *packages;
			std::vector<std::queue <ChannelPacket *> > channelQueues;
			std::vector<ChannelPacket *> outgoingPackets;
			std::vector<uint> channelXferCyclesLeft;
	};
}
#endif
