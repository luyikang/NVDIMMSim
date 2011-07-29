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
#include "PCMLogger.h"
#include "PCMGCLogger.h"

using std::string;

namespace NVDSim{
	typedef CallbackBase<void,uint,uint64_t,uint64_t> Callback_t;
	typedef CallbackBase<void,uint,vector<vector<double>>,uint64_t> Callback_v;
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

			void powerCallback(void);

			void saveNVState(string filename);
			void loadNVState(string filename);

			void queuesNotFull(void);

			void GCReadDone(uint64_t vAddr);
			void GCWriteDone(uint64_t vAddr);

			Controller *controller;
			Ftl *ftl;
			Logger *log;

			vector<Package> *packages;

			Callback_t* ReturnReadData;
			Callback_t* WriteDataDone;
			Callback_v* ReturnPowerData;

			uint systemID, numReads, numWrites, numErases;
			uint epoch_count, epoch_cycles;

		private:
			string dev, sys, cDirectory;
	};
}
#endif
