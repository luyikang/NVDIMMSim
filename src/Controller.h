#ifndef NVCONTROLLER_H
#define NVCONTROLLER_H
//Controller.h
//header file for controller

#include "SimObj.h"
#include "FlashConfiguration.h"
#include "Die.h"
#include "Ftl.h"
#include "Channel.h"
#include "FlashTransaction.h"
#include "Logger.h"
#include "Util.h"

namespace NVDSim{
	typedef struct {
		Channel *channel;
		std::vector<Die *> dies;
	} Package;

	class NVDIMM;
	class Controller : public SimObj{
		public:
	                Controller(NVDIMM* parent, Logger* l);

			void attachPackages(vector<Package> *packages);
			void returnReadData(const FlashTransaction &trans);
			void returnPowerData(vector<double> idle_energy,
					 vector<double> access_energy,
					 vector<double> erase_energy,
					 vector<double> vpp_idle_energy,
					 vector<double> vpp_access_energy,
					 vector<double> vpp_erase_energy);
			void returnPowerData(vector<double> idle_energy,
					 vector<double> access_energy,
					 vector<double> vpp_idle_energy,
					 vector<double> vpp_access_energy);
			void returnPowerData(vector<double> idle_energy,
					 vector<double> access_energy,
					 vector<double> erase_energy);
			void returnPowerData(vector<double> idle_energy,
					 vector<double> access_energy);
			void attachChannel(Channel *channel);
			void receiveFromChannel(ChannelPacket *busPacket);
			bool addPacket(ChannelPacket *p);
			void update(void);

			void sendQueueLength(void);

			void channelDone(uint die, uint plane);

			// for fast forwarding
			void writeToPackage(ChannelPacket *packet);

			NVDIMM *parentNVDIMM;
			Logger *log;

		private:
			std::list<FlashTransaction> returnTransaction;
			std::vector<Package> *packages;
			std::vector<std::queue <ChannelPacket *> > channelQueues;
			std::vector<ChannelPacket *> outgoingPackets; //there can only ever be one outgoing packet per channel
			std::vector<std::list <ChannelPacket *> > pendingPackets; //there can be a pending package for each plane of each die of each package
			std::vector<uint> channelXferCyclesLeft; //cycles per channel beat
			std::vector<uint> channelBeatsLeft; //channel beats per page

			uint*** busyPlanes;
	};
}
#endif
