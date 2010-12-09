//Page.cpp
//.cpp file for the page class

#include "Page.h"

using namespace std;
using namespace NVDSim;

Page::Page(){

}

Page::Page(uint page){
	page_num = page;
}

void *Page::read(uint word_num){
	if (word_data.find(word_num) == word_data.end()){
		DEBUG("Request to read page "<<word_num<<" failed: nothing has been written to that address");
		return (void *)0x0;
	} else{
		return word_data[word_num];
	}
}

void *Page::read(){
  return (void *)0x0;
}

void Page::write(uint word_num, void *data){
	if (word_data.find(word_num) == word_data.end()){
		word_data[word_num]= data;
	} else{
		/*ERROR("Request to write page "<<word_num<<" failed: word has been written to and not erased"); 
		exit(1);*/
		
		//Until wear leveling is implemented, you can write to words that have already been written to
		word_data[word_num]= data;
	}
}

void Page::write(){
}
