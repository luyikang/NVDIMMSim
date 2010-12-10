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

        page = page_num;

	// this variable will hold the data we agrigate from the pages
	void *data;

	temp_size = size;

        // we're reading multiple pages worth of data
	while(temp_size > PAGE_SIZE){
	  if (pages.find(page) == pages.end()){
		DEBUG("Request to read page "<<page<<" failed: nothing has been written to that address");
		return (void *)0x0;
	  }
	  // data doesn't actually matter, its just a placeholder
	  // so we're just going to overwrite the pointer and pretend that we're making a packet out of pages
	  data = pages[page].read(PAGE_SIZE, word_num);

	  // we've got one pages worth of the stuff we were supposed to get
	  temp_size =  temp_size - PAGE_SIZE;

	  // we're done with this page, move to the next page
	  page = page + 1;	  
	}
  
	if (pages.find(page) == pages.end()){
		DEBUG("Request to read page "<<page<<" failed: nothing has been written to that address");
		return (void *)0x0;
	}
	
	// data doesn't actually matter, its just a placeholder
	// so we're just going to overwrite the pointer and pretend that we're making a packet out of pages
	data = pages[page].read(temp_size, word_num);


	// pages store words
	return data;
}

void Block::write(uint size, uint word_num, uint page_num, void *data){
        
        page = page_num;

        temp_size = size;

	// writing data across multiple pages
	while(temp_size > PAGE_SIZE){
       
	  // if this page has not yet been accessed yet, create a new page object for it and add it to the pages map
	  if (pages.find(page) == pages.end()){
	    pages[page]= data;
	  } else{
	    /*ERROR("Request to write page "<<page_num<<" failed: page has been written to and not erased"); 
	      exit(1);*/
		
	    //Until garbage collection is implemented, you can write to pages that have already been written to
	    pages[page].write(PAGE_SIZE, word_num, data);
	  }
	  
	  // we've written one pages worth of the stuff we were supposed to write
	  temp_size = temp_size - PAGE_SIZE;

	  // we're done writing to this page, move on to the next page
	  page = page + 1;
	}

	// if this page has not yet been accessed yet, create a new page object for it and add it to the pages map
	  if (pages.find(page) == pages.end()){
	    pages[page]= data;
	  } else{
	    /*ERROR("Request to write page "<<page_num<<" failed: page has been written to and not erased"); 
	      exit(1);*/
		
	    //Until garbage collection is implemented, you can write to pages that have already been written to
	    pages[page].write(temp_size, word_num, data);
	  }
}

void Block::erase(){
	pages.clear();
}

