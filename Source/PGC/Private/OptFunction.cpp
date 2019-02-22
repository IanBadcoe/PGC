#include "OptFunction.h"

#include "Util.h"

PRAGMA_DISABLE_OPTIMIZATION

namespace Opt
{

using namespace StructuralGraph;

// a count of doubles
#define PARAMS_PER_NODE 4

static int ParamToNode(int vect_idx)
{
	return vect_idx / PARAMS_PER_NODE;
}

static int NodeToParam(int node_idx, int param_in_node, int n)
{
	check(param_in_node >= 0 && param_in_node < PARAMS_PER_NODE);
	check(node_idx * PARAMS_PER_NODE + param_in_node < n);
	check(node_idx >= 0);

	return node_idx * PARAMS_PER_NODE + param_in_node;
}

static int ParamToParamInNode(int param_idx)
{
	return param_idx % PARAMS_PER_NODE;
}

// Get/Set a vector into the parameter array, using three consecutive doubles starting from start_param_in_node
static FVector GetVector(const double* x, int x_size, int node_idx, int start_param_in_node) {
	return FVector
	{
		(float)x[NodeToParam(node_idx, start_param_in_node + 0, x_size)],
		(float)x[NodeToParam(node_idx, start_param_in_node + 1, x_size)],
		(float)x[NodeToParam(node_idx, start_param_in_node + 2, x_size)]
	};
}

static void SetVector(double* x, int x_size, int node_idx, int start_param_in_node, const FVector& v)
{
	x[NodeToParam(node_idx, 0 + start_param_in_node, x_size)] = v.X;
	x[NodeToParam(node_idx, 1 + start_param_in_node, x_size)] = v.Y;
	x[NodeToParam(node_idx, 2 + start_param_in_node, x_size)] = v.Z;
}

static float GetParam(const double* x, int x_size, int node_idx, int start_param_in_node) {
	return (float)x[NodeToParam(node_idx, start_param_in_node, x_size)];
}

static void SetParam(double* x, int x_size, int node_idx, int start_param_in_node, const double& v)
{
	x[NodeToParam(node_idx, start_param_in_node, x_size)] = v;
}

//#define HISTO_SIZE 20

#ifdef HISTO_SIZE

static int histo[HISTO_SIZE];

static double histo_scale(double d)
{
	return d * 10;
}

#endif

static void add_hist(double d)
{
#ifdef HISTO_SIZE
	auto sd = (int)histo_scale(d);

	if (sd >= HISTO_SIZE - 1)
	{
		histo[HISTO_SIZE - 1]++;
	}
	else
	{
		histo[sd]++;
	}
#endif
}

static void reset_histo() {
#ifdef HISTO_SIZE
	for (int i = 0; i < HISTO_SIZE; i++)
	{
		histo[i] = 0;
	}
#endif
}

static void print_histo() {
#ifdef HISTO_SIZE
	FString out;

	for (int i = 0; i < HISTO_SIZE; i++)
	{
		out += FString::Printf(TEXT(" %d"), histo[i]);
	}

	UE_LOG(LogTemp, Warning, TEXT("HISTO: %s"), *out);

#endif
}


double OptFunction::UnconnectedNodeNodeDist_Val(const FVector& p1, const FVector& p2, float D)
{
	float dist = FVector::Dist(p1, p2);

	if (dist >= D)
		return 0;

	// dividing by expected distance makes the penalty be for proportional error, rather than distance
	// which I think makes sense as you don't want a large distance to be able to crush a small one disproportionately
	return FMath::Pow((dist - D) / D, 2);
}

double OptFunction::ConnectedNodeNodeTorsion_Val(const FVector & up1, const FVector & up2, const FVector & p1, const FVector & p2, bool flipped)
{
	auto axis = (p2 - p1).GetSafeNormal();

	// if we are supplied two identical points, this becomes undefined, but is very bad anyway...
	if (axis.GetMax() == 0)
		return 2;

	auto up1_in_plane = Util::ProjectOntoPlane(up1, axis);
	auto up2_in_plane = Util::ProjectOntoPlane(up2, axis);

	if (flipped)
	{
		up1_in_plane = -up1_in_plane;
	}

	auto cos = FVector::DotProduct(up1_in_plane, up2_in_plane);

	auto angle = FMath::Acos(cos);

	return FMath::Pow(angle / PI, 2);
}

double OptFunction::ConnectedNodeNodeDist_Val(const FVector& p1, const FVector& p2, float D0)
{
	float dist = FVector::Dist(p1, p2);

	add_hist(dist / D0);

	// dividing by expected distance makes the penalty be for proportional error, rather than distance
	// which I think makes sense as you don't want a large distance to be able to crush a small one disproportionately
	return pow((dist - D0) / D0, 2);
}

double Opt::OptFunction::ConnectedNodeNodeBend_Val(const FVector& prev_pos, const FVector& pos, const FVector& next_pos)
{
	auto d1 = (pos - prev_pos).GetSafeNormal();
	auto d2 = (next_pos - pos).GetSafeNormal();

	auto cos = FVector::DotProduct(d1, d2);

	auto angle = FMath::Acos(cos);

	return FMath::Pow(angle / PI, 2);
}

double OptFunction::JunctionAngle_Energy(const TSharedPtr<SNode> node)
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

double OptFunction::JunctionPlanar_Energy(const TSharedPtr<SNode> node)
{
	check(node->MyType == StructuralGraph::SNode::Type::Junction);

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

OptFunction::OptFunction(TSharedPtr<SGraph> g,
	double connected_scale, double unconnected_scale, double torsion_scale,
	double bend_scale, double jangle_scale, double jplanar_scale)
	: G(g),
	  ConnectedScale(connected_scale), UnconnectedScale(unconnected_scale), TorsionScale(torsion_scale),
	  BendScale(bend_scale), JunctionAngleScale(jangle_scale), JunctionPlanarScale(jplanar_scale)
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

				// where we have a 1/2 turns, we can end up with one edge where the
				// normal has to reverse, we must preserve that in the target function
				//
				// do we need to project onto one node's forward plane?
				auto flipped = FVector::DotProduct(ep->ToNode.Pin()->CachedUp, ep->FromNode.Pin()->CachedUp) < 0;

				Connected.Add(JoinIdxs(n, other_n)) = JoinData{ flipped, ep->D0 };
			}
		}
	}
}

int OptFunction::GetSize() const
{
	return G->Nodes.Num() * PARAMS_PER_NODE;
}

//template <typename... OtherArgs>
//using CheckFunPtr = double(*)(const FVector&, const FVector&, OtherArgs...);
//
//template <typename... OtherArgs>
//double check_grad(const FVector& pGrad, const FVector& pOther, int axis, double delta, OtherArgs... other_args, CheckFunPtr<OtherArgs...> pFun)
//{
//	FVector offset{ 0, 0, 0 };
//
//	offset[axis] = delta;
//
//	auto pGradPlus = pGrad + offset;
//	auto pGradMinus = pGrad - offset;
//
//	auto valPlus = pFun(pGradPlus, pOther, other_args...);
//	auto valMinus = pFun(pGradMinus, pOther, other_args...);
//
//	return (valPlus - valMinus) / (2 * delta);
//}

double OptFunction::f(int n, const double* x, double* grad)
{
	check(n == GetSize());

	ConnectedEnergy = 0;
	UnconnectedEnergy = 0;
	TorsionEnergy = 0;
	BendEnergy = 0;
	JunctionAngleEnergy = 0;
	JunctionPlanarEnergy = 0;

	SetState(x, n);

	for(int i = 0; i < G->Nodes.Num() - 1; i++)
	{
		const auto& node_a = G->Nodes[i];

		for (int j = i + 1; j < G->Nodes.Num(); j++)
		{
			const auto& node_b = G->Nodes[j];

			auto idxs = JoinIdxs{ node_a, node_b };

			if (Connected.Contains(idxs))
			{
				ConnectedEnergy += ConnectedNodeNodeDist_Val(node_a->Position, node_b->Position, Connected[idxs].D0) * ConnectedScale;

				TorsionEnergy += ConnectedNodeNodeTorsion_Val(node_a->CachedUp, node_b->CachedUp, node_a->Position, node_b->Position, Connected[idxs].Flipped) * TorsionScale;
			}
			else
			{
				UnconnectedEnergy += UnconnectedNodeNodeDist_Val(node_a->Position, node_b->Position, node_a->Radius + node_b->Radius) * UnconnectedScale;
			}
		}

		// Junctions attach their connectors at all angles, so this could be counterproductive for them
		if (node_a->MyType == SNode::Type::Junction)
		{
			JunctionAngleEnergy += JunctionAngle_Energy(node_a) * JunctionAngleScale;

			JunctionPlanarEnergy += JunctionPlanar_Energy(node_a) * JunctionPlanarScale;
		}
		else if (node_a->Edges.Num() == 2)
		{
			const auto& pos = node_a->Position;
			const auto& pos_other1 = node_a->Edges[0].Pin()->OtherNode(node_a).Pin()->Position;
			const auto& pos_other2 = node_a->Edges[1].Pin()->OtherNode(node_a).Pin()->Position;

			BendEnergy += ConnectedNodeNodeBend_Val(pos_other1, pos, pos_other2) * BendScale;
		}
	}

	return ConnectedEnergy
		+ UnconnectedEnergy
		+ TorsionEnergy
		+ BendEnergy
		* JunctionAngleEnergy
		+ JunctionPlanarEnergy;
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
		SetVector(x, n, i, 0, G->Nodes[i]->Position);
		SetParam(x, n, i, 3, G->Nodes[i]->Rotation);
	}
}

void OptFunction::SetState(const double* x, int n)
{
	check(n == GetSize());

	// we have reordered the nodes so that parents are always before children
	// allowing this to be a simple loop

	for (int i = 0; i < G->Nodes.Num(); i++)
	{
		auto& node = G->Nodes[i];

		node->Position = GetVector(x, n, i, 0);
		node->Rotation = GetParam(x, n, i, 3);

		node->RecalcForward();
		node->ApplyRotation();
	}
}

TArray<FString> OptFunction::GetEnergyTermNames() const
{
	return TArray<FString> { "Connected", "Unconnected", "Torsion", "Bend", "Junction Angles", "Junction Planar" };
}

TArray<double> OptFunction::GetLastEnergyTerms() const
{
	return TArray<double> { ConnectedEnergy, UnconnectedEnergy, TorsionEnergy, BendEnergy, JunctionAngleEnergy, JunctionPlanarEnergy };
}

void Opt::OptFunction::GetLimits(double* lower, double* upper, int n) const
{
	check(n == GetSize());

	int i = 0;

	for (const auto& node : G->Nodes)
	{
		check(i + 3 < n);

		lower[i + 0] = node->Position.X - 100;
		lower[i + 1] = node->Position.Y - 100;
		lower[i + 2] = node->Position.Z - 100;
		lower[i + 3] = -PI;

		upper[i + 0] = node->Position.X + 100;
		upper[i + 1] = node->Position.Y + 100;
		upper[i + 2] = node->Position.Z + 100;
		upper[i + 3] = PI;

		i += 4;
	}
}

//void OptFunction::reset_histo()
//{
//	::reset_histo();
//}
//
//void OptFunction::print_histo()
//{
//	::print_histo();
//}

}

PRAGMA_ENABLE_OPTIMIZATION
