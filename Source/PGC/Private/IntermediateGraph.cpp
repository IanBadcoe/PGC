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
IGraph<NM, GM>::IGraph(const IGraph& rhs)
	: GraphMD(rhs.GraphMD)
{
	for (const auto& n : rhs.Nodes)
	{
		Nodes.Emplace(MakeShared<INode>());
		Nodes.Last()->Position = n->Position;
		Nodes.Last()->Radius = n->Radius;
		Nodes.Last()->Reference = n->Reference;
	}

	for (const auto& e : rhs.Edges)
	{
		auto ifrom = rhs.FindNodeIdx(e->FromNode);
		auto ito = rhs.FindNodeIdx(e->ToNode);

		Connect(Nodes[ifrom], Nodes[ito], e->D0);
	}
}

template<typename NM, typename GM>
void IGraph<NM, GM>::Connect(const TSharedPtr<INode> n1, const TSharedPtr<INode> n2, double D)
{
	Edges.Add(MakeShared<IEdge>(n1, n2, D));

	// edges are no particular order in nodes
	n1->Edges.Add(Edges.Last());
	n2->Edges.Add(Edges.Last());
}

static FVector RandomPointInBox(const FBox& box) {
	auto max = box.Max;
	auto min = box.Min;

	return FVector(
		FMath::RandRange(min.X, max.X),
		FMath::RandRange(min.Y, max.Y),
		FMath::RandRange(min.Z, max.Z));
}

template<typename NM, typename GM>
TSharedPtr<IGraph<NM, GM>> IGraph<NM, GM>::Randomize(const FBox & box) const
{
	auto ret = MakeShared<IGraph>(*this);

	for (auto& n : Nodes)
	{
		n->Position = RandomPointInBox(box);
	}

	return ret;
}

template<typename NM, typename GM>
FBox IGraph<NM, GM>::CalcBoundingBox() const
{
	FBox ret(EForceInit::ForceInit);

	for (const auto& n : Nodes)
	{
		ret += n->Position;
	}

	return ret;
}

template<typename NM, typename GM>
int IGraph<NM, GM>::FindNodeIdx(const TWeakPtr<INode>& node) const
{
	for (int i = 0; i < Nodes.Num(); i++)
	{
		if (Nodes[i] == node)
		{
			return i;
		}
	}

	return -1;
}

template<typename NM>
IEdge<NM>::IEdge(TWeakPtr<INode> fromNode, TWeakPtr<INode> toNode, double d0)
	: FromNode(fromNode), ToNode(toNode), D0(d0) {
}

}
PRAGMA_ENABLE_OPTIMIZATION

