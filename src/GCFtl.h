#ifndef GCFTL_H
#define GCFTL_H
//GCFtl.h
//header file for the ftl with garbage collection

#include <iostream>
#include <fstream>
#include "SimObj.h"
#include "FlashConfiguration.h"
#include "ChannelPacket.h"
#include "FlashTransaction.h"
#include "Controller.h"
#include "Ftl.h"

namespace NVDSim{
	class GCFtl : public Ftl{
		public:
			GCFtl(Controller *c);
			bool addTransaction(FlashTransaction &t);
			void update(void);
			bool checkGC(void); 
			void runGC(void);

		protected:
			std::vector<vector<bool>> dirty;
	};
}
#endif
