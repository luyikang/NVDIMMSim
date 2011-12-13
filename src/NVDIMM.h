#ifndef NVDIMM_H
#define NVDIMM_H
//NVDIMM.h
//Header for nonvolatile memory dimm system wrapper

#include "SimObj.h"
#include "FlashConfiguration.h"
#include "Controller.h"
#include "Ftl.h"
#include "GCFtl.h"
#include "Die.h"
#include "FlashTransaction.h"
#include "Callbacks.h"
#include "Logger.h"
#include "GCLogger.h"
#include "P8PLogger.h"
#include "P8PGCLogger.h"

using std::string;

namespace NVDSim{
    typedef CallbackBase<void,uint,uint64_t,uint64_t,bool> Callback_t;
    typedef CallbackBase<void,uint,vector<vector<double>>,uint64_t,bool> Callback_v;
	class NVDIMM : public SimObj{
		public:
			NVDIMM(uint id, string dev, string sys, string pwd, string trc);
			void update(void);
			bool add(FlashTransaction &trans);
			bool addTransaction(bool isWrite, uint64_t addr);
			void printStats(void);
			void saveStats(void);
			string SetOutputFileName(string tracefilename);
			void RegisterCallbacks(Callback_t *readDone, Callback_t *writeDone, Callback_v *Power);
			void RegisterCallbacks(Callback_t *readDone, Callback_t *critLine, Callback_t *writeDone, Callback_v *Power); 

			void powerCallback(void);

			void saveNVState(string filename);
			void loadNVState(string filename);

			void queuesNotFull(void);

			void GCReadDone(uint64_t vAddr);

			Controller *controller;
			Ftl *ftl;
			Logger *log;

			vector<Package> *packages;

			Callback_t* ReturnReadData;
			Callback_t* CriticalLineDone;
			Callback_t* WriteDataDone;
			Callback_v* ReturnPowerData;

			uint systemID, numReads, numWrites, numErases;
			uint epoch_count, epoch_cycles;
			uint64_t channel_cycles_per_cycle, controller_cycles_left;
			uint64_t* cycles_left;
	
			bool faster_channel;

		private:
			string dev, sys, cDirectory;
	};

	NVDIMM *getNVDIMMInstance(uint id, string deviceFile, string sysFile, string pwd, string trc);
}
#endif
