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
	virtual void GetLimits(double* lower, double* upper, int n) const = 0;
	virtual void GetState(double* x, int n) const = 0;
	virtual void SetState(const double* x, int n) = 0;

	virtual TArray<FString> GetEnergyTermNames() const = 0;
	virtual TArray<double> GetLastEnergyTerms() const = 0;

	//virtual void reset_histo() = 0;
	//virtual void print_histo() = 0;
};

class NlOptWrapper
{
	const TSharedPtr<NlOptIface> NlIface;

	int LoggingFreq = 1000;
	bool First = true;
	double BestEnergy;
	TArray<double> BestEnergyComponents;

	static double f_callback(unsigned n, const double* x, double* grad, void* data);
	double f_callback_inner(unsigned n, const double* x, double* grad);

	bool RunOptimization(nlopt_algorithm alg, int steps, bool use_limits);

	void Log(const char* note);

public:
	NlOptWrapper(const TSharedPtr<NlOptIface> iface);
	~NlOptWrapper();

	// logging freq in ms
	bool RunOptimization(bool use_limits, int loggingFreq);
};

