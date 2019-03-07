#pragma once

#include "Runtime/Core/Public/Templates/SharedPointer.h"

namespace IntermediateGraph {
	template <typename NM>
	class INode;

	template <typename NM, typename GM>
	class IGraph;

	template <typename NM>
	class IEdge {
	public:
		using INode = INode<NM>;

		TWeakPtr<INode> FromNode;
		TWeakPtr<INode> ToNode;

		double D0;

		IEdge() : D0(0) {}
		IEdge(const TWeakPtr<INode>& fromNode, const TWeakPtr<INode>& toNode, double d0);

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
		NM MD;

		INode(float radius, const FVector& pos, const NM& md) : Radius(radius), Position(pos), MD(md) {}
		INode(const INode&) = delete;

		const INode& operator=(const INode&) = delete;
		virtual ~INode() = default;

		void Serialize(FArchive& Ar);
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
		void Connect(const TSharedPtr<INode>& from_n, const TSharedPtr<INode>& to_n, double D0);
		// as above but when the edge already exists (happens in serialization)
		void Connect(const TSharedPtr<IEdge>& e, const TSharedPtr<INode>& from_n, const TSharedPtr<INode>& to_n, double D0);

		TArray<TSharedPtr<INode>> Nodes;
		TArray<TSharedPtr<IEdge>> Edges;

		GM MD;

		// randomizes all node positions within box
		TSharedPtr<IGraph> Randomize(const FBox& box, FRandomStream& rand_stream) const;

		FBox CalcBoundingBox() const;

		int FindNodeIdx(const TWeakPtr<INode>& node) const;

		void Serialize(FArchive& Ar);
	};

	template<typename NM>
	inline void INode<NM>::Serialize(FArchive& Ar)
	{
		Ar << Radius;

		Ar << Position;

		Ar << MD;
	}

	template<typename NM, typename GM>
	inline void SerializeEdge(TSharedPtr<IEdge<NM>> edge, FArchive& Ar, IGraph<NM, GM>& graph)
	{
		Ar << edge->D0;

		int fi, ti;


		if (Ar.IsSaving())
		{
			fi = graph.FindNodeIdx(edge->FromNode);
			ti = graph.FindNodeIdx(edge->ToNode);
		}

		Ar << fi;
		Ar << ti;

		if (!Ar.IsSaving())
		{
			graph.Connect(edge, graph.Nodes[fi], graph.Nodes[ti], edge->D0);
		}
	}
}
