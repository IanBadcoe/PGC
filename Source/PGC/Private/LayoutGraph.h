#pragma once

#include "Runtime/Core/Public/Templates/SharedPointer.h"
#include "Runtime/Core/Public/Templates/SharedPointer.h"

namespace LayoutGraph {
	// in all the following, TypeId is something like an enum class, for us as an Id for the types of connector

	template <typename TypeId> class Node;

	template <typename TypeId>
	class Edge {
		using NodeT = Node<TypeId>;

		TWeakPtr<NodeT> FromNode;
		TWeakPtr<NodeT> ToNode;
	};

	template <typename TypeId>
	class ConnectorDef {
	public:
		const TypeId Id;
		const TArray<FVector2D> Profile;		// x is L->R across the width of the incoming edge
												// y is D->U on it
		ConnectorDef(TypeId&& id, TArray<FVector2D>&& profile)
			: Id(id), 
			  Profile(profile) {}
		ConnectorDef() = delete;
		ConnectorDef(const ConnectorDef&) = delete;
		const ConnectorDef& operator=(const ConnectorDef&) = delete;
	};

	template <typename TypeId>
	class ConnectorInst {
	public:
		using ConnectorDefT = ConnectorDef<TypeId>;

		const ConnectorDefT& Definition;

		FVector Pos;			// centre of the connector in node-space
		FVector Normal;			// normal, pointing out of the node and into the edge

		ConnectorInst(const ConnectorDefT& definition, FVector&& pos, FVector&& normal)
			: Definition(definition), Pos(pos), Normal(normal) {}
	};

	template <typename TypeId>
	class Node {
	public:
		using EdgeT = Edge<TypeId>;
		using ConnectorT = ConnectorInst<TypeId>;

		TArray<TWeakPtr<EdgeT>> Edges;
		const TArray<ConnectorT> Connectors;

		Node(TArray<ConnectorT>&& connectors)
			: Connectors(connectors) {}
		Node() = delete;
		Node(const Node&) = delete;
		const Node& operator=(const Node&) = delete;
	};

	template <typename TypeId>
	class Graph {
		using NodeT = Node<TypeId>;
		using EdgeT = Edge<TypeId>;
		using ConnectorTypeT = ConnectorDef<TypeId>;

		TArray<TSharedPtr<NodeT>> Nodes;
		TArray<TSharedPtr<EdgeT>> Edges;
		TArray<ConnectorTypeT> ConnectorTypes;
	};
}

