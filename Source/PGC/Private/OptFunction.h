#pragma once

#include "StructuralGraph.h"

namespace Opt {

struct JoinIdxs {
	const int I;
	const int J;

	JoinIdxs(int i, int j) : I(FMath::Min(i, j)), J(FMath::Max(i, j)) {}

	static inline friend uint32 GetTypeHash(const JoinIdxs& other) {
		return ::GetTypeHash(other.I) ^ ::GetTypeHash(other.J) * 37;
	}

	bool operator==(const JoinIdxs& rhs) const {
		return I == rhs.I && J == rhs.J;
	}
};

struct AngleIdxs {
	const int I;
	const int J;
	const int K;

	AngleIdxs(int i, int j, int k) : I(i), J(j), K(K) {}

	static inline friend uint32 GetTypeHash(const AngleIdxs& a) {
		return ::GetTypeHash(a.I) ^ ::GetTypeHash(a.J) ^ ::GetTypeHash(a.K);
	}

	bool operator==(const AngleIdxs& rhs) const {
		return I == rhs.I && J == rhs.J && K == rhs.K;
	}
};

class OptFunction : public NlOptIface {
	TSharedPtr<StructuralGraph::SGraph> G;
	TMap<JoinIdxs, double> Connected;
	TSet<AngleIdxs> Angles;

	static double UnconnectedNodeNodeDist_Val(const FVector& p1, const FVector& p2, float D0);
	static double UnconnectedNodeNodeDist_Grad(const FVector& pGrad, const FVector& pOther, float D0, int axis);

	static double ConnectedNodeNodeTorsion_Val(const FVector& up1, const FVector& up2);
	static double ConnectedNodeNodeTorsion_Grad(const FVector& upGrad, const FVector& upOther, int axis);

	static double ConnectedNodeNodeDist_Val(const FVector& p1, const FVector& p2, float D0);
	static double ConnectedNodeNodeDist_Grad(const FVector& pGrad, const FVector& pOther, float D0, int axis);

	// R = distance, D = optimal distance, N = power
	double LeonardJonesVal(double R, double D, int N) const;
//	double LeonardJonesGrad(double R, double D, int N) const;

public:
	OptFunction(TSharedPtr<StructuralGraph::SGraph> g);
	virtual ~OptFunction() = default;

	// Inherited via NlOptIface
	int GetSize() const;
	virtual double f(int n, const double * x, double * grad) override;
	virtual void GetInitialStepSize(double* steps, int n) const override;
	virtual void GetLimits(double * lower, double * upper, int n) const override;
	virtual void GetState(double* x, int n) const override;
	virtual void SetState(const double* x, int n) override;

	// Inherited via NlOptIface
	virtual void reset_histo() override;
	virtual void print_histo() override;
};

}