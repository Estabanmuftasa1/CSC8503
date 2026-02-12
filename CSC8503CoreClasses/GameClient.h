#pragma once
#include "NetworkBase.h"
#include <stdint.h>
#include <thread>
#include <atomic>

namespace NCL {
	namespace CSC8503 {
		class GameClient : public NetworkBase, public PacketReceiver {
		public:
			GameClient();
			~GameClient();

			bool Connect(const std::string& ip, int port);
			void Disconnect();

			void UpdateClient();
			bool SendPacket(GamePacket& packet);

			void ReceivePacket(int type, GamePacket* payload, int source = -1) override;

			int localID = 0; // set 0 for host window, 1 for join window
			bool connected = false;

			bool hasSeed = false;
			unsigned int levelSeed = 0;


			bool hasRemote = false;
			PlayerTransformPacket remote;

			struct RemoteEnemyState {
				bool valid = false;
				NCL::Maths::Vector3 position;
				float yaw = 0.0f;
			};

			RemoteEnemyState remoteEnemies[16];


		protected:
			_ENetPeer* serverPeer = nullptr;
			_ENetHost* netHandle = nullptr;

		};
	}
}