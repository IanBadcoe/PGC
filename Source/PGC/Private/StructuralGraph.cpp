#include "StructuralGraph.h"

#include "Util.h"

#pragma optimize ("", off)

namespace StructuralGraph {

SGraph::SGraph(TSharedPtr<LayoutGraph::Graph> input)
{
	for (const auto& n : input->GetNodes())
	{
		auto new_node = MakeShared<SNode>(Nodes.Num(), nullptr);
		new_node->Position = n->Position.GetLocation();
		new_node->Up = n->Position.GetUnitAxis(EAxis::Z);
		new_node->Forwards = n->Position.GetUnitAxis(EAxis::X);
		new_node->LayoutNode = n;					// nodes directly from the input note their input node so they can make the right mesh for it
		Nodes.Add(new_node);
	}

	for (int i = 0; i < input->GetNodes().Num(); i++)
	{
		// the start of Nodes correspond to the nodes in input
		auto here_node = Nodes[i];
		auto n = input->GetNodes()[i];

		// now add a node for each connector on the original node
		// these will be the same edge indices in new_node as the connectors in n
		for (const auto& conn : n->Connectors)
		{
			auto conn_node = MakeShared<SNode>(Nodes.Num(), &conn->Definition);

			auto tot_trans = conn->Transform * n->Position;

			conn_node->Position = tot_trans.GetLocation();
			conn_node->Up = tot_trans.GetUnitAxis(EAxis::Z);
			conn_node->Forwards = tot_trans.GetUnitAxis(EAxis::X);

			Nodes.Add(conn_node);

			Connect(here_node, conn_node, FVector::Dist(here_node->Position, conn_node->Position));
		}
	}

	// fill-in out edges
	for (int i = 0; i < input->GetNodes().Num(); i++)
	{
		// the start of Nodes correspond to the nodes in input
		auto here_node = Nodes[i];
		auto n = input->GetNodes()[i];

		for (int j = 0; j < n->Edges.Num(); j++)
		{
			auto edge = n->Edges[j].Pin();

			if (edge.IsValid() && edge->FromNode == n)
			{
				auto to_node = edge->ToNode.Pin();
				auto to_idx = input->FindNodeIdx(to_node);

				auto to_conn_idx = to_node->FindConnectorIdx(edge->ToConnector.Pin());

				auto here_edge = here_node->Edges[j].Pin();
				auto here_conn_node = here_edge->ToNode.Pin();

				auto here_to_node = Nodes[to_idx];
				auto here_to_conn_node = here_to_node->Edges[to_conn_idx].Pin()->ToNode.Pin();

				ConnectAndFillOut(here_node, here_conn_node, here_to_node, here_to_conn_node, edge->Divs, input->SegLength, here_conn_node->Profile);
			}
		}
	}
}

void SGraph::RefreshTransforms() const
{
	// first node is defined as having a unit transform
	CachedTransforms.Add(Nodes[0].Get()) = FTransform::Identity;

	for(const auto& n : Nodes)
	{
		CachedTransforms.Add(n.Get()) = Util::MakeTransform(n->Position, n->Up, n->Forwards);
	}
}

void SGraph::Connect(const TSharedPtr<SNode> n1, const TSharedPtr<SNode> n2, double D)
{
	Edges.Add(MakeShared<SEdge>(n1, n2, D));

	// edges are no particular order in nodes
	n1->Edges.Add(Edges.Last());
	n2->Edges.Add(Edges.Last());
}

void SGraph::ConnectAndFillOut(const TSharedPtr<SNode> from_n, TSharedPtr<SNode> from_c, const TSharedPtr<SNode> to_n, TSharedPtr<SNode> to_c,
	int divs, float D0, const LayoutGraph::ConnectorDef* profile)
{
	auto from_forward = from_c->Position - from_n->Position;
	auto to_forward = to_c->Position - to_n->Position;

	from_forward.Normalize();
	to_forward.Normalize();

	const auto in_pos = from_c->Position;
	const auto out_pos = to_c->Position;

	const auto dist = FVector::Distance(in_pos, out_pos);

	const auto in_dir = from_forward * dist /* / 3 */;		// dividing by 3 means if the two connectors are pointing at each other
	const auto out_dir = -to_forward * dist /* / 3 */;		// then we divide the space evenly

	const auto from_up = from_c->Up;
	const auto to_up = to_c->Up;

	auto current_up = from_up;

	struct Frame {
		FVector pos;
		FVector up;
		FVector forwards;
	};

	TArray<Frame> frames;

	for (auto i = 0; i <= divs; i++) {
		float t = (float)i / divs;

		FVector pos = SplineUtil::CubicBezier(t, in_pos, in_pos + in_dir, out_pos - out_dir, out_pos);

		FVector forward = SplineUtil::CubicBezierTangent(t, in_pos, in_pos + in_dir, out_pos - out_dir, out_pos);

		forward.Normalize();

		auto right = FVector::CrossProduct(forward, current_up);

		right.Normalize();

		current_up = FVector::CrossProduct(right, forward);

		current_up.Normalize();

		frames.Add({pos, current_up, forward});
	}

	auto angle_mismatch = FMath::Acos(FVector::DotProduct(current_up, to_up));

	auto sign_check = FVector::CrossProduct(current_up, to_up);

	// forward and this plane would be facing different ways
	if (FVector::DotProduct(sign_check, to_forward) > 0)
	{
		angle_mismatch = -angle_mismatch;
	}

	//// take this one further than required, so we can assert we arrived the right way up
	//for (auto i = 0; i <= divs; i++) {
	//	float t = (float)i / divs;

	//	auto angle_correct = t * angle_mismatch;

	//	FVector forward = SplineUtil::HermiteTangent(t, in_pos, out_pos, in_dir, out_dir);

	//	forward.Normalize();

	//	frames[i].up = FTransform(FQuat(forward, angle_correct)).TransformVector(frames[i].up);
	//}

	//auto check_up = frames.Last().up;

	//auto angle_check = FVector::DotProduct(check_up, to_up);

	//check(angle_check > 0.99f);

	auto curr_node = from_c;

	// i = 0 is the from-connector
	// i = divs is the to-connector
	for (auto i = 1; i <= divs; i++) {
		float t = (float)i / divs;

		TSharedPtr<SNode> next_node;
		
		if (i < divs)
		{
			next_node = MakeShared<SNode>(Nodes.Num(), profile);
			next_node->Position = frames[i].pos;
			next_node->Up = frames[i].up;
			next_node->Forwards = frames[i].forwards;

			Nodes.Add(next_node);
		}
		else
		{
			next_node = to_c;
		}

		Connect(curr_node, next_node, D0);
		
		curr_node = next_node;
	}
}

int SGraph::FindNodeIdx(const TSharedPtr<SNode>& node) const
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

void SGraph::MakeMesh(TSharedPtr<Mesh> mesh)
{
	mesh->Clear();

	RefreshTransforms();

	for (const auto& node : Nodes)
	{
		node->AddToMesh(mesh);
	}

	for (const auto& e : Edges)
	{
		auto from_n = e->FromNode.Pin();
		auto to_n = e->ToNode.Pin();

		if (from_n->Profile && to_n->Profile)
		{
			TArray<FVector> verts_to;
			TArray<FVector> verts_from;

			auto to_trans = CachedTransforms[to_n.Get()];
			const auto from_trans = CachedTransforms[from_n.Get()];

			// if the two forward vectors are not within 180 degrees, rotate

			FVector to_forwards = to_trans.GetUnitAxis(EAxis::X);
			FVector from_forwards = from_trans.GetUnitAxis(EAxis::X);

			if (FVector::DotProduct(to_forwards, from_forwards) < 0)
			{
				to_trans = FTransform(FRotator(0, 180, 0)) * to_trans;
			}

			for (int i = 0; i < to_n->Profile->NumVerts(); i++)
			{
				verts_to.Add(to_n->Profile->GetTransformedVert(i, to_trans) /* + to_forwards * 0.01f */);
				verts_from.Add(from_n->Profile->GetTransformedVert(i, from_trans) /* - from_forwards * 0.01f */);
			}

			int prev_vert = to_n->Profile->NumVerts() - 1;

			//Util::AddPolyToMesh(mesh, verts_from);
			//Util::AddPolyToMesh(mesh, verts_to);

			for (int i = 0; i < to_n->Profile->NumVerts(); i++)
			{
				Util::AddPolyToMesh(mesh, { verts_from[i], verts_from[prev_vert], verts_to[prev_vert], verts_to[i] });

				prev_vert = i;
			}
		}
	}
}

SEdge::SEdge(TWeakPtr<SNode> fromNode, TWeakPtr<SNode> toNode, double d0)
	: FromNode(fromNode), ToNode(toNode), D0(d0) {
}

//double OptFunction::CalcGrad(const double* x, int n) const
//{
//	int node_idx = n / 6;
//	int param_idx = n % 6;
//
//	double ret = 0;
//
//	for (int i = 0; i < Working.Num(); i++)
//	{
//		if (i != node_idx)
//		{
//			if (Connected.Contains(TPair<int, int>{node_idx, i}))
//			{
//				ret += ConnectedNodeNodeGrad(param_idx,
//					Working[param_idx].trans.GetLocation(),
//					Working[i].trans.GetLocation());
//			}
//		}
//	}
//
//	return ret;
//}

//double OptFunction::CalcVal(const double* x) const
//{
//	double ret = 0.0;
//
//	//const auto& nodes = G.GetNodes();
//	//const auto& edges = G.GetEdges();
//
//	for (int i = 0; i < Working.Num() - 1; i++)
//	{
//		for (int j = i + 1; j < Working.Num(); j++)
//		{
//			if (Connected.Contains(TPair<int, int>{ i, j }))
//			{
//				ret += ConnectedNodeNodeVal(Working[i].trans.GetLocation(), Working[i].trans.GetLocation());
//			}
//		}
//	}
//
//	return ret;
//}

double OptFunction::UnconnectedNodeNodeVal(const FVector& p1, const FVector& p2, float D) const
{
	float dist = FVector::Dist(p1, p2);

	return LeonardJonesVal(dist, D, 1);
}

double OptFunction::UnconnectedNodeNodeGrad(int i, const FVector& pGrad, const FVector& pOther, float D) const
{
	if (i > 2)		// angle has no effect on distance
	{
		return 0.0;
	}

	float dist = FVector::Dist(pGrad, pOther);

	double delta = pGrad[i] - pOther[i];

	return delta * LeonardJonesGrad(dist, D, 1) / dist;
}

double OptFunction::ConnectedNodeNodeVal(const FVector & p1, const FVector & p2, float D0) const
{
	float dist = FVector::Dist(p1, p2);

	return LeonardJonesVal(dist, D0, 2);
}

double OptFunction::ConnectedNodeNodeGrad(int i, const FVector & pGrad, const FVector & pOther, float D0) const
{
	if (i > 2)		// angle has no effect on distance
	{
		return 0.0;
	}

	float dist = FVector::Dist(pGrad, pOther);

	double delta = pGrad[i] - pOther[i];

	return delta * LeonardJonesGrad(dist, D0, 2) / dist;
}

double OptFunction::LeonardJonesVal(double R, double D, int N) const
{
	double rat = D / R;

	return FMath::Pow(rat, N * 2) - 2 * FMath::Pow(rat, N);
}

double OptFunction::LeonardJonesGrad(double R, double D, int N) const
{
	double rat = D / R;

	return 2 * N * (FMath::Pow(rat, N) - 1) * FMath::Pow(rat, N) / R;
}

//static double CalcZRot(const TSharedPtr<SNode>& parent, const TSharedPtr<SNode>& node)
//{
//	auto node_up = node->Position.GetUnitAxis(EAxis::Z);
//
//	auto parent_up = parent->Position.GetUnitAxis(EAxis::Z);
//
//	auto forward = node->Position.GetLocation() - parent->Position.GetLocation();
//
//	forward.Normalize();
//
//	// adjust parent up into the same plane as ours
//	{
//		// hpr is normal to local forward and parent_up
//		auto here_parent_right = FVector::CrossProduct(forward, parent_up);
//
//		here_parent_right.Normalize();
//
//		// so now parent_up with be forward and hpr, making it co-planar with
//		// local up and right, but still possibly rotated around forwards
//		// compared to local up...
//		parent_up = FVector::CrossProduct(here_parent_right, forward);
//
//		parent_up.Normalize();
//	}
//
//	auto angle = FMath::Acos(FVector::DotProduct(parent_up, node_up));
//
//	auto sign_check = FVector::CrossProduct(parent_up, node_up);
//
//	// forward and this plane would be facing different ways
//	if (FVector::DotProduct(sign_check, forward) > 0)
//	{
//		angle = -angle;
//	}
//
//	return angle;
//}

//void OptFunction::BuildPropagationFrom(TSet<TSharedPtr<SNode>>& found, const TSharedPtr<SNode>& node, const TSharedPtr<SNode>& parent, int addingIdx)
//{
//	check(!found.Contains(node));
//	found.Add(node);
//	Working[addingIdx].trans = node->Position;
//	if (parent.IsValid())
//	{
//		Working[addingIdx].z_rot = CalcZRot(parent, node);
//	}
//	else
//	{
//		Working[addingIdx].z_rot = 0.0;
//	}
//
//	Propagation.Add(addingIdx);
//
//	for (const auto& edge : node->Edges)
//	{
//		auto edge_ptr = edge.Pin();
//
//		// if it is an out edge
//		if (edge_ptr->FromNode == node)
//		{
//			const auto& next_node = edge_ptr->ToNode.Pin();
//
//			int next_idx = G->FindNodeIdx(next_node);
//
//			check(!Connected.Contains(TPair<int, int>{addingIdx, next_idx}));
//			check(!Connected.Contains(TPair<int, int>{next_idx, addingIdx}));
//
//			Connected.Add(TPair<int, int>{addingIdx, next_idx});
//			Connected.Add(TPair<int, int>{next_idx, addingIdx});
//
//			if (!found.Contains(next_node))
//			{
//				Propagation[addingIdx].Add(next_idx);
//
//				BuildPropagationFrom(found, next_node, node, next_idx);
//			}
//		}
//	}
//}
//
//void OptFunction::PropagateTransform(const double* x, int from, int to) const
//{
//	auto to_pos = FVector{ (float)x[from * 4 + 0], (float)x[from * 4 + 1], (float)x[from * 4 + 2] };
//
//	auto forward = to_pos - Working[from].trans.GetLocation();
//	forward.Normalize();
//
//	auto from_up = Working[from].trans.GetUnitAxis(EAxis::Z);
//
//	auto to_right = FVector::CrossProduct(forward, from_up);
//
//	to_right.Normalize();
//
//	auto to_up = FVector::CrossProduct(to_right, forward);
//
//	to_up.Normalize();
//
//	// position at to_pos, oriented the same as from accept for up/righ adjusted to be orthogonal to our local forward
//	Working[to].trans = FTransform(forward, to_right, to_up, to_pos);
//
//	double angle = x[to * 4 + 3];
//
//	// rotate that according to how the other param wants to rotate up....
//	Working[to].trans = FTransform(FQuat({ 1, 0, 0 }, (float)angle)) * Working[to].trans;
//	Working[to].z_rot = angle;
//}

static FVector GetVector(const double* x, int vect_idx) {
	return FVector
	{
		(float)x[vect_idx * 3 + 0],
		(float)x[vect_idx * 3 + 1],
		(float)x[vect_idx * 3 + 2]
	};
}

static void SetVector(double* x, int vect_idx, const FVector& v)
{
	x[vect_idx * 3 + 0] = v.X;
	x[vect_idx * 3 + 1] = v.Y;
	x[vect_idx * 3 + 2] = v.Z;
}

void OptFunction::ApplyParams(const double* x, int n)
{
	check(n == GetSize());

	for (int i = 0; i < G->Nodes.Num(); i++)
	{
		auto norm = GetVector(x, i * 2 + 1);
		norm.Normalize();
		G->Nodes[i]->SetPosition(GetVector(x, i * 2), norm);
	}
}

OptFunction::OptFunction(TSharedPtr<SGraph> g) : G(g)
{
	for (const auto& n : G->Nodes)
	{
		for (const auto& e : n->Edges)
		{
			auto ep = e.Pin();

			// only need to consider each edge in one direction
			if (ep->FromNode == n)
			{
				auto other_n = ep->ToNode.Pin();

				Connected.Add({FMath::Min(n->Idx, other_n->Idx), FMath::Max(n->Idx, other_n->Idx)});

				for (const auto other_e : other_n->Edges)
				{
					auto pother_e = other_e.Pin();

					if (pother_e != e)
					{
						auto third_n = (pother_e->FromNode == other_n ? pother_e->ToNode : pother_e->FromNode).Pin();

						Angles.Add(
						{
							FMath::Min(n->Idx, third_n->Idx),
							other_n->Idx,
							FMath::Max(n->Idx, third_n->Idx)
						});
					}
				}
			}
		}
	}
}

int OptFunction::GetSize() const
{
	// pos -> 3
	// up -> 3 (will normalise internally)

	return G->Nodes.Num() * 6;
}

double OptFunction::f(int n, const double* x, double* grad)
{
	check(n == GetSize());

	for (int i = 0; i < n; i++)
	{
		grad[i] = 0;
	}

	ApplyParams(x, n);

	double ret = 0;

	for (const auto& join : Connected)
	{
		for (int i = 0; i < 3; i++)
		{
			double slope = ConnectedNodeNodeGrad(i, G->Nodes[join.i]->Position, G->Nodes[join.j]->Position, join.D0);
			grad[join.i * 3 + i] += slope;
			grad[join.j * 3 + i] -= slope;
		}

		ret += ConnectedNodeNodeVal(G->Nodes[join.i]->Position, G->Nodes[join.j]->Position, join.D0);
	}

	return ret;
}

void OptFunction::GetInitialStepSize(double* steps, int n) const
{
	check(n == GetSize());

	for (int i = 0; i < n; i++)
	{
		steps[i] = 0.1;
	}
}

void OptFunction::GetState(double* x, int n) const
{
	check(n == GetSize());

	for (int i = 0; i < G->Nodes.Num(); i++)
	{
		SetVector(x, i * 2 + 0, G->Nodes[i]->Position);
		SetVector(x, i * 2 + 1, G->Nodes[i]->Up);
	}
}

void OptFunction::SetState(const double* x, int n)
{
	check(n == GetSize());

	ApplyParams(x, n);
}

#pragma optimize ("", on)

}