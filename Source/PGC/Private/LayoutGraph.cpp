#include "LayoutGraph.h"

#include "Util.h"
#include "SplineUtil.h"

#pragma optimize ("", off)

namespace LayoutGraph {

BackToBack::BackToBack(const ConnectorDef& def)
	: Node({ MakeShared<ConnectorInst>(def, FVector{ 0, 0, 0 }, FVector{ 1, 0, 0 }, FVector{ 0, 0, 1 }),
			 MakeShared<ConnectorInst>(def, FVector{ 0, 0, 0 }, FVector{ -1, 0, 0 }, FVector{ 0, 0, 1 }),},
		   {},
		   {})
{
}

Node* BackToBack::FactoryMethod() const
{
	return new BackToBack(Connectors[0]->Definition);
}


Graph::Graph(float segLength) : SegLength(segLength)
{

}

void Graph::Connect(int nodeFrom, int nodeFromConnector, int nodeTo, int nodeToConnector,
	int divs /* = 10 */, int twists /* = 0 */)
{
	check(!Nodes[nodeFrom]->Edges[nodeFromConnector].IsValid());
	check(!Nodes[nodeTo]->Edges[nodeToConnector].IsValid());

	// for the moment, for simplicity, support only the same connector type at either end...
	check(&(Nodes[nodeFrom]->Connectors[nodeFromConnector]->Definition) == &(Nodes[nodeTo]->Connectors[nodeToConnector]->Definition));

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

void Node::AddToMesh(TSharedPtr<Mesh> mesh)
{
	for (const auto& pol : Polygons)
	{
		TArray<FVector> verts;

		for (const auto& idx : pol.VertexIndices)
		{
			verts.Add(GetTransformedVert(idx));
		}

		Util::AddPolyToMesh(mesh, verts);
	}

	// fill-in any connectors that aren't connected
	for (int i = 0; i < Connectors.Num(); i++)
	{
		if (!Edges[i].IsValid())
		{
			TArray<FVector> verts;
	
			for (int j = 0; j < Connectors[i]->Definition.Profile.Num(); j++)
			{
				verts.Add(GetTransformedVert({i, j}));
			}

			Util::AddPolyToMesh(mesh, verts);
		}
	}
}

FVector Node::GetTransformedVert(const Polygon::Idx& idx) const
{
	if (idx.Connector == -1)
	{
		return Position.TransformPosition(Vertices[idx.Vertex]);
	}
	else
	{
		return Connectors[idx.Connector]->GetTransformedVert(idx.Vertex, Position);
	}
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

inline ConnectorInst::ConnectorInst(const ConnectorDef& definition,
	const FVector& pos, const FVector& normal, const FVector& up)
	: Definition(definition)
{
	FVector right = FVector::CrossProduct(up, normal);

	// untransformed has the normal on X, the right on Y and Z up...
	Transform = FTransform(normal, right, up, pos);
}

FVector ConnectorInst::GetTransformedVert(int vert_idx, const FTransform& node_trans) const
{
	FTransform tot_trans = Transform * node_trans;

	return Definition.GetTransformedVert(vert_idx, tot_trans);
}

Edge::Edge(TWeakPtr<Node> fromNode, TWeakPtr<Node> toNode, TWeakPtr<ConnectorInst> fromConnector, TWeakPtr<ConnectorInst> toConnector)
	: FromNode(fromNode), ToNode(toNode),
	FromConnector(fromConnector), ToConnector(toConnector) {
	check(FromNode.Pin()->FindConnectorIdx(FromConnector.Pin()) != -1);
	check(ToNode.Pin()->FindConnectorIdx(ToConnector.Pin()) != -1);
}

FVector ConnectorDef::GetTransformedVert(int vert_idx, const FTransform& total_trans) const
{
	// an untransformed connector faces down X and it upright w.r.t Z
	// so its width (internal X) is mapped onto Y and its height (internal Y) onto Z
	FVector temp{ 0.0f, Profile[vert_idx].Position.X, Profile[vert_idx].Position.Y };

	return total_trans.TransformPosition(temp);
}

#pragma optimize ("", on)
 }