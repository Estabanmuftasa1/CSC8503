#include "GameClient.h"
#include "./enet/enet.h"
#include <iostream>
#include <NetworkBase.h>

using namespace NCL;
using namespace CSC8503;


GameClient::GameClient() {
	netHandle = nullptr;
	serverPeer = nullptr;
	connected = false;
	hasRemote = false;
	hasSeed = false;
	levelSeed = 0;
	localID = -1;
}

GameClient::~GameClient() {
	Disconnect();
}

bool GameClient::Connect(const std::string& ip, int port) {

	Disconnect();

	ENetAddress address;
	enet_address_set_host(&address, ip.c_str());
	address.port = port;

	netHandle = enet_host_create(nullptr, 1, 1, 0, 0);
	if (!netHandle) {
		std::cout << "Client: failed to create host\n";
		return false;
	}

	std::cout << "Client: connecting to " << ip << ":" << port << "...\n";
	serverPeer = enet_host_connect(netHandle, &address, 1, 0);
	if (!serverPeer) {
		std::cout << "Client: failed to start connection\n";
		enet_host_destroy(netHandle);
		netHandle = nullptr;
		return false;
	}

	connected = false;
	hasRemote = false;
	hasSeed = false;

	RegisterPacketHandler(BasicNetworkMessages::Level_Seed, this);
	RegisterPacketHandler(BasicNetworkMessages::Player_Transform, this);
	RegisterPacketHandler(BasicNetworkMessages::Enemy_Transform, this);


	return true;
}




void GameClient::UpdateClient() {
	if (!netHandle) return;

	ENetEvent event;
	while (enet_host_service(netHandle, &event, 0) > 0) {

		if (event.type == ENET_EVENT_TYPE_CONNECT) {
			connected = true;
			std::cout << "Client: connected!\n";
		}
		else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
			GamePacket* packet = (GamePacket*)event.packet->data;
			ProcessPacket(packet, event.peer->incomingPeerID);
			enet_packet_destroy(event.packet);
		}
		else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
			std::cout << "Client: disconnected\n";
			connected = false;
		}
	}
}




void GameClient::Disconnect() {
	if (serverPeer) {
		enet_peer_disconnect(serverPeer, 0);
		serverPeer = nullptr;
	}
	if (netHandle) {
		enet_host_destroy(netHandle);
		netHandle = nullptr;
	}
	connected = false;
}

bool GameClient::SendPacket(GamePacket& packet) {
	if (!connected || !serverPeer) return false;

	ENetPacket* dataPacket = enet_packet_create(&packet, packet.GetTotalSize(), 0);
	enet_peer_send(serverPeer, 0, dataPacket);
	enet_host_flush(netHandle);
	return true;
}


void GameClient::ReceivePacket(int type, GamePacket* payload, int source) {
	if (type == BasicNetworkMessages::Level_Seed) {
		LevelSeedPacket* s = (LevelSeedPacket*)payload;
		levelSeed = s->seed;
		hasSeed = true;

		std::cout << "[CLIENT " << localID << "] got seed=" << levelSeed << "\n";
		return;
	}

	if (type == BasicNetworkMessages::Player_Transform) {
		PlayerTransformPacket* p = (PlayerTransformPacket*)payload;

		if (p->playerID == localID) {
			return;
		}

		remote = *p;
		hasRemote = true;

		std::cout << "[CLIENT " << localID << "] got remote id=" << p->playerID
			<< " pos=(" << p->position.x << "," << p->position.y << "," << p->position.z
			<< ") yaw=" << p->yaw << "\n";
	}
	if (type == BasicNetworkMessages::Enemy_Transform) {
		EnemyTransformPacket* p = (EnemyTransformPacket*)payload;

		if (p->enemyID < 0 || p->enemyID >= 16) return;

		remoteEnemies[p->enemyID].valid = true;
		remoteEnemies[p->enemyID].position = p->position;
		remoteEnemies[p->enemyID].yaw = p->yaw;
	}


}

