#pragma once
#include "RenderObject.h"
#include "StateGameObject.h"
#include "vector.h"
#include "StateTransition.h"
#include "State.h"
#include "GameServer.h"
#include "GameClient.h"
#include "NavigationPath.h"
#include "NavigationMesh.h"

namespace NCL {
	class Controller;

	namespace Rendering {
		class Mesh;
		class Texture;
		class Shader;
	}
	namespace CSC8503 {
		class GameTechRendererInterface;
		class PhysicsSystem;
		class GameWorld;
		class GameObject;
		class StateMachine;
		class State;
		class StateTransition;
		class Gameserver;
		class GameClient;
		class localPlayer;
		class remotePlayer;

		class TutorialGame {
		public:
			TutorialGame(GameWorld& gameWorld, GameTechRendererInterface& renderer, PhysicsSystem& physics);
			~TutorialGame();

			virtual void UpdateGame(float dt);

		protected:
			void InitCamera();

			void InitWorld();

			/*
			These are some of the world/object creation functions I created when testing the functionality
			in the module. Feel free to mess around with them to see different objects being created in different
			test scenarios (constraints, collision types, and so on).
			*/
			void InitGameExamples();

			void CreateSphereGrid(int numRows, int numCols, float rowSpacing, float colSpacing, float radius);
			void CreatedMixedGrid(int numRows, int numCols, float rowSpacing, float colSpacing);
			void CreateAABBGrid(int numRows, int numCols, float rowSpacing, float colSpacing, const NCL::Maths::Vector3& cubeDims);

			bool SelectObject();
			void MoveSelectedObject();
			void DebugObjectMovement();
			void LockedObjectMovement();

			GameObject* AddFloorToWorld(const NCL::Maths::Vector3& position);
			GameObject* AddSphereToWorld(const NCL::Maths::Vector3& position, float radius, float inverseMass = 10.0f);
			GameObject* AddCubeToWorld(const NCL::Maths::Vector3& position, NCL::Maths::Vector3 dimensions, float inverseMass = 10.0f);

			GameObject* AddPlayerToWorld(const NCL::Maths::Vector3& position);
			GameObject* AddEnemyToWorld(const NCL::Maths::Vector3& position);
			GameObject* AddBonusToWorld(const NCL::Maths::Vector3& position);

			StateGameObject* AddStateObjectToWorld(const Vector3& position);
			StateGameObject* testStateObject;


			GameWorld& world;
			GameTechRendererInterface& renderer;
			PhysicsSystem& physics;
			Controller* controller;

			bool useGravity;
			bool inSelectionMode;

			float		forceMagnitude;

			GameObject* selectionObject = nullptr;

			Rendering::Mesh* capsuleMesh	= nullptr;
			Rendering::Mesh* cubeMesh		= nullptr;
			Rendering::Mesh* sphereMesh		= nullptr;

			Rendering::Texture* defaultTex  = nullptr;
			Rendering::Texture* checkerTex	= nullptr;
			Rendering::Texture* glassTex	= nullptr;

			//Coursework Meshes
			Rendering::Mesh* catMesh	= nullptr;
			Rendering::Mesh* kittenMesh = nullptr;
			Rendering::Mesh* enemyMesh	= nullptr;
			Rendering::Mesh* bonusMesh	= nullptr;



			GameTechMaterial checkerMaterial;
			GameTechMaterial glassMaterial;
			GameTechMaterial notexMaterial;

			//Coursework Additional functionality	
			GameObject* lockedObject = nullptr;
			NCL::Maths::Vector3 lockedOffset = NCL::Maths::Vector3(0, 14, 20);
			void LockCameraToObject(GameObject* o) {
				lockedObject = o;
			}

			GameObject* objClosest = nullptr;

		private:

			GameObject* player = nullptr;
			void PlayerMovement(float dt);

			void UpdateThirdPersonCamera(float dt);


			float tpYaw = 315.0f;
			float tpPitch = -15.0f;
			float tpDistance = 20.0f;
			float tpHeight = 14.0f;
			float tpSmooth = 12.0f;
			float tpLookSens = 0.15f;


			enum class GameState {
				Menu,
				Playing,
				Win,
				Lose
			};

			GameState gameState = GameState::Menu;

			float timeLimit = 120.0f;
			float timeRemaining = 120.0f;

				
			int score = 0;
			int itemsRemaining = 0;

			Vector3 playerSpawn = Vector3(0, 5, 0);

			void ResetGame();
			void UpdateMenu(float dt);
			void UpdatePlaying(float dt);
			void UpdateEndScreen(float dt);


			//item delivery
			std::vector<GameObject*> pickupItems;
			std::vector<GameObject*> carriedItems;

			GameObject* endZoneVisual = nullptr;
			Vector3 endZonePos = Vector3(-165.0f, 2.0f, -165.0f);
			Vector3 endZoneHalf = Vector3(10.0f, 2.0f, 10.0f);

			int deliveryScore = 100;

			GameObject* AddEndZoneVisual(const Vector3& pos, const Vector3& halfSize);
			GameObject* AddPickupItemVisual(const Vector3& pos, float radius);

			void TryAutoPickup();
			void UpdateCarriedItem();
			void TryDeliver();
			bool IsPlayerInEndZone() const;


			NavigationMesh* navMesh = nullptr;


			//enemy ai
			struct EnemyController {
				int netID = 0;
				GameObject* enemy = nullptr;

				std::vector<Vector3> patrolPoints;
				int patrolIndex = 0;

				StateMachine* sm = nullptr;

				float patrolForce = 40.0f;
				float chaseForce = 20.0f;

				float waypointRadius = 8.0f;
				float detectRadius = 80.0f;
				float loseRadius = 110.0f;
				float killRadius = 10.0f;

				// --- A* path following state ---
				NavigationPath path;          // queued waypoints
				Vector3 currentWaypoint;      // current target point
				bool hasPath = false;

				float repathTimer = 0.0f;     // countdown
				Vector3 lastGoal;             // last requested goal
			};



			std::vector<EnemyController*> enemies;

			void InitEnemies();
			void UpdateEnemies(float dt);

			bool RepathTo(EnemyController& e, const Vector3& goal);
			bool FollowPath(EnemyController& e, float moveForce);


			void EnemyPatrol(EnemyController& e, float dt);
			void EnemyChase(EnemyController& e, float dt);

			bool EnemyShouldChase(const EnemyController& e) const;
			bool EnemyShouldPatrol(const EnemyController& e) const;

			bool EnemyCanSeePlayer(const EnemyController& e) const;
			Vector3 AvoidWalls(GameObject& enemy, const Vector3& desiredDir) const;


			//networking
			NCL::CSC8503::GameServer* server = nullptr;
			NCL::CSC8503::GameClient* client = nullptr;

			NCL::CSC8503::GameObject* localPlayer = nullptr;
			NCL::CSC8503::GameObject* remotePlayer = nullptr;

			float netSendAccum = 0.0f;
			Vector3 remoteTargetPos;
			Quaternion remoteTargetOri;
			float   remoteTargetYaw = 0.0f;
			bool hasRemoteTarget = false;


			uint32_t levelSeed = 0;
			bool seedReceived = false;
			bool isHost = false; 

			bool worldBuilt = false;




		};
	}
}