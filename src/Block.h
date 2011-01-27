#ifndef BLOCK_H
#define BLOCK_H
//Block.h
//header file for the Block class

#include "FlashConfiguration.h"

#if SMALL_ACCESS
#include "Page.h"
#endif

namespace NVDSim{
	class Block{
		public:
			Block(uint block);
			Block();
#if SMALL_ACCESS
			void *read(uint size, uint word_num, uint page_num);
			void write(uint size, uint word_num, uint page_num, void *data);
#else
			void *read(uint page_num);
			void write(uint page_num, void *data);
#endif
			void erase(void);
		private:
			uint block_num;

#if SMALL_ACCESS
			std::unordered_map<uint, Page> pages;
#else
	                std::unordered_map<uint, void *> page_data;
#endif
	};
}
#endif
