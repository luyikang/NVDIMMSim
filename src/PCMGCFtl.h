#ifndef PCMGCFTL_H
#define PCMGCFTL_H
//PCMFtl.h
//header file for the ftl with PCM power stuff

#include <iostream>
#include <fstream>
#include "SimObj.h"
#include "FlashConfiguration.h"
#include "ChannelPacket.h"
#include "FlashTransaction.h"
#include "Controller.h"
#include "GCFtl.h"

namespace NVDSim{
	class PCMGCFtl : public GCFtl{
		public:
			PCMGCFtl(Controller *c);
			void update(void);		
	};
}
#endif
