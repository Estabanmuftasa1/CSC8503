#include "GameServer.h"
#include "GameWorld.h"
#include "./enet/enet.h"
using namespace NCL;
using namespace CSC8503;

GameServer::GameServer(int onPort, int maxClients)	{
	port		= onPort;
	clientMax	= maxClients;
	clientCount = 0;
	netHandle	= nullptr;
}

GameServer::~GameServer()	{
	Shutdown();
}

void GameServer::Shutdown() {
	SendGlobalPacket(BasicNetworkMessages::Shutdown);
	enet_host_destroy(netHandle);
	netHandle = nullptr;
}

bool GameServer::Initialise() {
	ENetAddress address;
	address.host = ENET_HOST_ANY;
	address.port = port;

	netHandle = enet_host_create(&address, clientMax, 1, 0, 0);
	RegisterPacketHandler(BasicNetworkMessages::Player_Transform, this);
	std::cout << "[SERVER] enet_host_create done. netHandle=" << netHandle << "\n";

	if (!netHandle) {
		std::cout << __FUNCTION__ << " failed to create network handle!" << std::endl;
		return false;
	}
	return true;
}


bool GameServer::SendGlobalPacket(int msgID) {
	GamePacket packet;
	packet.type = msgID;
	return SendGlobalPacket(packet);
}

bool GameServer::SendGlobalPacket(GamePacket& packet) {
	ENetPacket* dataPacket = enet_packet_create(&packet,
		packet.GetTotalSize(), 0);
	enet_host_broadcast(netHandle, 0, dataPacket);
	return true;
}


void GameServer::UpdateServer() {
	if (!netHandle) { return; }

	ENetEvent event;
	while (enet_host_service(netHandle, &event, 0) > 0) {
		int peerID = event.peer->incomingPeerID;

		if (event.type == ENET_EVENT_TYPE_CONNECT) {
			std::cout << "Server: New client connected (peer=" << peerID << ")\n";

			LevelSeedPacket seedPkt(levelSeed);
			SendPacketToPeer(peerID, seedPkt);

			std::cout << "[SERVER] sent seed=" << levelSeed << " to peer=" << peerID << "\n";
		}
		else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
			std::cout << "Server: Client disconnected (peer=" << peerID << ")\n";
		}
		else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
			GamePacket* packet = (GamePacket*)event.packet->data;
			ProcessPacket(packet, peerID);

			if (packet->type == BasicNetworkMessages::Player_Transform) {
				SendGlobalPacket(*packet);
			}

			enet_packet_destroy(event.packet);
		}
	}
}




void GameServer::ReceivePacket(int type, GamePacket* payload, int source) {
	if (type == BasicNetworkMessages::Player_Transform) {
		SendGlobalPacket(*payload); 
	}
}


void GameServer::SetGameWorld(GameWorld &g) {
	gameWorld = &g;
}

bool GameServer::SendPacketToPeer(int peerID, GamePacket& packet) {
	if (!NetworkBase::netHandle) return false;

	ENetPacket* dataPacket = enet_packet_create(&packet, packet.GetTotalSize(), 0);
	ENetPeer* peer = &netHandle->peers[peerID];
	enet_peer_send(peer, 0, dataPacket);
	return true;
}
