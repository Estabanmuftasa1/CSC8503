#pragma once
//#include "./enet/enet.h"
struct _ENetHost;
struct _ENetPeer;
struct _ENetEvent;
#include <vector.h>


enum BasicNetworkMessages {
	None,
	Hello,
	Message,
	String_Message,
	Delta_State,	//1 byte per channel since the last state
	Full_State,		//Full transform etc
	Received_State, //received from a client, informs that its received packet n
	Player_Connected,
	Player_Disconnected,
	Shutdown,
	Player_Transform = 100,
	Carry_Toggle,
	Carry_State,
	Game_State,
	High_Scores,
	Level_Seed = 200,
	Enemy_Transform =110

};

struct GamePacket {
	short size;
	short type;

	GamePacket() {
		type = BasicNetworkMessages::None;
		size = 0;
	}

	GamePacket(short type) : GamePacket() {
		this->type = type;
	}

	int GetTotalSize() {
		return sizeof(GamePacket) + size;
	}
};


struct LevelSeedPacket : public GamePacket {
	uint32_t seed;

	LevelSeedPacket() {
		type = BasicNetworkMessages::Level_Seed;
		size = sizeof(LevelSeedPacket) - sizeof(GamePacket);
		seed = 0;
	}
	LevelSeedPacket(uint32_t s) : LevelSeedPacket() {
		seed = s;
	}
};



struct StringPacket : public GamePacket {
	char stringData[256];

	StringPacket(const std::string& message) {
		type = BasicNetworkMessages::String_Message;
		size = (short)message.length();
		memcpy(stringData, message.data(), size);
	}

	std::string GetStringFromData() {
		std::string realString(stringData);
		realString.resize(size);
		return realString;
	}
};


class PacketReceiver {
public:
	virtual void ReceivePacket(int type, GamePacket* payload, int source = -1) = 0;
};

class NetworkBase	{
public:
	static void Initialise();
	static void Destroy();

	static int GetDefaultPort() {
		return 1234;
	}

	void RegisterPacketHandler(int msgID, PacketReceiver* receiver) {
		packetHandlers.insert(std::make_pair(msgID, receiver));
	}
protected:
	NetworkBase();
	~NetworkBase();

	bool ProcessPacket(GamePacket* p, int peerID = -1);

	typedef std::multimap<int, PacketReceiver*>::const_iterator PacketHandlerIterator;

	bool GetPacketHandlers(int msgID, PacketHandlerIterator& first, PacketHandlerIterator& last) const {
		auto range = packetHandlers.equal_range(msgID);

		if (range.first == packetHandlers.end()) {
			return false; //no handlers for this message type!
		}
		first	= range.first;
		last	= range.second;
		return true;
	}

	_ENetHost* netHandle;

	std::multimap<int, PacketReceiver*> packetHandlers;
};


struct PlayerTransformPacket : public GamePacket {
	int playerID;
	NCL::Maths::Vector3 position;
	float yaw;

	PlayerTransformPacket() {
		type = BasicNetworkMessages::Player_Transform;
		size = sizeof(PlayerTransformPacket) - sizeof(GamePacket);
		playerID = -1;
		position = NCL::Maths::Vector3();
		yaw = 0.0f;
	}

	PlayerTransformPacket(int id, const NCL::Maths::Vector3& pos, float y) : PlayerTransformPacket() {
		playerID = id;
		position = pos;
		yaw = y;
	}


};

struct EnemyTransformPacket : public GamePacket {
	int enemyID;
	NCL::Maths::Vector3 position;
	float yaw;

	EnemyTransformPacket() {
		type = BasicNetworkMessages::Enemy_Transform;
		size = sizeof(EnemyTransformPacket) - sizeof(GamePacket);
		enemyID = -1;
		position = NCL::Maths::Vector3();
		yaw = 0.0f;
	}

	EnemyTransformPacket(int id, const NCL::Maths::Vector3& pos, float y) : EnemyTransformPacket() {
		enemyID = id;
		position = pos;
		yaw = y;
	}
};