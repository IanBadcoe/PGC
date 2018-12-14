#pragma once

#include "Runtime/Core/Public/Templates/SharedPointer.h"
#include "Runtime/Core/Public/Templates/SharedPointer.h"

#include "Mesh.h"

namespace LayoutGraph {
	class Node;

	class Edge {
		TWeakPtr<Node> FromNode;
		TWeakPtr<Node> ToNode;
	};

	class ConnectorDef {
	public:
		const int Id;
		const TArray<FVector2D>& Profile;		// x is L->R across the width of the incoming edge
												// y is D->U on it
												// rotate clockwise when looking from outside the Node

		// any identified type that reasonably casts to int
		template <typename IdType>
		ConnectorDef(IdType id, const TArray<FVector2D>& profile)
			: Id(static_cast<int>(id)),
			Profile(profile) {}
		ConnectorDef() = delete;
		ConnectorDef(const ConnectorDef&) = delete;
		const ConnectorDef& operator=(const ConnectorDef&) = delete;
	};

	class ConnectorInst {
	public:
		const ConnectorDef& Definition;

		FTransform Transform;

		ConnectorInst(const ConnectorDef& definition, const FVector& pos, const FVector& normal, const FVector& up);
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
		using ConnectorArray = TArray<ConnectorInst>;
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

		void AddToMesh(TSharedPtr<Mesh> mesh);
		FVector GetTransformedVert(const Polygon::Idx& idx);
	};

	class Graph {
	public:
		void MakeMesh(TSharedPtr<Mesh> mesh) const;

	protected:
		TArray<TSharedPtr<Node>> Nodes;
		TArray<TSharedPtr<Edge>> Edges;
		TArray<ConnectorDef> ConnectorTypes;
	};
}
