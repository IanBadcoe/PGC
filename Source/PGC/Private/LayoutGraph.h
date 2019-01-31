#pragma once

#include "Runtime/Core/Public/Templates/SharedPointer.h"
#include "Runtime/Core/Public/Templates/SharedPointer.h"

#include "Mesh.h"
#include "NlOptWrapper.h"
#include "ParameterisedProfile.h"

namespace LayoutGraph {

class Node;
class ConnectorInst;

using namespace Profile;

class Edge {
public:
	const TWeakPtr<Node> FromNode;
	const TWeakPtr<Node> ToNode;
	const TWeakPtr<ConnectorInst> FromConnector;
	const TWeakPtr<ConnectorInst> ToConnector;

	int Divs;					// how many points along the spline required for smoothness, swivels etc...
	int Twists;					// how many +ve or -ve half-twists to make along the edge

	// empty means use profile of start node
	// otherwise divide these profiles evenly over the length of the connection
	TArray <TSharedPtr<ParameterisedProfile>> Profiles;

	Edge(TWeakPtr<Node> fromNode, TWeakPtr<Node> toNode,
		TWeakPtr<ConnectorInst> fromConnector, TWeakPtr<ConnectorInst> toConnector);
};

class ConnectorInst {
public:
	const TSharedPtr<ParameterisedProfile> Profile;

	FTransform Transform;

	ConnectorInst(const TSharedPtr<ParameterisedProfile>& profile, const FVector& pos, FVector forward, FVector up);
};

class Node {
public:
	using ConnectorArray = TArray<TSharedRef<ConnectorInst>>;

	TArray<TWeakPtr<Edge>> Edges;
	const ConnectorArray Connectors;

	FTransform Transform;

	Node(const ConnectorArray& connectors, const FVector& pos, const FVector& rot)
		: Connectors(connectors)
	{
		Transform.SetLocation(pos);
		Transform.SetRotation(FQuat(FRotator(rot.Y, rot.Z, rot.X)));
		// every connector is potentially the start of an edge
		Edges.AddDefaulted(Connectors.Num());
	}

	Node(const TArray<TSharedPtr<ParameterisedProfile>>& profiles, const FVector& pos, const FVector& rot);

	Node() = delete;
	Node(const Node&) = delete;
	const Node& operator=(const Node&) = delete;
	virtual ~Node() = default;

	int FindConnectorIdx(const TSharedPtr<ConnectorInst>& conn) const;

	static const ConnectorArray MakeConnectorsFromProfiles(const TArray<TSharedPtr<ParameterisedProfile>>& profiles);
};

// built-in node type, used to connect edges end-to-end when filling-out their geometry
class BackToBack : public Node {
public:
	BackToBack(const TSharedPtr<ParameterisedProfile>& profile, const FVector& pos, const FVector& rot);
	virtual ~BackToBack() = default;
};

class Graph {
public:
	const float SegLength;

	Graph(float segLength);

//		void MakeMesh(TSharedPtr<Mesh> mesh) const;
	// connect "from" to "to" directly with an edge and no regard to geometry...
	void Connect(int nodeFrom, int nodeFromconnector,
		int nodeTo, int nodeToconnector,
		int divs = 10, int twists = 0,
		const TArray<TSharedPtr<ParameterisedProfile>>& profiles = TArray<TSharedPtr<ParameterisedProfile>>());

	int FindNodeIdx(const TSharedPtr<Node>& node) const;

	const TArray<TSharedPtr<Node>>& GetNodes() const { return Nodes; }
	const TArray<TSharedPtr<Edge>>& GetEdges() const { return Edges; }

protected:
	TArray<TSharedPtr<Node>> Nodes;
	TArray<TSharedPtr<Edge>> Edges;
};

}
