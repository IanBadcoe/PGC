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
			auto conn_node = MakeShared<SNode>(Nodes.Num(), conn->Profile);

			auto tot_trans = conn->Transform * n->Position;

			conn_node->Position = tot_trans.GetLocation();
			conn_node->Up = tot_trans.GetUnitAxis(EAxis::Z);
			conn_node->Forwards = tot_trans.GetUnitAxis(EAxis::X);

			Nodes.Add(conn_node);

			Connect(here_node, conn_node, FVector::Dist(here_node->Position, conn_node->Position), false);
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

				auto profiles = edge->Profiles;

				if (profiles.Num() == 0)
				{
					profiles.Push(here_conn_node->Profile);
					profiles.Push(here_to_conn_node->Profile);
				}

				ConnectAndFillOut(here_node, here_conn_node, here_to_node, here_to_conn_node,
					edge->Divs, edge->Twists,
					input->SegLength, profiles);
			}
		}
	}
}

void SGraph::RefreshTransforms() const
{
	for(const auto& n : Nodes)
	{
		n->CachedTransform = Util::MakeTransform(n->Position, n->Up, n->Forwards);
		n->Flipped = false;
	}
}

void SGraph::Connect(const TSharedPtr<SNode> n1, const TSharedPtr<SNode> n2, double D, bool flipping)
{
	Edges.Add(MakeShared<SEdge>(n1, n2, D, flipping));

	// edges are no particular order in nodes
	n1->Edges.Add(Edges.Last());
	n2->Edges.Add(Edges.Last());
}

//// calculate the distance to the intersection with a plane along a line
//// return false if there is no intersection of it the  convergence is small enough than
//// numerical precision might be a problem
//static bool dist_to_plane(const FVector& line_start, const FVector& line_dir, const FVector& plane_point, const FVector& plane_normal, float& out)
//{
//	auto normals_ratio = FVector::DotProduct(line_dir, plane_normal);
//
//	// quite a loose tolerance but I'm only interested in broadly close collisions at the moment
//	if (FMath::Abs(normals_ratio) < 1.0E-6f)
//	{
//		return false;
//	}
//
//	auto rel_pos = line_start - plane_point;
//
//	// project vector between the two points onto the plane normal
//
//	auto proj = FVector::DotProduct(rel_pos, plane_normal);
//
//	// the distance along the line is longer than this in the ratio of normals_ratio
//
//	auto line_dist = proj / normals_ratio;
//
//	out = line_dist;
//
//	return true;
//}

void SGraph::ConnectAndFillOut(const TSharedPtr<SNode> from_n, TSharedPtr<SNode> from_c, const TSharedPtr<SNode> to_n, TSharedPtr<SNode> to_c,
	int divs, int twists, 
	float D0, const TArray<TSharedPtr<LayoutGraph::ParameterisedProfile>>& profiles)
{
	// profiles.Num() is generally less than divs since we need lots of divs to give us a smooth curve but the profile does not change
	// that often...

	// in order to divide an edge once we need three "frames"
	// the start node
	// the division
	// the end node
	// adding one to give us an interpolation range of 0 -> divs + 1
	// which for one div means start @ 0, div @ 1, end @ 2 etc
	divs += 1;

	const auto from_pos = from_c->Position;
	const auto to_pos = to_c->Position;

	auto dist = FVector::Distance(from_pos, to_pos);

	// if the end points are further apart than our nominal length
	// take the actual distance as the working length so as to get suitable curvy curves
	// otherwise use the nominal length so as not to build something sillily compressed
	//
	// actually we have to take the actual length, otherwise we can just get silly curves
	auto working_length = dist; // FMath::Max(D0 * divs, dist);

	auto from_forward = from_pos - from_n->Position;
	auto to_forward = to_pos - to_n->Position;

	from_forward.Normalize();
	to_forward.Normalize();


	// we need at least a small distance to work in...
	check(FVector::DistSquared(from_pos, to_pos) > 1.0f);

	// the curve is shorter than the line: from_pos -> intermediate1 -> intermetiate2 -> to_pos
	// and anyway the distance intermediate1 -> intermediate2 is unknown
	// so halving this is a complete guess :-)
	auto intermediate1 = from_pos + from_forward * working_length;
	auto intermediate2 = to_pos + to_forward * working_length;

	// an intermediate point hits to_pos, we get a silly curve
	// (in the extreme case, if the arrangement is like this
	// from --->  <--- to
	// then the curve can run forwards and backwards along the line, which breaks coordinate frame interpolation
	//
	// so if either intermediate point is too close to the end point that wasn't used to calculate it
	// try backing it off a bit...
	//
	// "too close" is arbitrarily defined here...
	if (FVector::Distance(to_pos, intermediate1) < working_length / 2)
	{
		intermediate1 = from_pos + from_forward * working_length / 2;
	}
	if (FVector::Distance(from_pos, intermediate2) < working_length / 2)
	{
		intermediate2 = to_pos + to_forward * working_length / 2;
	}

	const auto from_up = from_c->Up;
	const auto to_up = to_c->Up;

	auto current_up = from_up;

	struct Frame {
		FVector pos;
		FVector up;
		FVector forwards;
	};

	TArray<Frame> frames;

	// we're not going to use frames.Last() (it corresponds to to_c)
	// we just use it to calculate, correct and check up-drift...

	for (auto i = 0; i <= divs; i++) {
		float t = (float)i / divs;

		FVector pos = SplineUtil::CubicBezier(t, from_pos, intermediate1, intermediate2, to_pos);

		FVector forward = SplineUtil::CubicBezierTangent(t, from_pos, intermediate1, intermediate2, to_pos);

		forward.Normalize();

		auto right = FVector::CrossProduct(forward, current_up);

		right.Normalize();

		current_up = FVector::CrossProduct(right, forward);

		current_up.Normalize();

		frames.Add({pos, current_up, forward});
	}

	auto angle_mismatch = Util::SignedAngle(to_up, frames.Last().up, frames.Last().forwards);

	angle_mismatch += twists * PI;

	FVector temp;

	//// take this one further than required, so we can assert we arrived the right way up
	for (auto i = 0; i <= divs; i++) {
		float t = (float)i / divs;

		auto angle_correct = t * angle_mismatch;

		FVector forward = SplineUtil::CubicBezierTangent(t, from_pos, intermediate1, intermediate2, to_pos);

		forward.Normalize();

		frames[i].up = FTransform(FQuat(forward, angle_correct)).TransformVector(frames[i].up);

		temp = FTransform(FQuat(forward, -angle_correct)).TransformVector(frames[i].up);
	}

	auto check_up = frames.Last().up;

	auto angle_check = FVector::DotProduct(frames.Last().up, to_up);

	bool rolling = (twists & 1) != 0;

	// we expect to come out either 100% up or 100% down, according to whether we have an odd number of half-twists or not
	if (rolling)
	{
		check(angle_check < -0.99f);
	}
	else
	{
		check(angle_check > 0.99f);
	}

	auto curr_node = from_c;

	// i = 0 is the from-connector
	// i = divs is the to-connector
	for (auto i = 1; i < divs; i++) {
		float t = (float)i / divs;

		// as with rotations, i = 0 would be the profile of the start node
		// i = divs would be the profile off the end node
		// if the first and last entries in the "profiles" are also set to those
		// then we will get even distribution of each profile along the sequence:
		const auto& profile = profiles[profiles.Num() * t];

		TSharedPtr<SNode> next_node;
		
		next_node = MakeShared<SNode>(Nodes.Num(), profile);
		next_node->Position = frames[i].pos;
		next_node->Up = frames[i].up;
		next_node->Forwards = frames[i].forwards;

		Nodes.Add(next_node);

		// the last connection needs to rotate the mapping onto the target connector 180 degrees if we've
		// accumulated a half-twist along the connection
		Connect(curr_node, next_node, D0, false);
		
		curr_node = next_node;
	}

	Connect(curr_node, to_c, D0, rolling);
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

	for (const auto& e : Edges)
	{
		auto from_n = e->FromNode.Pin();
		auto to_n = e->ToNode.Pin();

		if (from_n->Profile && to_n->Profile)
		{
			TArray<FVector> verts_to;
			TArray<FVector> verts_from;

			auto to_trans = to_n->CachedTransform;
			const auto from_trans = from_n->CachedTransform;

			// if the two forward vectors are not within 180 degrees, because the
			// dest profile will be the wrong way around (not doing this will cause the connection to "bottle neck" and
			// also crash mesh generation since we'll be trying to add two faces rotating the same way to one edge)
			FVector to_forwards = to_trans.GetUnitAxis(EAxis::X);
			FVector from_forwards = from_trans.GetUnitAxis(EAxis::X);

			if (FVector::DotProduct(to_forwards, from_forwards) < 0)
			{
				to_trans = FTransform(FRotator(0, 180, 0)) * to_trans;
				// so that the node construction can know we did this...
				to_n->Flipped = true;
			}

			if (e->Rolling)
			{
				// if we have a half-twist in the overall sequence of edges, we need to apply the same thing here
				// as the profile need not be symmetrical
				to_trans = FTransform(FRotator(0, 0, 180)) * to_trans;

				to_n->Rolled = true;
			}

			// so that node meshing can use it without re-rotating
			to_n->CachedTransform = to_trans;

			for (int i = 0; i < to_n->Profile->NumVerts; i++)
			{
				verts_to.Add(to_n->Profile->GetTransformedVert(i, to_trans));
				verts_from.Add(from_n->Profile->GetTransformedVert(i, from_trans));
			}

			int prev_vert = to_n->Profile->NumVerts - 1;

			for (int i = 0; i < to_n->Profile->NumVerts; i++)
			{
				TArray<FVector> poly{ verts_from[i], verts_from[prev_vert], verts_to[prev_vert], verts_to[i] };

				// skip redundant faces generated by zero lengths in profile parameterization
				// INSIDE AddPolyToMesh
				Util::AddPolyToMeshReversed(mesh, poly);

				prev_vert = i;
			}
		}
	}

	for (const auto& node : Nodes)
	{
		// may need better control of this, currently complex junction nodes have no profile (due to potentially having more than two connections)
		// junction nodes have one adjoining node for each connection, and each connection has (if present) a chain of nodes leading off from the other side
		// so Edges (below) are meshed using the profile from either end
		//
		// the central node in a junction gets meshed to fill the rest in, and that is the one with no profile...
		if (!node->Profile)
		{
			node->AddToMesh(mesh);
		}
	}
}

SEdge::SEdge(TWeakPtr<SNode> fromNode, TWeakPtr<SNode> toNode, double d0, bool rolling)
	: FromNode(fromNode), ToNode(toNode), D0(d0), Rolling(rolling) {
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

// connects the same edge between two connectors around a node
// does this twice, once on the top and once on the bottom
static void C2CFacePair(TSharedPtr<Mesh>& mesh,
	const TSharedPtr<LayoutGraph::ParameterisedProfile> from_profile, const TSharedPtr<LayoutGraph::ParameterisedProfile> to_profile,
	const int from_quarters_map[4], const int to_quarters_map[4],
	const FTransform& from_trans, const FTransform& to_trans,
	LayoutGraph::ParameterisedProfile::VertTypes first_vert, LayoutGraph::ParameterisedProfile::VertTypes second_vert)
{
	{
		TArray<FVector> verts;

		verts.Push(to_profile->GetTransformedVert(first_vert, to_quarters_map[3], to_trans));
		verts.Push(from_profile->GetTransformedVert(first_vert, from_quarters_map[0], from_trans));
		verts.Push(from_profile->GetTransformedVert(second_vert, from_quarters_map[0], from_trans));
		verts.Push(to_profile->GetTransformedVert(second_vert, to_quarters_map[3], to_trans));

		Util::AddPolyToMesh(mesh, verts);
	}

	{
		TArray<FVector> verts;

		verts.Push(to_profile->GetTransformedVert(second_vert, to_quarters_map[2], to_trans));
		verts.Push(from_profile->GetTransformedVert(second_vert, from_quarters_map[1], from_trans));
		verts.Push(from_profile->GetTransformedVert(first_vert, from_quarters_map[1], from_trans));
		verts.Push(to_profile->GetTransformedVert(first_vert, to_quarters_map[2], to_trans));

		Util::AddPolyToMesh(mesh, verts);
	}
}

inline void SNode::AddToMesh(TSharedPtr<Mesh> mesh) {
	// unused nodes disappear for the moment...
	if (Edges.Num() == 0)
		return;

	// order connectors by angle around up vector

	struct OrdCon {
		TSharedPtr<SNode> ConNode;
		float Angle;
		FVector Axis;

		int QuartersMap[4]{ 3, 2, 1, 0 };		// if the connector is Flipped and/or Rolled its quarters change relative location
	};

	TArray<OrdCon> connectors;

	{
		OrdCon oe;
		oe.ConNode = Edges[0].Pin()->ToNode.Pin();
		oe.Angle = 0;
		oe.Axis = (oe.ConNode->Position - Position).GetSafeNormal();

		connectors.Push(oe);
	}

	for (int i = 1; i < Edges.Num(); i++)
	{
		OrdCon oc;
		oc.ConNode = Edges[i].Pin()->ToNode.Pin();
		oc.Axis = (oc.ConNode->Position - Position).GetSafeNormal();
		oc.Angle = Util::SignedAngle(connectors[0].Axis, oc.Axis, Up);

		if (oc.Angle < 0) oc.Angle += PI * 2;

		bool found = false;

		for(int j = 0; j < connectors.Num(); j++)
		{
			if (connectors[j].Angle > oc.Angle)
			{
				connectors.Insert(oc, j);
				found = true;

				break;
			}
		}

		if (!found)
		{
			connectors.Push(oc);
		}
	}

	for (auto& oc : connectors)
	{
		if (oc.ConNode->Flipped)
		{
			Swap(oc.QuartersMap[0], oc.QuartersMap[3]);
			Swap(oc.QuartersMap[1], oc.QuartersMap[2]);
		}

		if (oc.ConNode->Rolled)
		{
			Swap(oc.QuartersMap[0], oc.QuartersMap[2]);
			Swap(oc.QuartersMap[1], oc.QuartersMap[3]);
		}
	}

	{
		// roadbed surface
		TArray<FVector> verts_top;
		TArray<FVector> verts_bot;

		for (const auto& oc : connectors)
		{
			verts_top.Push(oc.ConNode->Profile->GetTransformedVert(LayoutGraph::ParameterisedProfile::VertTypes::RoadbedInner, oc.QuartersMap[3], oc.ConNode->CachedTransform));
			verts_top.Push(oc.ConNode->Profile->GetTransformedVert(LayoutGraph::ParameterisedProfile::VertTypes::RoadbedInner, oc.QuartersMap[0], oc.ConNode->CachedTransform));

			verts_bot.Push(oc.ConNode->Profile->GetTransformedVert(LayoutGraph::ParameterisedProfile::VertTypes::RoadbedInner, oc.QuartersMap[2], oc.ConNode->CachedTransform));
			verts_bot.Push(oc.ConNode->Profile->GetTransformedVert(LayoutGraph::ParameterisedProfile::VertTypes::RoadbedInner, oc.QuartersMap[1], oc.ConNode->CachedTransform));
		}

		Util::AddPolyToMesh(mesh, verts_top);
		Util::AddPolyToMeshReversed(mesh, verts_bot);
	}

	{
		// faces between connectors around the outside
		auto prev_oc = connectors.Last();

		for (const auto& oc : connectors)
		{
			{
				TArray<FVector> verts;

				verts.Push(oc.ConNode->Profile->GetTransformedVert(LayoutGraph::ParameterisedProfile::VertTypes::BarrierTopOuter, oc.QuartersMap[3], oc.ConNode->CachedTransform));
				verts.Push(prev_oc.ConNode->Profile->GetTransformedVert(LayoutGraph::ParameterisedProfile::VertTypes::BarrierTopOuter, prev_oc.QuartersMap[0], prev_oc.ConNode->CachedTransform));
				verts.Push(prev_oc.ConNode->Profile->GetTransformedVert(LayoutGraph::ParameterisedProfile::VertTypes::RoadbedOuter, prev_oc.QuartersMap[0], prev_oc.ConNode->CachedTransform));
				verts.Push(prev_oc.ConNode->Profile->GetTransformedVert(LayoutGraph::ParameterisedProfile::VertTypes::RoadbedOuter, prev_oc.QuartersMap[1], prev_oc.ConNode->CachedTransform));
				verts.Push(prev_oc.ConNode->Profile->GetTransformedVert(LayoutGraph::ParameterisedProfile::VertTypes::BarrierTopOuter, prev_oc.QuartersMap[1], prev_oc.ConNode->CachedTransform));
				verts.Push(oc.ConNode->Profile->GetTransformedVert(LayoutGraph::ParameterisedProfile::VertTypes::BarrierTopOuter, oc.QuartersMap[2], oc.ConNode->CachedTransform));
				verts.Push(oc.ConNode->Profile->GetTransformedVert(LayoutGraph::ParameterisedProfile::VertTypes::RoadbedOuter, oc.QuartersMap[2], oc.ConNode->CachedTransform));
				verts.Push(oc.ConNode->Profile->GetTransformedVert(LayoutGraph::ParameterisedProfile::VertTypes::RoadbedOuter, oc.QuartersMap[3], oc.ConNode->CachedTransform));

				Util::AddPolyToMesh(mesh, verts);
			}

			C2CFacePair(mesh,
				prev_oc.ConNode->Profile, oc.ConNode->Profile,
				prev_oc.QuartersMap, oc.QuartersMap,
				prev_oc.ConNode->CachedTransform, oc.ConNode->CachedTransform,
				LayoutGraph::ParameterisedProfile::VertTypes::OverhangEndOuter, LayoutGraph::ParameterisedProfile::VertTypes::BarrierTopOuter);

			C2CFacePair(mesh,
				prev_oc.ConNode->Profile, oc.ConNode->Profile,
				prev_oc.QuartersMap, oc.QuartersMap,
				prev_oc.ConNode->CachedTransform, oc.ConNode->CachedTransform,
				LayoutGraph::ParameterisedProfile::VertTypes::OverhangEndInner, LayoutGraph::ParameterisedProfile::VertTypes::OverhangEndOuter);

			C2CFacePair(mesh,
				prev_oc.ConNode->Profile, oc.ConNode->Profile,
				prev_oc.QuartersMap, oc.QuartersMap,
				prev_oc.ConNode->CachedTransform, oc.ConNode->CachedTransform,
				LayoutGraph::ParameterisedProfile::VertTypes::BarrierTopInner, LayoutGraph::ParameterisedProfile::VertTypes::OverhangEndInner);

			C2CFacePair(mesh,
				prev_oc.ConNode->Profile, oc.ConNode->Profile,
				prev_oc.QuartersMap, oc.QuartersMap,
				prev_oc.ConNode->CachedTransform, oc.ConNode->CachedTransform,
				LayoutGraph::ParameterisedProfile::VertTypes::RoadbedInner, LayoutGraph::ParameterisedProfile::VertTypes::BarrierTopInner);

			prev_oc = oc;
		}
	}
}

}

#pragma optimize ("", on)

