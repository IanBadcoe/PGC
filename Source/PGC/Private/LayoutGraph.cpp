#include "LayoutGraph.h"

#include "Util.h"
#include "SplineUtil.h"

PRAGMA_DISABLE_OPTIMIZATION

using namespace LayoutGraph;

BackToBack::BackToBack(const FVector& pos, const FVector& rot)
	: Node({ MakeShared<ConnectorInst>(FVector{ 0, 3, 0 }, FVector{ 1, 0, 0 }, FVector{ 0, 0, 1 }),
			 MakeShared<ConnectorInst>(FVector{ 0, -3, 0 }, FVector{ -1, 0, 0 }, FVector{ 0, 0, 1 }),},
		pos, rot)
{
}


Graph::Graph(float segLength) : SegLength(segLength)
{

}

void Graph::Connect(int nodeFrom, int nodeFromConnector, int nodeTo, int nodeToConnector,
	int divs /* = 10 */, int twists /* = 0 */)
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

Node::Node(int num_radial_connectors, const FVector& pos, const FVector& rot)
	: Node(MakeRadialConnectors(num_radial_connectors), pos, rot)
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

const Node::ConnectorArray Node::MakeRadialConnectors(int num)
{
	auto assumed_profile_radius = 2.0f;

	// A regular polygon with this many lines of assumed_profile_radius lines around it looks like this:
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
	// and the angle A is half that.  The adjacent is assumed_profile_radius
	// so the line from the centre to the mid-point of the profile is of length:
	// assumed_profile_radius * tan((180 - 180 / N)/2) (in degrees)
	//
	// and throw in an extra 10% to allow for some space if the profile is oriented straight along its radius

	auto corner_angle = PI - 2 * PI / num;
	auto node_radius = assumed_profile_radius * FMath::Tan(corner_angle / 2) * 1.1f;
	auto angle_step = 2 * PI / num;
	auto angle = 0.0f;

	ConnectorArray ret;

	for (int i = 0; i < num; i++)
	{
		ret.Emplace(MakeShared<ConnectorInst>(
			FVector{ FMath::Cos(angle) * node_radius, FMath::Sin(angle) * node_radius, 0 },
			FVector{ FMath::Cos(angle), FMath::Sin(angle), 0 },
			FVector{ 0, 0, 1 }));

		angle += angle_step;
	}

	return ret;
}

inline ConnectorInst::ConnectorInst(const FVector& pos, FVector forward, FVector up)
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

PRAGMA_ENABLE_OPTIMIZATION