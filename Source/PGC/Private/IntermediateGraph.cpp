#include "IntermediateGraph.h"

#include "Util.h"

PRAGMA_DISABLE_OPTIMIZATION

namespace IntermediateGraph
{

template<typename NM, typename GM>
IGraph<NM, GM>::IGraph()
{
}

template<typename NM, typename GM>
void IGraph<NM, GM>::Connect(const TSharedPtr<INode<NM>> n1, const TSharedPtr<INode<NM>> n2, double D)
{
	Edges.Add(MakeShared<IEdge<NM>>(n1, n2, D));

	// edges are no particular order in nodes
	n1->Edges.Add(Edges.Last());
	n2->Edges.Add(Edges.Last());
}

template<typename NM>
IEdge<NM>::IEdge(TWeakPtr<INode<NM>> fromNode, TWeakPtr<INode<NM>> toNode, double d0)
	: FromNode(fromNode), ToNode(toNode), D0(d0) {
}

}
PRAGMA_ENABLE_OPTIMIZATION

