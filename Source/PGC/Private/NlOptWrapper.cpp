#include "NlOptWrapper.h"

#include "Runtime/Core/Public/Templates/SharedPointer.h"


NlOptWrapper::NlOptWrapper(const TSharedPtr<NlOptIface> iface)
	: NlIface(iface)
{
}

NlOptWrapper::~NlOptWrapper()
{
}

bool NlOptWrapper::RunOptimization()
{
	if (RunOptimization(NLOPT_LN_SBPLX, 10000))
	{
		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("Simplex Completed"));

	return RunOptimization(NLOPT_LD_SLSQP, 10000);
}

double NlOptWrapper::f_callback(unsigned n, const double* x, double* grad, void* data)
{
	NlOptWrapper* This = reinterpret_cast<NlOptWrapper*>(data);

	This->NlIface->reset_histo();

	auto ret = This->NlIface->f(n, x, grad);

	static auto best = 0.0f;
	static auto first = true;

	if (first || ret < best) {
		best = ret;
		first = false;

		UE_LOG(LogTemp, Warning, TEXT("Target function best seen: %f"), best);
		This->NlIface->print_histo();
	}

	return ret;
}

bool NlOptWrapper::RunOptimization(nlopt_algorithm alg, int steps)
{
	nlopt_opt NlOpt = nlopt_create(alg, NlIface->GetSize());
	nlopt_set_min_objective(NlOpt, &f_callback, this);
	nlopt_set_ftol_rel(NlOpt, 1.0E-6);
	nlopt_set_maxeval(NlOpt, steps);

	TArray<double> initial_step;
	initial_step.AddDefaulted(NlIface->GetSize());

	NlIface->GetInitialStepSize(initial_step.GetData(), NlIface->GetSize());
	nlopt_set_initial_step(NlOpt, initial_step.GetData());

	TArray<double> upper, lower;
	upper.AddDefaulted(NlIface->GetSize());
	lower.AddDefaulted(NlIface->GetSize());

	NlIface->GetLimits(lower.GetData(), upper.GetData(), NlIface->GetSize());
	nlopt_set_lower_bounds(NlOpt, lower.GetData());
	nlopt_set_upper_bounds(NlOpt, upper.GetData());

	TArray<double> state;
	state.AddDefaulted(NlIface->GetSize());
	NlIface->GetState(state.GetData(), NlIface->GetSize());

	double val = 0.0;

	auto res = nlopt_optimize(NlOpt, state.GetData(), &val);

	NlIface->SetState(state.GetData(), NlIface->GetSize());

	nlopt_destroy(NlOpt);

	return res != NLOPT_MAXEVAL_REACHED;
}

