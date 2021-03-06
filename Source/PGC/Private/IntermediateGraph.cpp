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
	: MD(rhs.MD)
{
	for (const auto& n : rhs.Nodes)
	{
		Nodes.Emplace(MakeShared<INode>(n->Radius, n->Position, n->MD));
	}

	for (const auto& e : rhs.Edges)
	{
		auto ifrom = rhs.FindNodeIdx(e->FromNode);
		auto ito = rhs.FindNodeIdx(e->ToNode);

		Connect(Nodes[ifrom], Nodes[ito], e->D0);
	}
}

template<typename NM, typename GM>
void IGraph<NM, GM>::Connect(const TSharedPtr<INode>& from_n, const TSharedPtr<INode>& to_n, double D)
{
	Edges.Add(MakeShared<IEdge>());

	Connect(Edges.Last(), from_n, to_n, D);
}

template<typename NM, typename GM>
void IGraph<NM, GM>::Connect(const TSharedPtr<IEdge>& e, const TSharedPtr<INode>& from_n, const TSharedPtr<INode>& to_n, double D0)
{
	e->D0 = D0;
	e->FromNode = from_n;
	e->ToNode = to_n;

	// edges are no particular order in nodes
	from_n->Edges.Add(Edges.Last());
	to_n->Edges.Add(Edges.Last());
}

template<typename NM, typename GM>
TSharedPtr<IGraph<NM, GM>> IGraph<NM, GM>::Randomize(const FBox& box, FRandomStream& random_stream) const
{
	auto ret = MakeShared<IGraph>(*this);

	for (auto& n : Nodes)
	{
		n->Position = Util::RandPointInBox(box, random_stream);
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

template<typename NM, typename GM>
void IGraph<NM, GM>::Serialize(FArchive& Ar)
{
	auto num = Nodes.Num();

	Ar << num;

	while(num > Nodes.Num())
	{
		Nodes.Push(MakeShared<INode>());
	}

	for (auto& n : Nodes)
	{
		n->Serialize(Ar);
	}

	num = Edges.Num();

	Ar << num;

	while (num > Edges.Num())
	{
		Edges.Push(MakeShared<IEdge>());
	}

	for (auto& e : Edges)
	{
		SerializeEdge(e, Ar, *this);
	}
}

template<typename NM>
IEdge<NM>::IEdge(const TWeakPtr<INode>& fromNode, const TWeakPtr<INode>& toNode, double d0)
	: FromNode(fromNode), ToNode(toNode), D0(d0) {
}

}
PRAGMA_ENABLE_OPTIMIZATION

