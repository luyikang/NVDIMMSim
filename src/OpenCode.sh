#!/bin/sh

emacs TraceBasedSim.cpp SimObj.cpp Plane.cpp Page.cpp NVDIMM.cpp Init.cpp Ftl.cpp FlashTransaction.cpp  FlashDIMM.cpp Die.cpp ChannelPacket.cpp Channel.cpp Block.cpp --eval '(delete-other-windows)'&

emacs TraceBasedSim.h SimObj.h Plane.h Page.h NVDIMM.h Init.h Ftl.h FlashTransaction.h FlashConfiguration.h FlashDIMM.h Die.h ChannelPacket.h Channel.h Callbacks.h  Block.h --eval '(delete-other-windows)'&

echo opening files