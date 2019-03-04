#pragma once

#include "Runtime/Core/Public/Templates/SharedPointer.h"
#include "Runtime/Core/Public/Templates/SharedPointer.h"

#include "Mesh.h"

namespace LayoutGraph {

class Node;
class Graph;
class ConnectorInst;

class Edge {
public:
	const TWeakPtr<Node> FromNode;
	const TWeakPtr<Node> ToNode;
	const TWeakPtr<ConnectorInst> FromConnector;
	const TWeakPtr<ConnectorInst> ToConnector;

	int Divs;					// how many points along the spline required for smoothness, swivels etc...
	int Twists;					// how many +ve or -ve half-twists to make along the edge

	// this is not a matter for layout....  some sort of edge type/style might be
	// and that might drive profile selection later...
	//// empty means use profile of start node
	//// otherwise divide these profiles evenly over the length of the connection
	//TArray <TSharedPtr<ParameterisedProfile>> Profiles;

	Edge(TWeakPtr<Node> fromNode, TWeakPtr<Node> toNode,
		TWeakPtr<ConnectorInst> fromConnector, TWeakPtr<ConnectorInst> toConnector);

	// Graph so we can use the indices of our nodes as their identities
	// if we ever need a hash in the absence of the graph, we may need to embed
	// the indices in the nodes
	uint32 GetTypeHash(const Graph& graph) const;
};

class ConnectorInst {
public:
	// this is not a matter for layout....  some sort of edge type/style might be
	// and that might drive profile selection later...
	//const TSharedPtr<ParameterisedProfile> Profile;

	FTransform Transform;

	ConnectorInst(const FVector& pos, FVector forward, FVector up);

	uint32 GetTypeHash() const {
		return Transform.ToMatrixWithScale().ComputeHash();
	}
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

	Node(int num_radial_connectors, const FVector& pos, const FVector& rot);

	Node() = delete;
	Node(const Node&) = delete;
	const Node& operator=(const Node&) = delete;
	virtual ~Node() = default;

	int FindConnectorIdx(const TWeakPtr<ConnectorInst>& conn) const;

	static const ConnectorArray MakeRadialConnectors(int count);

	// Graph so we can use the indices of our edges as their identities
	// if we ever need a hash in the absence of the graph, we may need to embed
	// the indices in the edges
	uint32 GetTypeHash(const Graph& g) const;
};

class Graph {
public:
	const float SegLength;

	Graph(float segLength);

	// connect "from" to "to" directly with an edge and no regard to geometry...
	void Connect(int nodeFrom, int nodeFromconnector,
		int nodeTo, int nodeToconnector,
		int divs = 10, int twists = 0);

	int FindNodeIdx(const TWeakPtr<Node>& node) const;
	int FindEdgeIdx(const TWeakPtr<Edge>& edge) const;

	const TArray<TSharedPtr<Node>>& GetNodes() const { return Nodes; }
	const TArray<TSharedPtr<Edge>>& GetEdges() const { return Edges; }

	uint32 GetTypeHash() const;

protected:
	TArray<TSharedPtr<Node>> Nodes;
	TArray<TSharedPtr<Edge>> Edges;
};

}
