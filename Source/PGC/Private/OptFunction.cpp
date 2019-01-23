#include "OptFunction.h"

using namespace Opt;
using namespace StructuralGraph;

#pragma optimize ("", off)

#define PARAMS_PER_NODE 6

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

	return FMath::Pow(dist - D, 2) * 0.01;		//LeonardJonesVal(dist, D, 1);
}

double OptFunction::UnconnectedNodeNodeDist_Grad(const FVector & pGrad, const FVector & pOther, float D0, int axis)
{
	check(axis >= 0 && axis < 3);

	float dist = FVector::Dist(pGrad, pOther);

	if (dist >= D0)
		return 0.0;

	return 2 * (pGrad[axis] - pOther[axis]) * (dist - D0) / dist * 0.01;
}

double Opt::OptFunction::ConnectedNodeNodeTorsion_Val(const FVector& up1, const FVector& up2)
{
	// we have to take non-normalized up1 here for gradient-check purposes (perturbing a normalised vector is very different
	// to perturbing a non-normal one...)
	check(up2.IsNormalized());

	auto len_up1 = up1.Size();

	auto dp = FVector::DotProduct(up1 / len_up1, up2);

	return 1 / (1.1 + dp)
		+ FMath::Pow(1 - len_up1, 2);		// extra term to keep he vector roughly normal, since getting near the origin makes things terribly unstable...

	//return FMath::Pow(1 - dp, 2)
	//	+ FMath::Pow(1 - len_up1, 2);		// extra term to keep he vector roughly normal, since getting near the origin makes things terribly unstable...
}

double Opt::OptFunction::ConnectedNodeNodeTorsion_Grad(const FVector& upGrad, const FVector& upOther, int axis)
{
	// up1 is non-normal as we need the actual parameter values
	check(upOther.IsNormalized());

	auto par = upGrad[axis];

	auto dist = upGrad.Size();

	auto dp = FVector::DotProduct(upGrad, upOther);

	return -(upOther[axis] / dist - par * dp / FMath::Pow(dist, 3)) / FMath::Pow(dp / dist + 1.1, 2)
		+ par * (2 - 2 / dist);

	//return 2 * (par * dp / FMath::Pow(dist, 3) - upOther[axis] / dist) * (1 - dp / dist)
	//	+ par * (2 - 2 / dist);
}

double OptFunction::ConnectedNodeNodeDist_Val(const FVector& p1, const FVector& p2, float D0)
{
	float dist = FVector::Dist(p1, p2);

	add_hist(dist / D0);

	return pow(dist - D0, 2); //LeonardJonesVal(dist, D0, 2);
}

double OptFunction::ConnectedNodeNodeDist_Grad(const FVector& pGrad, const FVector& pOther, float D0, int axis)
{
	check(axis >= 0 && axis < 3);
		
	float dist = FVector::Dist(pGrad, pOther);
	
	return 2 * (pGrad[axis] - pOther[axis]) * (dist - D0) / dist;
}

double OptFunction::LeonardJonesVal(double R, double D, int N) const
{
	double rat = D / R;

	return FMath::Pow(rat, N * 2) - 2 * FMath::Pow(rat, N);
}

//double OptFunction::LeonardJonesGrad(double R, double D, int N) const
//{
//	double rat = D / R;
//
//	return 2 * N * (FMath::Pow(rat, N) - 1) * FMath::Pow(rat, N) / R;
//}

static FVector GetVector(const double* x, int x_size, int node_idx, int vect_idx) {
	return FVector
	{
		(float)x[NodeToParam(node_idx, vect_idx * 3 + 0, x_size)],
		(float)x[NodeToParam(node_idx, vect_idx * 3 + 1, x_size)],
		(float)x[NodeToParam(node_idx, vect_idx * 3 + 2, x_size)]
	};
}

static void SetVector(double* x, int x_size, int node_idx, int vect_idx, const FVector& v)
{
	x[NodeToParam(node_idx, 0 + vect_idx * 3, x_size)] = v.X;
	x[NodeToParam(node_idx, 1 + vect_idx * 3, x_size)] = v.Y;
	x[NodeToParam(node_idx, 2 + vect_idx * 3, x_size)] = v.Z;
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

				Connected.Add(JoinIdxs{ FMath::Min(n->Idx, other_n->Idx), FMath::Max(n->Idx, other_n->Idx)}) = ep->D0;

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
	return G->Nodes.Num() * PARAMS_PER_NODE;
}

template <typename... OtherArgs>
using CheckFunPtr = double(*)(const FVector&, const FVector&, OtherArgs...);

template <typename... OtherArgs>
double check_grad(const FVector& pGrad, const FVector& pOther, int axis, double delta, OtherArgs... other_args, CheckFunPtr<OtherArgs...> pFun)
{
	FVector offset{ 0, 0, 0 };

	offset[axis] = delta;

	auto pGradPlus = pGrad + offset;
	auto pGradMinus = pGrad - offset;

	auto valPlus = pFun(pGradPlus, pOther, other_args...);
	auto valMinus = pFun(pGradMinus, pOther, other_args...);

	return (valPlus - valMinus) / (2 * delta);
}

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
				ret += ConnectedNodeNodeDist_Val(G->Nodes[i]->GetPosition(), G->Nodes[j]->GetPosition(), Connected[idxs]);

				ret += ConnectedNodeNodeTorsion_Val(G->Nodes[i]->GetUp(), G->Nodes[j]->GetUp());
			}
			else
			{
				ret += UnconnectedNodeNodeDist_Val(G->Nodes[i]->GetPosition(), G->Nodes[j]->GetPosition(), G->Nodes[i]->Radius + G->Nodes[j]->Radius);
			}
		}
	}

	if (grad)
	{
		for (int par = 0; par < n; par++)
		{
			grad[par] = 0;

			auto i = ParamToNode(par);
			auto axis = ParamToParamInNode(par);
			TSharedPtr<SNode> node_i = G->Nodes[i];

			for (int j = 0; j < G->Nodes.Num(); j++)
			{
				if (j != i)
				{
					auto idxs = JoinIdxs{ i, j };
					TSharedPtr<SNode> node_j = G->Nodes[j];

					if (axis < 3)
					{
						// node distances depend only on positions
						// the node whose gradient we are calculating always gets passed first into the gradient function
						if (Connected.Contains(idxs))
						{
							{
								auto here_grad = ConnectedNodeNodeDist_Grad(node_i->GetPosition(), node_j->GetPosition(), Connected[idxs], axis);

#ifndef UE_BUILD_RELEASE
								double diff = here_grad - check_grad(node_i->GetPosition(), node_j->GetPosition(), axis, 1e-3, Connected[idxs], ConnectedNodeNodeDist_Val);
								check(abs(diff) < 1E-2);
#endif

								grad[par] += here_grad;
							}
						}
						else
						{
							float radius = node_i->Radius + node_j->Radius;

							auto here_grad = UnconnectedNodeNodeDist_Grad(node_i->GetPosition(), node_j->GetPosition(), radius, axis);

#ifndef UE_BUILD_RELEASE
							double diff = here_grad - check_grad(node_i->GetPosition(), node_j->GetPosition(), axis, 1e-3, radius, UnconnectedNodeNodeDist_Val);
							check(abs(diff) < 1E-2);
#endif

							grad[par] += here_grad;
						}
					}
					else
					{
						// torsions depend only on normals
						if (Connected.Contains(idxs))
						{
							// we need the raw up for the actual param value of the one we're getting the gradient for...
							auto here_grad = ConnectedNodeNodeTorsion_Grad(node_i->GetRawUp(), node_j->GetUp(), axis - 3);

#ifndef UE_BUILD_RELEASE
							double check = check_grad(node_i->GetRawUp(), node_j->GetUp(), axis - 3, 1e-4, ConnectedNodeNodeTorsion_Val);
							check(abs(check - here_grad) < 2E-2);
#endif

							grad[par] += here_grad;
						}
					}
				}
			}
		}
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
		SetVector(x, n, i, 0, G->Nodes[i]->GetPosition());
		SetVector(x, n, i, 1, G->Nodes[i]->GetRawUp());
	}
}

void OptFunction::SetState(const double* x, int n)
{
	check(n == GetSize());

	for (int i = 0; i < G->Nodes.Num(); i++)
	{
		auto& node = G->Nodes[i];

		node->SetPosition(GetVector(x, n, i, 0));
		node->SetUp(GetVector(x, n, i, 1));

		FVector forward;

		check(node->Edges.Num() > 0);

		const auto& n1 = node->Edges[0].Pin()->OtherNode(node).Pin();

		if (node->Edges.Num() == 2)
		{
			const auto& n2 = node->Edges[1].Pin()->OtherNode(node).Pin();

			// we "grow" these linkages away from an existing node, so
			// "forward" is towards the second node added
			forward = n2->GetPosition() - n1->GetPosition();
		}
		else
		{
			forward = n1->GetPosition() - node->GetPosition();
		}

		forward.Normalize();

		node->SetForward(forward);
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

void Opt::OptFunction::GetLimits(double* lower, double* upper, int n) const
{
	for (int i = 0; i < G->Nodes.Num(); i++)
	{
		lower[NodeToParam(i, 0, n)] = G->Nodes[i]->GetPosition().X - 10000;
		lower[NodeToParam(i, 1, n)] = G->Nodes[i]->GetPosition().Y - 10000;
		lower[NodeToParam(i, 2, n)] = G->Nodes[i]->GetPosition().Z - 10000;

		lower[NodeToParam(i, 3, n)] = -1;
		lower[NodeToParam(i, 4, n)] = -1;
		lower[NodeToParam(i, 5, n)] = -1;

		upper[NodeToParam(i, 0, n)] = G->Nodes[i]->GetPosition().X + 10000;
		upper[NodeToParam(i, 1, n)] = G->Nodes[i]->GetPosition().Y + 10000;
		upper[NodeToParam(i, 2, n)] = G->Nodes[i]->GetPosition().Z + 10000;

		upper[NodeToParam(i, 3, n)] = 1;
		upper[NodeToParam(i, 4, n)] = 1;
		upper[NodeToParam(i, 5, n)] = 1;
	}
}

#pragma optimize ("", on)

