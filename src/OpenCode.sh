#!/bin/sh

emacs Util.cpp TraceBasedSim.cpp SimObj.cpp Plane.cpp NVDIMM.cpp Init.cpp PCMGCLogger.cpp PCMLogger.cpp GCLogger.cpp Logger.cpp GCFtl.cpp Ftl.cpp FlashTransaction.cpp  Die.cpp Controller.cpp ChannelPacket.cpp Channel.cpp Buffer.cpp Block.cpp --eval '(delete-other-windows)'&

emacs Util.h TraceBasedSim.h SimObj.h Plane.h NVDIMM.h Init.h PCMGCLogger.h PCMLogger.h GCLogger.h Logger.h GCFtl.h Ftl.h FlashTransaction.h FlashConfiguration.h Die.h Controller.h ChannelPacket.h Channel.h Callbacks.h Buffer.h Block.h --eval '(delete-other-windows)'&

echo opening files
