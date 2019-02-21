#pragma once

#include "Runtime/Core/Public/Templates/SharedPointer.h"

namespace IntermediateGraph {
	template <typename T>
	class INode;

	template <typename T>
	class IEdge {
	public:
		TWeakPtr<INode<T>> FromNode;
		TWeakPtr<INode<T>> ToNode;

		const double D0;

		IEdge(TWeakPtr<INode<T>> fromNode, TWeakPtr<INode<T>> toNode, double d0);

		TWeakPtr<INode<T>> OtherNode(const INode<T>* n) const
		{
			if (FromNode.Pin().Get() == n)
			{
				return ToNode;
			}

			check(n == ToNode.Pin().Get());
			return FromNode;
		}

		TWeakPtr<INode<T>> OtherNode(const TWeakPtr<INode<T>>& n) const
		{
			return OtherNode(n.Pin().Get());
		}

		bool Contains(const TWeakPtr<INode<T>>& e)
		{
			return FromNode == e || ToNode == e;
		}
	};

	template <typename T>
	class INode {
	public:
		TArray<TWeakPtr<IEdge<T>>> Edges;

		float Radius = 0.0f;

		FVector Position;

		T Reference;

		INode(float radius, const FVector& pos,
			T ref) : Radius(radius), Position(pos), Reference(ref) {}

		INode(const INode&) = delete;
		const INode& operator=(const INode&) = delete;
		virtual ~INode() = default;
	};

	template <typename T>
	class IGraph {
	public:
		IGraph();

		// connect "from" to "to" directly with an edge and no regard to geometry...
		void Connect(const TSharedPtr<INode<T>> n1, const TSharedPtr<INode<T>> n2, double D0);
		// connect "from" to "to" via "Divs" intermediate back-to-back nodes
		
		TArray<TSharedPtr<INode<T>>> Nodes;
		TArray<TSharedPtr<IEdge<T>>> Edges;
	};
}
