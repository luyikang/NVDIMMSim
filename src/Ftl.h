#ifndef FTL_H
#define FTL_H
//Ftl.h
//header file for the ftl

#include "SimObj.h"
#include "FlashConfiguration.h"
#include "ChannelPacket.h"
#include "FlashTransaction.h"
#include "Controller.h"

namespace NVDSim{
	class Ftl : public SimObj{
		public:
                        Ftl(Controller *c);
			ChannelPacket *translate(ChannelPacketType type, uint64_t vAddr, uint64_t pAddr);
			bool addTransaction(FlashTransaction &t);
			void update(void);
			bool checkGC(void); 
			void runGC(void); 
			uint64_t get_ptr(void); 
			void inc_ptr(void); 

			//Accessors for power data
			//Writing correct object oriented code up in this piece, what now?
			vector<double> getIdleEnergy(void);
			vector<double> getAccessEnergy(void);
			vector<double> getEraseEnergy(void);

			Controller *controller;
		private:
			uint offset,  pageBitWidth, blockBitWidth, planeBitWidth, dieBitWidth, packageBitWidth;
			uint channel, die, plane, lookupCounter;
			uint64_t used_page_count;
			FlashTransaction currentTransaction;
			uint busy;
			std::unordered_map<uint64_t,uint64_t> addressMap;
			std::vector<vector<bool>> dirty;
			std::vector<vector<bool>> used;
			std::list<FlashTransaction> transactionQueue;
			std::unordered_map<uint64_t,uint64_t> erase_counter;

			// Power Stuff
			// This is computed per package
			std::vector<double> idle_energy;
			std::vector<double> access_energy;
			std::vector<double> erase_energy;
			
	};
}
#endif
