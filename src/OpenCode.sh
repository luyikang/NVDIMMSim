#!/bin/sh

emacs TraceBasedSim.cpp SimObj.cpp Plane.cpp NVDIMM.cpp Init.cpp PCMFullGCLogger.cpp PCMGCLogger.cpp PCMLogger.cpp FullGCLogger.cpp GCLogger.cpp Logger.cpp GCFtl.cpp Ftl.cpp FlashTransaction.cpp  Die.cpp Controller.cpp ChannelPacket.cpp Channel.cpp Block.cpp --eval '(delete-other-windows)'&

emacs TraceBasedSim.h SimObj.h Plane.h NVDIMM.h Init.h PCMFullGCLogger.h PCMGCLogger.h PCMLogger.h FullGCLogger.h GCLogger.h Logger.h GCFtl.h Ftl.h FlashTransaction.h FlashConfiguration.h Die.h Controller.h ChannelPacket.h Channel.h Callbacks.h Block.h --eval '(delete-other-windows)'&

echo opening files
