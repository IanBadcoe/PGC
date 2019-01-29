#include "OptFunction.h"

#include "Util.h"

using namespace Opt;
using namespace StructuralGraph;

#pragma optimize ("", off)

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

#define HISTO_SIZE 20

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

	return FMath::Pow(dist - D, 2) * 0.01;
}

//double OptFunction::UnconnectedNodeNodeDist_Grad(const FVector & pGrad, const FVector & pOther, float D0, int axis)
//{
//	check(axis >= 0 && axis < 3);
//
//	float dist = FVector::Dist(pGrad, pOther);
//
//	if (dist >= D0)
//		return 0.0;
//
//	return 2 * (pGrad[axis] - pOther[axis]) * (dist - D0) / dist * 0.01;
//}

double Opt::OptFunction::ConnectedNodeNodeTorsion_Val(const FVector & up1, const FVector & up2, const FVector & p1, const FVector & p2, bool flipped)
{
	auto axis = (p2 - p1).GetSafeNormal();

	auto up1_in_plane = Util::ProjectOntoPlane(up1, axis);
	auto up2_in_plane = Util::ProjectOntoPlane(up2, axis);

	if (flipped)
	{
		up1_in_plane = -up1_in_plane;
	}

	auto cos = FVector::DotProduct(up1_in_plane, up2_in_plane);

	return FMath::Pow(1 - cos, 2);
}

//double Opt::OptFunction::ConnectedNodeNodeTorsion_Grad(const FVector & upGrad, const FVector & upOther, const FVector & pGrad, const FVector & pOther, bool flipped, int axis)
//{
//	return 0.0;
//}

double OptFunction::ConnectedNodeNodeDist_Val(const FVector& p1, const FVector& p2, float D0)
{
	float dist = FVector::Dist(p1, p2);

	add_hist(dist / D0);

	return pow(dist - D0, 2);
}

//double OptFunction::ConnectedNodeNodeDist_Grad(const FVector& pGrad, const FVector& pOther, float D0, int axis)
//{
//	check(axis >= 0 && axis < 3);
//		
//	float dist = FVector::Dist(pGrad, pOther);
//	
//	return 2 * (pGrad[axis] - pOther[axis]) * (dist - D0) / dist;
//}

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

				// where we have a 1/2 turns, we can end up with one edge where the
				// normal has to reverse, we must preserve that in the target function
				auto flipped = FVector::DotProduct(ep->ToNode.Pin()->CachedUp, ep->FromNode.Pin()->CachedUp) < 0;

				Connected.Add(JoinIdxs(n->Idx, other_n->Idx)) = JoinData{ flipped, ep->D0 };
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

	SetState(x, n);

	double ret = 0;

	for(int i = 0; i < G->Nodes.Num() - 1; i++)
	{
		for (int j = i + 1; j < G->Nodes.Num(); j++)
		{
			auto idxs = JoinIdxs{ i, j };
			if (Connected.Contains(idxs))
			{
				ret += ConnectedNodeNodeDist_Val(G->Nodes[i]->Position, G->Nodes[j]->Position, Connected[idxs].D0);

				ret += ConnectedNodeNodeTorsion_Val(G->Nodes[i]->CachedUp, G->Nodes[j]->CachedUp, G->Nodes[i]->Position, G->Nodes[j]->Position, Connected[idxs].Flipped);
			}
			else
			{
				ret += UnconnectedNodeNodeDist_Val(G->Nodes[i]->Position, G->Nodes[j]->Position, G->Nodes[i]->Radius + G->Nodes[j]->Radius);
			}
		}
	}

//	if (grad)
//	{
//		for (int par = 0; par < n; par++)
//		{
//			grad[par] = 0;
//
//			auto i = ParamToNode(par);
//			auto axis = ParamToParamInNode(par);
//			TSharedPtr<SNode> node_i = G->Nodes[i];
//
//			for (int j = 0; j < G->Nodes.Num(); j++)
//			{
//				if (j != i)
//				{
//					auto idxs = JoinIdxs{ i, j };
//					TSharedPtr<SNode> node_j = G->Nodes[j];
//
//					if (axis < 3)
//					{
//						// node distances depend only on positions
//						// the node whose gradient we are calculating always gets passed first into the gradient function
//						if (Connected.Contains(idxs))
//						{
//							{
//								auto here_grad = ConnectedNodeNodeDist_Grad(node_i->Position, node_j->Position, Connected[idxs].D0, axis);
//
//#ifndef UE_BUILD_RELEASE
//								double diff = here_grad - check_grad(node_i->Position, node_j->Position, axis, 1e-3, Connected[idxs].D0, ConnectedNodeNodeDist_Val);
//								check(abs(diff) < 1E-2);
//#endif
//
//								grad[par] += here_grad;
//							}
//						}
//						else
//						{
//							float radius = node_i->Radius + node_j->Radius;
//
//							auto here_grad = UnconnectedNodeNodeDist_Grad(node_i->Position, node_j->Position, radius, axis);
//
//#ifndef UE_BUILD_RELEASE
//							double diff = here_grad - check_grad(node_i->Position, node_j->Position, axis, 1e-3, radius, UnconnectedNodeNodeDist_Val);
//							check(abs(diff) < 1E-2);
//#endif
//
//							grad[par] += here_grad;
//						}
//					}
//					else
//					{
//						// torsions depend only on normals
//						if (Connected.Contains(idxs))
//						{
//							grad[par] = 0;
//							// we need the raw up for the actual param value of the one we're getting the gradient for...
////							auto here_grad = ConnectedNodeNodeTorsion_Grad(node_i->Up, node_j->Up, axis - 3);
////
////#ifndef UE_BUILD_RELEASE
////							double check = check_grad(node_i->Up, node_j->Up, axis - 3, 1e-4, ConnectedNodeNodeTorsion_Val);
////							check(abs(check - here_grad) < 2E-2);
////#endif
////
////							grad[par] += here_grad;
//						}
//					}
//				}
//			}
//		}
//	}

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
		SetVector(x, n, i, 0, G->Nodes[i]->Position);
		SetParam(x, n, i, 3, G->Nodes[i]->Rotation);
	}
}

void OptFunction::SetState(const double* x, int n)
{
	check(n == GetSize());

	for (int i = 0; i < G->Nodes.Num(); i++)
	{
		auto& node = G->Nodes[i];

		node->Position = GetVector(x, n, i, 0);
		node->Rotation = GetParam(x, n, i, 3);

		node->RecalcForward();
		node->ApplyRotation();
	}
}

void OptFunction::reset_histo()
{
	::reset_histo();
}

void OptFunction::print_histo()
{
	::print_histo();
}

#pragma optimize ("", on)

