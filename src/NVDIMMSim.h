#ifndef NVDIMMSIM_H
#define NVDIMMSIM_H
/*
 * This is a public header for NVDIMM including this along with libnvdimmsim.so should
 * provide all necessary functionality to talk to an external simulator
 */

#include "Callbacks.h"

using std::string;

namespace NVDSim
{
    class NVDIMM
    {
    public:
	void update(void);
	bool add(FlashTransaction &trans);
	bool addTransaction(bool isWrite, uint64_t addr);
	void printStats(void);
	void saveStats(void);
	void RegisterCallbacks(Callback_t *readDone, Callback_t *writeDone, Callback_v *Power);

	void saveNVState(string filename);
	void loadNVState(string filename);
    };
    NVDIMM *getNVDIMMInstance(uint id, string deviceFile, string sysFile, string pwd, string trc);
}

#endif
