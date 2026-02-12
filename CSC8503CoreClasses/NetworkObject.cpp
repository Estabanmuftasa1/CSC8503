#include "NetworkObject.h"
#include "./enet/enet.h"

using namespace NCL;
using namespace CSC8503;

NetworkObject::NetworkObject(GameObject& o, int id) : object(o)	{
	deltaErrors = 0;
	fullErrors  = 0;
	networkID   = id;
}

NetworkObject::~NetworkObject()	{
}

bool NetworkObject::ReadPacket(GamePacket& p) {
	if (p.type == Delta_State)
		return ReadDeltaPacket((DeltaPacket&)p);
	if (p.type == Full_State)
		return ReadFullPacket((FullPacket&)p);
	return false;
}


bool NetworkObject::WritePacket(GamePacket** p, bool deltaFrame, int stateID) {
	if (deltaFrame) {
		if (!WriteDeltaPacket(p, stateID)) {
			return WriteFullPacket(p);
		}
	}
	return WriteFullPacket(p);
}
//Client objects recieve these packets
bool NetworkObject::ReadDeltaPacket(DeltaPacket& p) {
	if (p.fullID != lastFullState.stateID)
		return false;

	UpdateStateHistory(p.fullID);

	Vector3 pos = lastFullState.position;
	Quaternion q = lastFullState.orientation;

	pos.x += p.pos[0];
	pos.y += p.pos[1];
	pos.z += p.pos[2];

	q.x += p.orientation[0] / 127.0f;
	q.y += p.orientation[1] / 127.0f;
	q.z += p.orientation[2] / 127.0f;
	q.w += p.orientation[3] / 127.0f;

	object.GetTransform().SetPosition(pos);
	object.GetTransform().SetOrientation(q);
	return true;
}

bool NetworkObject::ReadFullPacket(FullPacket& p) {
	if (p.fullState.stateID < lastFullState.stateID)
		return false;

	lastFullState = p.fullState;
	object.GetTransform().SetPosition(lastFullState.position);
	object.GetTransform().SetOrientation(lastFullState.orientation);

	stateHistory.push_back(lastFullState);
	return true;
}


bool NetworkObject::WriteDeltaPacket(GamePacket** p, int stateID) {
	NetworkState base;
	if (!GetNetworkState(stateID, base)) {
		return false;
	}

	DeltaPacket* dp = new DeltaPacket();
	dp->fullID = stateID;
	dp->objectID = networkID;

	Vector3 pos = object.GetTransform().GetPosition() - base.position;
	Quaternion q = object.GetTransform().GetOrientation() - base.orientation;

	dp->pos[0] = (char)pos.x;
	dp->pos[1] = (char)pos.y;
	dp->pos[2] = (char)pos.z;

	dp->orientation[0] = (char)(q.x * 127.0f);
	dp->orientation[1] = (char)(q.y * 127.0f);
	dp->orientation[2] = (char)(q.z * 127.0f);
	dp->orientation[3] = (char)(q.w * 127.0f);

	*p = dp;
	return true;
}


bool NetworkObject::WriteFullPacket(GamePacket** p) {
	FullPacket* fp = new FullPacket();

	fp->objectID = networkID;
	fp->fullState.position = object.GetTransform().GetPosition();
	fp->fullState.orientation = object.GetTransform().GetOrientation();
	fp->fullState.stateID = ++lastFullState.stateID;

	*p = fp;
	return true;
}


NetworkState& NetworkObject::GetLatestNetworkState() {
	return lastFullState;
}

bool NetworkObject::GetNetworkState(int id, NetworkState& out) {
	for (auto& s : stateHistory) {
		if (s.stateID == id) {
			out = s;
			return true;
		}
	}
	return false;
}


void NetworkObject::UpdateStateHistory(int minID) {
	for (auto i = stateHistory.begin(); i != stateHistory.end();) {
		if (i->stateID < minID)
			i = stateHistory.erase(i);
		else
			++i;
	}
}

