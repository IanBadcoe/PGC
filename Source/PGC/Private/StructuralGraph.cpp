#include "StructuralGraph.h"

#include "Util.h"
#include "SetupOptFunction.h"

#include "IntermediateGraph.h"

PRAGMA_DISABLE_OPTIMIZATION

namespace StructuralGraph
{

using namespace Profile;
using namespace SetupOpt;

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

	// we'll need the radii to copy to the INodes in a moment, so let's compute radii for what we've got...
	for (auto &n : Nodes)
	{
		n->FindRadius();
	}

	auto i_graph = IntermediateOptimize(input);

	// copy node positions back from IGraph to us
	for (const auto& n : i_graph->Nodes)
	{
		if (n->MD.Source.IsValid())
		{
			n->MD.Source->Position = n->Position;

			// refresh the up-vector on junctions for how it may have rotated
			if (n->MD.Type == INodeType::Junction) {
				TArray<FVector> rel_verts;

				for (const auto& e : n->Edges)
				{
					// makes no difference to the normal making verts relative (except maybe improving precision)
					// and we need them relative for the angles
					rel_verts.Emplace(e.Pin()->OtherNode(n).Pin()->Position - n->Position);
				}

				n->MD.Source->CachedUp = Util::NewellPolyNormal(rel_verts);

				// and set its connectors the same way up
				for (const auto& e : n->MD.Source->Edges)
				{
					e.Pin()->OtherNode(n->MD.Source).Pin()->CachedUp = n->MD.Source->CachedUp;
				}
			}
		}
	}

	// fill-out our connections using the intermediate points from the IGraph
	for (const auto& conn : i_graph->MD.IntermediatePoints)
	{
		auto from_c = conn.FromConn;
		auto to_c = conn.ToConn;

		auto profiles = _ProfileSource->GetCompatibleProfileSequence(from_c->Profile, to_c->Profile,
			conn.Edge->Divs);

		auto int_pos1 = conn.Intermediate1Node->Position;
		auto int_pos2 = conn.Intermediate2Node->Position;

		if (DM != DebugMode::IntermediateSkeleton)
		{
			ConnectAndFillOut(from_c, to_c, int_pos1, int_pos2,
				conn.Edge->Divs, conn.Edge->Twists,
				input->SegLength, profiles);
		}
		else
		{
			auto int1n = MakeShared<SNode>(profiles[0], SNode::Type::Connection);
			int1n->Position = int_pos1;
			int1n->CachedUp = from_c->CachedUp;
			int1n->Forward = (int_pos2 - int_pos1).GetSafeNormal();
			Nodes.Push(int1n);

			auto length = conn.Edge->Divs * input->SegLength;

			Connect(from_c, int1n, length / 3);

			auto int2n = MakeShared<SNode>(profiles[0], SNode::Type::Connection);
			int2n->Position = int_pos2;
			int2n->CachedUp = to_c->CachedUp;
			int2n->Forward = (to_c->Position - int_pos2).GetSafeNormal();
			Nodes.Push(int2n);

			Connect(int1n, int2n, length / 3);
			Connect(int2n, to_c, length / 3);
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

	// this will contain the nodes in an order suitable for updating from
	// e.g. everything will be later in the list that its parent
	TArray<TSharedPtr<SNode>> new_order;

	auto orig_size = Nodes.Num();

	while(Nodes.Num())
	{
		MakeIntoDagInner(Nodes[0], Closed, new_order);
	}

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

void StructuralGraph::SGraph::MakeMeshReal(TSharedPtr<Mesh> mesh) const
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
				TArray<FVector> poly{
					verts_to[i],
					verts_to[prev_vert],
					verts_from[prev_vert],
					verts_from[i], };

				// building a poly like this:
				//
				//   prev_vert       i
				//   |               |
				//   v               v
				//   +-- (rounded) --+     <- edge in from_n->Profile
				//   |               |
				//   |               |     (these two edges sharpness from from_n->Profile)
				//   |               |
				//   |               |
				//   +-- (rounded) --+     <- edge in to_n->Profile
				//
				// redundant faces generated by zero lengths in profile are removed inside AddPolyToMesh
				//
				// edges from the profile are all set to PGCEdgeType::Rounded since what we are building here is
				// an extrusion that should be smooth along its length
				// (PGCEdgeType::Auto would be an alternative, to put corners only on sharp angles...)
				//
				// the other two edges sharpness comes from from_n->Profile
				Util::AddPolyToMesh(mesh, poly,
					{
						PGCEdgeType::Rounded,
						from_n->Profile->GetOutgoingEdgeType(prev_vert),
						PGCEdgeType::Rounded,
						from_n->Profile->GetOutgoingEdgeType(i),
					},
					to_n->Profile->IsDrivable(prev_vert) || from_n->Profile->IsDrivable(prev_vert) ? 1 : 0);

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

void StructuralGraph::SGraph::MakeMeshSkeleton(TSharedPtr<Mesh> mesh) const
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

			poly.Add(from_p + orth1);
			poly.Add(from_p + orth2);
			poly.Add(to_p);

			Util::AddPolyToMesh(mesh, poly, { PGCEdgeType::Sharp, PGCEdgeType::Sharp, PGCEdgeType::Sharp }, 0);
		}

		{
			TArray<FVector> poly;

			poly.Add(from_p - orth1);
			poly.Add(from_p + orth2);
			poly.Add(to_p);

			Util::AddPolyToMeshReversed(mesh, poly, { PGCEdgeType::Sharp, PGCEdgeType::Sharp, PGCEdgeType::Sharp }, 0);
		}

		{
			TArray<FVector> poly;

			poly.Add(from_p + orth1);
			poly.Add(from_p - orth2);
			poly.Add(to_p);

			Util::AddPolyToMeshReversed(mesh, poly, { PGCEdgeType::Sharp, PGCEdgeType::Sharp, PGCEdgeType::Sharp }, 0);
		}

		{
			TArray<FVector> poly;

			poly.Add(from_p - orth1);
			poly.Add(from_p - orth2);
			poly.Add(to_p);

			Util::AddPolyToMesh(mesh, poly, { PGCEdgeType::Sharp, PGCEdgeType::Sharp, PGCEdgeType::Sharp }, 0);
		}

		{
			TArray<FVector> poly;

			poly.Add(from_p - orth2);
			poly.Add(from_p - orth1);
			poly.Add(from_p + orth2);
			poly.Add(from_p + orth1);

			Util::AddPolyToMesh(mesh, poly, { PGCEdgeType::Sharp, PGCEdgeType::Sharp, PGCEdgeType::Sharp, PGCEdgeType::Sharp }, 0);
		}
	}

	for (const auto& n : Nodes)
	{
		auto p1 = n->Position;
		auto p2 = p1 + n->Forward;
		auto p3 = p1 + n->CachedTransform.GetUnitAxis(EAxis::Z) * 3;
		auto p4 = p1 + FVector::CrossProduct(n->Forward, n->CachedUp);

		{
			TArray<FVector> poly;

			poly.Add(p1);
			poly.Add(p2);
			poly.Add(p3);

			Util::AddPolyToMesh(mesh, poly, { PGCEdgeType::Sharp, PGCEdgeType::Sharp, PGCEdgeType::Sharp }, 0);
		}

		{
			TArray<FVector> poly;

			poly.Add(p2);
			poly.Add(p4);
			poly.Add(p3);

			Util::AddPolyToMesh(mesh, poly, { PGCEdgeType::Sharp, PGCEdgeType::Sharp, PGCEdgeType::Sharp }, 0);
		}

		{
			TArray<FVector> poly;

			poly.Add(p4);
			poly.Add(p1);
			poly.Add(p3);

			Util::AddPolyToMesh(mesh, poly, { PGCEdgeType::Sharp, PGCEdgeType::Sharp, PGCEdgeType::Sharp }, 0);
		}

		{
			TArray<FVector> poly;

			poly.Add(p1);
			poly.Add(p4);
			poly.Add(p2);

			Util::AddPolyToMesh(mesh, poly, { PGCEdgeType::Sharp, PGCEdgeType::Sharp, PGCEdgeType::Sharp }, 0);
		}
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

void SGraph::CalcEdgeStartParams(const TSharedPtr<SNode>& from_c, const TSharedPtr<SNode>& to_c,
	const TSharedPtr<SNode>& from_n, const TSharedPtr<SNode>& to_n, float length,
	FVector& out1, FVector& out2)
{
	const auto& from_pos = from_n->Position;
	const auto& to_pos = to_n->Position;

	auto dist = FVector::Distance(from_pos, to_pos);

	// if the end points are further apart than our nominal length
	// take the actual distance as the working length so as to get suitably curvy curves
	// otherwise use the nominal length so as not to build something sillily compressed
	auto working_length = FMath::Max(length, dist);

	auto from_forward = from_c->Position - from_pos;
	auto to_forward = to_c->Position - to_pos;

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

	out1 = intermediate1;
	out2 = intermediate2;
}

double StructuralGraph::SGraph::OptimizeIGraph(TSharedPtr<IGraph> i_graph, double precision, bool final)
{
	auto SOF = MakeShared<SetupOptFunction>(i_graph,
		1.0,		// NodeAngleDistEnergyScale
		1.0,		// EdgeAngleEnergyScale
		1.0,		// PlanarEnergyScale
		1.0,		// LengthEnergyScale
		1.0,		// EdgeEdgeEnergyScale

		3.0			// edge radius scale
	);

	NlOptWrapper opt(SOF);

	double ret;

	opt.RunOptimization(true, final ? 50 : -1, precision, &ret);

	return ret;
}

static INodeType TranslateType(SNode::Type t) {
	switch (t) {
	case SNode::Type::Junction:
		return INodeType::Junction;
	case SNode::Type::JunctionConnector:
		return INodeType::JunctionConnector;
	case SNode::Type::Connection:
		return INodeType::Connection;
	default:
		check(false);

		return INodeType::Junction;
	}
}

TSharedPtr<StructuralGraph::IGraph> StructuralGraph::SGraph::IntermediateOptimize(TSharedPtr<LayoutGraph::Graph> input)
{
	auto i_graph = MakeShared<IGraph>();

	// make an IGraph node for every node (Junction and Connector) that we have...
	for (const auto& n : Nodes)
	{
		auto new_junction = MakeShared<INode>(n->Radius, n->Position, INodeMetaData{ n, TranslateType(n->MyType) } );
		i_graph->Nodes.Push(new_junction);
	}

	// add IEdges in the same way
	for (const auto& e : Edges)
	{
		auto from_node_idx = FindNodeIdx(e->FromNode);
		auto to_node_idx = FindNodeIdx(e->ToNode);

		i_graph->Connect(i_graph->Nodes[from_node_idx], i_graph->Nodes[to_node_idx], e->D0);
	}

	// for each edge in the input, add a couple of intermediate points to the IGraph
	// and connect them with the desired edge lengths

	// this is really hard to understand, because in input edges and connectors are different classes on the
	// node, but here, and in IGraph, connectors are nodes in their own right (and attached to the original nodes
	// ("junctions") by more edges) so what we are doing is
	// for each node in the input
	//   for each connector on that node
	//     if it has an edge attached
	//       find the corresponding four nodes (from, from-connector, to, to-connector) in this object
	//       (that we made just above)
	//       then from their indices find the corresponding nodes in the IGraph
	//       then add two extra nodes to the IGraph (representing corners in the edge)
	//       and connect from-connector -> int1 -> int2 -> to-connector
	//       setting the lengths to 1/3 of the total desired length on each
	//
	//       and store some meta-data about what we did in IntermediatePoints, so we know how to read the intermediate positions back
	//       after optimization

	for (int i = 0; i < input->GetNodes().Num(); i++)
	{
		// the start of Nodes correspond to the nodes in input
		auto here_from_node = Nodes[i];
		auto input_node = input->GetNodes()[i];

		for (int from_conn_idx = 0; from_conn_idx < input_node->Edges.Num(); from_conn_idx++)
		{
			auto input_edge = input_node->Edges[from_conn_idx].Pin();

			if (input_edge.IsValid())
			{
				if (input_edge->FromNode == input_node)
				{
					auto input_from_node = input_edge->FromNode.Pin();

					auto input_to_node = input_edge->ToNode.Pin();
					auto to_node_idx = input->FindNodeIdx(input_to_node);
					auto from_node_idx = input->FindNodeIdx(input_from_node);

					auto to_conn_idx = input_to_node->FindConnectorIdx(input_edge->ToConnector.Pin());

					// loop-back edges should only be considered in one direction
					if (input_edge->ToNode == input_edge->FromNode && to_conn_idx == from_conn_idx)
						continue;

					auto here_from_conn_node = here_from_node->Edges[from_conn_idx].Pin()->ToNode.Pin();

					auto here_to_node = Nodes[to_node_idx];
					auto here_to_conn_node = here_to_node->Edges[to_conn_idx].Pin()->ToNode.Pin();

					auto here_to_conn_idx = FindNodeIdx(here_to_conn_node);
					auto here_from_conn_idx = FindNodeIdx(here_from_conn_node);

					FVector int1, int2;

					CalcEdgeStartParams(here_from_conn_node, here_to_conn_node, here_from_node, here_to_node,
						input_edge->Divs * input->SegLength, int1, int2);

					i_graph->MD.IntermediatePoints.AddDefaulted(1);
					auto& cc = i_graph->MD.IntermediatePoints.Last();

					cc.FromConn = here_from_conn_node;
					cc.ToConn = here_to_conn_node;

					cc.Edge = input_edge;

					auto new_int1 = MakeShared<INode>(here_from_node->Radius, int1, INodeMetaData());
					i_graph->Nodes.Push(new_int1);
					cc.Intermediate1Node = new_int1;

					auto new_int2 = MakeShared<INode>(here_to_node->Radius, int2, INodeMetaData());
					i_graph->Nodes.Push(new_int2);
					cc.Intermediate2Node = new_int2;

					auto length = cc.Edge->Divs * input->SegLength;

					i_graph->Connect(i_graph->Nodes[here_from_conn_idx], new_int1, length / 3);
					i_graph->Connect(new_int1, new_int2, length / 3);
					i_graph->Connect(new_int2, i_graph->Nodes[here_to_conn_idx], length / 3);
				}
			}
		}
	}

	// just to get a reasonable estimate of the required bound size
	OptimizeIGraph(i_graph, 0.01, true);

	static const auto NumSpecies = 10;
	static const auto SpeciesSize = 2;
	static const auto GenerationsPerLevel = 4;
	static const auto Mutations = 1;

	TArray<TArray<TSharedPtr<IGraph>>> all_species{ {i_graph} };
	all_species.AddDefaulted(NumSpecies - 1);

	auto box = i_graph->CalcBoundingBox();

	FVector c, e;
	box.GetCenterAndExtents(c, e);

	auto scale = e.GetAbsMax();

	// expand the box to its maximum size on all axes
	auto expand_factor = (FVector{ scale, scale, scale } - e);
	box = box.ExpandBy(expand_factor);

	for (auto& pop : all_species)
	{
		while (pop.Num() < SpeciesSize)
		{
			pop.Push(i_graph->Randomize(box));
		}
	}

	struct GEnergyDiffer {
		bool operator()(const TSharedPtr<IGraph>& a, const TSharedPtr<IGraph>& b) const
		{
			return a->MD.Energy < b->MD.Energy;
		}
	};

	for (const auto p : { 0.1, 0.01, 0.001, 0.0001, 0.00001 })
	{
		UE_LOG(LogTemp, Warning, TEXT("Genetic algorithm optimizing to: %f "), p);
		UE_LOG(LogTemp, Warning, TEXT("Optimizing initial all_species"), p);

		// optimize whole Species to current target gradient
		for (const auto& pop : all_species)
		{
			for (auto& g : pop)
			{
				g->MD.Energy = OptimizeIGraph(g, p, false);
			}
		}

		for (auto& pop : all_species)
		{
			pop.Sort(GEnergyDiffer());

			UE_LOG(LogTemp, Warning, TEXT("---"));

			for (auto& g : pop)
			{
				UE_LOG(LogTemp, Warning, TEXT("Individual Energy: %f"), g->MD.Energy);
			}
		}

		for (int i = 0; i < GenerationsPerLevel; i++)
		{
			for (auto pop_idx = 0; pop_idx < SpeciesSize; pop_idx++)
			{
				auto parent_idx = FMath::RandRange(0, SpeciesSize - 1);

				const auto& parent = all_species[pop_idx][parent_idx];

				auto new_individual = MakeShared<IGraph>(*parent);

				for (int j = 0; j < Mutations; j++)
				{
					auto& node = new_individual->Nodes[FMath::RandRange(0, new_individual->Nodes.Num() - 1)];
					if (FMath::RandRange(0.0f, 1.0f) > 0.5f && node->MD.Source.IsValid() && node->MD.Type == INodeType::Junction)
					{
						auto eidx1 = FMath::RandRange(0, node->Edges.Num() - 1);
						int eidx2;

						do
						{
							eidx2 = FMath::RandRange(0, node->Edges.Num() - 1);
						} while (eidx1 == eidx2);

						Swap(node->Edges[eidx1].Pin()->OtherNode(node).Pin()->Position,
							node->Edges[eidx2].Pin()->OtherNode(node).Pin()->Position);
					}
					else
					{
						node->Position = FMath::RandPointInBox(box);
					}

				}

				new_individual->MD.Energy = OptimizeIGraph(new_individual, p, false);

				if (new_individual->MD.Energy < all_species[pop_idx].Last()->MD.Energy)
				{
					all_species[pop_idx].Last() = new_individual;

					all_species[pop_idx].Sort(GEnergyDiffer());

					UE_LOG(LogTemp, Warning, TEXT("New Individual in species: %d, at %f"), pop_idx, new_individual->MD.Energy);
				}
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("Final populations"));

	TArray<TSharedPtr<IGraph>> temp;

	for (const auto& pop : all_species)
	{
		temp.Push(pop[0]);

		UE_LOG(LogTemp, Warning, TEXT("---"));

		for (auto& g : pop)
		{
			UE_LOG(LogTemp, Warning, TEXT("Energy: %f"), g->MD.Energy);
		}
	}

	temp.Sort(GEnergyDiffer());

	auto energy = OptimizeIGraph(temp[0], 0.0000001, true);

	return temp[0];
}

void SGraph::ConnectAndFillOut(TSharedPtr<SNode> from_c, TSharedPtr<SNode> to_c,
	const FVector& int1, const FVector& int2, 
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

		FVector pos = SplineUtil::CubicBezier(t, from_pos, int1, int2, to_pos);

		FVector forward = SplineUtil::CubicBezierTangent(t, from_pos, int1, int2, to_pos);

		forward.Normalize();

		auto right = FVector::CrossProduct(forward, current_up);

		right.Normalize();

		current_up = FVector::CrossProduct(right, forward);

		current_up.Normalize();

		frames.Add({pos, current_up, forward });
	}

	auto angle_mismatch = Util::SignedAngle(to_up, frames.Last().up, frames.Last().forward, false);

	angle_mismatch += twists * PI;

	//// take this one further than required, so we can assert we arrived the right way up
	for (auto i = 0; i <= divs; i++) {
		float t = (float)i / divs;

		auto angle_correct = t * -angle_mismatch;

		FVector forward = SplineUtil::CubicBezierTangent(t, from_pos, int1, int2, to_pos);

		forward.Normalize();

		frames[i].up = FTransform(FQuat(forward, angle_correct)).TransformVector(frames[i].up);

		// up and forward really should still be normal...
		check(FMath::Abs(FVector::DotProduct(frames[i].forward, frames[i].up)) < 1e-4f);
	}

	auto angle_check = Util::SignedAngle(to_up, frames.Last().up, frames.Last().forward, false);

	bool rolling = (twists & 1) != 0;

	// we expect to come out either 100% up or 100% down, according to whether we have an odd number of half-twists or not
	// 5 degrees is quite tolerant but it shouldn't matter too much
	if (rolling)
	{
		check(FMath::Abs(angle_check) > 175.0f / 180.0f * PI);
	}
	else
	{
		check(FMath::Abs(angle_check) < 5.0f / 180.0f * PI);
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

int SGraph::FindNodeIdx(const TWeakPtr<SNode>& node) const
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

void SGraph::MakeMesh(TSharedPtr<Mesh> mesh) const
{
	mesh->Clear();

	RefreshTransforms();

	if (DM != DebugMode::Normal)
	{
		MakeMeshSkeleton(mesh);
	}
	else
	{
		MakeMeshReal(mesh);
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
	PGCEdgeType in_profile_edge_type, PGCEdgeType between_profile_edge_type,
	int channel)
{
	{
		TArray<FVector> verts;

		verts.Push(to_profile->GetTransformedVert(first_vert, to_quarters_map[3], to_trans));
		verts.Push(from_profile->GetTransformedVert(first_vert, from_quarters_map[0], from_trans));
		verts.Push(from_profile->GetTransformedVert(second_vert, from_quarters_map[0], from_trans));
		verts.Push(to_profile->GetTransformedVert(second_vert, to_quarters_map[3], to_trans));

		Util::AddPolyToMesh(mesh, verts,
			{ between_profile_edge_type, in_profile_edge_type, between_profile_edge_type, in_profile_edge_type },
			channel);
	}

	{
		TArray<FVector> verts;

		verts.Push(to_profile->GetTransformedVert(second_vert, to_quarters_map[2], to_trans));
		verts.Push(from_profile->GetTransformedVert(second_vert, from_quarters_map[1], from_trans));
		verts.Push(from_profile->GetTransformedVert(first_vert, from_quarters_map[1], from_trans));
		verts.Push(to_profile->GetTransformedVert(first_vert, to_quarters_map[2], to_trans));

		Util::AddPolyToMesh(mesh, verts,
			{ between_profile_edge_type, in_profile_edge_type, between_profile_edge_type, in_profile_edge_type },
			channel);
	}
}

inline void SNode::AddToMesh(TSharedPtr<Mesh> mesh) const {
	// unused nodes disappear for the moment...
	if (Edges.Num() == 0)
		return;

	// order connectors by angle around up vector

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
		oc.Angle = Util::SignedAngle(oc.Axis, connectors[0].Axis, CachedUp, true);
		oc.Flipped = FVector::DotProduct(oc.ConNode->Forward, oc.Axis) < 0;

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

			oc.UpsideDown = true;
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

		Util::AddPolyToMesh(mesh, verts_top, PGCEdgeType::Rounded, 1);
		Util::AddPolyToMeshReversed(mesh, verts_bot, PGCEdgeType::Rounded, 1);
	}

	{
		auto prev_oc = connectors.Last();

		for (const auto& oc : connectors)
		{
			// between connectors around the outside, joining top and bottom into one
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

				Util::AddPolyToMesh(mesh, verts, PGCEdgeType::Auto, 0);
			}

			C2CFacePair(mesh,
				prev_oc.ConNode->Profile, oc.ConNode->Profile,
				prev_oc.QuartersMap, oc.QuartersMap,
				prev_oc.ConNode->CachedTransform, oc.ConNode->CachedTransform,
				ParameterisedProfile::VertTypes::OverhangEndOuter, ParameterisedProfile::VertTypes::BarrierTopOuter,
				PGCEdgeType::Auto, PGCEdgeType::Auto, 0);

			C2CFacePair(mesh,
				prev_oc.ConNode->Profile, oc.ConNode->Profile,
				prev_oc.QuartersMap, oc.QuartersMap,
				prev_oc.ConNode->CachedTransform, oc.ConNode->CachedTransform,
				ParameterisedProfile::VertTypes::OverhangEndInner, ParameterisedProfile::VertTypes::OverhangEndOuter,
				PGCEdgeType::Auto, PGCEdgeType::Auto, 0);

			C2CFacePair(mesh,
				prev_oc.ConNode->Profile, oc.ConNode->Profile,
				prev_oc.QuartersMap, oc.QuartersMap,
				prev_oc.ConNode->CachedTransform, oc.ConNode->CachedTransform,
				ParameterisedProfile::VertTypes::BarrierTopInner, ParameterisedProfile::VertTypes::OverhangEndInner,
				PGCEdgeType::Auto, PGCEdgeType::Auto, 0);

			// between connectors connecting the inner barrier edges
			C2CFacePair(mesh,
				prev_oc.ConNode->Profile, oc.ConNode->Profile,
				prev_oc.QuartersMap, oc.QuartersMap,
				prev_oc.ConNode->CachedTransform, oc.ConNode->CachedTransform,
				ParameterisedProfile::VertTypes::RoadbedInner, ParameterisedProfile::VertTypes::BarrierTopInner,
				PGCEdgeType::Auto, PGCEdgeType::Auto, 0);

			prev_oc = oc;
		}
	}

	// much simpler now, if all connectors are for tunnels, we'll fill-in the missing triangle to close the node ceiling
	// (taking out the overhang-end-inner/out rectangles above)
	AddCeilingToMesh(mesh, connectors, true);
	AddCeilingToMesh(mesh, connectors, false);

	// fill-in any unused connector ends
	for (const auto& oc : connectors)
	{
		if (oc.ConNode->Edges.Num() == 1)
		{
			for (int qtr = 0; qtr < 4; qtr++)
			{
				{
					TArray<FVector> verts;

					verts.Add(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::RoadbedInner, qtr, oc.ConNode->CachedTransform));
					verts.Add(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::BarrierTopInner, qtr, oc.ConNode->CachedTransform));
					verts.Add(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::BarrierTopOuter, qtr, oc.ConNode->CachedTransform));
					verts.Add(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::RoadbedOuter, qtr, oc.ConNode->CachedTransform));

					if (oc.Flipped ^ (qtr & 1))
					{
						Util::AddPolyToMeshReversed(mesh, verts, PGCEdgeType::Sharp, 0);
					}
					else
					{
						Util::AddPolyToMesh(mesh, verts, PGCEdgeType::Sharp, 0);
					}
				}

				{
					TArray<FVector> verts;

					verts.Add(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::BarrierTopInner, qtr, oc.ConNode->CachedTransform));
					verts.Add(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::OverhangEndInner, qtr, oc.ConNode->CachedTransform));
					verts.Add(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::OverhangEndOuter, qtr, oc.ConNode->CachedTransform));
					verts.Add(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::BarrierTopOuter, qtr, oc.ConNode->CachedTransform));

					if (oc.Flipped ^ (qtr & 1))
					{
						Util::AddPolyToMeshReversed(mesh, verts, PGCEdgeType::Sharp, 0);
					}
					else
					{
						Util::AddPolyToMesh(mesh, verts, PGCEdgeType::Sharp, 0);
					}
				}
			}

			TArray<FVector> verts;

			verts.Add(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::RoadbedOuter, 0, oc.ConNode->CachedTransform));
			verts.Add(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::RoadbedInner, 0, oc.ConNode->CachedTransform));
			verts.Add(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::RoadbedInner, 3, oc.ConNode->CachedTransform));
			verts.Add(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::RoadbedOuter, 3, oc.ConNode->CachedTransform));
			verts.Add(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::RoadbedOuter, 2, oc.ConNode->CachedTransform));
			verts.Add(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::RoadbedInner, 2, oc.ConNode->CachedTransform));
			verts.Add(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::RoadbedInner, 1, oc.ConNode->CachedTransform));
			verts.Add(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::RoadbedOuter, 1, oc.ConNode->CachedTransform));

			if (!oc.Flipped)
			{
				Util::AddPolyToMeshReversed(mesh, verts, PGCEdgeType::Sharp, 0);
			}
			else
			{
				Util::AddPolyToMesh(mesh, verts, PGCEdgeType::Sharp, 0);
			}
		}
	}
}

void StructuralGraph::SNode::AddCeilingToMesh(TSharedPtr<Mesh> mesh, const TArray<OrdCon>& connectors, bool top) const
{
	// only proceed if we are completely closed
	for (const auto& conn : connectors)
	{
		// this seems the wrong way up to me, top should be top when we're not upsidedown, but
		// this works and the other doesn't
		if (conn.ConNode->Profile->IsOpenCeiling(top == conn.UpsideDown))
		{
			return;
		}
	}

	int qforward = top ? 0 : 1;
	int qbackward = top ? 3 : 2;

	// first delete existing overhang-inner
	// adding it again the other way around deletes it...

	auto prev_oc = connectors.Last();

	for (const auto& oc : connectors)
	{
		TArray<FVector> verts;

		verts.Push(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::OverhangEndInner, oc.QuartersMap[qbackward], oc.ConNode->CachedTransform));
		verts.Push(prev_oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::OverhangEndInner, prev_oc.QuartersMap[qforward], prev_oc.ConNode->CachedTransform));
		verts.Push(prev_oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::OverhangEndOuter, prev_oc.QuartersMap[qforward], prev_oc.ConNode->CachedTransform));
		verts.Push(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::OverhangEndOuter, oc.QuartersMap[qbackward], oc.ConNode->CachedTransform));

		if (!top)
		{
			Util::AddPolyToMesh(mesh, verts, PGCEdgeType::Auto, 0);
		}
		else
		{
			Util::AddPolyToMeshReversed(mesh, verts, PGCEdgeType::Auto, 0);
		}

		prev_oc = oc;
	}

	// now add-in the outer triangle
	{
		TArray<FVector> verts;

		// it doesn't matter which of qforward or qbackward we use, because in a closed profile they are both in the same place

		for (const auto& oc : connectors)
		{
			verts.Push(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::OverhangEndOuter, oc.QuartersMap[qforward], oc.ConNode->CachedTransform));
		}

		if (!top)
		{
			Util::AddPolyToMeshReversed(mesh, verts, PGCEdgeType::Auto, 0);
		}
		else
		{
			Util::AddPolyToMesh(mesh, verts, PGCEdgeType::Auto, 0);
		}
	}

	// now add-in the inner triangle
	{
		TArray<FVector> verts;

		// it doesn't matter which of qforward or qbackward we use, because in a closed profile they are both in the same place

		for (const auto& oc : connectors)
		{
			verts.Push(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::OverhangEndInner, oc.QuartersMap[qforward], oc.ConNode->CachedTransform));
		}

		if (top)
		{
			Util::AddPolyToMeshReversed(mesh, verts, PGCEdgeType::Auto, 0);
		}
		else
		{
			Util::AddPolyToMesh(mesh, verts, PGCEdgeType::Auto, 0);
		}
	}

	//{
	//	// to handle connectors with closed ceilings
	//	// (where we would like the node to have a closed ceiling too)
	//	// 1) find contiguous sequences of closed connectors
	//	// 2) draw each of those as:
	//	// 2a) a ceiling-top polygon that goes left-barrier-top-outer -> shared-overhang-end-outer -> right-barrier-top-outer -> (next connector)
	//	// 2b) a ceiling-bottom polygon that goes left-barrier-top-inner -> shared-overhang-end-inner -> right-barrier-top-inner -> (next connector)
	//	// 2c) (if that doesn't come full circle) close the open vertical edge
	//	//
	//	// e.g. if we have:
	//	//
	//	//                  +---+---+    <--- Connector1, closed ceiling
	//	//                /           \
	//	//               /             \
	//	//              /               \
	//	//             +                 +
	//	//              \               /
	//	//               +             +    Connector3                            
	//	// Connector2,                      open
	//	// open            +         +
	//	//                  \       /
	//	//                   +-----+
	//	//
	//	// Then there are two 2a polygons:
	//	// Two 2b polygons (the same shape, but beneath those and a little smaller)
	//	// And two 2c polygons (joining the open ends, shown edge-on)
	//	//
	//	//                  +---+---+
	//	//                /           \
	//	//               /             \
	//	//              /      2a/b     \
	//	//             +                 +
	//	//              \               /
	//	//               +-------------+
	//	//                    ^
	//	//        2 X 2c ----<
	//	//                    v
	//	//                 +---------+
	//	//                  \  2a/b /
	//	//                   +-----+
	//	//

	//	// first find a connector with an open ceiling, if there is one

	//	// if we don't find one, then we can start anywhere...
	//	int open_conn = 0;

	//	for (int i = 0; i < connectors.Num(); i++)
	//	{
	//		const auto& oc = connectors[i];

	//		// this seems the wrong way up to me, top should be top when we're not upsidedown, but...
	//		if (oc.ConNode->Profile->IsOpenCeiling(top == oc.UpsideDown))
	//		{
	//			open_conn = i;
	//			break;
	//		}
	//	}

	//	TArray<TArray<OrdCon>> subsets;
	//	TArray<OrdCon> subset{ connectors[open_conn] };

	//	for (int i = open_conn + 1; i < open_conn + connectors.Num() + 1; i++)
	//	{
	//		auto hi = i % connectors.Num();

	//		const auto& oc = connectors[hi];

	//		subset.Push(oc);

	//		// this seems the wrong way up to me, top should be top when we're not upsidedown, but...
	//		if (oc.ConNode->Profile->IsOpenCeiling(top == oc.UpsideDown))
	//		{
	//			subsets.Push(subset);

	//			subset = TArray<OrdCon>{ oc };
	//		}
	//	}

	//	if (subset.Num() > 1)
	//	{
	//		subsets.Push(subset);
	//	}

	//	// at this point subsets contains up to N sequences of OCs
	//	// where each sequence begins and ends with an open one (if there is one)
	//	// and the start and end OCs appear in previous and following sequences, e.g. it 0 and 2 are open, we will have:
	//	// 0 -> 1 -> 2
	//	// 2 -> 1
	//	// these represent the sub-parts that the ceiling will be split into 

	//	int qforward = top ? 0 : 1;
	//	int qbackward = top ? 3 : 2;

	//	for (const auto& seq : subsets)
	//	{
	//		// outside of ceiling
	//		{
	//			TArray<FVector> verts;

	//			for (int i = 0; i < seq.Num(); i++)
	//			{
	//				const auto& oc = seq[i];

	//				if (i != 0)
	//				{
	//					verts.Push(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::BarrierTopOuter, oc.QuartersMap[qbackward], oc.ConNode->CachedTransform));
	//					verts.Push(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::OverhangEndOuter, oc.QuartersMap[qbackward], oc.ConNode->CachedTransform));
	//				}

	//				if (i != seq.Num() - 1)
	//				{
	//					verts.Push(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::OverhangEndOuter, oc.QuartersMap[qforward], oc.ConNode->CachedTransform));
	//					verts.Push(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::BarrierTopOuter, oc.QuartersMap[qforward], oc.ConNode->CachedTransform));
	//				}
	//			}

	//			if (top)
	//			{
	//				Util::AddPolyToMesh(mesh, verts, PGCEdgeType::Auto, 0);
	//			}
	//			else
	//			{
	//				Util::AddPolyToMeshReversed(mesh, verts, PGCEdgeType::Auto, 0);
	//			}
	//		}

	//		// inside of ceiling
	//		{
	//			TArray<FVector> verts;

	//			for (int i = 0; i < seq.Num(); i++)
	//			{
	//				const auto& oc = seq[i];

	//				if (i != 0)
	//				{
	//					verts.Push(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::BarrierTopInner, oc.QuartersMap[qbackward], oc.ConNode->CachedTransform));
	//					verts.Push(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::OverhangEndInner, oc.QuartersMap[qbackward], oc.ConNode->CachedTransform));
	//				}

	//				if (i != seq.Num() - 1)
	//				{
	//					verts.Push(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::OverhangEndInner, oc.QuartersMap[qforward], oc.ConNode->CachedTransform));
	//					verts.Push(oc.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::BarrierTopInner, oc.QuartersMap[qforward], oc.ConNode->CachedTransform));
	//				}
	//			}

	//			if (top)
	//			{
	//				Util::AddPolyToMeshReversed(mesh, verts, PGCEdgeType::Auto, 0);
	//			}
	//			else
	//			{
	//				Util::AddPolyToMesh(mesh, verts, PGCEdgeType::Auto, 0);
	//			}
	//		}

	//		{
	//			TArray<FVector> verts;

	//			const auto& oc_first = seq[0];
	//			const auto& oc_last = seq.Last();

	//			verts.Push(oc_first.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::OverhangEndInner, oc_first.QuartersMap[qforward], oc_first.ConNode->CachedTransform));
	//			verts.Push(oc_first.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::OverhangEndOuter, oc_first.QuartersMap[qforward], oc_first.ConNode->CachedTransform));
	//			verts.Push(oc_last.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::OverhangEndOuter, oc_last.QuartersMap[qbackward], oc_last.ConNode->CachedTransform));
	//			verts.Push(oc_last.ConNode->Profile->GetTransformedVert(ParameterisedProfile::VertTypes::OverhangEndInner, oc_last.QuartersMap[qbackward], oc_last.ConNode->CachedTransform));

	//			if (top)
	//			{
	//				Util::AddPolyToMesh(mesh, verts, PGCEdgeType::Auto, 0);
	//			}
	//			else
	//			{
	//				Util::AddPolyToMeshReversed(mesh, verts, PGCEdgeType::Auto, 0);
	//			}
	//		}
	//	}
	//}
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

	Rotation = Util::SignedAngle(here_up_ref, CachedUp, Forward, false);

	FVector keep = CachedUp;
	ApplyRotation();
	check(FMath::Abs(FVector::DotProduct(CachedUp, Forward)) < 1e-4f);
// this was checking that our up didn't move just as a result of calculating Rotation, however
// now we've also moved Forward since CachedUp was calculated
//	check((keep - CachedUp).Size() < 1e-1f);
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

}

PRAGMA_ENABLE_OPTIMIZATION

