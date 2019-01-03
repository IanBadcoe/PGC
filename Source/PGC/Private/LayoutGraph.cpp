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

//void Graph::MakeMesh(TSharedPtr<Mesh> mesh) const
//{
//	mesh->Clear();
//
//	for (const auto& node : Nodes)
//	{
//		node->AddToMesh(mesh);
//	}
//
//	for (const auto& edge : Edges)
//	{
//		edge->AddToMesh(mesh);
//	}
//}

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

//void Edge::AddToMesh(TSharedPtr<Mesh> mesh)
//{
//	// for the moment, for simplicity, support only the same connector type at either end...
//	auto from_c = FromConnector.Pin();
//	auto to_c = ToConnector.Pin();
//
//	// currently insist on same type of connector at either end
//	check(&from_c->Definition == &to_c->Definition);
//
//	auto from_n = FromNode.Pin();
//	auto to_n = ToNode.Pin();
//
//	FTransform from_trans = from_c->Transform * from_n->Position;
//	FTransform to_trans = to_c->Transform * to_n->Position;
//
//	// "GetScaledAxis" should be giving us unit-vectors since we asked for a "NoScale" matrix
//
//	const auto from_forward = from_trans.ToMatrixNoScale().GetScaledAxis(EAxis::X);
//	const auto to_forward = -to_trans.ToMatrixNoScale().GetScaledAxis(EAxis::X);
//
//	const auto in_dir = from_forward * InSpeed;
//	const auto out_dir = to_forward * OutSpeed;
//
//	const auto in_pos = from_trans.GetLocation();
//	const auto out_pos = to_trans.GetLocation();
//
//	const auto from_up = from_trans.ToMatrixNoScale().GetScaledAxis(EAxis::Z);
//	const auto to_up = to_trans.ToMatrixNoScale().GetScaledAxis(EAxis::Z);
//
//	auto current_up = from_up;
//
//	TArray<FTransform> frames;
//
//	for (auto i = 0; i <= Divs; i++) {
//		float t = (float)i / Divs;
//
//		FVector pos = SplineUtil::Hermite(t, in_pos, out_pos, in_dir, out_dir);
//
//		FVector forward = SplineUtil::HermiteTangent(t, in_pos, out_pos, in_dir, out_dir);
//
//		forward.Normalize();
//
//		auto right = FVector::CrossProduct(forward, current_up);
//
//		right.Normalize();
//
//		current_up = FVector::CrossProduct(right, forward);
//
//		current_up.Normalize();
//
//		frames.Add(FTransform(forward, right, current_up, pos));
//	}
//
//	check(FVector::DotProduct(frames.Last().ToMatrixNoScale().GetScaledAxis(EAxis::Z), current_up) > 0.99f);
//
//	auto angle_mismatch = FMath::Acos(FVector::DotProduct(current_up, to_up));
//
//	auto sign_check = FVector::CrossProduct(current_up, to_up);
//
//	// forward and this plane would be facing different ways
//	if (FVector::DotProduct(sign_check, to_forward) > 0)
//	{
//		angle_mismatch = -angle_mismatch;
//	}
//
//	for (auto i = 0; i <= Divs; i++) {
//		float t = (float)i / Divs;
//
//		auto angle_correct = t * angle_mismatch;
//
//		FVector forward = SplineUtil::HermiteTangent(t, in_pos, out_pos, in_dir, out_dir);
//
//		forward.Normalize();
//
//		frames[i] = FTransform(FQuat({1, 0, 0}, angle_correct)) * frames[i];
//	}
//
//	auto check_up = frames.Last().ToMatrixNoScale().GetScaledAxis(EAxis::Z);
//
//	auto angle_check = FVector::DotProduct(check_up, to_up);
//
//	check(angle_check > 0.99f);
//
//	TArray<FVector> prev_slice;
//
//	for (auto i = 0; i <= Divs; i++) {
//		TArray<FVector> slice;
//
//		for (auto j = 0; j < from_c->Definition.NumVerts(); j++) {
//			slice.Add(from_c->Definition.GetTransformedVert(j, frames[i]));
//		}
//
//		if (prev_slice.Num())
//		{
//			auto prev_vert = prev_slice.Num() - 1;
//
//			for (auto j = 0; j < prev_slice.Num(); j++)
//			{
//				Util::AddPolyToMesh(mesh, TArray<FVector> {
//					slice[prev_vert], slice[j],
//					prev_slice[j], prev_slice[prev_vert]
//				});
//				//Util::AddPolyToMesh(mesh, TArray<FVector> {
//				//	slice[prev_vert], slice[j],
//				//	slice[j] + (prev_slice[j] - slice[j]) * 0.1,
//				//	slice[prev_vert] + (prev_slice[prev_vert] - slice[prev_vert]) * 0.1
//				//});
//
//				prev_vert = j;
//			}
//		}
//
//		prev_slice = std::move(slice);
//	}
//}

FVector ConnectorDef::GetTransformedVert(int vert_idx, const FTransform& total_trans) const
{
	// an untransformed connector faces down X and it upright w.r.t Z
	// so its width (internal X) is mapped onto Y and its height (internal Y) onto Z
	FVector temp{ 0.0f, Profile[vert_idx].Position.X, Profile[vert_idx].Position.Y };

	return total_trans.TransformPosition(temp);
}

#pragma optimize ("", on)
 }