#ifndef TBS_H
#define TBS_H
#include "NVDIMM.h"
class test_obj{
	public:
		void read_cb(uint, uint64_t, uint64_t);
		void write_cb(uint, uint64_t, uint64_t);
		void run_test(void);
};
#endif
