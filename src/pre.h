namespace Flash{
    enum class PageState{
        FREE,  // free to program
        VALID, // stores valid data
        STALE, // will be garbage collected, invalid now
        SIZE
    }
    // page should contain an attribute that points to its mapped new page (GC)
    // page should use the idea of unordered_map
    //[channel][chip][die][plane][block][page] <- address
    //read: free error() valid return data stale return not here 
    //write: free goon valid/stale{set this page to stale write to a random page map the two}

    enum class BlockState{
        FREE, //clean
        OCCUPIED, //at least one page is valid, nobody is stale
        STALE, //if containing stale pages
        SIZE
    }
    //if erase a block with gc, copy all the valid data to a new block. set the target block free.
    //Block should contain wear leveling indicator.

    //ftl expose an array of logical block address to the host. part of the controller.
    //logical block address -> physical block address. block-level mapping
    //
};