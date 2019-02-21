#include "IntermediateGraph.h"

#include "Util.h"

PRAGMA_DISABLE_OPTIMIZATION

using namespace IntermediateGraph;

template<typename T>
IGraph<T>::IGraph()
{
}

template<typename T>
void IGraph<T>::Connect(const TSharedPtr<INode<T>> n1, const TSharedPtr<INode<T>> n2, double D)
{
	Edges.Add(MakeShared<IEdge<T>>(n1, n2, D));

	// edges are no particular order in nodes
	n1->Edges.Add(Edges.Last());
	n2->Edges.Add(Edges.Last());
}

template<typename T>
IEdge<T>::IEdge(TWeakPtr<INode<T>> fromNode, TWeakPtr<INode<T>> toNode, double d0)
	: FromNode(fromNode), ToNode(toNode), D0(d0) {
}

PRAGMA_ENABLE_OPTIMIZATION

