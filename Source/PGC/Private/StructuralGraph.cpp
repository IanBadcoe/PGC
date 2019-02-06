#include "StructuralGraph.h"

#include "Util.h"

PRAGMA_DISABLE_OPTIMIZATION

using namespace StructuralGraph;
using namespace Profile;

SGraph::SGraph(TSharedPtr<LayoutGraph::Graph> input, ProfileSource* profile_source)
	: _ProfileSource(profile_source)
{
	for (const auto& n : input->GetNodes())
	{
		auto new_node = MakeShared<SNode>(nullptr, SNode::Type::Junction);
		new_node->Position = n->Transform.GetLocation();
		new_node->CachedUp = n->Transform.GetUnitAxis(EAxis::Z);		// for the moment this is all we have, after MakeIntoDAG will calculate as an angle from parent Up and store in Rotation instead
		new_node->Forward = n->Transform.GetUnitAxis(EAxis::X);
		Nodes.Add(new_node);
	}

	for (int i = 0; i < input->GetNodes().Num(); i++)
	{
		// the start of Nodes correspond to the nodes in input
		auto here_node = Nodes[i];
		auto n = input->GetNodes()[i];

		TArray<TSharedPtr<ParameterisedProfile>> node_profiles;
		float max_profile_radius = 0.0f;

		for (int i = 0; i < n->Edges.Num(); i++)
		{
			node_profiles.Add(profile_source->GetProfile());

			max_profile_radius = FMath::Max(max_profile_radius, node_profiles.Last()->Radius());
		}

		// using the radius is an exaggeration, as that would only apply if the profile was rotated exactly so that its diagonal lay in the
		// node plane, however the profile could rotate later, and allowing for that would require dynamic re-calculation of this, so let's take the
		// extreme case from the start...

		// A regular polygon with this many lines max_profile_radius long around it looks like this:
		//
		//          ^  +---------------+
		//          |  |A\     |      X|
		// adjacent |  |   \   |       |
		//          |  |     \ |       |
		//          v  |-------+-------|
		//             |       |       |
		//             |       |       |
		//             |       |       |
		//             +---------------+
		//
		// The angle X is (180 - 360 / N) / 2
		// and the angle A is half that.  The adjacent is max_profile_radius
		// so the line from the centre to the mid-point of the profile is of length:
		// max_profile_radius * tan((180 - 180 / N)/2) (in degrees)
		//
		// and throw in an extra 10% to allow for some space if the profile is oriented straight along its radius

		auto corner_angle = PI - 2 * PI / n->Edges.Num();
		auto node_radius = max_profile_radius * FMath::Tan(corner_angle / 2) * 1.1f;

		// now add a node for each connector on the original node
		// (giving them the profiles we just picked... could always do this later if we also fixed the D0 on the connectors)

		// these will be the same edge indices in new_node as the connectors in n
		for (int i = 0; i < n->Connectors.Num(); i++)
		{
			const auto& conn = n->Connectors[i];

			auto conn_node = MakeShared<SNode>(node_profiles[i], SNode::Type::JunctionConnector);

			auto tot_trans = conn->Transform * n->Transform;

			conn_node->Position = tot_trans.GetLocation();
			conn_node->CachedUp = tot_trans.GetUnitAxis(EAxis::Z);		// for the moment this is all we have, after MakeIntoDAG will calculate as an angle from parent Up and store in Rotation instead
			conn_node->Forward = tot_trans.GetUnitAxis(EAxis::X);

			Nodes.Add(conn_node);

			// we have positioned this where the layout graph had it, at an assumed radius, but we'll
			// tell the optimizer that we want it at our preferred radius
			Connect(here_node, conn_node, node_radius);
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

				// loop-back edges should only be considered in one direction
				if (edge->ToNode == edge->FromNode && to_conn_idx == j)
					continue;

				auto here_edge = here_node->Edges[j].Pin();
				auto here_conn_node = here_edge->ToNode.Pin();

				auto here_to_node = Nodes[to_idx];
				auto here_to_conn_node = here_to_node->Edges[to_conn_idx].Pin()->ToNode.Pin();

				auto profiles = _ProfileSource->GetCompatibleProfileSequence(here_conn_node->Profile, here_to_conn_node->Profile,
					edge->Divs);

				ConnectAndFillOut(here_node, here_conn_node, here_to_node, here_to_conn_node,
					edge->Divs, edge->Twists,
					input->SegLength, profiles);
			}
		}
	}

	for (auto &n : Nodes)
	{
		// now it is all laid out set the node radii
		n->FindRadius();
	}

	MakeIntoDAG();
}

void SGraph::MakeIntoDAG()
{
	if (!Nodes.Num())
		return;

	TSet<TSharedPtr<SNode>> Closed;

	// this will contain the nodes in an order suitablew for updating from
	// e.g. everything will be later in the list that its parent
	TArray<TSharedPtr<SNode>> new_order;

	auto orig_size = Nodes.Num();

	MakeIntoDagInner(Nodes[0], Closed, new_order);

	// we're assuming that the graph comes as a single part, if it eventually does not, we need to
	// loop on the previous line to pull out each part in turn...
	check(Nodes.Num() == 0);
	check(new_order.Num() == orig_size);

	Nodes = new_order;
}

// by doing this depth-first we ensure we traverse the whole of an unbranching sequence in one
// which avoids any awkwardness around knowing which way around the profile should be
// when examining an edge from the middle of it in isolation
// (otherwise you can end up with -->-->--><--<--<-- and the profile orientations are indeterminate
//  unless you follow it all the way from one end to the other (so as you know that a reverse has had to be propagated
//  across the join and into the whole of the later section)
void SGraph::MakeIntoDagInner(TSharedPtr<SNode> node, TSet<TSharedPtr<SNode>>& closed, TArray<TSharedPtr<SNode>>& new_order)
{
	Nodes.Remove(node);
	new_order.Add(node);
	closed.Add(node);

	// this can be done when all connected node positions are known
	// but since we aren't moving any nodes in this routine can do at any time...
	node->RecalcForward();

	// this can only be done after our parent's CachedUp has been updated
	node->CalcRotation();

	for (auto& edge : node->Edges)
	{
		auto ep = edge.Pin();

		auto other = ep->OtherNode(node).Pin();

		if (closed.Contains(other))
			continue;

		other->Parent = node;

		if (ep->FromNode != node)
		{
			auto temp = ep->ToNode;
			ep->ToNode = ep->FromNode;
			ep->FromNode = temp;
		}

		for (int i = 1; i < other->Edges.Num(); i++)
		{
			if (other->Edges[i].Pin()->OtherNode(other) == node)
			{
				Swap(other->Edges[0], other->Edges[i]);
			}
		}

		MakeIntoDagInner(other, closed, new_order);
	}
}

void SGraph::RefreshTransforms() const
{
	for(const auto& n : Nodes)
	{
		// currently use this in MakeMesh and CachedUp will have been set by any previous step
		// could move this data out of SNode into something temporary to MakeMesh...
		n->CachedTransform = Util::MakeTransform(n->Position, n->CachedUp, n->Forward);
	}
}

void SGraph::Connect(const TSharedPtr<SNode> n1, const TSharedPtr<SNode> n2, double D)
{
	Edges.Add(MakeShared<SEdge>(n1, n2, D));

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
	float D0, const TArray<TSharedPtr<ParameterisedProfile>>& profiles)
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
	auto working_length = FMath::Max(D0 * divs, dist);

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
	// then the curve can run forward and backwards along the line, which breaks coordinate frame interpolation
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

	// we haven't started using "Rotation" yet...
	const auto from_up = from_c->CachedUp;
	const auto to_up = to_c->CachedUp;

	auto current_up = from_up;

	struct Frame {
		FVector pos;
		FVector up;
		FVector forward;
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

		frames.Add({pos, current_up, forward });
	}

	auto angle_mismatch = Util::SignedAngle(to_up, frames.Last().up, frames.Last().forward);

	angle_mismatch += twists * PI;

	//// take this one further than required, so we can assert we arrived the right way up
	for (auto i = 0; i <= divs; i++) {
		float t = (float)i / divs;

		auto angle_correct = t * -angle_mismatch;

		FVector forward = SplineUtil::CubicBezierTangent(t, from_pos, intermediate1, intermediate2, to_pos);

		forward.Normalize();

		frames[i].up = FTransform(FQuat(forward, angle_correct)).TransformVector(frames[i].up);

		// up and forward really should still be normal...
		check(FMath::Abs(FVector::DotProduct(frames[i].forward, frames[i].up)) < 1e-4f);
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
		
		next_node = MakeShared<SNode>(profile, SNode::Type::Connection);
		next_node->Position = frames[i].pos;
		next_node->CachedUp = frames[i].up;
		next_node->Forward = frames[i].forward;

		Nodes.Add(next_node);

		// the last connection needs to rotate the mapping onto the target connector 180 degrees if we've
		// accumulated a half-twist along the connection
		Connect(curr_node, next_node, D0);
		
		curr_node = next_node;
	}

	Connect(curr_node, to_c, D0);
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

void SGraph::MakeMesh(TSharedPtr<Mesh> mesh, bool skeleton_only)
{
	mesh->Clear();

	RefreshTransforms();

	if (skeleton_only)
	{
		for (const auto& e : Edges)
		{
			auto from_n = e->FromNode.Pin();
			auto to_n = e->ToNode.Pin();

			auto from_p = from_n->Position;
			auto to_p = to_n->Position;

			auto vec = to_p - from_p;

			auto orth1 = FVector::CrossProduct(vec, from_n->CachedUp);

			orth1.Normalize();

			auto orth2 = FVector::CrossProduct(orth1, vec);

			orth2.Normalize();

			auto dist = FVector::Dist(from_p, to_p);

			{
				TArray<FVector> poly;

				poly.Add(from_p + orth1 * dist / 10);
				poly.Add(from_p + orth2 * dist / 10);
				poly.Add(to_p);

				Util::AddPolyToMesh(mesh, poly, { PGCEdgeType::Sharp, PGCEdgeType::Sharp, PGCEdgeType::Sharp });
			}

			{
				TArray<FVector> poly;

				poly.Add(from_p - orth1 * dist / 10);
				poly.Add(from_p + orth2 * dist / 10);
				poly.Add(to_p);

				Util::AddPolyToMeshReversed(mesh, poly, { PGCEdgeType::Sharp, PGCEdgeType::Sharp, PGCEdgeType::Sharp });
			}

			{
				TArray<FVector> poly;

				poly.Add(from_p + orth1 * dist / 10);
				poly.Add(from_p - orth2 * dist / 10);
				poly.Add(to_p);

				Util::AddPolyToMeshReversed(mesh, poly, { PGCEdgeType::Sharp, PGCEdgeType::Sharp, PGCEdgeType::Sharp });
			}

			{
				TArray<FVector> poly;

				poly.Add(from_p - orth1 * dist / 10);
				poly.Add(from_p - orth2 * dist / 10);
				poly.Add(to_p);

				Util::AddPolyToMesh(mesh, poly, { PGCEdgeType::Sharp, PGCEdgeType::Sharp, PGCEdgeType::Sharp });
			}

			{
				TArray<FVector> poly;

				poly.Add(from_p - orth2 * dist / 10);
				poly.Add(from_p - orth1 * dist / 10);
				poly.Add(from_p + orth2 * dist / 10);
				poly.Add(from_p + orth1 * dist / 10);

				Util::AddPolyToMesh(mesh, poly, { PGCEdgeType::Sharp, PGCEdgeType::Sharp, PGCEdgeType::Sharp, PGCEdgeType::Sharp });
			}
		}

		for (const auto& n : Nodes)
		{
			auto p1 = n->Position;
			auto p2 = p1 + n->Forward;
			auto p3 = p1 + n->CachedTransform.GetUnitAxis(EAxis::Z) * 5;
			auto p4 = p1 + FVector::CrossProduct(n->Forward, n->CachedUp);

			{
				TArray<FVector> poly;

				poly.Add(p1);
				poly.Add(p2);
				poly.Add(p3);

				Util::AddPolyToMesh(mesh, poly, { PGCEdgeType::Sharp, PGCEdgeType::Sharp, PGCEdgeType::Sharp });
			}

			{
				TArray<FVector> poly;

				poly.Add(p2);
				poly.Add(p4);
				poly.Add(p3);

				Util::AddPolyToMesh(mesh, poly, { PGCEdgeType::Sharp, PGCEdgeType::Sharp, PGCEdgeType::Sharp });
			}

			{
				TArray<FVector> poly;

				poly.Add(p4);
				poly.Add(p1);
				poly.Add(p3);

				Util::AddPolyToMesh(mesh, poly, { PGCEdgeType::Sharp, PGCEdgeType::Sharp, PGCEdgeType::Sharp });
			}

			{
				TArray<FVector> poly;

				poly.Add(p1);
				poly.Add(p4);
				poly.Add(p2);

				Util::AddPolyToMesh(mesh, poly, { PGCEdgeType::Sharp, PGCEdgeType::Sharp, PGCEdgeType::Sharp });
			}
		}
	}
	else
	{
		for (const auto& e : Edges)
		{
			auto from_n = e->FromNode.Pin();
			auto to_n = e->ToNode.Pin();

			if (from_n->Profile && to_n->Profile)
			{
				TArray<FVector> verts_to;
				TArray<FVector> verts_from;

				auto to_trans = to_n->CachedTransform;
				auto from_trans = from_n->CachedTransform;

				// if the two forward vectors are not within 180 degrees, because the
				// dest profile will be the wrong way around (not doing this will cause the connection to "bottle neck" and
				// also crash mesh generation since we'll be trying to add two faces rotating the same way to one edge)
				FVector to_forward = to_trans.GetUnitAxis(EAxis::X);
				FVector from_forward = from_trans.GetUnitAxis(EAxis::X);

				if (FVector::DotProduct(to_forward, from_forward) < 0)
				{
					to_trans = FTransform(FRotator(0, 180, 0)) * to_trans;
					// so that the node construction can know we did this...
				}

				if (FVector::DotProduct(to_n->CachedUp, from_n->CachedUp) < 0)
				{
					// because we always keep all of a connection pointing the same way
					// and also reserve accommodating the 1/2 turn to the point where we hit
					// another junction, this can only happen at the point where the string of "Connection" type
					// nodes hits a "JunctionConnector"
					if (to_n->MyType == SNode::Type::JunctionConnector)
					{
						// if we have a half-twist in the overall sequence of edges, we need to apply the same thing here
						// as the profile need not be symmetrical
						to_trans = FTransform(FRotator(0, 0, 180)) * to_trans;
					}
					else
					{
						check(from_n->MyType == SNode::Type::JunctionConnector);
						from_trans = FTransform(FRotator(0, 0, 180)) * from_trans;
					}
				}

				// so that node meshing can use it without re-rotating
				to_n->CachedTransform = to_trans;
				from_n->CachedTransform = from_trans;

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
					Util::AddPolyToMeshReversed(mesh, poly,
						{
							PGCEdgeType::Rounded,
							from_n->Profile->GetOutgoungEdgeType(prev_vert),
							PGCEdgeType::Rounded,
							from_n->Profile->GetOutgoungEdgeType(i)
						});

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
}

SEdge::SEdge(TWeakPtr<SNode> fromNode, TWeakPtr<SNode> toNode, double d0)
	: FromNode(fromNode), ToNode(toNode), D0(d0) {
}

inline void SNode::FindRadius() {
	if (Profile.IsValid())
	{
		Radius = Profile->Radius();
	}
	else
	{
		// we have set all our edge radii the same, so just take the length of the first...
		// and as this is for non-connected stuff, back-off a further 50%
		Radius = Edges[0].Pin()->D0 * 1.5f;
	}
}

// connects the same edge between two connectors around a node
// does this twice, once on the top and once on the bottom
static void C2CFacePair(TSharedPtr<Mesh>& mesh,
	const TSharedPtr<ParameterisedProfile> from_profile, const TSharedPtr<ParameterisedProfile> to_profile,
	const int from_quarters_map[4], const int to_quarters_map[4],
	const FTransform& from_trans, const FTransform& to_trans,
	ParameterisedProfile::VertTypes first_vert, ParameterisedProfile::VertTypes second_vert,
	PGCEdgeType in_profile_edge_type, PGCEdgeType between_profile_edge_type)
{
	{
		TArray<FVector> verts;

		verts.Push(to_profile->GetTransformedVert(first_vert, to_quarters_map[3], to_trans));
		verts.Push(from_profile->GetTransformedVert(first_vert, from_quarters_map[0], from_trans));
		verts.Push(from_profile->GetTransformedVert(second_vert, from_quarters_map[0], from_trans));
		verts.Push(to_profile->GetTransformedVert(second_vert, to_quarters_map[3], to_trans));

		Util::AddPolyToMesh(mesh, verts, { between_profile_edge_type, in_profile_edge_type, between_profile_edge_type, in_profile_edge_type });
	}

	{
		TArray<FVector> verts;

		verts.Push(to_profile->GetTransformedVert(second_vert, to_quarters_map[2], to_trans));
		verts.Push(from_profile->GetTransformedVert(second_vert, from_quarters_map[1], from_trans));
		verts.Push(from_profile->GetTransformedVert(first_vert, from_quarters_map[1], from_trans));
		verts.Push(to_profile->GetTransformedVert(first_vert, to_quarters_map[2], to_trans));

		Util::AddPolyToMesh(mesh, verts, { between_profile_edge_type, in_profile_edge_type, between_profile_edge_type, in_profile_edge_type });
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
		bool Flipped;

		int QuartersMap[4]{ 3, 2, 1, 0 };		// if the connector is Flipped and/or Rolled its quarters change relative location
	};

	TArray<OrdCon> connectors;

	{
		OrdCon oe;
		oe.ConNode = Edges[0].Pin()->OtherNode(this).Pin();
		oe.Angle = 0;
		oe.Axis = (oe.ConNode->Position - Position).GetSafeNormal();
		oe.Flipped = FVector::DotProduct(oe.ConNode->Forward, oe.Axis) < 0;

		connectors.Push(oe);
	}

	for (int i = 1; i < Edges.Num(); i++)
	{
		OrdCon oc;
		oc.ConNode = Edges[i].Pin()->OtherNode(this).Pin();
		oc.Axis = (oc.ConNode->Position - Position).GetSafeNormal();
		oc.Angle = Util::SignedAngle(oc.Axis, connectors[0].Axis, CachedUp);
		oc.Flipped = FVector::DotProduct(oc.ConNode->Forward, oc.Axis) < 0;

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
		if (oc.Flipped)
		{
			Swap(oc.QuartersMap[0], oc.QuartersMap[3]);
			Swap(oc.QuartersMap[1], oc.QuartersMap[2]);
		}

		// if the incoming connection is the opposite way up
		if (FVector::DotProduct(oc.ConNode->CachedTransform.GetUnitAxis(EAxis::Z), CachedTransform.GetUnitAxis(EAxis::Z)) < 0)
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
			verts_top.Push(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::RoadbedInner, oc.QuartersMap[3], oc.ConNode->CachedTransform));
			verts_top.Push(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::RoadbedInner, oc.QuartersMap[0], oc.ConNode->CachedTransform));

			verts_bot.Push(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::RoadbedInner, oc.QuartersMap[2], oc.ConNode->CachedTransform));
			verts_bot.Push(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::RoadbedInner, oc.QuartersMap[1], oc.ConNode->CachedTransform));
		}

		Util::AddPolyToMesh(mesh, verts_top, PGCEdgeType::Rounded);
		Util::AddPolyToMeshReversed(mesh, verts_bot, PGCEdgeType::Rounded);
	}

	{
		// faces between connectors around the outside
		auto prev_oc = connectors.Last();

		for (const auto& oc : connectors)
		{
			{
				TArray<FVector> verts;

				verts.Push(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::BarrierTopOuter, oc.QuartersMap[3], oc.ConNode->CachedTransform));
				verts.Push(prev_oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::BarrierTopOuter, prev_oc.QuartersMap[0], prev_oc.ConNode->CachedTransform));
				verts.Push(prev_oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::RoadbedOuter, prev_oc.QuartersMap[0], prev_oc.ConNode->CachedTransform));
				verts.Push(prev_oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::RoadbedOuter, prev_oc.QuartersMap[1], prev_oc.ConNode->CachedTransform));
				verts.Push(prev_oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::BarrierTopOuter, prev_oc.QuartersMap[1], prev_oc.ConNode->CachedTransform));
				verts.Push(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::BarrierTopOuter, oc.QuartersMap[2], oc.ConNode->CachedTransform));
				verts.Push(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::RoadbedOuter, oc.QuartersMap[2], oc.ConNode->CachedTransform));
				verts.Push(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::RoadbedOuter, oc.QuartersMap[3], oc.ConNode->CachedTransform));

				Util::AddPolyToMesh(mesh, verts, PGCEdgeType::Rounded);
			}

			C2CFacePair(mesh,
				prev_oc.ConNode->Profile, oc.ConNode->Profile,
				prev_oc.QuartersMap, oc.QuartersMap,
				prev_oc.ConNode->CachedTransform, oc.ConNode->CachedTransform,
				ParameterisedProfile::VertTypes::OverhangEndOuter, ParameterisedProfile::VertTypes::BarrierTopOuter,
				PGCEdgeType::Sharp, PGCEdgeType::Rounded);

			C2CFacePair(mesh,
				prev_oc.ConNode->Profile, oc.ConNode->Profile,
				prev_oc.QuartersMap, oc.QuartersMap,
				prev_oc.ConNode->CachedTransform, oc.ConNode->CachedTransform,
				ParameterisedProfile::VertTypes::OverhangEndInner, ParameterisedProfile::VertTypes::OverhangEndOuter,
				PGCEdgeType::Sharp, PGCEdgeType::Rounded);

			C2CFacePair(mesh,
				prev_oc.ConNode->Profile, oc.ConNode->Profile,
				prev_oc.QuartersMap, oc.QuartersMap,
				prev_oc.ConNode->CachedTransform, oc.ConNode->CachedTransform,
				ParameterisedProfile::VertTypes::BarrierTopInner, ParameterisedProfile::VertTypes::OverhangEndInner,
				PGCEdgeType::Sharp, PGCEdgeType::Rounded);

			C2CFacePair(mesh,
				prev_oc.ConNode->Profile, oc.ConNode->Profile,
				prev_oc.QuartersMap, oc.QuartersMap,
				prev_oc.ConNode->CachedTransform, oc.ConNode->CachedTransform,
				ParameterisedProfile::VertTypes::RoadbedInner, ParameterisedProfile::VertTypes::BarrierTopInner,
				PGCEdgeType::Sharp, PGCEdgeType::Rounded);

			prev_oc = oc;
		}
	}
}

void SNode::RecalcForward()
{
	// to avoid having to optimize a separate "forward" parameter
	// (
	//   which after some experimentation with doing that for up
	//   seems to be a fraught activity due to the fact that it is a normal vector,
	//   rather than trying to get the optimizer to keep it normal, I was letting it range over the whole
	//   (-1 -> 1) ^ 3 space and normalizing, which means there is a singularity in the middle :-(
	// )
	// forward is defined as:
	// - no edges							-> invariant (catch-all for incomplete graphs)
	// - root node							-> towards first child
	// - node with only one (parent) edge	-> away from parent
	// - node with two edges				-> from parent towards the other connected node
	// - node with > two edges				-> from parent towards average of the other nodes

	if (!Edges.Num())
		return;

	if (!Parent.IsValid())
	{
		Forward = Edges[0].Pin()->OtherNode(this).Pin()->Position - Position;
	}
	else if (Edges.Num() == 1)
	{
		Forward = Position - Parent->Position;
	}
	else
	{
		FVector avg(0, 0, 0);

		// Edges[0] points at the p
		for (int i = 1; i < Edges.Num(); i++)
		{
			avg += Edges[i].Pin()->OtherNode(this).Pin()->Position;
		}

		avg /= (Edges.Num() - 1);

		Forward = avg - Parent->Position;
	}

	Forward.Normalize();
}

void SNode::CalcRotation()
{
	// should we always start orthogonal?
	// no, because we can have just changed forward
	// and we wouldn't recalculate cached-up necessarily until now...
	//check(FMath::Abs(FVector::DotProduct(CachedUp, Forward)) < 1e-4f);

	auto here_up_ref = ProjectParentUp();

	check(here_up_ref.IsNormalized());

	auto plane_check = FVector::DotProduct(here_up_ref, Forward);

	check(FMath::Abs(plane_check) < 1e-4f);

	Rotation = Util::SignedAngle(here_up_ref, CachedUp, Forward);

	FVector keep = CachedUp;
	ApplyRotation();
	check(FMath::Abs(FVector::DotProduct(CachedUp, Forward)) < 1e-4f);
	check((keep - CachedUp).Size() < 1e-1f);
}

void SNode::ApplyRotation()
{
	auto rot = Util::AxisAngleToQuaternion(Forward, Rotation);

	auto here_up_ref = ProjectParentUp();

	CachedUp = rot.RotateVector(here_up_ref);
}

const FVector SNode::ProjectParentUp() const
{
	FVector parent_up;
	FVector parent_forward;

	if (!Parent.IsValid())
	{
		// for the moment, trying to allow root node to rotate, but that has to be relative to something
		// if we ever see it aligned up or down Z this will give us gimble-lock :-o
		parent_up.Set(0, 0, 1);
		parent_forward.Set(0, 1, 0);
	}
	else
	{
		parent_up = Parent->CachedUp;
		parent_forward = Parent->Forward;
	}

	auto proj_up = Util::ProjectOntoPlane(parent_up, Forward);

	// 0.3 is an arbitrary number, we could probably go much closer to zero...
	if (proj_up.GetAbsMax() < 0.3f)
	{
		// we know that parent_up is too close to orthogonal to our Forward
		// so we want to step through an intermediate 45 degree vector compromising between the two forwards
		// HOWEVER both (Forward + parent_forward) and (Forward - parent_forward) have that property
		// (being roughly 45 degrees one way and 45 degrees the other way from Forward)
		// one of them will flip the sign of up and one won't, we need to pick the right one
		// or rather, if we picked the wrong one, we just need to negate our final answer
		// see "correct sign"
		auto av_forward = (Forward - parent_forward);

		auto int_up = Util::ProjectOntoPlane(parent_up, av_forward.GetSafeNormal());
		check(int_up.Size() > 0.3f);

		// the component of parent up normal to our forwards vector
		proj_up = Util::ProjectOntoPlane(int_up.GetSafeNormal(), Forward);
		check(proj_up.Size() > 0.3f);
	}

	if (FVector::DotProduct(proj_up, CachedUp) < 0)
	{
		// correct sign...
		proj_up = -proj_up;
	}

	return proj_up.GetSafeNormal();
}

PRAGMA_ENABLE_OPTIMIZATION