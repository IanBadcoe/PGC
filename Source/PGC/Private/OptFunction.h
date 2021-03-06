#pragma once

#include "StructuralGraph.h"

namespace Opt {

struct JoinIdxs {
	const TSharedPtr<StructuralGraph::SNode> I;
	const TSharedPtr<StructuralGraph::SNode> J;

	JoinIdxs(TSharedPtr<StructuralGraph::SNode> i, TSharedPtr<StructuralGraph::SNode> j) : I(i.Get() < j.Get() ? i : j), J(i.Get() < j.Get() ? j : i)
	{
		check(I != J);
	}

	static inline friend uint32 GetTypeHash(const JoinIdxs& other) {
		return HashCombine(::GetTypeHash(other.I.Get()), ::GetTypeHash(other.J.Get()));
	}

	bool operator==(const JoinIdxs& rhs) const {
		return I == rhs.I && J == rhs.J;
	}
};

struct JoinData {
	bool Flipped;
	double D0;

	JoinData(bool flipped, double d0) : Flipped(flipped), D0(d0) {}
	JoinData() : Flipped(false), D0(0) {}
};

class OptFunction : public NlOptIface {
	TSharedPtr<StructuralGraph::SGraph> G;
	TMap<JoinIdxs, JoinData> Connected;

	double ConnectedEnergy;
	double UnconnectedEnergy;
	double TorsionEnergy;
	double BendEnergy;
	double JunctionAngleEnergy;
	double JunctionPlanarEnergy;

	const double ConnectedScale;
	const double UnconnectedScale;
	const double TorsionScale;
	const double BendScale;
	const double JunctionAngleScale;
	const double JunctionPlanarScale;

	static double UnconnectedNodeNodeDist_Val(const FVector& p1, const FVector& p2, float D0);
	// torsion in a parent-child pair is theoretically defined by the Rotation on the child,
	// however not all connections are parent-child pairs, so more general to do our own torsion
	// calculation
	static double ConnectedNodeNodeTorsion_Val(const FVector& up1, const FVector& up2, const FVector& p1, const FVector& p2, bool flipped);
	static double ConnectedNodeNodeDist_Val(const FVector& p1, const FVector& p2, float D0);
	static double ConnectedNodeNodeBend_Val(const FVector& prev_pos, const FVector& pos, const FVector& next_pos);
	static double JunctionAngle_Energy(const TSharedPtr<StructuralGraph::SNode> node);
	static double JunctionPlanar_Energy(const TSharedPtr<StructuralGraph::SNode> node);

public:
	OptFunction(TSharedPtr<StructuralGraph::SGraph> g,
		double connected_scale, double unconnected_scale, double torsion_scale,
		double bend_scale, double jangle_scale, double jplanar_scale);
	virtual ~OptFunction() = default;

	// Inherited via NlOptIface
	int GetSize() const;
	virtual double f(int n, const double * x, double * grad) override;
	virtual void GetInitialStepSize(double* steps, int n) const override;
	virtual void GetLimits(double * lower, double * upper, int n) const override;
	virtual void GetState(double* x, int n) const override;
	virtual void SetState(const double* x, int n) override;

	virtual TArray<FString> GetEnergyTermNames() const override;
	virtual TArray<double> GetLastEnergyTerms() const override;

	//virtual void reset_histo() override;
	//virtual void print_histo() override;
};

}