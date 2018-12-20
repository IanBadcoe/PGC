#include "NlOptWrapper.h"

#include "Runtime/Core/Public/Templates/SharedPointer.h"


NlOptWrapper::NlOptWrapper(const TSharedPtr<NlOptIface> iface)
	: NlIface(iface)
{
	NlOpt = nlopt_create(NLOPT_LD_SLSQP, NlIface->GetSize());
	nlopt_set_min_objective(NlOpt, &f_callback, this);
	nlopt_set_ftol_rel(NlOpt, 0.00001);
	nlopt_set_maxeval(NlOpt, 1);

	TArray<double> initial_step;
	initial_step.AddDefaulted(NlIface->GetSize());

	nlopt_set_initial_step(NlOpt, initial_step.GetData());
}

NlOptWrapper::~NlOptWrapper()
{
	nlopt_destroy(NlOpt);
}

bool NlOptWrapper::Optimize()
{
	TArray<double> state;
	state.AddDefaulted(NlIface->GetSize());
	NlIface->GetState(state);

	double val = 0.0;

	auto res = nlopt_optimize(NlOpt, state.GetData(), &val);

	NlIface->SetState(state);

	return res != NLOPT_MAXEVAL_REACHED;
}

double NlOptWrapper::f_callback(unsigned n, const double* x, double* grad, void* data)
{
	NlOptWrapper* This = reinterpret_cast<NlOptWrapper*>(data);

	return This->NlIface->f(n, x, grad);
}

