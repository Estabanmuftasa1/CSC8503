#pragma once
#include "NetworkBase.h"

namespace NCL {
	namespace CSC8503 {
		class GameWorld;
		class GameServer : public NetworkBase, public PacketReceiver {
		public:
			GameServer(int onPort, int maxClients);
			~GameServer();

			bool Initialise();
			void Shutdown();

			void SetGameWorld(GameWorld &g);

			bool SendGlobalPacket(int msgID);
			bool SendGlobalPacket(GamePacket& packet);

			virtual void UpdateServer();
			void ReceivePacket(int type, GamePacket* payload, int source = -1) override;

			bool SendPacketToPeer(int peerID, GamePacket& packet);

			void SetLevelSeed(unsigned int s) { levelSeed = s; }
			unsigned int GetLevelSeed() const { return levelSeed; }

		protected:
			int			port;
			int			clientMax;
			int			clientCount;
			GameWorld*	gameWorld;

			int incomingDataRate;
			int outgoingDataRate;

		private:
			unsigned int levelSeed = 0;
		};
	}
}
