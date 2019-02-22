#pragma once

#include "Runtime/Core/Public/Templates/SharedPointer.h"

namespace IntermediateGraph {
	template <typename NM>
	class INode;

	template <typename NM>
	class IEdge {
	public:
		TWeakPtr<INode<NM>> FromNode;
		TWeakPtr<INode<NM>> ToNode;

		const double D0;

		IEdge(TWeakPtr<INode<NM>> fromNode, TWeakPtr<INode<NM>> toNode, double d0);

		TWeakPtr<INode<NM>> OtherNode(const INode<NM>* n) const
		{
			if (FromNode.Pin().Get() == n)
			{
				return ToNode;
			}

			check(n == ToNode.Pin().Get());
			return FromNode;
		}

		TWeakPtr<INode<NM>> OtherNode(const TWeakPtr<INode<NM>>& n) const
		{
			return OtherNode(n.Pin().Get());
		}

		bool Contains(const TWeakPtr<INode<NM>>& e)
		{
			return FromNode == e || ToNode == e;
		}
	};

	template <typename NM>
	class INode {
	public:
		TArray<TWeakPtr<IEdge<NM>>> Edges;

		float Radius = 0.0f;

		FVector Position;

		NM Reference;

		INode(float radius, const FVector& pos,
			NM ref) : Radius(radius), Position(pos), Reference(ref) {}

		INode(const INode&) = delete;
		const INode& operator=(const INode&) = delete;
		virtual ~INode() = default;
	};

	// NM is "NodeMetadata" to be stored on each node
	// GM is "GraphMetadata" and an array of that is stored for the whole graph
	template <typename NM, typename GM>
	class IGraph {
	public:
		IGraph();

		// connect "from" to "to" directly with an edge and no regard to geometry...
		void Connect(const TSharedPtr<INode<NM>> n1, const TSharedPtr<INode<NM>> n2, double D0);
		// connect "from" to "to" via "Divs" intermediate back-to-back nodes
		
		TArray<TSharedPtr<INode<NM>>> Nodes;
		TArray<TSharedPtr<IEdge<NM>>> Edges;

		TArray<GM> IntermediatePoints;
	};
}
