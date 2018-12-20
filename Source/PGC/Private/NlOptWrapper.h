#pragma once

extern "C" {
#include "nlopt.h"
}

class NlOptIface {
public:
	virtual double f(int n, const double* x, double* grad) const = 0;
	virtual int GetSize() const = 0;
};

class NlOptWrapper
{
	friend double f_callback(int, const double*, double*, void*);

	nlopt_opt NlOpt;
	const NlOptIface& NlIface;

	static double f_callback(unsigned n, const double* x, double* grad, void* data);

public:
	NlOptWrapper(const NlOptIface& iface);
	~NlOptWrapper();
};

