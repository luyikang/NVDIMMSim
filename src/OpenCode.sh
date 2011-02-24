#!/bin/sh

emacs TraceBasedSim.cpp SimObj.cpp Plane.cpp NVDIMM.cpp Init.cpp PCMGCFtl.cpp PCMFtl.cpp GCFtl.cpp Ftl.cpp FlashTransaction.cpp  Die.cpp Controller.cpp ChannelPacket.cpp Channel.cpp Block.cpp --eval '(delete-other-windows)'&

emacs TraceBasedSim.h SimObj.h Plane.h NVDIMM.h Init.h PCMGCFtl.h PCMFtl.h GCFtl.h Ftl.h FlashTransaction.h FlashConfiguration.h Die.h Controller.h ChannelPacket.h Channel.h Callbacks.h Block.h --eval '(delete-other-windows)'&

echo opening files
