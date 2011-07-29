#ifndef NVGCFTL_H
#define NVGCFTL_H
//GCFtl.h
//header file for the ftl with garbage collection

#include <iostream>
#include <fstream>
#include "SimObj.h"
#include "FlashConfiguration.h"
#include "ChannelPacket.h"
#include "FlashTransaction.h"
#include "Controller.h"
#include "Ftl.h"
#include "Logger.h"
#include "GCLogger.h"

namespace NVDSim{
        class NVDIMM;
	class GCFtl : public Ftl{
		public:
	                GCFtl(Controller *c, Logger *l, NVDIMM *p);
			bool addTransaction(FlashTransaction &t);
			void addGcTransaction(FlashTransaction &t);
			void update(void);
			void write_used_handler(uint64_t vAddr);
			bool checkGC(void); 
			void runGC(void);

			void saveNVState(void);
			void loadNVState(void);

			void GCReadDone(uint64_t vAddr);
			void GCWriteDone(uint64_t vAddr);

		protected:
			uint gc_status, panic_mode;
			uint64_t start_erase;

			uint erase_pointer;			
			
			class PendingErase
			{
			public:
			    std::list<uint64_t> pending_writes;
			    uint64_t erase_block;
			    
			    PendingErase()
			    {
				erase_block = 0;
			    }
			};
			std::list<PendingErase> gc_pending_erase;  

			std::vector<vector<bool>> dirty;
	};
}
#endif
