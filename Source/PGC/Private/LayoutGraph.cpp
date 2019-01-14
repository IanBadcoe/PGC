#include "LayoutGraph.h"

#include "Util.h"
#include "SplineUtil.h"

#pragma optimize ("", off)

namespace LayoutGraph {

const bool ParameterisedProfile::UsesBarrierHeight[ParameterisedProfile::NumVerts]{
	false, true, true, true, true, false,
	false, true, true, true, true, false,
	false, true, true, true, true, false,
	false, true, true, true, true, false,
};

const bool ParameterisedProfile::UsesOverhangWidth[ParameterisedProfile::NumVerts]{
	false, false, true, true, false, false,
	false, false, true, true, false, false,
	false, false, true, true, false, false,
	false, false, true, true, false, false,
};

const bool ParameterisedProfile::IsXOuter[ParameterisedProfile::NumVerts]{
	false, false, false, false, true, true,
	true, true, false, false, false, false,
	false, false, false, false, true, true,
	true, true, false, false, false, false,
};

const bool ParameterisedProfile::IsYOuter[ParameterisedProfile::NumVerts]{
	false, false, false, true, true, false,
	false, true, true, false, false, false,
	false, false, false, true, true, false,
	false, true, true, false, false, false,
};



BackToBack::BackToBack(const TSharedPtr<ParameterisedProfile>& profile)
	: Node({ MakeShared<ConnectorInst>(profile, FVector{ 0, 0, 0 }, FVector{ 1, 0, 0 }, FVector{ 0, 0, 1 }),
			 MakeShared<ConnectorInst>(profile, FVector{ 0, 0, 0 }, FVector{ -1, 0, 0 }, FVector{ 0, 0, 1 }),})
{
}


Graph::Graph(float segLength) : SegLength(segLength)
{

}

void Graph::Connect(int nodeFrom, int nodeFromConnector, int nodeTo, int nodeToConnector,
	int divs /* = 10 */, int twists /* = 0 */,
	const TArray<TSharedPtr<ParameterisedProfile>>& profiles)
{
	check(!Nodes[nodeFrom]->Edges[nodeFromConnector].IsValid());
	check(!Nodes[nodeTo]->Edges[nodeToConnector].IsValid());

	auto edge = MakeShared<Edge>(Nodes[nodeFrom], Nodes[nodeTo],
		Nodes[nodeFrom]->Connectors[nodeFromConnector], Nodes[nodeTo]->Connectors[nodeToConnector]);

	Edges.Add(edge);

	Nodes[nodeFrom]->Edges[nodeFromConnector] = edge;
	Nodes[nodeTo]->Edges[nodeToConnector] = edge;

	edge->Divs = divs;
	edge->Twists = twists;
	edge->Profiles = profiles;
}

int Graph::FindNodeIdx(const TSharedPtr<Node>& node) const
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

int Node::FindConnectorIdx(const TSharedPtr<ConnectorInst>& conn) const
{
	for (int i = 0; i < Connectors.Num(); i++)
	{
		if (Connectors[i] == conn)
		{
			return i;
		}
	}

	return -1;
}

inline ConnectorInst::ConnectorInst(const TSharedPtr<ParameterisedProfile>& profile,
	const FVector& pos, FVector forward, FVector up)
	: Profile(profile)
{
	// untransformed has the normal on X, the right on Y and Z up...
	Transform = Util::MakeTransform(pos, up, forward);
}

Edge::Edge(TWeakPtr<Node> fromNode, TWeakPtr<Node> toNode, TWeakPtr<ConnectorInst> fromConnector, TWeakPtr<ConnectorInst> toConnector)
	: FromNode(fromNode), ToNode(toNode),
	FromConnector(fromConnector), ToConnector(toConnector) {
	check(FromNode.Pin()->FindConnectorIdx(FromConnector.Pin()) != -1);
	check(ToNode.Pin()->FindConnectorIdx(ToConnector.Pin()) != -1);
}

void ParameterisedProfile::CheckConsistent() const {
	check(Width > 0);

	for (int i = 0; i < 4; i++)
	{
		// no -ve values
		check(OverhangWidths[i] >= 0);
		check(BarrierHeights[i] >= 0);

		// cannot have an overhang unless the barrier is high enough
		check(OverhangWidths[i] == 0 || BarrierHeights[i] >= 1);

		const auto other_half_idx = i ^ 1;

		// the overhangs do not collide
		check(OverhangWidths[i] + OverhangWidths[other_half_idx] < Width || FMath::Abs(BarrierHeights[i] - BarrierHeights[other_half_idx]) >= 1);
	}
}

FVector2D ParameterisedProfile::GetPoint(int idx) const {
	const auto quarter = GetQuarterIdx(idx);

	float x{ Width / 2 };
	float y{ 0.5f };

	if (UsesBarrierHeight[idx])
		y += BarrierHeights[quarter];

	if (IsXOuter[idx] && BarrierHeights[quarter] > 0)
		x += 1;

	if (UsesOverhangWidth[idx])
		x -= OverhangWidths[quarter];

	if (IsYOuter[idx] && OverhangWidths[quarter])
		y += 1;

	if (quarter > 1)
		x = -x;

	if (quarter == 1 || quarter == 2)
		y = -y;

	return FVector2D{ x, y };
}

FVector ParameterisedProfile::GetTransformedVert(int vert_idx, const FTransform & trans) const
{
	const auto point = GetPoint(vert_idx);

	// an untransformed connector faces down X and it upright w.r.t Z
	// so its width (internal X) is mapped onto Y and its height (internal Y) onto Z
	FVector temp{ 0.0f, point.X, point.Y };

	return trans.TransformPosition(temp);
}

FVector ParameterisedProfile::GetTransformedVert(VertTypes type, int quarter_idx, const FTransform & trans) const
{
	int idx = quarter_idx * VertsPerQuarter;

	if (quarter_idx & 1)
	{
		idx = idx + 5 - (int)type;
	}
	else
	{
		idx = idx + (int)type;
	}

	return GetTransformedVert(idx, trans);
}

}

#pragma optimize ("", on)
