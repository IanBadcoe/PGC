#include "SetupOptFunction.h"

#include "GVector.h"

// SetupOptFunction for initial optimisation of nodes and edge-intermediate-points

PRAGMA_DISABLE_OPTIMIZATION

double SetupOptFunction::EdgeLength_Energy(const FVector & p1, const FVector & p2, double D0)
{
	auto len = (p1 - p2).Size();

	return FMath::Pow((D0 - len) / D0, 2);
}

double SetupOptFunction::JunctionAngle_Energy(const TSharedPtr<INode> node)
{
	// the normal calculation cannot work for less than a triangle
	// if we ever get genuine 2-edge junctions, then we can add an alternative form of this that just uses the angle between
	// them (and this is zero for 0 or 1 edges)
	check(node->Edges.Num() > 2);

	TArray<FVector> rel_verts;

	for (const auto& e : node->Edges)
	{
		// makes no difference to the normal making verts relative (except maybe improving precision)
		// and we need them relative for the angles
		rel_verts.Emplace(e.Pin()->OtherNode(node).Pin()->Position - node->Position);
	}

	FVector plane_normal = Util::NewellPolyNormal(rel_verts);

	TArray<float> angles;

	angles.Emplace(0);

	for (int i = 1; i < rel_verts.Num(); i++)
	{
		angles.Emplace(Util::SignedAngle(rel_verts[0], rel_verts[i], plane_normal, true));
	}

	angles.Sort();

	angles.Emplace(2 * PI);

	for (int i = angles.Num() - 1; i > 0; i--)
	{
		angles[i] = angles[i] - angles[i - 1];

		check(angles[i] >= 0);
		check(angles[i] <= 2 * PI);
	}

	angles.RemoveAt(0);

	auto target = 2 * PI / angles.Num();

	float ret = 0.0f;

	for (const auto& ang : angles)
	{
		ret += FMath::Pow((ang - target) / target, 2);
	}
	
	return ret;
}

double SetupOptFunction::EdgeAngle_Energy(const TSharedPtr<INode> node)
{
	check(node->Edges.Num() <= 2);

	// does not apply to terminal node
	if (node->Edges.Num() < 2)
		return 0;

	auto in_dir = (node->Position - node->Edges[0].Pin()->OtherNode(node).Pin()->Position).GetSafeNormal();
	auto out_dir = (node->Edges[1].Pin()->OtherNode(node).Pin()->Position - node->Position).GetSafeNormal();

	auto cos = FVector::DotProduct(in_dir, out_dir);

	auto ang = FMath::Acos(cos);

	// ignore any bend up to 30 degrees
	static const auto tol = PI / 6;

	if (ang < tol)
	{
		return 0;
	}

	return FMath::Pow((ang - tol) / (PI - tol), 2);
}

double SetupOptFunction::JunctionPlanar_Energy(const TSharedPtr<INode> node)
{
	check(node->Reference.IsValid() && node->Reference->MyType == StructuralGraph::SNode::Type::Junction);

	TArray<FVector> rel_verts;

	for (const auto& e : node->Edges)
	{
		// makes no difference to the normal making verts relative (except maybe improving precision)
		// and we need them relative for the angles
		rel_verts.Emplace(e.Pin()->OtherNode(node).Pin()->Position - node->Position);
	}

	FVector plane_normal = Util::NewellPolyNormal(rel_verts);

	float ret{ 0 };

	for (const auto& v : rel_verts)
	{
		ret += FMath::Pow(FVector::DotProduct(v, plane_normal), 2);
	}

	return ret;
}

double SetupOptFunction::EdgeEdge_Energy(const TSharedPtr<IEdge> e1, const TSharedPtr<IEdge> e2)
{
	const auto& e1nf = e1->FromNode.Pin();
	const auto& e1nt = e1->ToNode.Pin();
	const auto& e2nf = e2->FromNode.Pin();
	const auto& e2nt = e2->ToNode.Pin();

	auto combined_radius = FMath::Max(e1nf->Radius, e1nt->Radius) +
		FMath::Max(e2nf->Radius, e2nt->Radius);

	auto dist = Util::SegmentSegmentDistance(
		GVector(e1nf->Position),
		GVector(e1nt->Position),
		GVector(e2nf->Position),
		GVector(e2nt->Position)
	);

	if (dist > combined_radius)
		return 0;

	return FMath::Pow((combined_radius - dist) / combined_radius, 2);
}

SetupOptFunction::SetupOptFunction(const TSharedPtr<IGraph> graph, double nade_scale, double eae_scale, double pe_scale, double le_scale, double eee_scale)
	: Graph(graph),
	NodeAngleDistEnergyScale(nade_scale),
	EdgeAngleEnergyScale(eae_scale),
	PlanarEnergyScale(pe_scale),
	LengthEnergyScale(le_scale),
	EdgeEdgeEnergyScale(eee_scale)
{
	for (const auto& e1 : Graph->Edges)
	{
		for (const auto& e2 : Graph->Edges)
		{
			// just examine one triangle from the NxN matrix
			if (e1 == e2)
				break;

			// collect all the pairs of edges that do not share an end
			// could store the inverse of this in a TSet, if its size becomes a problem...
			if (!e1->Contains(e2->FromNode) && !e1->Contains(e2->ToNode))
			{
				CollidableEdges.Emplace<EdgePair>({ e1, e2 });
			}
		}
	}
}

double SetupOptFunction::f(int n, const double * x, double * grad)
{
	NodeAngleEnergy = 0;
	EdgeAngleEnergy = 0;
	PlanarEnergy = 0;
	LengthEnergy = 0;
	EdgeEdgeEnergy = 0;

	SetState(x, n);

	for (const auto& e : Graph->Edges)
	{
		LengthEnergy += EdgeLength_Energy(e->FromNode.Pin()->Position, e->ToNode.Pin()->Position, e->D0);
	}

	for (const auto& node : Graph->Nodes)
	{
		if (node->Reference.IsValid() && node->Reference->MyType == StructuralGraph::SNode::Type::Junction)
		{
			NodeAngleEnergy += JunctionAngle_Energy(node);
			PlanarEnergy += JunctionPlanar_Energy(node);
		}
		else
		{
			EdgeAngleEnergy += EdgeAngle_Energy(node);
		}
	}

	for (const auto& ep : CollidableEdges)
	{
		EdgeEdgeEnergy += EdgeEdge_Energy(ep.Edge1, ep.Edge2);
	}

	return 	NodeAngleEnergy +
		EdgeAngleEnergy +
		PlanarEnergy +
		LengthEnergy +
		EdgeEdgeEnergy;
}

int SetupOptFunction::GetSize() const
{
	return Graph->Nodes.Num() * 3;
}

void SetupOptFunction::GetInitialStepSize(double * steps, int n) const
{
	check(n == GetSize());

	for (int i = 0; i < n; i++)
	{
		steps[i] = 10.0;
	}
}

void SetupOptFunction::GetLimits(double * lower, double * upper, int n) const
{
	check(n == GetSize());

	int i = 0;

	for (const auto& node : Graph->Nodes)
	{
		check(i + 2 < n);

		lower[i + 0] = node->Position.X - 30;
		lower[i + 1] = node->Position.Y - 30;
		lower[i + 2] = node->Position.Z - 30;

		upper[i + 0] = node->Position.X + 30;
		upper[i + 1] = node->Position.Y + 30;
		upper[i + 2] = node->Position.Z + 30;

		i += 3;
	}
}

void SetupOptFunction::GetState(double * x, int n) const
{
	check(n == GetSize());

	int i = 0;

	for (const auto& node : Graph->Nodes)
	{
		check(i + 2 < n);

		x[i + 0] = node->Position.X;
		x[i + 1] = node->Position.Y;
		x[i + 2] = node->Position.Z;

		i += 3;
	}
}

void SetupOptFunction::SetState(const double * x, int n)
{
	check(n == GetSize());

	int i = 0;

	for (const auto& node : Graph->Nodes)
	{
		check(i + 2 < n);

		node->Position.X = x[i + 0];
		node->Position.Y = x[i + 1];
		node->Position.Z = x[i + 2];

		i += 3;
	}
}

TArray<FString> SetupOptFunction::GetEnergyTermNames() const
{
	return { "NodeAngleDistEnergy", "EdgeAngleEnergy", "PlanarEnergy", "LengthEnergy", "EdgeEdgeEnergy", };
}

TArray<double> SetupOptFunction::GetLastEnergyTerms() const
{
	return { NodeAngleEnergy, EdgeAngleEnergy, PlanarEnergy, LengthEnergy, EdgeEdgeEnergy };
}

PRAGMA_ENABLE_OPTIMIZATION
