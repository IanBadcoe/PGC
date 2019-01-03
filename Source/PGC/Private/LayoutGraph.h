#pragma once

#include "Runtime/Core/Public/Templates/SharedPointer.h"
#include "Runtime/Core/Public/Templates/SharedPointer.h"

#include "Mesh.h"
#include "NlOptWrapper.h"

namespace LayoutGraph {
	class Node;
	class ConnectorInst;

	class Edge {
	public:
		const TWeakPtr<Node> FromNode;
		const TWeakPtr<Node> ToNode;
		const TWeakPtr<ConnectorInst> FromConnector;
		const TWeakPtr<ConnectorInst> ToConnector;

		//float InSpeed;				// a measure of how straight we want to come out of the "from" connector
		//float OutSpeed;				// a measure of how straight we want to go into the "to" connector

		int Divs;					// how many points along the spline required for smoothness, swivels etc...
		int Twists;					// how many +ve or -ve half-twists to make along the edge

		Edge(TWeakPtr<Node> fromNode, TWeakPtr<Node> toNode,
			TWeakPtr<ConnectorInst> fromConnector, TWeakPtr<ConnectorInst> toConnector);

//		void AddToMesh(TSharedPtr<Mesh> mesh);
	};

	struct ProfileVert {
		FVector2D Position;
		PGCEdgeType FollowingEdgeType;

		ProfileVert(const FVector2D& position,
			PGCEdgeType followingEdgeType)
			: Position(position), FollowingEdgeType(followingEdgeType){}
	};

	class ConnectorDef {
	public:
		using ProfileArray = TArray<ProfileVert>;

		const int Id;
		const ProfileArray& Profile;		// x is L->R across the width of the incoming edge
											// y is D->U on it
											// rotate clockwise when looking from outside the Node

		// any identified type that reasonably casts to int
		template <typename IdType>
		ConnectorDef(IdType id, const ProfileArray& profile)
			: Id(static_cast<int>(id)),
			Profile(profile) {}
		ConnectorDef() = delete;
		ConnectorDef(const ConnectorDef&) = delete;
		const ConnectorDef& operator=(const ConnectorDef&) = delete;

		FVector GetTransformedVert(int vert_idx, const FTransform& total_trans) const;
		int NumVerts() const { return Profile.Num(); }
	};

	class ConnectorInst {
	public:
		const ConnectorDef& Definition;

		FTransform Transform;

		ConnectorInst(const ConnectorDef& definition, const FVector& pos, const FVector& normal, const FVector& up);

		FVector GetTransformedVert(int vert_idx, const FTransform& trans) const;
	};

	class Polygon {
	public:
		struct Idx {
			Idx(int c, int v) : Connector(c), Vertex(v) {}

			int Connector;		// index into the Node's Connector array, or -1 for the Node's own vertex array
			int Vertex;			// index into whichever vertex array we specified above
		};

		const TArray<Idx> VertexIndices;

		Polygon(TArray<Idx>&& vert_idxs)
			: VertexIndices(vert_idxs) {}
		Polygon(std::initializer_list<Idx>&& vert_idxs)
			: VertexIndices(vert_idxs) {}
	};

	class Node {
	public:
		using ConnectorArray = TArray<TSharedRef<ConnectorInst>>;
		using VertexArray = TArray<FVector>;
		using PolygonArray = TArray<Polygon>;

		TArray<TWeakPtr<Edge>> Edges;
		const ConnectorArray Connectors;
		const VertexArray Vertices;
		const PolygonArray Polygons;

		FTransform Position;

		Node(const ConnectorArray& connectors,
			 const VertexArray& vertices,
			 const PolygonArray& polygons)
			: Connectors(connectors),
		      Vertices(vertices),
		      Polygons(polygons)
		{
			// every connector is potentially the start of an edge
			Edges.AddDefaulted(Connectors.Num());
		}

		Node() = delete;
		Node(const Node&) = delete;
		const Node& operator=(const Node&) = delete;
		virtual ~Node() = default;

		// still in use temporarily until I do node meshing properly in StructuralGraph
		void AddToMesh(TSharedPtr<Mesh> mesh);
		FVector GetTransformedVert(const Polygon::Idx& idx) const;

		virtual Node* FactoryMethod() const = 0;

		int FindConnectorIdx(const TSharedPtr<ConnectorInst>& conn) const;
	};

	// built-in node type, used to connect edges end-to-end when filling-out their geometry
	class BackToBack : public Node {
	public:
		BackToBack(const ConnectorDef& def);
		virtual ~BackToBack() = default;

		// Creates an empty Node of the same type...
		virtual Node* FactoryMethod() const override;

	private:
		static const ConnectorArray ConnectorData;
		static const VertexArray VertexData;
		static const PolygonArray PolygonData;
	};

	class Graph {
	public:
		const float SegLength;

		Graph(float segLength);

//		void MakeMesh(TSharedPtr<Mesh> mesh) const;
		// connect "from" to "to" directly with an edge and no regard to geometry...
		void Connect(int nodeFrom, int nodeFromconnector,
			int nodeTo, int nodeToconnector,
			int divs = 10, int twists = 0);

		int FindNodeIdx(const TSharedPtr<Node>& node) const;

		const TArray<TSharedPtr<Node>>& GetNodes() const { return Nodes; }
		const TArray<TSharedPtr<Edge>>& GetEdges() const { return Edges; }

	protected:
		TArray<TSharedPtr<Node>> Nodes;
		TArray<TSharedPtr<Edge>> Edges;
	};
}
