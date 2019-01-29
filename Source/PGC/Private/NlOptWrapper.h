#pragma once

#include "Runtime/Core/Public/Templates/SharedPointer.h"

extern "C" {
#include "nlopt.h"
}

class NlOptIface {
public:
	virtual double f(int n, const double* x, double* grad) = 0;
	virtual int GetSize() const = 0;
	virtual void GetInitialStepSize(double* steps, int n) const = 0;
	virtual void GetState(double* x, int n) const = 0;
	virtual void SetState(const double* x, int n) = 0;

	virtual void reset_histo() = 0;
	virtual void print_histo() = 0;
};

class NlOptWrapper
{
	friend double f_callback(int, const double*, double*, void*);

	const TSharedPtr<NlOptIface> NlIface;

	static double f_callback(unsigned n, const double* x, double* grad, void* data);

	bool RunOptimization(nlopt_algorithm alg, int steps);

public:
	NlOptWrapper(const TSharedPtr<NlOptIface> iface);
	~NlOptWrapper();

	bool RunOptimization();
};

