#ifndef BLOCK_H
#define BLOCK_H
//Block.h
//header file for the Block class

#include "FlashConfiguration.h"
#include "Page.h"

namespace NVDSim{
	class Block{
		public:
			Block(uint block);
			Block();
			void *read(uint size, unit word_num, uint page_num);
			void write(uint size, unit word_num, uint page_num, void *data);
			void erase(void);
		private:
			uint block_num;
			std::unordered_map<uint, Page> pages;
	};
}
#endif
