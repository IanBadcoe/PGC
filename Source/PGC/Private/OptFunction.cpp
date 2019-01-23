#include "OptFunction.h"

using namespace Opt;
using namespace StructuralGraph;

#pragma optimize ("", off)

#define PARAMS_PER_NODE 3

static int ParamToNode(int vect_idx)
{
	return vect_idx / PARAMS_PER_NODE;
}

static int NodeToParamStart(int vect_idx, int axis)
{
	check(axis >= 0 && axis < PARAMS_PER_NODE);

	return vect_idx * PARAMS_PER_NODE + axis;
}

static int ParamToAxis(int vect_idx)
{
	return vect_idx % 3;
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


double OptFunction::UnconnectedNodeNodeVal(const FVector& p1, const FVector& p2, float D)
{
	float dist = FVector::Dist(p1, p2);

	if (dist >= D)
		return 0;

	return FMath::Pow(dist - D, 2) * 0.01;		//LeonardJonesVal(dist, D, 1);
}

double OptFunction::UnconnectedNodeNodeGrad(const FVector & pGrad, const FVector & pOther, float D0, int axis)
{
	check(axis >= 0 && axis < 3);

	float dist = FVector::Dist(pGrad, pOther);

	if (dist >= D0)
		return 0.0;

	return 2 * (pGrad[axis] - pOther[axis]) * (dist - D0) / dist * 0.01;
}

double OptFunction::ConnectedNodeNodeVal(const FVector & p1, const FVector & p2, float D0)
{
	float dist = FVector::Dist(p1, p2);

	add_hist(dist / D0);

	return pow(dist - D0, 2); //LeonardJonesVal(dist, D0, 2);
}

double OptFunction::ConnectedNodeNodeGrad(const FVector & pGrad, const FVector & pOther, float D0, int axis)
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

static FVector GetVector(const double* x, int x_size, int vect_idx) {
	check(vect_idx >= 0);
	check(NodeToParamStart(vect_idx, 2) < x_size);

	return FVector
	{
		(float)x[NodeToParamStart(vect_idx, 0)],
		(float)x[NodeToParamStart(vect_idx, 1)],
		(float)x[NodeToParamStart(vect_idx, 2)]
	};
}

static void SetVector(double* x, int x_size, int vect_idx, const FVector& v)
{
	check(vect_idx >= 0);
	check(NodeToParamStart(vect_idx, 2) < x_size);

	x[NodeToParamStart(vect_idx, 0)] = v.X;
	x[NodeToParamStart(vect_idx, 1)] = v.Y;
	x[NodeToParamStart(vect_idx, 2)] = v.Z;
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

typedef double (*ValueFunction)(const FVector&, const FVector&, float D0);

double check_grad(const FVector& pGrad, const FVector& pOther, float D0, int par, ValueFunction pFun)
{
	FVector offset{ 0, 0, 0 };

	offset[par] = 1e-3;

	auto pGradPlus = pGrad + offset;
	auto pGradMinus = pGrad - offset;

	auto valPlus = pFun(pGradPlus, pOther, D0);
	auto valMinus = pFun(pGradMinus, pOther, D0);

	return (valPlus - valMinus) / 2E-3;
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
				ret += ConnectedNodeNodeVal(G->Nodes[i]->Position, G->Nodes[j]->Position, Connected[idxs]);
			}
			else
			{
				ret += UnconnectedNodeNodeVal(G->Nodes[i]->Position, G->Nodes[j]->Position, G->Nodes[i]->Radius + G->Nodes[j]->Radius);
			}
		}
	}

	if (grad)
	{
		for (int par = 0; par < n; par++)
		{
			grad[par] = 0;

			auto i = ParamToNode(par);
			auto axis = ParamToAxis(par);
			TSharedPtr<SNode> node_i = G->Nodes[i];

			for (int j = 0; j < G->Nodes.Num(); j++)
			{
				if (j != i)
				{
					auto idxs = JoinIdxs{ i, j };
					TSharedPtr<SNode> node_j = G->Nodes[j];

					// the node whose gradient we are calculating always gets passed first into the gradient function
					if (Connected.Contains(idxs))
					{
						auto here_grad = ConnectedNodeNodeGrad(node_i->Position, node_j->Position, Connected[idxs], axis);

#ifndef UE_BUILD_RELEASE
						double diff = here_grad - check_grad(node_i->Position, node_j->Position, Connected[idxs], axis, ConnectedNodeNodeVal);
						check(abs(diff) < 1E-2);
#endif

						grad[par] += here_grad;
					}
					else
					{
						float radius = node_i->Radius + node_j->Radius;

						auto here_grad = UnconnectedNodeNodeGrad(node_i->Position, node_j->Position, radius, axis);

#ifndef UE_BUILD_RELEASE
						double diff = here_grad - check_grad(node_i->Position, node_j->Position, radius, axis, UnconnectedNodeNodeVal);
						check(abs(diff) < 1E-2);
#endif

						grad[par] += here_grad;
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
		SetVector(x, n, i + 0, G->Nodes[i]->Position);

		//SetVector(x, i * 2 + 0, G->Nodes[i]->Position);
		//SetVector(x, i * 2 + 1, G->Nodes[i]->Up);
	}
}

void OptFunction::SetState(const double* x, int n)
{
	check(n == GetSize());

	for (int i = 0; i < G->Nodes.Num(); i++)
	{
		auto& node = G->Nodes[i];

		//auto up = GetVector(x, i * 2 + 1);
		//up.Normalize();
		//node->SetPosition(GetVector(x, i * 2), up);

		node->SetPosition(GetVector(x, n, i), node->Up);

		FVector forwards;

		check(node->Edges.Num() > 0);

		const auto& n1 = node->Edges[0].Pin()->OtherNode(node).Pin();

		if (node->Edges.Num() == 2)
		{
			const auto& n2 = node->Edges[1].Pin()->OtherNode(node).Pin();

			// we "grow" these linkages away from an existing node, so
			// "forwards" is towards the second node added
			forwards = n2->Position - n1->Position;
		}
		else
		{
			forwards = n1->Position - node->Position;
		}

		forwards.Normalize();

		node->Forwards = forwards;
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

//double ConnectedEnergy::Value(const TSharedPtr<StructuralGraph::SGraph>& graph) const
//{
//	const auto& p1 = graph->Nodes[Nodes[0]]->Position;
//	const auto& p2 = graph->Nodes[Nodes[1]]->Position;
//
//	float dist = FVector::Dist(p1, p2);
//
//	add_hist(dist / D0);
//
//	return pow(dist - D0, 2); //LeonardJonesVal(dist, D0, 2);
//}
//
//double ConnectedEnergy::Derivative(const TSharedPtr<StructuralGraph::SGraph>& graph, int node, int axis) const
//{
//	check(NumNodes == 2);
//	check(node >= 0 && node < NumNodes);
//	check(axis <= 0 && axis < 3);
//
//	const auto& p1 = graph->Nodes[Nodes[0]]->Position;
//	const auto& p2 = graph->Nodes[Nodes[1]]->Position;
//
//	FVector ps[2] = { p1, p2 };
//
//	float dist = FVector::Dist(p1, p2);
//
//	return 2 * (ps[node][axis] - ps[1 - node][axis]) * (dist - D0) / dist;
//}
//
//bool ConnectedEnergy::UsesNode(int idx) const
//{
//	return Nodes[0] == idx || Nodes[1] == idx;
//}
//
//double UnconnectedEnergy::Value(const TSharedPtr<StructuralGraph::SGraph>& graph) const
//{
//	const auto& p1 = graph->Nodes[Nodes[0]]->Position;
//	const auto& p2 = graph->Nodes[Nodes[1]]->Position;
//
//	float dist = FVector::Dist(p1, p2);
//
//	if (dist >= D0)
//	{
//		return 0.0;
//	}
//
//	add_hist(dist / D0);
//
//	return pow(dist - D0, 2); //LeonardJonesVal(dist, D0, 2);
//}
//
//double UnconnectedEnergy::Derivative(const TSharedPtr<StructuralGraph::SGraph>& graph, int node, int axis) const
//{
//	check(NumNodes == 2);
//	check(node >= 0 && node < NumNodes);
//	check(axis <= 0 && axis < 3);
//
//	const auto& p1 = graph->Nodes[Nodes[0]]->Position;
//	const auto& p2 = graph->Nodes[Nodes[1]]->Position;
//
//	FVector ps[2] = { p1, p2 };
//
//	float dist = FVector::Dist(p1, p2);
//
//	if (dist >= D0)
//		return 0.0;
//
//	return 2 * (ps[node][axis] - ps[1 - node][axis]) * (dist - D0) / dist;
//}
//
//bool UnconnectedEnergy::UsesNode(int idx) const
//{
//	return Nodes[0] == idx || Nodes[1] == idx;
//}

#pragma optimize ("", on)

