#pragma once

#include "Runtime/Core/Public/Templates/SharedPointer.h"
#include "Runtime/Core/Public/Templates/SharedPointer.h"

namespace GraphLayout {
	// in all the following, TypeId is something like an enum class, for us as an Id for the types of connector

	template <typename TypeId> class Node;

	template <typename TypeId>
	class Edge {
		using NodeT = Node<TypeId>;

		TWeakPtr<NodeT> FromNode;
		TWeakPtr<NodeT> ToNode;
	};

	template <typename TypeId>
	class Connector {
	public:
		const TypeId Id;
		const TArray<FVector2D> Profile;		// x is L->R across the width of the incoming edge
												// y is D->U on it
		const FVector Pos;
		const FRotator Rot;

		Connector(TypeId&& id, TArray<FVector2D>&& profile,
			FVector&& pos, FRotator&& rot)
			: Id(id), 
			  Profile(profile), 
			  Pos(pos),
		      Rot(rot) {}
	};

	template <typename TypeId>
	class Node {
	public:
		using EdgeT = Edge<TypeId>;
		using ConnectorT = Connector<TypeId>;

		TArray<TWeakPtr<EdgeT>> Edges;
		const TArray<TUniquePtr<ConnectorT>> Connectors;

		Node(TArray<TUniquePtr<ConnectorT>>&& connectors)
			: Connectors(connectors) {}
	};

	template <typename TypeId>
	class Graph {
		using NodeT = Node<TypeId>;
		using EdgeT = Edge<TypeId>;
		using ConnectorTypeT = Connector<TypeId>;

		TArray<TSharedPtr<NodeT>> Nodes;
		TArray<TSharedPtr<EdgeT>> Edges;
		TArray<ConnectorTypeT> ConnectorTypes;
	};
}

