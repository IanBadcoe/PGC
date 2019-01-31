#include "LayoutGraph.h"

#include "Util.h"
#include "SplineUtil.h"

#pragma optimize ("", off)

using namespace LayoutGraph;

BackToBack::BackToBack(const TSharedPtr<ParameterisedProfile>& profile, const FVector& pos, const FVector& rot)
	: Node({ MakeShared<ConnectorInst>(profile, FVector{ 0, 3, 0 }, FVector{ 1, 0, 0 }, FVector{ 0, 0, 1 }),
			 MakeShared<ConnectorInst>(profile, FVector{ 0, -3, 0 }, FVector{ -1, 0, 0 }, FVector{ 0, 0, 1 }),},
		pos, rot)
{
}


Graph::Graph(float segLength) : SegLength(segLength)
{

}

void Graph::Connect(int nodeFrom, int nodeFromConnector, int nodeTo, int nodeToConnector,
	int divs /* = 10 */, int twists /* = 0 */,
	const TArray<TSharedPtr<ParameterisedProfile>>& profiles /* = {} */)
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

Node::Node(const TArray<TSharedPtr<ParameterisedProfile>>& profiles, const FVector& pos, const FVector& rot)
	: Node(MakeConnectorsFromProfiles(profiles), pos, rot)
{
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

const Node::ConnectorArray Node::MakeConnectorsFromProfiles(const TArray<TSharedPtr<ParameterisedProfile>>& profiles)
{
	auto max_profile_radius = 0.0f;

	// using the radius is an exaggeration, as that would only apply if the profile was rotated exactly so that its diagonal lies in the
	// node plane, however since the profile could rotate later, allowing for that would require dynamic re-calculation of this, so lets take the
	// extreme case for the moment...
	for (const auto& profile : profiles)
	{
		max_profile_radius = FMath::Max(max_profile_radius, profile->Radius());
	}

    // A regular polygon with this many lines max_profile_radius lines around it looks like this:
	//
	//  +---------------+
	//  |A\     |      X|
	//  |   \   |       |
	//  |     \ |       |
	//  |-------+-------|
	//  |       |       |
	//  |       |       |
	//  |       |       |
	//  +---------------+
	//
	// The angle X is (180 - 360 / N) / 2
	// and the angle A is half than.  The adjacent is max_profile_radius
	// so the line from the centre to the mid-point of the profile is of length:
	// max_profile_radius * tan((180 - 180 / N)/2) (in degrees)
	//
	// and throw in an extra 10% to allow for some space if the profile is oriented straight along its radius

	auto corner_angle = PI - 2 * PI / profiles.Num();
	auto node_radius = max_profile_radius * FMath::Tan(corner_angle / 2) * 1.1f;
	auto angle_step = 2 * PI / profiles.Num();
	auto angle = 0.0f;

	ConnectorArray ret;

	for (const auto& profile : profiles)
	{
		ret.Emplace(MakeShared<ConnectorInst>(profile, 
			FVector{FMath::Cos(angle) * node_radius, FMath::Sin(angle) * node_radius, 0},
			FVector{FMath::Cos(angle), FMath::Sin(angle), 0},
			FVector{0, 0, 1}));

		angle += angle_step;
	}

	return ret;
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

#pragma optimize ("", on)
