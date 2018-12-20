#include "LayoutGraph.h"

#pragma optimize ("", off)

using namespace LayoutGraph;

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


void Graph::MakeMesh(TSharedPtr<Mesh> mesh) const
{
	for (const auto& node : Nodes)
	{
		node->AddToMesh(mesh);
	}

	for (const auto& edge : Edges)
	{
		edge->AddToMesh(mesh);
	}
}

void Graph::Connect(int nodeFrom, int nodeFromConnector, int nodeTo, int nodeToConnector,
	int divs,
	float inSpeed, float outSpeed)
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
	edge->InSpeed = inSpeed;
	edge->OutSpeed = outSpeed;
}

void Graph::ConnectAndFillOut(int nodeFromIdx, int nodeFromconnectorIdx, int nodeToIdx, int nodeToconnectorIdx,
	int divs, float inSpeed, float outSpeed)
{
	auto from_n = Nodes[nodeFromIdx];
	auto to_n = Nodes[nodeToIdx];

	// for the moment, for simplicity, support only the same connector type at either end...
	auto from_c = from_n->Connectors[nodeFromconnectorIdx];
	auto to_c = to_n->Connectors[nodeToconnectorIdx];

	// currently insist on same type of connector at either end
	check(&from_c->Definition == &to_c->Definition);

	FTransform from_trans = from_c->Transform * from_n->Position;
	FTransform to_trans = to_c->Transform * to_n->Position;

	// "GetScaledAxis" should be giving us unit-vectors since we asked for a "NoScale" matrix

	const auto from_forward = from_trans.ToMatrixNoScale().GetScaledAxis(EAxis::X);
	const auto to_forward = -to_trans.ToMatrixNoScale().GetScaledAxis(EAxis::X);

	const auto in_dir = from_forward * inSpeed;
	const auto out_dir = to_forward * outSpeed;

	const auto in_pos = from_trans.GetLocation();
	const auto out_pos = to_trans.GetLocation();

	const auto from_up = from_trans.ToMatrixNoScale().GetScaledAxis(EAxis::Z);
	const auto to_up = to_trans.ToMatrixNoScale().GetScaledAxis(EAxis::Z);

	auto current_up = from_up;

	TArray<FTransform> frames;

	for (auto i = 0; i <= divs; i++) {
		float t = (float)i / divs;

		FVector pos = SplineUtil::Hermite(t, in_pos, out_pos, in_dir, out_dir);

		FVector forward = SplineUtil::HermiteTangent(t, in_pos, out_pos, in_dir, out_dir);

		forward.Normalize();

		auto right = FVector::CrossProduct(forward, current_up);

		right.Normalize();

		current_up = FVector::CrossProduct(right, forward);

		current_up.Normalize();

		frames.Add(FTransform(forward, right, current_up, pos));
	}

	check(FVector::DotProduct(frames.Last().ToMatrixNoScale().GetScaledAxis(EAxis::Z), current_up) > 0.99f);

	auto angle_mismatch = FMath::Acos(FVector::DotProduct(current_up, to_up));

	auto sign_check = FVector::CrossProduct(current_up, to_up);

	// forward and this plane would be facing different ways
	if (FVector::DotProduct(sign_check, to_forward) > 0)
	{
		angle_mismatch = -angle_mismatch;
	}

	// take this one further than required, so we can assert we arrived the right way up
	for (auto i = 0; i <= divs; i++) {
		float t = (float)i / divs;

		auto angle_correct = t * angle_mismatch;

		FVector forward = SplineUtil::HermiteTangent(t, in_pos, out_pos, in_dir, out_dir);

		forward.Normalize();

		frames[i] = FTransform(FQuat({ 1, 0, 0 }, angle_correct)) * frames[i];
	}

	auto check_up = frames.Last().ToMatrixNoScale().GetScaledAxis(EAxis::Z);

	auto angle_check = FVector::DotProduct(check_up, to_up);

	check(angle_check > 0.99f);

	auto prev_node_idx = nodeFromIdx;
	int prev_conn_idx = nodeFromconnectorIdx;
	int next_conn_idx = 0;

	for (auto i = 1; i <= divs; i++) {
		float t = (float)i / divs;

		int next_node_idx;
		
		if (i < divs)
		{
			next_node_idx = Nodes.Num();
			auto new_node = MakeShared<BackToBack>(from_c->Definition);
			new_node->Position = frames[i];
			Nodes.Add(new_node);
		}
		else
		{
			next_node_idx = nodeToIdx;
			next_conn_idx = nodeToconnectorIdx;
		}

		auto interp_speed = inSpeed + (outSpeed - inSpeed) * t;
		Connect(prev_node_idx, prev_conn_idx, next_node_idx, next_conn_idx, 1, interp_speed, interp_speed);
		
		prev_conn_idx = 1;
		prev_node_idx = next_node_idx;
	}
}

void Graph::FillOutStructuralGraph(Graph& sg)
{
	for (const auto& n : Nodes)
	{
		auto new_node = TSharedPtr<Node>(n->FactoryMethod());
		new_node->Position = n->Position;

		sg.Nodes.Add(new_node);
	}

	for (const auto& e : Edges)
	{
		auto from_node = e->FromNode.Pin();
		auto to_node = e->ToNode.Pin();

		int from_idx = FindNodeIdx(from_node);
		int to_idx = FindNodeIdx(to_node);

		int from_connector_idx = from_node->FindConnectorIdx(e->FromConnector.Pin());
		int to_connector_idx = to_node->FindConnectorIdx(e->ToConnector.Pin());

		sg.ConnectAndFillOut(from_idx, from_connector_idx, to_idx, to_connector_idx, e->Divs, e->InSpeed, e->OutSpeed);
	}
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

// temp util to gloss over awkwardness of UVs and edge-types
static void AddPolyToMesh(TSharedPtr<Mesh> mesh, const TArray<FVector>& verts)
{
	TArray<FVector2D> UVs;
	TArray<PGCEdgeType> EdgeTypes;

	for (int i = 0; i < verts.Num(); i++)
	{
		UVs.Add(FVector2D(0, 0));
		EdgeTypes.Add(PGCEdgeType::Rounded);
	}

	mesh->AddFaceFromVects(verts, UVs, 0, EdgeTypes);
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

		AddPolyToMesh(mesh, verts);
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

			AddPolyToMesh(mesh, verts);
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

void Edge::AddToMesh(TSharedPtr<Mesh> mesh)
{
	// for the moment, for simplicity, support only the same connector type at either end...
	auto from_c = FromConnector.Pin();
	auto to_c = ToConnector.Pin();

	// currently insist on same type of connector at either end
	check(&from_c->Definition == &to_c->Definition);

	auto from_n = FromNode.Pin();
	auto to_n = ToNode.Pin();

	FTransform from_trans = from_c->Transform * from_n->Position;
	FTransform to_trans = to_c->Transform * to_n->Position;

	// "GetScaledAxis" should be giving us unit-vectors since we asked for a "NoScale" matrix

	const auto from_forward = from_trans.ToMatrixNoScale().GetScaledAxis(EAxis::X);
	const auto to_forward = -to_trans.ToMatrixNoScale().GetScaledAxis(EAxis::X);

	const auto in_dir = from_forward * InSpeed;
	const auto out_dir = to_forward * OutSpeed;

	const auto in_pos = from_trans.GetLocation();
	const auto out_pos = to_trans.GetLocation();

	const auto from_up = from_trans.ToMatrixNoScale().GetScaledAxis(EAxis::Z);
	const auto to_up = to_trans.ToMatrixNoScale().GetScaledAxis(EAxis::Z);

	auto current_up = from_up;

	TArray<FTransform> frames;

	for (auto i = 0; i <= Divs; i++) {
		float t = (float)i / Divs;

		FVector pos = SplineUtil::Hermite(t, in_pos, out_pos, in_dir, out_dir);

		FVector forward = SplineUtil::HermiteTangent(t, in_pos, out_pos, in_dir, out_dir);

		forward.Normalize();

		auto right = FVector::CrossProduct(forward, current_up);

		right.Normalize();

		current_up = FVector::CrossProduct(right, forward);

		current_up.Normalize();

		frames.Add(FTransform(forward, right, current_up, pos));
	}

	check(FVector::DotProduct(frames.Last().ToMatrixNoScale().GetScaledAxis(EAxis::Z), current_up) > 0.99f);

	auto angle_mismatch = FMath::Acos(FVector::DotProduct(current_up, to_up));

	auto sign_check = FVector::CrossProduct(current_up, to_up);

	// forward and this plane would be facing different ways
	if (FVector::DotProduct(sign_check, to_forward) > 0)
	{
		angle_mismatch = -angle_mismatch;
	}

	for (auto i = 0; i <= Divs; i++) {
		float t = (float)i / Divs;

		auto angle_correct = t * angle_mismatch;

		FVector forward = SplineUtil::HermiteTangent(t, in_pos, out_pos, in_dir, out_dir);

		forward.Normalize();

		frames[i] = FTransform(FQuat({1, 0, 0}, angle_correct)) * frames[i];
	}

	auto check_up = frames.Last().ToMatrixNoScale().GetScaledAxis(EAxis::Z);

	auto angle_check = FVector::DotProduct(check_up, to_up);

	check(angle_check > 0.99f);

	TArray<FVector> prev_slice;

	for (auto i = 0; i <= Divs; i++) {
		TArray<FVector> slice;

		for (auto j = 0; j < from_c->Definition.NumVerts(); j++) {
			slice.Add(from_c->Definition.GetTransformedVert(j, frames[i]));
		}

		if (prev_slice.Num())
		{
			auto prev_vert = prev_slice.Num() - 1;

			for (auto j = 0; j < prev_slice.Num(); j++)
			{
				AddPolyToMesh(mesh, TArray<FVector> {
					slice[prev_vert], slice[j],
					prev_slice[j], prev_slice[prev_vert]
				});
				//AddPolyToMesh(mesh, TArray<FVector> {
				//	slice[prev_vert], slice[j],
				//	slice[j] + (prev_slice[j] - slice[j]) * 0.1,
				//	slice[prev_vert] + (prev_slice[prev_vert] - slice[prev_vert]) * 0.1
				//});

				prev_vert = j;
			}
		}

		prev_slice = std::move(slice);
	}
}

FVector ConnectorDef::GetTransformedVert(int vert_idx, const FTransform& total_trans) const
{
	// an untransformed connector faces down X and it upright w.r.t Z
	// so its width (internal X) is mapped onto Y and its height (internal Y) onto Z
	FVector temp{ 0.0f, Profile[vert_idx].Position.X, Profile[vert_idx].Position.Y };

	return total_trans.TransformPosition(temp);
}

double OptFunction::CalcGrad(const double* x, int n) const
{
	int node_idx = n / 4;
	int param_idx = n % 4;

	double ret = 0;

	for (int i = 0; i < Working.Num(); i++)
	{
		if (i != node_idx)
		{
			if (Connected.Contains(TPair<int, int>{node_idx, i}))
			{
				ret += ConnectedNodeNodeGrad(param_idx, Working[param_idx].GetLocation(), Working[i].GetLocation());
			}
		}
	}

	return ret;
}

double OptFunction::CalcVal(const double* x) const
{
	double ret = 0.0;

	//const auto& nodes = G.GetNodes();
	//const auto& edges = G.GetEdges();

	for (int i = 0; i < Working.Num() - 1; i++)
	{
		for (int j = i + 1; j < Working.Num(); j++)
		{
			if (Connected.Contains(TPair<int, int>{ i, j }))
			{
				ret += ConnectedNodeNodeVal(Working[i].GetLocation(), Working[i].GetLocation());
			}
		}
	}

	return ret;
}

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

double OptFunction::ConnectedNodeNodeVal(const FVector & p1, const FVector & p2) const
{
	float dist = FVector::Dist(p1, p2);

	return LeonardJonesVal(dist, G.SegLength, 2);
}

double OptFunction::ConnectedNodeNodeGrad(int i, const FVector & pGrad, const FVector & pOther) const
{
	if (i > 2)		// angle has no effect on distance
	{
		return 0.0;
	}

	float dist = FVector::Dist(pGrad, pOther);

	double delta = pGrad[i] - pOther[i];

	return delta * LeonardJonesGrad(dist, G.SegLength, 2) / dist;
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

void OptFunction::BuildPropagationFrom(TSet<TSharedPtr<Node>>& found, const TSharedPtr<Node>& node, int addingIdx)
{
	check(!found.Contains(node));
	found.Add(node);
	Working[addingIdx] = node->Position;
	Propagation.Add(addingIdx);

	for (const auto& edge : node->Edges)
	{
		auto edge_ptr = edge.Pin();

		// if it is an out edge
		if (edge_ptr->FromNode == node)
		{
			const auto& next_node = edge_ptr->ToNode.Pin();

			int next_idx = G.FindNodeIdx(next_node);

			check(!Connected.Contains(TPair<int, int>{addingIdx, next_idx}));
			check(!Connected.Contains(TPair<int, int>{next_idx, addingIdx}));

			Connected.Add(TPair<int, int>{addingIdx, next_idx});
			Connected.Add(TPair<int, int>{next_idx, addingIdx});

			if (!found.Contains(next_node))
			{
				Propagation[addingIdx].Add(next_idx);

				BuildPropagationFrom(found, next_node, next_idx);
			}
		}
	}
}

void OptFunction::SetupWorkingTransforms(const double* x, int propagateFrom /* = 0 */) const
{
	for (auto i : Propagation[propagateFrom])
	{
		PropagateTransform(x, propagateFrom, i);

		SetupWorkingTransforms(x, i);
	}
}

void OptFunction::PropagateTransform(const double* x, int from, int to) const
{
	auto to_pos = FVector{ (float)x[from * 4 + 0], (float)x[from * 4 + 1], (float)x[from * 4 + 2] };

	auto forward = to_pos - Working[from].GetLocation();
	forward.Normalize();

	auto from_up = Working[from].GetUnitAxis(EAxis::Z);

	auto to_right = FVector::CrossProduct(forward, from_up);

	to_right.Normalize();

	auto to_up = FVector::CrossProduct(to_right, forward);

	to_up.Normalize();

	// position at to_pos, oriented the same as from accept for up/righ adjusted to be orthogonal to our local forward
	Working[to] = FTransform(forward, to_right, to_up, to_pos);

	// rotate that according to how the other param wants to rotate up....
	Working[to] = FTransform(FQuat({ 1, 0, 0 }, (float)x[to * 4 + 3])) * Working[to];
}

OptFunction::OptFunction(const Graph& g)
	: G(g)
{
	TSet<TSharedPtr<Node>> found;

	Working.AddDefaulted(G.GetNodes().Num());

	BuildPropagationFrom(found, G.GetNodes()[0], 0);

	// was the graph a DAG?
	check(found.Num() == G.GetNodes().Num());
}

int OptFunction::GetSize() const
{
	// 4 DoF per node = position (3) + rotation of up vector
	// - our "parent" node is the one whose line in "Propagation" we are in
	// - forward vector is oriented along the line from our parent
	// - up vector is the up vector of our parent, made orthogonal with our forwards, and rotated by our rotation
	// - right vector is normal to both of those

	return G.GetNodes().Num() * 4;
}

double OptFunction::f(int n, const double* x, double* grad) const
{
	check(n == GetSize());

	SetupWorkingTransforms(x);

	for (int i = 0; i < n; i++)
	{
		grad[i] = CalcGrad(x, i);
	}

	return CalcVal(x);
}

#pragma optimize ("", on)
