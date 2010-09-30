#ifndef SIMOBJ_H
#define SIMOBJ_H

//SimObj.h
//
//Header file for simulator object class
//
//Code copied from DRAMSim

#include <stdint.h>

namespace NVDSim
{
	class SimObj
	{
	public:
		uint64_t currentClockCycle;
		
		void step();
		virtual void update()=0;
	};
}

#endif

