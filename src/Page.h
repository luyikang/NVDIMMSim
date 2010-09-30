#ifndef PAGE_H
#define PAGE_H

// Page.h
// header file for page class

#include "FlashConfiguration.h"

namespace NVDSim{
  class Page{
    
  public:
    Page();
    Page(unit page_num);

    void* read(unit word_num);
    void* read();

    void write(unit word_num);
    void write();

  private:
    unit page_num;
    void* page_data;
    std::unordered_map<unit, void*> word_data;
    
  };
}

#endif

