#pragma once

#include "IntermediateGraph.h"
#include "StructuralGraph.h"

namespace SetupOpt
{

class SetupOptFunction : public NlOptIface {
	using IGraph = StructuralGraph::IGraph;
	using IEdge = StructuralGraph::IEdge;
	using INode = StructuralGraph::INode;

	const TSharedPtr<IGraph> Graph;

	const double NodeAngleDistEnergyScale;
	const double EdgeAngleEnergyScale;
	const double PlanarEnergyScale;
	const double LengthEnergyScale;
	const double EdgeEdgeEnergyScale;

	const double EdgeRadiusScale;		// scale-up nodes to try and allow some clearance

	double NodeAngleEnergy = 0;
	double EdgeAngleEnergy = 0;
	double PlanarEnergy = 0;
	double LengthEnergy = 0;
	double EdgeEdgeEnergy = 0;

	static double EdgeLength_Energy(const FVector& p1, const FVector& p2, double D0);
	static double JunctionAngle_Energy(const TSharedPtr<INode> node);
	static double EdgeAngle_Energy(const TSharedPtr<INode> node);
	static double JunctionPlanar_Energy(const TSharedPtr<INode> node);
	// non-static to access radius scale, could pass in...
	double EdgeEdge_Energy(const TSharedPtr<IEdge> e1, const TSharedPtr<IEdge> e2);

	struct EdgePair {
		TSharedPtr<IEdge> Edge1;
		TSharedPtr<IEdge> Edge2;
	};

	TArray<EdgePair> CollidableEdges;

public:
	SetupOptFunction(const TSharedPtr<IGraph> graph,
		double junction_angle_energy_scale, double edge_angle_energy_scale,
		double planar_energy_scale, double length_energy_scale, double edge_edge_energy_scale,
		double edge_radius_scale);
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

}