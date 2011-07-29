/*TraceBasedSim.cpp
 *
 * This will eventually run traces. Right now the name is a little misleading...
 * It adds a certain amount (NUM_WRITES) of write transactions to the flash dimm
 * linearly starting at address 0 and then simulates a certain number (SIM_CYCLES)
 * of cycles before exiting.
 *
 * The output should be fairly straightforward. If you would like to see the writes
 * as they take place, change OUTPUT= 0; to OUTPUT= 1;
 */
#include <iostream>
#include "FlashConfiguration.h"
#include "FlashTransaction.h"
#include <time.h>
#include "TraceBasedSim.h"

#define NUM_WRITES 50
#define SIM_CYCLES 500000

/*temporary assignments for externed variables.
 * This should really be done with another class
 * that reads .ini files
 *
 * values from a samsung flash dimm:
 * */

/*uint NUM_PACKAGES= 1;
uint DIES_PER_PACKAGE= 2;
uint PLANES_PER_DIE= 4;
uint BLOCKS_PER_PLANE= 2048;
uint PAGES_PER_BLOCK= 64;
uint FLASH_PAGE_SIZE= 4;

uint READ_TIME= 25;
uint WRITE_TIME= 200;
uint ERASE_TIME= 1500;
uint DATA_TIME= 100;
uint COMMAND_TIME= 10;
*/
uint OUTPUT= 1;

using namespace NVDSim;
using namespace std;

int main(void){
	test_obj t;
	t.run_test();
	return 0;
}

void test_obj::read_cb(uint id, uint64_t address, uint64_t cycle){
	cout<<"[Callback] read complete: "<<id<<" "<<address<<" cycle="<<cycle<<endl;
}

void test_obj::write_cb(uint id, uint64_t address, uint64_t cycle){
	cout<<"[Callback] write complete: "<<id<<" "<<address<<" cycle="<<cycle<<endl;
}

void test_obj::power_cb(uint id, vector<vector<double>> data, uint64_t cycle){
        cout<<"[Callback] Power Data for cycle: "<<cycle<<endl;
	for(uint i = 0; i < NUM_PACKAGES; i++){
	  for(uint j = 0; j < data.size(); j++){
	    if(DEVICE_TYPE.compare("PCM") == 0){
	      if(j == 0){
		cout<<"    Package: "<<i<<" Idle Energy: "<<data[0][i]<<"\n";
	      }else if(j == 1){
		cout<<"    Package: "<<i<<" Access Energy: "<<data[1][i]<<"\n";
	      }
	      if(GARBAGE_COLLECT == 1){
		if(j == 2){
		  cout<<"    Package: "<<i<<" Erase Energy: "<<data[2][i]<<"\n";
		}else if(j == 3){
		  cout<<"    Package: "<<i<<" VPP Idle Energy: "<<data[3][i]<<"\n";
		}else if(j == 4){
		  cout<<"    Package: "<<i<<" VPP Access Energy: "<<data[4][i]<<"\n";
		}else if(j == 5){
		  cout<<"    Package: "<<i<<" VPP Erase Energy: "<<data[5][i]<<"\n";
		}
	      }else{
		if(j == 2){
		  cout<<"    Package: "<<i<<" VPP Idle Energy: "<<data[2][i]<<"\n";
		}else if(j == 3){
		  cout<<"    Package: "<<i<<" VPP Access Energy: "<<data[3][i]<<"\n";
		}
	      }
	    }else{
	      if(j == 0){
		cout<<"    Package: "<<i<<" Idle Energy: "<<data[0][i]<<"\n";
	      }else if(j == 1){
		cout<<"    Package: "<<i<<" Access Energy: "<<data[1][i]<<"\n";
	      }else if(j == 2){
		cout<<"    Package: "<<i<<" Erase Energy: "<<data[2][i]<<"\n";
	      }
	    }
	  }
	}
}

void test_obj::run_test(void){
	clock_t start= clock(), end;
	uint write, cycle;
	NVDIMM *NVDimm= new NVDIMM(1,"ini/samsung_K9XXG08UXM(mod).ini","ini/def_system.ini","","");
	//NVDIMM *NVDimm= new NVDIMM(1,"ini/PCM_TEST.ini","ini/def_system.ini","","");
	typedef CallbackBase<void,uint,uint64_t,uint64_t> Callback_t;
	Callback_t *r = new Callback<test_obj, void, uint, uint64_t, uint64_t>(this, &test_obj::read_cb);
	Callback_t *w = new Callback<test_obj, void, uint, uint64_t, uint64_t>(this, &test_obj::write_cb);
	Callback_v *p = new Callback<test_obj, void, uint, vector<vector<double>>, uint64_t>(this, &test_obj::power_cb);
	NVDimm->RegisterCallbacks(r, w, p);
	
	FlashTransaction t;

	/*for (write= 0; write<NUM_WRITES; write++){
		t= FlashTransaction(DATA_WRITE, write, (void *)0xdeadbeef);
		(*NVDimm).add(t);
		t= FlashTransaction(DATA_READ, write, (void *)0xdeadbeef);
		(*NVDimm).add(t);
		}*/

	int writes = 0;
	bool result = 0;
	int write_addr = 0;
	
	for (cycle= 0; cycle<SIM_CYCLES; cycle++){
	  if(writes < NUM_WRITES){
	      t = FlashTransaction(DATA_WRITE, write_addr, (void *)0xdeadbeef);
	      result = (*NVDimm).add(t);
	      if(result == 1)
	      {
		  writes++;
		  write_addr++;
		  if(write_addr > 6)
		  {
		      write_addr = 0;
		  }
	      }
	  }

		(*NVDimm).update();
		/*if (cycle < NUM_WRITES){
			t= FlashTransaction(DATA_READ, cycle, (void *)0xfeedface);

			(*NVDimm).add(t);
			//(*NVDimm).addTransaction(false, cycle*64);
		}
		if (NVDimm->numReads == NUM_WRITES)
		break;*/
		if (NVDimm->numReads == NUM_WRITES)
			break;
	}

	end= clock();
	cout<<"Simulation Results:\n";
	cout<<"Cycles simulated: "<<cycle<<endl;
	NVDimm->printStats();
	NVDimm->saveStats();
	cout<<"Execution time: "<<(end-start)<<" cycles. "<<(double)(end-start)/CLOCKS_PER_SEC<<" seconds.\n";

	//cout<<"Callback test: \n";
	//NVDimm->powerCallback();
}
