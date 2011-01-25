#ifndef NVDIMM_H
#define NVDIMM_H
//NVDIMM.h
//Header for nonvolatile memory dimm system wrapper

#include "SimObj.h"
#include "FlashConfiguration.h"
#include "Controller.h"

#if SMALL_ACCESS
#include "SmallAccessFtl.h"
#else
#include "Ftl.h"
#endif

#include "Die.h"
#include "FlashTransaction.h"
#include "Callbacks.h"

using std::string;

namespace NVDSim{
	typedef CallbackBase<void,uint,uint64_t,uint64_t> Callback_t;
	class NVDIMM : public SimObj{
		public:
			NVDIMM(uint id, string dev, string sys, string pwd, string trc);
			void update(void);
			bool add(FlashTransaction &trans);
			bool addTransaction(bool isWrite, uint64_t addr);
			void printStats(void);
			string SetOutputFileName(string tracefilename);
			void RegisterCallbacks(Callback_t *readDone, Callback_t *writeDone);

			Controller *controller;

#if SMALL_ACCESS
			SmallAccessFtl *ftl;
#else
			Ftl *ftl;
#endif
			vector<Package> *packages;

			//std::ofsream visDataOut;
			//
			Callback_t* ReturnReadData;
			Callback_t* WriteDataDone;

			uint systemID, numReads, numWrites, numErases;
		private:
			string dev, sys, cDirectory;
	};
}
#endif
