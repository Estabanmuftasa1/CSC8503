#include "NavigationMesh.h"
#include "Assets.h"
#include "Maths.h"
#include <fstream>
#include <queue>
#include <vector>
#include <limits>
#include <algorithm>
#include <Debug.h>
#include <iostream>
using namespace NCL;
using namespace CSC8503;
using namespace std;
//for the commit
NavigationMesh::NavigationMesh()
{
}

NavigationMesh::NavigationMesh(const std::string&filename)
{
	ifstream file(Assets::DATADIR + filename);
	Vector3 mn(
		std::numeric_limits<float>::infinity(),
		std::numeric_limits<float>::infinity(),
		std::numeric_limits<float>::infinity()
	);
	Vector3 mx(
		-std::numeric_limits<float>::infinity(),
		-std::numeric_limits<float>::infinity(),
		-std::numeric_limits<float>::infinity()
	);

	for (const auto& v : allVerts) {
		mn.x = std::min(mn.x, v.x); mn.y = std::min(mn.y, v.y); mn.z = std::min(mn.z, v.z);
		mx.x = std::max(mx.x, v.x); mx.y = std::max(mx.y, v.y); mx.z = std::max(mx.z, v.z);
	}

	std::cout << "[NAV] bounds min(" << mn.x << "," << mn.y << "," << mn.z
		<< ") max(" << mx.x << "," << mx.y << "," << mx.z << ")\n";


	int numVertices = 0;
	int numIndices	= 0;

	file >> numVertices;
	file >> numIndices;

	for (int i = 0; i < numVertices; ++i) {
		Vector3 vert;
		file >> vert.x;
		file >> vert.y;
		file >> vert.z;

		allVerts.emplace_back(vert);
	}

	allTris.resize(numIndices / 3);

	for (int i = 0; i < allTris.size(); ++i) {
		NavTri* tri = &allTris[i];
		file >> tri->indices[0];
		file >> tri->indices[1];
		file >> tri->indices[2];

		tri->centroid = allVerts[tri->indices[0]] +
			allVerts[tri->indices[1]] +
			allVerts[tri->indices[2]];

		tri->centroid = allTris[i].centroid / 3.0f;

		tri->triPlane = Plane::PlaneFromTri(allVerts[tri->indices[0]],
			allVerts[tri->indices[1]],
			allVerts[tri->indices[2]]);

		tri->area = Maths::AreaofTri3D(allVerts[tri->indices[0]], allVerts[tri->indices[1]], allVerts[tri->indices[2]]);
	}
	for (int i = 0; i < allTris.size(); ++i) {
		NavTri* tri = &allTris[i];
		for (int j = 0; j < 3; ++j) {
			int index = 0;
			file >> index;
			if (index != -1) {
				tri->neighbours[j] = &allTris[index];
			}
		}
	}
}

NavigationMesh::~NavigationMesh()
{
}

bool NavigationMesh::FindPath(const Vector3& from, const Vector3& to, NavigationPath& outPath) {
	outPath.Clear(); //extra security for DEBUG probably remove later
	const NavTri* start = GetTriForPosition(from);
	const NavTri* end = GetTriForPosition(to);

	if (!start || !end) {
		std::cout << "[NAV] FindPath FAIL start=" << (start ? "OK" : "NULL")
			<< " end=" << (end ? "OK" : "NULL")
			<< " from(" << from.x << "," << from.y << "," << from.z << ")"
			<< " to(" << to.x << "," << to.y << "," << to.z << ")\n";
		return false;
	}


	//straight line is ok
	if (start == end) {
		outPath.PushWaypoint(to);
		return true;
	}
	auto TriIndex = [&](const NavTri* t) -> int {
		return int(t - &allTris[0]);
		};

	const int startIdx = TriIndex(start);
	const int endIdx = TriIndex(end);
	const int triCount = (int)allTris.size();
	std::cout << "[NAV] startIdx=" << startIdx << " endIdx=" << endIdx << "\n";//debug cause the fucking navmesh is fuckingg up piece of shit


	std::vector<float> g(triCount, std::numeric_limits<float>::infinity());
	std::vector<float> f(triCount, std::numeric_limits<float>::infinity());
	std::vector<int>   parent(triCount, -1);
	std::vector<bool>  closed(triCount, false);


	// might want to tweak this
	auto Heuristic = [&](int triIdx) -> float {
		Vector3 d = allTris[triIdx].centroid - allTris[endIdx].centroid;
		return Vector::Length(d);
		};

	struct OpenNode {
		int   triIdx;
		float fScore;
	};

	struct Compare {
		bool operator()(const OpenNode& a, const OpenNode& b) const {
			return a.fScore > b.fScore; 
		}
	};

	std::priority_queue<OpenNode, std::vector<OpenNode>, Compare> open;

	//basic a* algorithm, not much here
	g[startIdx] = 0.0f;
	f[startIdx] = Heuristic(startIdx);
	open.push({ startIdx, f[startIdx] });

	while (!open.empty()) {
		const int current = open.top().triIdx;
		open.pop();

		if (closed[current]) {
			continue;
		}
		closed[current] = true;

		if (current == endIdx) {

			std::vector<int> triPath;
			for (int t = endIdx; t != -1; t = parent[t]) {
				triPath.push_back(t);
			}
			std::reverse(triPath.begin(), triPath.end());
			outPath.Clear();

			outPath.PushWaypoint(to);

			for (int i = (int)triPath.size() - 1; i >= 1; --i) { 
				outPath.PushWaypoint(allTris[triPath[i]].centroid);
			}

			return true;

		}

		const NavTri* curTri = &allTris[current];

		for (int edge = 0; edge < 3; ++edge) {
			const NavTri* nb = curTri->neighbours[edge];
			if (!nb) continue;

			const int nbIdx = TriIndex(nb);
			if (closed[nbIdx]) continue;

			Vector3 d = nb->centroid - curTri->centroid;
			const float stepCost = Vector::Length(d);

			const float tentativeG = g[current] + stepCost;
			if (tentativeG < g[nbIdx]) {
				parent[nbIdx] = current;
				g[nbIdx] = tentativeG;
				f[nbIdx] = tentativeG + Heuristic(nbIdx);
				open.push({ nbIdx, f[nbIdx] });
			}
		}
	}



	std::vector<char> seen(triCount, 0);
	std::queue<int> q;
	seen[startIdx] = 1;
	q.push(startIdx);

	int visited = 0;
	bool canReachEnd = false;

	while (!q.empty()) {
		int t = q.front(); q.pop();
		visited++;

		if (t == endIdx) { canReachEnd = true; break; }

		const NavTri* tri = &allTris[t];
		for (int edge = 0; edge < 3; ++edge) {
			const NavTri* nb = tri->neighbours[edge];
			if (!nb) continue;

			int nbIdx = int(nb - &allTris[0]);
			if (nbIdx < 0 || nbIdx >= triCount) continue;

			if (!seen[nbIdx]) {
				seen[nbIdx] = 1;
				q.push(nbIdx);
			}
		}
	}


	// another debug in case this shit fails
	std::cout << "[NAV] A* failed. visited=" << visited
		<< " / " << triCount
		<< " reachEnd=" << (canReachEnd ? "YES" : "NO")
		<< "\n";


	return false; // cannoit find path
}

// should work for 3d levels as long as good navmesh exists

const NavigationMesh::NavTri* NavigationMesh::GetTriForPosition(const Vector3& pos) const {
	for (const NavTri& t : allTris) {
		Vector3 planePoint = t.triPlane.ProjectPointOntoPlane(pos);

		float ta = Maths::AreaofTri3D(allVerts[t.indices[0]], allVerts[t.indices[1]], planePoint);
		float tb = Maths::AreaofTri3D(allVerts[t.indices[1]], allVerts[t.indices[2]], planePoint);
		float tc = Maths::AreaofTri3D(allVerts[t.indices[2]], allVerts[t.indices[0]], planePoint);

		float areaSum = ta + tb + tc;

		if (abs(areaSum - t.area) > 0.05f) continue;

		return &t;
	}
	return nullptr;
}

bool NavigationMesh::DebugHasTriForPosition(const Vector3& pos) const {
	return GetTriForPosition(pos) != nullptr;
}// debug for navmesh
