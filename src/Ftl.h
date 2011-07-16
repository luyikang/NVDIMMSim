#ifndef NVFTL_H
#define NVFTL_H
//Ftl.h
//header file for the ftl

#include <iostream>
#include <fstream>
#include <string>
#include "SimObj.h"
#include "FlashConfiguration.h"
#include "ChannelPacket.h"
#include "FlashTransaction.h"
#include "Controller.h"
#include "Logger.h"
#include "Util.h"

namespace NVDSim{
        class NVDIMM;
	class Ftl : public SimObj{
		public:
	                Ftl(Controller *c, Logger *l, NVDIMM *p);

			ChannelPacket *translate(ChannelPacketType type, uint64_t vAddr, uint64_t pAddr);
			virtual bool addTransaction(FlashTransaction &t);
			void addFfTransaction(FlashTransaction &t);
			virtual void update(void);
			void attemptWrite(uint64_t start, uint64_t *vAddr, uint64_t *pAddr, bool *done);
			uint64_t get_ptr(void); 
			void inc_ptr(void); 

			void sendQueueLength(void);
			
			void powerCallback();

			virtual void saveNVState(void);
			virtual void loadNVState(void);
		       
			Controller *controller;

			NVDIMM *parent;

			Logger *log;

			uint64_t write_counter;

		protected:
			bool gc_flag;
			uint offset,  pageBitWidth, blockBitWidth, planeBitWidth, dieBitWidth, packageBitWidth;
			uint channel, die, plane, lookupCounter;
			uint64_t max_queue_length;
			FlashTransaction currentTransaction;
			uint busy;
			std::unordered_map<uint64_t,uint64_t> addressMap;
			std::vector<vector<bool>> used;
			std::list<FlashTransaction> transactionQueue;
	};
}
#endif
