#ifndef SMALLCHANNELPACKET_H
#define SMALLCHANNELPACKET_H
//SmallChannelPacket.h
//
//Header file for bus packet object used when small access is enabled
//

#include "FlashConfiguration.h"

namespace NVDSim
{
	enum ChannelPacketType
	{
		READ,
		WRITE,
		ERASE,
		DATA
	};

	class SmallChannelPacket
	{
	public:
		//Fields
		ChannelPacketType busPacketType;
		uint size;
		uint word;
		uint page;
		uint block;
		uint plane;
		uint die;
		uint package;
		uint64_t virtualAddress;
		uint64_t physicalAddress;
		void *data;

		//Functions
		ChannelPacket(ChannelPacketType packtype, uint64_t virtualAddr, uint64_t physicalAddr, uint siz, uint word,
                              uint page, uint block, uint plane, uint die, uint package, void *dat);
		ChannelPacket();

		//void print();
		void print(uint64_t currentClockCycle);
		static void printData(const void *data);
	};
}

#endif

