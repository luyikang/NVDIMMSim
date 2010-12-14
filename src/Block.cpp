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

        uint page = page_num;
	uint word = word_num;

	// this variable will hold the data we agrigate from the pages
	void *data;

	uint temp_size = size;
	uint slice_size = 0;

	// taking care of the case where we start reading from the middle of a page	
	if(word != 0){
	  // figure out how many words are left in the page
	  uint temp = (WORDS_PER_PAGE - word)*WORD_SIZE;
	  // set the size to the lesser of the size of temp size or the size of the words left
	  slice_size = ((temp > temp_size)?temp_size:temp);
	}else{
	  slice_size = PAGE_SIZE;
	}	

        // we're reading multiple pages worth of data
	while(temp_size > PAGE_SIZE || temp_size > slice_size){
	  if (pages.find(page) == pages.end()){
		DEBUG("Request to read page "<<page<<" failed: nothing has been written to that address");
		return (void *)0x0;
	  }
	  // data doesn't actually matter, its just a placeholder
	  // so we're just going to overwrite the pointer and pretend that we're making a packet out of pages
	  data = pages[page].read(slice_size, word);

	  // we've got one pages worth of the stuff we were supposed to get
	  temp_size =  temp_size - slice_size;

	  // we're done with this page, move to the next page
	  page = page + 1;	  

	  // if we started from the specified word in this page, next page we should start from the beginning
	  word = 0;

	  // reset the slice size to PAGE_SIZE cause now if we get back here we should be reading whole pages
	  slice_size = PAGE_SIZE;
	}
  
	if (pages.find(page) == pages.end()){
		DEBUG("Request to read page "<<page<<" failed: nothing has been written to that address");
		return (void *)0x0;
	}
	
	// data doesn't actually matter, its just a placeholder
	// so we're just going to overwrite the pointer and pretend that we're making a packet out of pages
	data = pages[page].read(temp_size, word);


	// pages store words
	return data;
}

void Block::write(uint size, uint word_num, uint page_num, void *data){
        
        uint page = page_num;
	uint word = word_num;

        uint temp_size = size;
	uint slice_size = 0;

	// taking care of the case where we start writing in the middle of a page	
	if(word != 0){
	  // figure out how many words are left in the page
	  uint temp = (WORDS_PER_PAGE - word)*WORD_SIZE;
	  // set the size to the lesser of the size of temp size or the size of the words left
	  slice_size = ((temp > temp_size)?temp_size:temp);
	}else{
	  slice_size = PAGE_SIZE;
	}	

	// writing data across multiple pages
	while(temp_size > PAGE_SIZE || temp_size > slice_size){
       
	  // if this page has not yet been accessed yet, create a new page object for it and add it to the pages map
	  if (pages.find(page) == pages.end()){
	    pages[page] = Page(page);
	    pages[page].write(slice_size, word, data);
	  } else{
	    /*ERROR("Request to write page "<<page_num<<" failed: page has been written to and not erased"); 
	      exit(1);*/
		
	    //Until garbage collection is implemented, you can write to pages that have already been written to
	    pages[page].write(slice_size, word, data);
	  }
	  
	  // we've written one pages worth of the stuff we were supposed to write
	  temp_size = temp_size - slice_size;

	  // we're done writing to this page, move on to the next page
	  page = page + 1;

	  // if we started from the specified word in this page, next page we should start from the beginning
	  word = 0;

	  // reset the slice size to PAGE_SIZE cause now if we get back here we should be writing whole pages
	  slice_size = PAGE_SIZE;
	}

	// if this page has not yet been accessed yet, create a new page object for it and add it to the pages map
	  if (pages.find(page) == pages.end()){
	    pages[page] = Page(page);
	    pages[page].write(slice_size, word, data);
	  } else{
	    /*ERROR("Request to write page "<<page_num<<" failed: page has been written to and not erased"); 
	      exit(1);*/
		
	    //Until garbage collection is implemented, you can write to pages that have already been written to
	    pages[page].write(temp_size, word, data);
	  }
}

void Block::erase(){
	pages.clear();
}

