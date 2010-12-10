//Block.cpp
//Block functions

#include "Block.h"

using namespace std;
using namespace NVDSim;

Block::Block(uint block){
	block_num = block;
}

Block::Block(){

}

void *Block::read(uint size, uint word_num, uint page_num){
	// this variable will hold the data we agrigate from the pages and words
	void* data;

	// we're reading multiple pages worth of data
	temp_size = size;
	while(temp_size > PAGE_SIZE){
	  if (pages.find(page_num) == pages.end()){
		DEBUG("Request to read page "<<page_num<<" failed: nothing has been written to that address");
		return (void *)0x0;
	  }
	  // data doesn't actually matter, its just a placeholder
	  // so we're just going to overwrite the pointer and pretend that we're making a packet out of pages
	  data = data pages[page_num].read(PAGE_SIZE, word_num);

	  // we've got one pages worth of the stuff we were supposed to get
	  temp_size =  temp_size - PAGE_SIZE;

	  // we're done with this page, move to the next page
	  page_num = page_num + 1;
	  
	}
  
	if (pages.find(page_num) == pages.end()){
		DEBUG("Request to read page "<<page_num<<" failed: nothing has been written to that address");
		return (void *)0x0;
	}
	
	// data doesn't actually matter, its just a placeholder
	// so we're just going to overwrite the pointer and pretend that we're making a packet out of pages
	data = data pages[page_num].read(temp_size, word_num);


	// pages store words
	return data;
}

void Block::write(uint page_num, void *data){
	if (pages.find(page_num) == pages.end()){
		pages[page_num]= data;
	} else{
		/*ERROR("Request to write page "<<page_num<<" failed: page has been written to and not erased"); 
		exit(1);*/
		
		//Until garbage collection is implemented, you can write to pages that have already been written to
		pages[page_num]= data;
	}
}

void Block::erase(){
	pages.clear();
}

