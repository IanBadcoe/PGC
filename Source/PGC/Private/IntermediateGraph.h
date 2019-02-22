#pragma once

#include "Runtime/Core/Public/Templates/SharedPointer.h"

namespace IntermediateGraph {
	template <typename NM>
	class INode;

	template <typename NM>
	class IEdge {
	public:
		using INode = INode<NM>;

		TWeakPtr<INode> FromNode;
		TWeakPtr<INode> ToNode;

		const double D0;

		IEdge(TWeakPtr<INode> fromNode, TWeakPtr<INode> toNode, double d0);

		TWeakPtr<INode> OtherNode(const INode* n) const
		{
			if (FromNode.Pin().Get() == n)
			{
				return ToNode;
			}

			check(n == ToNode.Pin().Get());
			return FromNode;
		}

		TWeakPtr<INode> OtherNode(const TWeakPtr<INode>& n) const
		{
			return OtherNode(n.Pin().Get());
		}

		bool Contains(const TWeakPtr<INode>& e)
		{
			return FromNode == e || ToNode == e;
		}
	};

	template <typename NM>
	class INode {
	public:
		using IEdge = IEdge<NM>;

		INode() = default;

		TArray<TWeakPtr<IEdge>> Edges;

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
		using INode = INode<NM>;
		using IEdge = IEdge<NM>;

		IGraph();
		IGraph(const IGraph& rhs);

		// connect "from" to "to" directly with an edge and no regard to geometry...
		void Connect(const TSharedPtr<INode> n1, const TSharedPtr<INode> n2, double D0);
		// connect "from" to "to" via "Divs" intermediate back-to-back nodes
		
		TArray<TSharedPtr<INode>> Nodes;
		TArray<TSharedPtr<IEdge>> Edges;

		GM GraphMD;

		// randomizes all node positions within box
		TSharedPtr<IGraph> Randomize(const FBox& box) const;

		FBox CalcBoundingBox() const;

		int FindNodeIdx(const TWeakPtr<INode>& node) const;

	};
}
