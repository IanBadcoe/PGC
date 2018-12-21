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
};

class NlOptWrapper
{
	friend double f_callback(int, const double*, double*, void*);

	nlopt_opt NlOpt;
	const TSharedPtr<NlOptIface> NlIface;

	static double f_callback(unsigned n, const double* x, double* grad, void* data);

public:
	NlOptWrapper(const TSharedPtr<NlOptIface> iface);
	~NlOptWrapper();

	bool Optimize();
};

