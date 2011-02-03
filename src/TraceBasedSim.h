#ifndef TBS_H
#define TBS_H

#include "NVDIMM.h"

class test_obj{
	public:
		void read_cb(uint, uint64_t, uint64_t);
		void write_cb(uint, uint64_t, uint64_t);
		void idle_cb(uint, vector<double>, uint64_t);
		void access_cb(uint, vector<double>, uint64_t);
		void erase_cb(uint, vector<double>, uint64_t);
		void run_test(void);
};
#endif
