#pragma once

#include "IntermediateGraph.h"
#include "StructuralGraph.h"

class SetupOptFunction : public NlOptIface {
	using IGraph = IntermediateGraph::IGraph<TSharedPtr<SNode>>;
	using IEdge = IntermediateGraph::IEdge<TSharedPtr<SNode>>;
	using INode = IntermediateGraph::INode<TSharedPtr<SNode>>;

	const IGraph& Graph;

	const double NodeAngleDistEnergyScale;
	const double EdgeAngleEnergyScale;
	const double PlanarEnergyScale;
	const double LengthEnergyScale;
	const double EdgeEdgeEnergyScale;

	double NodeAngleEnergy = 0;
	double EdgeAngleEnergy = 0;
	double PlanarEnergy = 0;
	double LengthEnergy = 0;
	double EdgeEdgeEnergy = 0;

	static double EdgeLength_Energy(const FVector& p1, const FVector& p2, double D0);
	static double NodeAngle_Energy(const TSharedPtr<INode> node);
	static double EdgeAngle_Energy(const TSharedPtr<INode> node);
	static double NodePlanar_Energy(const TSharedPtr<INode> node);
	static double EdgeEdge_Energy(const TSharedPtr<IEdge> e1, const TSharedPtr<IEdge> e2);

	struct EdgePair {
		TSharedPtr<IEdge> Edge1;
		TSharedPtr<IEdge> Edge2;
	};

	TArray<EdgePair> CollidableEdges;

public:
	SetupOptFunction(const IGraph& graph,
		double nade_scale, double eae_scale, double pe_scale, double le_scale, double eee_scale);
	virtual ~SetupOptFunction() {}

	// Inherited via NlOptIface
	virtual double f(int n, const double * x, double * grad) override;

	virtual int GetSize() const override;

	virtual void GetInitialStepSize(double* steps, int n) const override;

	virtual void GetLimits(double* lower, double* upper, int n) const override;

	virtual void GetState(double* x, int n) const override;

	virtual void SetState(const double * x, int n) override;

	virtual TArray<FString> GetEnergyTermNames() const override;

	virtual TArray<double> GetLastEnergyTerms() const override;
};



