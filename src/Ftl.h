#ifndef NVFTL_H
#define NVFTL_H
//Ftl.h
//header file for the ftl

#include <iostream>
#include <fstream>
#include "SimObj.h"
#include "FlashConfiguration.h"
#include "ChannelPacket.h"
#include "FlashTransaction.h"
#include "Controller.h"
#include "Logger.h"

namespace NVDSim{
        class NVDIMM;
	class Ftl : public SimObj{
		public:
	                Ftl(Controller *c, Logger *l, NVDIMM *p);

			ChannelPacket *translate(ChannelPacketType type, uint64_t vAddr, uint64_t pAddr);
			bool addTransaction(FlashTransaction &t);
			virtual void update(void);
			uint64_t get_ptr(void); 
			void inc_ptr(void); 

			void sendQueueLength(void);
			
			void powerCallback();
		       
			Controller *controller;

			NVDIMM *parent;

			Logger *log;

		protected:
			bool gc_flag;
			uint offset,  pageBitWidth, blockBitWidth, planeBitWidth, dieBitWidth, packageBitWidth;
			uint channel, die, plane, lookupCounter, gc_status, panic_mode, gc_counter;
			uint64_t max_queue_length;
			uint64_t used_page_count, start_erase;
			FlashTransaction currentTransaction;
			uint busy;
			std::unordered_map<uint64_t,uint64_t> addressMap;
			std::vector<vector<bool>> used;
			std::list<FlashTransaction> transactionQueue;

			void addGcTransaction(FlashTransaction &t);
	};
}
#endif
