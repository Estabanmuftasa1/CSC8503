#include "NetworkBase.h"
#include "./enet/enet.h"
NetworkBase::NetworkBase()	{
	netHandle = nullptr;
}

NetworkBase::~NetworkBase()	{
	if (netHandle) {
		enet_host_destroy(netHandle);
	}
}

void NetworkBase::Initialise() {
	enet_initialize();
}

void NetworkBase::Destroy() {
	enet_deinitialize();
}

bool NetworkBase::ProcessPacket(GamePacket* packet, int peerID) {
    PacketHandlerIterator first;
    PacketHandlerIterator last;

    if (!GetPacketHandlers(packet->type, first, last)) {
        return false;
    }

    for (auto i = first; i != last; ++i) {
        i->second->ReceivePacket(packet->type, packet, peerID);
    }
    return true;
}


