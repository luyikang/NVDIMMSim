#ifndef PCMFTL_H
#define PCMFTL_H
//PCMFtl.h
//header file for the ftl with PCM power stuff

#include <iostream>
#include <fstream>
#include "SimObj.h"
#include "FlashConfiguration.h"
#include "ChannelPacket.h"
#include "FlashTransaction.h"
#include "Controller.h"
#include "Ftl.h"

namespace NVDSim{
	class PCMFtl : public Ftl{
		public:
			PCMFtl(Controller *c);
			void update(void);
	};
}
#endif
