#include "TutorialGame.h"
#include "GameWorld.h"
#include "PhysicsSystem.h"
#include "PhysicsObject.h"
#include "RenderObject.h"
#include "TextureLoader.h"

#include "PositionConstraint.h"
#include "OrientationConstraint.h"
#include "StateGameObject.h"

#include "Window.h"
#include "Texture.h"
#include "Shader.h"
#include "Mesh.h"

#include "Debug.h"

#include "KeyboardMouseController.h"

#include "GameTechRendererInterface.h"

#include "Ray.h"

#include "StateTransition.h"
#include "State.h"
#include "StateMachine.h"

#include <cstdlib>  
#include <ctime>    
#include <NetworkObject.h>
#include <GameClient.h>
#include <GameServer.h>



using namespace NCL;
using namespace CSC8503;

TutorialGame::TutorialGame(GameWorld& inWorld, GameTechRendererInterface& inRenderer, PhysicsSystem& inPhysics)
	:	world(inWorld),
		renderer(inRenderer),
		physics(inPhysics)
{

	forceMagnitude	= 10.0f;
	useGravity		= true;
	inSelectionMode = true;

	static const unsigned int kSeed = 12345;
	srand(kSeed);

	controller = new KeyboardMouseController(*Window::GetWindow()->GetKeyboard(), *Window::GetWindow()->GetMouse());

	world.GetMainCamera().SetController(*controller);

	world.SetSunPosition({ -200.0f, 60.0f, -200.0f });
	world.SetSunColour({ 0.8f, 0.8f, 0.5f });

	controller->MapAxis(0, "Sidestep");
	controller->MapAxis(1, "UpDown");
	controller->MapAxis(2, "Forward");

	controller->MapAxis(3, "XLook");
	controller->MapAxis(4, "YLook");

	cubeMesh	= renderer.LoadMesh("cube.msh");
	sphereMesh	= renderer.LoadMesh("sphere.msh");
	catMesh		= renderer.LoadMesh("ORIGAMI_Chat.msh");
	kittenMesh	= renderer.LoadMesh("Kitten.msh");

	enemyMesh	= renderer.LoadMesh("Keeper.msh");

	bonusMesh	= renderer.LoadMesh("19463_Kitten_Head_v1.msh");
	capsuleMesh = renderer.LoadMesh("capsule.msh");

	defaultTex	= renderer.LoadTexture("Default.png");
	checkerTex	= renderer.LoadTexture("checkerboard.png");
	glassTex	= renderer.LoadTexture("stainedglass.tga");

	checkerMaterial.type		= MaterialType::Opaque;
	checkerMaterial.diffuseTex	= checkerTex;

	glassMaterial.type			= MaterialType::Transparent;
	glassMaterial.diffuseTex	= glassTex;

	InitCamera();
	gameState = GameState::Menu;
}

TutorialGame::~TutorialGame()	{
}

void TutorialGame::UpdateGame(float dt) {
	switch (gameState) {
	case GameState::Menu:
		UpdateMenu(dt);
		break;
	case GameState::Playing:
		UpdatePlaying(dt);
		break;
	case GameState::Win:
	case GameState::Lose:
		UpdateEndScreen(dt);
		break;
	}
}



void TutorialGame::InitCamera() {
	world.GetMainCamera().SetNearPlane(0.1f);
	world.GetMainCamera().SetFarPlane(500.0f);

	tpYaw = 315.0f;
	tpPitch = -15.0f;

	world.GetMainCamera().SetPitch(-15.0f);
	world.GetMainCamera().SetYaw(315.0f);
	world.GetMainCamera().SetPosition(Vector3(-60, 40, 60));
	lockedObject = nullptr;
}

static float RandRange(float min, float max) {
	return min + (float(rand()) / float(RAND_MAX)) * (max - min);
}


void TutorialGame::InitWorld() {
	world.ClearAndErase();
	physics.Clear();
	physics.UseGravity(useGravity);
	for (auto* e : enemies) {
		if (e) {
			delete e->sm;   
			delete e;
		}
	}
	enemies.clear();


	localPlayer = AddPlayerToWorld(Vector3(-170, 5, -170));
	player = localPlayer;
	remotePlayer = AddPlayerToWorld(Vector3(-160, 5, -160));
	if (auto* p = remotePlayer->GetPhysicsObject()) {
		p->SetInverseMass(0.0f);
		p->SetLinearVelocity(Vector3());
		p->SetAngularVelocity(Vector3());
	}
	remoteTargetPos = remotePlayer->GetTransform().GetPosition();
	remoteTargetYaw = 0.0f;
	hasRemoteTarget = false; 
	netSendAccum = 0.0f;

	if (localPlayer && localPlayer->GetRenderObject()) {
		localPlayer->GetRenderObject()->SetColour(Vector4(0.9f, 0.2f, 0.2f, 1.0f)); // red
	}
	if (remotePlayer && remotePlayer->GetRenderObject()) {
		remotePlayer->GetRenderObject()->SetColour(Vector4(0.2f, 0.9f, 0.2f, 1.0f)); // green
	}



	inSelectionMode = true;
	lockedObject = player;
	selectionObject = nullptr;
	 
	score = 0;
	timeRemaining = timeLimit;
	itemsRemaining = 0;

	const float floorY = -2.0f;
	AddFloorToWorld(Vector3(0, floorY, 0));

	const float floorHalf = 200.0f;
	const float wallHalfT = 2.0f;
	const float wallHalfH = 10.0f;
	const float wallY = floorY + wallHalfH;
	const float outer = floorHalf - wallHalfT;

	auto WallX = [&](float x, float z, float halfLen) {
		AddCubeToWorld(Vector3(x, wallY, z), Vector3(wallHalfT, wallHalfH, halfLen), 0.0f);
		};
	auto WallZ = [&](float x, float z, float halfLen) {
		AddCubeToWorld(Vector3(x, wallY, z), Vector3(halfLen, wallHalfH, wallHalfT), 0.0f);
		};

	WallZ(0.0f, outer, floorHalf);
	WallZ(0.0f, -outer, floorHalf);
	WallX(outer, 0.0f, floorHalf);
	WallX(-outer, 0.0f, floorHalf);

	WallZ(-155, -120, 40);
	WallX(-115, -140, 20);

	WallZ(40, -140, 100);

	WallZ(-23, -60, 40);
	WallX(15, -20, 40);
	WallZ(76, 20, 63);
	WallX(140, -20, 40);

	WallZ(0, 120, 140);
	WallZ(-100, 50, 40);
	WallX(-60, 85, 35);
	WallX(-140, 0, 50);
	
	pickupItems.clear();
	carriedItems.clear();

	endZoneVisual = AddEndZoneVisual(endZonePos, endZoneHalf);

	pickupItems.clear();

	const int numPickups = 15;
	const float spawnY = 5.0f;
	const float radius = 1.0f;

	const float minX = -180.0f, maxX = 180.0f;
	const float minZ = -180.0f, maxZ = 180.0f;

	for (int i = 0; i < numPickups; ++i) {
		Vector3 pos(RandRange(minX, maxX), spawnY, RandRange(minZ, maxZ));
		pickupItems.push_back(AddPickupItemVisual(pos, radius));
	}

	itemsRemaining = (int)pickupItems.size();


	itemsRemaining = (int)pickupItems.size();
	InitEnemies();

	player = localPlayer;
	lockedObject = player;
	inSelectionMode = true;
	selectionObject = nullptr;

}


/*

A single function to add a large immoveable cube to the bottom of our world

*/
GameObject* TutorialGame::AddFloorToWorld(const Vector3& position) {
	GameObject* floor = new GameObject();

	Vector3 floorSize = Vector3(200, 2, 200);
	AABBVolume* volume = new AABBVolume(floorSize);
	floor->SetBoundingVolume(volume);
	floor->GetTransform()
		.SetScale(floorSize * 2.0f)
		.SetPosition(position);

	floor->SetRenderObject(new RenderObject(floor->GetTransform(), cubeMesh, checkerMaterial));
	floor->SetPhysicsObject(new PhysicsObject(floor->GetTransform(), floor->GetBoundingVolume()));

	floor->GetPhysicsObject()->SetInverseMass(0);
	floor->GetPhysicsObject()->InitCubeInertia();

	world.AddGameObject(floor);

	return floor;
}

/*

Builds a game object that uses a sphere mesh for its graphics, and a bounding sphere for its
rigid body representation. This and the cube function will let you build a lot of 'simple' 
physics worlds. You'll probably need another function for the creation of OBB cubes too.

*/
GameObject* TutorialGame::AddSphereToWorld(const Vector3& position, float radius, float inverseMass) {
	GameObject* sphere = new GameObject();

	Vector3 sphereSize = Vector3(radius, radius, radius);
	SphereVolume* volume = new SphereVolume(radius);
	sphere->SetBoundingVolume(volume);

	sphere->GetTransform()
		.SetScale(sphereSize)
		.SetPosition(position);

	sphere->SetRenderObject(new RenderObject(sphere->GetTransform(), sphereMesh, checkerMaterial));
	sphere->SetPhysicsObject(new PhysicsObject(sphere->GetTransform(), sphere->GetBoundingVolume()));

	sphere->GetPhysicsObject()->SetInverseMass(inverseMass);
	sphere->GetPhysicsObject()->InitSphereInertia();

	world.AddGameObject(sphere);

	return sphere;
}

GameObject* TutorialGame::AddCubeToWorld(const Vector3& position, Vector3 dimensions, float inverseMass) {
	GameObject* cube = new GameObject();

	AABBVolume* volume = new AABBVolume(dimensions);
	cube->SetBoundingVolume(volume);

	cube->GetTransform()
		.SetPosition(position)
		.SetScale(dimensions * 2.0f);

	cube->SetRenderObject(new RenderObject(cube->GetTransform(), cubeMesh, checkerMaterial));
	cube->SetPhysicsObject(new PhysicsObject(cube->GetTransform(), cube->GetBoundingVolume()));

	cube->GetPhysicsObject()->SetInverseMass(inverseMass);
	cube->GetPhysicsObject()->InitCubeInertia();

	world.AddGameObject(cube);

	return cube;
}

GameObject* TutorialGame::AddPlayerToWorld(const Vector3& position) {
	float meshSize		= 4.0f;
	float inverseMass	= 0.5f;

	GameObject* character = new GameObject();
	SphereVolume* volume  = new SphereVolume(3.0f);

	character->SetBoundingVolume(volume);

	character->GetTransform()
		.SetScale(Vector3(meshSize, meshSize, meshSize))
		.SetPosition(position);

	character->SetRenderObject(new RenderObject(character->GetTransform(), catMesh, checkerMaterial));
	character->SetPhysicsObject(new PhysicsObject(character->GetTransform(), character->GetBoundingVolume()));

	character->GetPhysicsObject()->SetInverseMass(inverseMass);
	character->GetPhysicsObject()->InitSphereInertia();

	world.AddGameObject(character);

	return character;
}




GameObject* TutorialGame::AddEnemyToWorld(const Vector3& position) {
	float meshSize		= 10.0f;
	float inverseMass	= 0.5f;

	GameObject* character = new GameObject();

	AABBVolume* volume = new AABBVolume(Vector3(0.3f, 0.9f, 0.3f) * meshSize);
	character->SetBoundingVolume(volume);

	character->GetTransform()
		.SetScale(Vector3(meshSize, meshSize, meshSize))
		.SetPosition(position);

	character->SetRenderObject(new RenderObject(character->GetTransform(), enemyMesh, notexMaterial));
	character->SetPhysicsObject(new PhysicsObject(character->GetTransform(), character->GetBoundingVolume()));

	character->GetPhysicsObject()->SetInverseMass(inverseMass);
	character->GetPhysicsObject()->InitSphereInertia();

	world.AddGameObject(character);

	return character;
}

StateGameObject* TutorialGame::AddStateObjectToWorld(const Vector3& position) {
	StateGameObject* apple = new StateGameObject();

	SphereVolume* volume = new SphereVolume(0.5f);
	apple->SetBoundingVolume(volume);
	apple->GetTransform()
		.SetScale(Vector3(2, 2, 2))
		.SetPosition(position);

	apple->SetRenderObject(new RenderObject(apple->GetTransform(), bonusMesh, glassMaterial));
	apple->SetPhysicsObject(new PhysicsObject(apple->GetTransform(), apple->GetBoundingVolume()));

	apple->GetPhysicsObject()->SetInverseMass(1.0f);
	apple->GetPhysicsObject()->InitSphereInertia();

	world.AddGameObject(apple);

	return apple;
}



GameObject* TutorialGame::AddBonusToWorld(const Vector3& position) {
	GameObject* apple = new GameObject();

	SphereVolume* volume = new SphereVolume(0.5f);
	apple->SetBoundingVolume(volume);
	apple->GetTransform()
		.SetScale(Vector3(2, 2, 2))
		.SetPosition(position);

	apple->SetRenderObject(new RenderObject(apple->GetTransform(), bonusMesh, glassMaterial));
	apple->SetPhysicsObject(new PhysicsObject(apple->GetTransform(), apple->GetBoundingVolume()));

	apple->GetPhysicsObject()->SetInverseMass(1.0f);
	apple->GetPhysicsObject()->InitSphereInertia();

	world.AddGameObject(apple);

	return apple;
}

void TutorialGame::InitGameExamples() {
	AddPlayerToWorld(Vector3(0, 5, 0));
	AddEnemyToWorld(Vector3(5, 5, 0));
	AddBonusToWorld(Vector3(10, 5, 0));
}

void TutorialGame::CreateSphereGrid(int numRows, int numCols, float rowSpacing, float colSpacing, float radius) {
	for (int x = 0; x < numCols; ++x) {
		for (int z = 0; z < numRows; ++z) {
			Vector3 position = Vector3(x * colSpacing, 10.0f, z * rowSpacing);
			AddSphereToWorld(position, radius, 1.0f);
		}
	}
	AddFloorToWorld(Vector3(0, -2, 0));
}

void TutorialGame::CreatedMixedGrid(int numRows, int numCols, float rowSpacing, float colSpacing) {
	float sphereRadius = 1.0f;
	Vector3 cubeDims = Vector3(1, 1, 1);

	for (int x = 0; x < numCols; ++x) {
		for (int z = 0; z < numRows; ++z) {
			Vector3 position = Vector3(x * colSpacing, 10.0f, z * rowSpacing);

			if (rand() % 2) {
				AddCubeToWorld(position, cubeDims);
			}
			else {
				AddSphereToWorld(position, sphereRadius);
			}
		}
	}
}

void TutorialGame::CreateAABBGrid(int numRows, int numCols, float rowSpacing, float colSpacing, const Vector3& cubeDims) {
	for (int x = 1; x < numCols+1; ++x) {
		for (int z = 1; z < numRows+1; ++z) {
			Vector3 position = Vector3(x * colSpacing, 10.0f, z * rowSpacing);
			AddCubeToWorld(position, cubeDims, 1.0f);
		}
	}
}

/*
Every frame, this code will let you perform a raycast, to see if there's an object
underneath the cursor, and if so 'select it' into a pointer, so that it can be 
manipulated later. Pressing Q will let you toggle between this behaviour and instead
letting you move the camera around. 

*/
bool TutorialGame::SelectObject() {
	if (Window::GetKeyboard()->KeyPressed(KeyCodes::Q)) {
		inSelectionMode = !inSelectionMode;

		if (inSelectionMode) {
			Window::GetWindow()->ShowOSPointer(false);
			Window::GetWindow()->LockMouseToWindow(true);
			lockedObject = player;
			selectionObject = nullptr;
		}
		else {
			Window::GetWindow()->ShowOSPointer(false);
			Window::GetWindow()->LockMouseToWindow(true);
			lockedObject = nullptr;
		}
	}

	if (inSelectionMode) {
		return false;
	}

	Debug::Print("Press Q to lock to player!", Vector2(5, 85));

	if (Window::GetMouse()->ButtonDown(NCL::MouseButtons::Left)) {
		if (selectionObject) {
			selectionObject->GetRenderObject()->SetColour(Vector4(1, 1, 1, 1));
			selectionObject = nullptr;
		}

		Ray ray = CollisionDetection::BuildRayFromMouse(world.GetMainCamera());
		RayCollision closestCollision;

		if (world.Raycast(ray, closestCollision, true)) {
			selectionObject = (GameObject*)closestCollision.node;
			selectionObject->GetRenderObject()->SetColour(Vector4(0, 1, 0, 1));
			return true;
		}
	}

	if (Window::GetKeyboard()->KeyPressed(NCL::KeyCodes::L)) {
		if (selectionObject) {
			lockedObject = selectionObject;
		}
	}

	return false;
}


/*
If an object has been clicked, it can be pushed with the right mouse button, by an amount
determined by the scroll wheel. In the first tutorial this won't do anything, as we haven't
added linear motion into our physics system. After the second tutorial, objects will move in a straight
line - after the third, they'll be able to twist under torque aswell.
*/

void TutorialGame::MoveSelectedObject() {

	forceMagnitude += Window::GetMouse()->GetWheelMovement() * 100.0f;

	if (!selectionObject) {
		return; 
	}
 
	if (Window::GetMouse()->ButtonPressed(NCL::MouseButtons::Right)) {
		Ray ray = CollisionDetection::BuildRayFromMouse(
			world.GetMainCamera()
		);

		RayCollision closestCollision;
		if (world.Raycast(ray, closestCollision, true)) {
			if (closestCollision.node == selectionObject) {
				selectionObject->GetPhysicsObject()->AddForceAtPosition(
					ray.GetDirection() * forceMagnitude,
					closestCollision.collidedAt);
			}
		}
	}
}

void TutorialGame::UpdateThirdPersonCamera(float dt) {
	if (!player) return;

	Vector2 mouseDelta = Window::GetMouse()->GetRelativePosition();
	tpYaw -= mouseDelta.x * tpLookSens;
	tpPitch -= mouseDelta.y * tpLookSens;

	tpPitch = std::min(5.0f, std::max(-35.0f, tpPitch));

	Vector3 playerPos = player->GetTransform().GetPosition();

	Quaternion yawRot = Quaternion::AxisAngleToQuaterion(Vector3(0, 1, 0), tpYaw);

	Vector3 back = yawRot * Vector3(0, 0, 1);
	Vector3 desiredPos = playerPos + Vector3(0, tpHeight, 0) + back * tpDistance;

	Vector3 currentPos = world.GetMainCamera().GetPosition();
	float t = 1.0f - std::exp(-tpSmooth * dt);
	Vector3 newPos = currentPos + (desiredPos - currentPos) * t;

	world.GetMainCamera().SetPosition(newPos);
	world.GetMainCamera().SetYaw(tpYaw);
	world.GetMainCamera().SetPitch(tpPitch);
}

void TutorialGame::PlayerMovement(float dt) {
	if (!player) return;

	PhysicsObject* phys = player->GetPhysicsObject();
	if (!phys) return;

	Quaternion faceCamYaw = Quaternion::AxisAngleToQuaterion(Vector3(0, 1, 0), tpYaw + 180.0f);
	player->GetTransform().SetOrientation(faceCamYaw);

	const float moveForce = 40.0f;

	Vector3 forward = faceCamYaw * Vector3(0, 0, -1);
	Vector3 right = faceCamYaw * Vector3(1, 0, 0);

	forward.y = 0.0f;
	right.y = 0.0f;

	forward = Vector::Normalise(forward);
	right = Vector::Normalise(right);

	if (Window::GetKeyboard()->KeyDown(KeyCodes::S)) {
		phys->AddForce(forward * moveForce);
	}
	if (Window::GetKeyboard()->KeyDown(KeyCodes::W)) {
		phys->AddForce(-forward * moveForce);
	}
	if (Window::GetKeyboard()->KeyDown(KeyCodes::D)) {
		phys->AddForce(-right * moveForce);
	}
	if (Window::GetKeyboard()->KeyDown(KeyCodes::A)) {
		phys->AddForce(right * moveForce);
	}

	if (Window::GetKeyboard()->KeyPressed(KeyCodes::SPACE)) {
		phys->AddForce(Vector3(0, 3000.0f, 0));
	}

}




void TutorialGame::ResetGame() {
	world.ClearAndErase();
	physics.Clear();

	score = 0;
	timeRemaining = timeLimit;

	InitCamera();

	player = AddPlayerToWorld(playerSpawn);
	lockedObject = nullptr;

	AddFloorToWorld(Vector3(0, -2, 0));

	itemsRemaining = 0;

	gameState = GameState::Playing;
}


void TutorialGame::UpdateMenu(float dt) {
	Debug::Print("COURIER GAME", Vector2(5, 10));

	Debug::Print("1: Host Game (Server + Client)", Vector2(5, 18));
	Debug::Print("2: Join Game (Client)", Vector2(5, 22));

	Debug::Print("ENTER: Local Single Player", Vector2(5, 28));
	Debug::Print("ESC: Quit", Vector2(5, 32));

	if (Window::GetKeyboard()->KeyPressed(KeyCodes::NUM1)) {
		NetworkBase::Initialise();
		std::cout << "[HOST] Starting server...\n";

		server = new GameServer(NetworkBase::GetDefaultPort(), 2);
		if (!server->Initialise()) {
			std::cout << "[HOST] Server initialise FAILED\n";
			delete server; server = nullptr;
			return;
		}

		unsigned int seed = 12345;
		server->SetLevelSeed(seed);

		client = new GameClient();
		client->localID = 0;

		gameState = GameState::Playing;
		worldBuilt = false;

		std::cout << "[HOST] Press F3 to connect host-client.\n";
	}


	if (Window::GetKeyboard()->KeyPressed(KeyCodes::NUM2)) {
		NetworkBase::Initialise();
		std::cout << "[CLIENT] Starting client...\n";

		client = new GameClient();
		client->localID = 1;

		gameState = GameState::Playing;
		worldBuilt = false;

		std::cout << "[CLIENT] Press F3 to connect client.\n";
	}




	if (Window::GetKeyboard()->KeyPressed(KeyCodes::RETURN)) {
		if (client) { delete client; client = nullptr; }
		if (server) { delete server; server = nullptr; }

		worldBuilt = false;

		srand(12345);

		InitWorld();

		worldBuilt = true;
		gameState = GameState::Playing;
	}

	if (Window::GetKeyboard()->KeyPressed(KeyCodes::ESCAPE)) {
		Window::GetWindow()->DestroyGameWindow();
	}
}


void TutorialGame::UpdateEndScreen(float dt) {
	if (gameState == GameState::Win) {
		Debug::Print("YOU WIN", Vector2(5, 10));
	}
	else {
		Debug::Print("YOU LOSE", Vector2(5, 10));
	}

	Debug::Print("Final Score: " + std::to_string(score), Vector2(5, 15));
	Debug::Print("R: Restart", Vector2(5, 20));
	Debug::Print("ESC: Quit", Vector2(5, 25));

	if (Window::GetKeyboard()->KeyPressed(KeyCodes::R)) {
		gameState = GameState::Menu;
	}

	if (Window::GetKeyboard()->KeyPressed(KeyCodes::ESCAPE)) {
		Window::GetWindow()->DestroyGameWindow();
	}
}

GameObject* TutorialGame::AddEndZoneVisual(const Vector3& pos, const Vector3& halfSize) {
	GameObject* zone = new GameObject();

	AABBVolume* volume = new AABBVolume(halfSize);
	zone->SetBoundingVolume(volume);

	zone->GetTransform()
		.SetPosition(pos)
		.SetScale(halfSize * 2.0f);

	RenderObject* r = new RenderObject(zone->GetTransform(), cubeMesh, checkerMaterial);
	r->SetColour(Vector4(0.1f, 0.9f, 0.1f, 1.0f)); // green
	zone->SetRenderObject(r);

	zone->SetPhysicsObject(new PhysicsObject(zone->GetTransform(), zone->GetBoundingVolume()));
	zone->GetPhysicsObject()->SetInverseMass(0.0f); // static
	zone->GetPhysicsObject()->InitCubeInertia();

	world.AddGameObject(zone);
	return zone;
}

GameObject* TutorialGame::AddPickupItemVisual(const Vector3& pos, float radius) {
	GameObject* item = new GameObject();

	SphereVolume* volume = new SphereVolume(radius);
	item->SetBoundingVolume(volume);

	item->GetTransform()
		.SetPosition(pos)
		.SetScale(Vector3(radius, radius, radius)); 

	RenderObject* r = new RenderObject(item->GetTransform(), sphereMesh, checkerMaterial);
	r->SetColour(Vector4(0.1f, 0.9f, 0.1f, 1.0f)); 
	item->SetRenderObject(r);

	item->SetPhysicsObject(new PhysicsObject(item->GetTransform(), item->GetBoundingVolume()));
	item->GetPhysicsObject()->SetInverseMass(1.0f);
	item->GetPhysicsObject()->InitSphereInertia();

	world.AddGameObject(item);
	return item;
}

void TutorialGame::TryAutoPickup() {
	if (!player) return;

	const Vector3 p = player->GetTransform().GetPosition();
	const float pickupR = 7.0f; 
	const float pickupR2 = pickupR * pickupR;

	for (GameObject* it : pickupItems) {
		if (!it) continue;

		bool alreadyCarried = false;
		for (GameObject* c : carriedItems) {
			if (c == it) { alreadyCarried = true; break; }
		}
		if (alreadyCarried) continue;

		Vector3 d = it->GetTransform().GetPosition() - p;
		if (Vector::Dot(d, d) <= pickupR2) {
			carriedItems.push_back(it);

			if (PhysicsObject* phys = it->GetPhysicsObject()) {
				phys->SetInverseMass(0.0f);     
				phys->SetLinearVelocity(Vector3());
				phys->SetAngularVelocity(Vector3());
			}
		}
	}
}

void TutorialGame::UpdateCarriedItem() {
	if (!player) return;

	Vector3 base = player->GetTransform().GetPosition() + Vector3(0, 12.0f, 0); 
	const float spacing = 2.5f;

	for (size_t i = 0; i < carriedItems.size(); ++i) {
		GameObject* it = carriedItems[i];
		if (!it) continue;

		Vector3 pos = base + Vector3(0, spacing * (float)i, 0);
		it->GetTransform().SetPosition(pos);
	}
}

bool TutorialGame::IsPlayerInEndZone() const {
	if (!player || !endZoneVisual) return false;

	Vector3 p = player->GetTransform().GetPosition();
	Vector3 z = endZoneVisual->GetTransform().GetPosition();

	if (std::abs(p.x - z.x) > endZoneHalf.x) return false;
	if (std::abs(p.z - z.z) > endZoneHalf.z) return false;

	float topY = z.y + endZoneHalf.y;
	return p.y >= topY + 0.5f; 
}

void TutorialGame::TryDeliver() {
	if (carriedItems.empty()) return;
	if (!IsPlayerInEndZone()) return;

	int deliveredCount = 0;

	for (GameObject* it : carriedItems) {
		if (!it) continue;


		it->GetTransform().SetPosition(Vector3(9999.0f, 9999.0f, 9999.0f));
		it->GetTransform().SetScale(Vector3(0.001f, 0.001f, 0.001f));

		if (PhysicsObject* phys = it->GetPhysicsObject()) {
			phys->SetLinearVelocity(Vector3());
			phys->SetAngularVelocity(Vector3());
			phys->SetInverseMass(0.0f);
		}

		deliveredCount++;
	}

	carriedItems.clear();

	itemsRemaining -= deliveredCount;
	if (itemsRemaining < 0) itemsRemaining = 0;

	score += deliveredCount * deliveryScore;
}

void TutorialGame::InitEnemies() {

	auto* e = new EnemyController();
	e->netID = 0;
	e->enemy = AddEnemyToWorld(Vector3(-60, 5, -60));
	e->patrolPoints = {
		Vector3(-60, 5, -60),
		Vector3(20, 5, -60),
		Vector3(20, 5,  20),
		Vector3(-60, 5,  20)
	};

	e->sm = new StateMachine();

	State* patrol = new State([this, e](float dt) { EnemyPatrol(*e, dt); });
	State* chase = new State([this, e](float dt) { EnemyChase(*e, dt); });

	e->sm->AddState(patrol);
	e->sm->AddState(chase);

	e->sm->AddTransition(new StateTransition(
		patrol, chase, [this, e]() { return EnemyShouldChase(*e); }
	));

	e->sm->AddTransition(new StateTransition(
		chase, patrol, [this, e]() { return EnemyShouldPatrol(*e); }
	));

	enemies.push_back(e);

}


void TutorialGame::UpdateEnemies(float dt) {
	if (!player) return;

	int debugLine = 0;

	for (auto* e : enemies) {
		if (!e || !e->enemy || !e->sm) continue;

		e->sm->Update(dt);

		// kill check
		Vector3 ep = e->enemy->GetTransform().GetPosition();
		Vector3 pp = player->GetTransform().GetPosition();

		float dx = ep.x - pp.x;
		float dz = ep.z - pp.z;

		if (dx * dx + dz * dz <= e->killRadius * e->killRadius) {
			player->GetTransform().SetPosition(playerSpawn);

			if (auto* phys = player->GetPhysicsObject()) {
				phys->SetLinearVelocity(Vector3());
				phys->SetAngularVelocity(Vector3());
			}

			score -= 25;
			timeRemaining -= 5.0f;
			if (timeRemaining < 0.0f) timeRemaining = 0.0f;
		}
	}
}



void TutorialGame::EnemyPatrol(EnemyController& e, float dt) {
	if (e.patrolPoints.empty()) return;

	Vector3 enemyPos = e.enemy->GetTransform().GetPosition();
	Vector3 target = e.patrolPoints[e.patrolIndex];

	Vector3 d = target - enemyPos;
	d.y = 0.0f;

	float r2 = e.waypointRadius * e.waypointRadius;
	if (Vector::Dot(d, d) <= r2) {
		e.patrolIndex = (e.patrolIndex + 1) % (int)e.patrolPoints.size();
		return;
	}

	d = Vector::Normalise(d);
	d = AvoidWalls(*e.enemy, d);

	if (auto* phys = e.enemy->GetPhysicsObject()) {
		phys->AddForce(d * e.patrolForce);
	}

}

void TutorialGame::EnemyChase(EnemyController& e, float dt) {
	Vector3 enemyPos = e.enemy->GetTransform().GetPosition();
	Vector3 playerPos = player->GetTransform().GetPosition();

	Vector3 d = playerPos - enemyPos;
	d.y = 0.0f;

	d = Vector::Normalise(d);

	if (auto* phys = e.enemy->GetPhysicsObject()) {
		phys->AddForce(d * e.chaseForce);
	}

}

bool TutorialGame::EnemyShouldChase(const EnemyController& e) const {
	Vector3 ep = e.enemy->GetTransform().GetPosition();
	Vector3 pp = player->GetTransform().GetPosition();

	float dx = ep.x - pp.x;
	float dz = ep.z - pp.z;

	bool inRange = (dx * dx + dz * dz) <= (e.detectRadius * e.detectRadius);
	if (!inRange) return false;

	return EnemyCanSeePlayer(e);
}


bool TutorialGame::EnemyShouldPatrol(const EnemyController& e) const {
	Vector3 ep = e.enemy->GetTransform().GetPosition();
	Vector3 pp = player->GetTransform().GetPosition();

	float dx = ep.x - pp.x;
	float dz = ep.z - pp.z;

	bool outOfRange = (dx * dx + dz * dz) >= (e.loseRadius * e.loseRadius);
	if (outOfRange) return true;


	return !EnemyCanSeePlayer(e);
}

Vector3 TutorialGame::AvoidWalls(GameObject& enemy, const Vector3& desiredDir) const {

	Vector3 pos = enemy.GetTransform().GetPosition();
	pos.y += 3.0f; 

	Vector3 fwd = desiredDir;
	if (Vector::Length(fwd) < 0.001f) return desiredDir;

	Vector3 right = Vector::Cross(Vector3(0, 1, 0), fwd); 
	right.y = 0.0f;
	right = Vector::Normalise(right);

	const float feelerLen = 10.0f;   
	const float originPush = 6.0f;     

	auto Cast = [&](const Vector3& dir, RayCollision& outHit)->bool {
		Ray ray(pos + fwd * originPush, dir);
		return world.Raycast(ray, outHit, true);
		};

	RayCollision hitF, hitL, hitR;
	bool hf = Cast(fwd, hitF);
	bool hl = Cast(Vector::Normalise(fwd - right * 0.6f), hitL);
	bool hr = Cast(Vector::Normalise(fwd + right * 0.6f), hitR);


	Debug::DrawLine(pos + fwd * originPush, pos + fwd * (originPush + feelerLen), Vector4(0, 0, 1, 1), 0.0f);
	Debug::DrawLine(pos + fwd * originPush, pos + Vector::Normalise(fwd - right * 0.6f) * (originPush + feelerLen), Vector4(0, 0, 1, 1), 0.0f);
	Debug::DrawLine(pos + fwd * originPush, pos + Vector::Normalise(fwd + right * 0.6f) * (originPush + feelerLen), Vector4(0, 0, 1, 1), 0.0f);

	auto IsCloseHit = [&](bool hit, const RayCollision& h)->bool {
		if (!hit) return false;

		GameObject* obj = (GameObject*)h.node;
		if (obj == player) return false;
		return h.rayDistance > 0.0f && h.rayDistance < feelerLen;
		};

	bool closeF = IsCloseHit(hf, hitF);
	bool closeL = IsCloseHit(hl, hitL);
	bool closeR = IsCloseHit(hr, hitR);


	if (closeF) {
		if (!closeL && closeR) return Vector::Normalise(fwd - right);  
		if (!closeR && closeL) return Vector::Normalise(fwd + right);  

		if (hitL.rayDistance < hitR.rayDistance) return Vector::Normalise(fwd + right);
		return Vector::Normalise(fwd - right);
	}


	Vector3 steer = fwd;
	if (closeL) steer += right * 0.8f;
	if (closeR) steer -= right * 0.8f;

	steer.y = 0.0f;
	return Vector::Normalise(steer);
}

void TutorialGame::UpdatePlaying(float dt) {

	if (server) {
		server->UpdateServer();
	}

	if (Window::GetKeyboard()->KeyPressed(KeyCodes::F3)) {
		if (client && !client->connected) {
			bool ok = client->Connect("127.0.0.1", NetworkBase::GetDefaultPort());
			std::cout << (ok ? "[NET] Client connect started\n" : "[NET] Client connect failed\n");
		}
	}

	if (client) {
		client->UpdateClient();
	}

	if (!worldBuilt) {
		if (!server && !client) {

			srand(12345);
			InitWorld();
			worldBuilt = true;
		}
		else if (server) {
			srand(server->GetLevelSeed());
			InitWorld();
			worldBuilt = true;
			std::cout << "[HOST] World built with seed.\n";
		}
		else if (client && client->hasSeed) {
			srand(client->levelSeed);
			InitWorld();
			worldBuilt = true;
			std::cout << "[CLIENT] World built with seed.\n";
		}
		else {

			return;
		}


		player = localPlayer;
		lockedObject = player;
		inSelectionMode = true;
		selectionObject = nullptr;
		Window::GetWindow()->ShowOSPointer(false);
		Window::GetWindow()->LockMouseToWindow(true);
	}



	SelectObject();

	if (!inSelectionMode && lockedObject == nullptr) {
		world.GetMainCamera().UpdateCamera(dt);
	}
	if (lockedObject != nullptr) {
		UpdateThirdPersonCamera(dt);
	}

	if (inSelectionMode && lockedObject == player) {
		PlayerMovement(dt);
	}
	else {
		DebugObjectMovement();
		MoveSelectedObject();
	}


	if (client && client->connected && localPlayer) {
		netSendAccum += dt;
		if (netSendAccum >= 0.05f) {
			netSendAccum = 0.0f;

			Vector3 pos = localPlayer->GetTransform().GetPosition();
			float yaw = 0.0f;
			PlayerTransformPacket pkt(client->localID, pos, yaw);
			client->SendPacket(pkt);
		}
	}


	if (client && remotePlayer && client->hasRemote) {
		remoteTargetPos = client->remote.position;
		remoteTargetYaw = client->remote.yaw;
		hasRemoteTarget = true;
	}
	if (remotePlayer && hasRemoteTarget) {
		Vector3 cur = remotePlayer->GetTransform().GetPosition();
		float smooth = 1.0f - std::exp(-12.0f * dt);
		remotePlayer->GetTransform().SetPosition(cur + (remoteTargetPos - cur) * smooth);
	}

	TryAutoPickup();
	UpdateCarriedItem();
	TryDeliver();

	UpdateEnemies(dt);


	timeRemaining -= dt;
	if (timeRemaining <= 0.0f) {
		timeRemaining = 0.0f;
		gameState = GameState::Lose;
		return;
	}

	Debug::Print("Score: " + std::to_string(score), Vector2(5, 5));
	Debug::Print("Time: " + std::to_string((int)timeRemaining), Vector2(5, 10));
	Debug::Print("Items Left: " + std::to_string(itemsRemaining), Vector2(5, 15));

	world.OperateOnContents([dt](GameObject* o) { o->Update(dt); });
}




bool TutorialGame::EnemyCanSeePlayer(const EnemyController& e) const {
	if (!player || !e.enemy) return false;

	Vector3 enemyPos = e.enemy->GetTransform().GetPosition();
	Vector3 playerPos = player->GetTransform().GetPosition();


	enemyPos.y += 3.0f;
	playerPos.y += 3.0f;

	Vector3 delta = playerPos - enemyPos;
	//delta.y = 0.0f;

	float dist = Vector::Length(delta);
	if (dist < 0.01f) return true;

	Vector3 dir = delta / dist;


	Vector3 start = enemyPos + dir * 4.0f;


	Vector3 d2 = playerPos - start;
	float distFromStart = Vector::Length(d2);

	Ray ray(start, dir);
	RayCollision hit;

	bool hitSomething = world.Raycast(ray, hit, true);


	Debug::DrawLine(start, start + dir * distFromStart, Vector4(0, 0, 1, 1), 0.0f);

	if (!hitSomething) {
		return false;
	}

	GameObject* hitObj = (GameObject*)hit.node;

	Debug::Print(
		hitObj == player ? "HIT: PLAYER" :
		hitObj == e.enemy ? "HIT: ENEMY" : "HIT: WALL/FLOOR",
		Vector2(5, 35)
	);

	Debug::DrawLine(start, hit.collidedAt,
		(hitObj == player) ? Vector4(0, 1, 0, 1) : Vector4(1, 0, 0, 1),
		0.0f);

	if (hitObj == e.enemy) {
		return false;
	}

	if (hitObj == player && hit.rayDistance <= distFromStart + 1.0f) {
		return true;
	}

	return false;
}



void TutorialGame::LockedObjectMovement() {
	Matrix4 view = world.GetMainCamera().BuildViewMatrix();
	Matrix4 camWorld = Matrix::Inverse(view);

	Vector3 rightAxis = Vector3(camWorld.GetColumn(0)); 

	//forward is more tricky -  camera forward is 'into' the screen...
	//so we can take a guess, and use the cross of straight up, and
	//the right axis, to hopefully get a vector that's good enough!

	Vector3 fwdAxis = Vector::Cross(Vector3(0, 1, 0), rightAxis);
	fwdAxis.y = 0.0f;
	fwdAxis = Vector::Normalise(fwdAxis);

	if (Window::GetKeyboard()->KeyDown(KeyCodes::UP)) {
		selectionObject->GetPhysicsObject()->AddForce(fwdAxis);
	}

	if (Window::GetKeyboard()->KeyDown(KeyCodes::DOWN)) {
		selectionObject->GetPhysicsObject()->AddForce(-fwdAxis);
	}

	if (Window::GetKeyboard()->KeyDown(KeyCodes::NEXT)) {
		selectionObject->GetPhysicsObject()->AddForce(Vector3(0, -10, 0));
	}
}

void TutorialGame::DebugObjectMovement() {
	//If we've selected an object, we can manipulate it with some key presses
	if (inSelectionMode && selectionObject) {
		//Twist the selected object!
		if (Window::GetKeyboard()->KeyDown(KeyCodes::LEFT)) {
			selectionObject->GetPhysicsObject()->AddTorque(Vector3(-10, 0, 0));
		}

		if (Window::GetKeyboard()->KeyDown(KeyCodes::RIGHT)) {
			selectionObject->GetPhysicsObject()->AddTorque(Vector3(10, 0, 0));
		}

		if (Window::GetKeyboard()->KeyDown(KeyCodes::NUM7)) {
			selectionObject->GetPhysicsObject()->AddTorque(Vector3(0, 10, 0));
		}

		if (Window::GetKeyboard()->KeyDown(KeyCodes::NUM8)) {
			selectionObject->GetPhysicsObject()->AddTorque(Vector3(0, -10, 0));
		}

		if (Window::GetKeyboard()->KeyDown(KeyCodes::RIGHT)) {
			selectionObject->GetPhysicsObject()->AddTorque(Vector3(10, 0, 0));
		}

		if (Window::GetKeyboard()->KeyDown(KeyCodes::UP)) {
			selectionObject->GetPhysicsObject()->AddForce(Vector3(0, 0, -10));
		}

		if (Window::GetKeyboard()->KeyDown(KeyCodes::DOWN)) {
			selectionObject->GetPhysicsObject()->AddForce(Vector3(0, 0, 10));
		}

		if (Window::GetKeyboard()->KeyDown(KeyCodes::NUM5)) {
			selectionObject->GetPhysicsObject()->AddForce(Vector3(0, -10, 0));
		}
	}
}
