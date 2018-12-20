#include "NlOptWrapper.h"


NlOptWrapper::NlOptWrapper(const NlOptIface& iface)
	: NlIface(iface)
{
	NlOpt = nlopt_create(NLOPT_LD_SLSQP, iface.GetSize());
	nlopt_set_min_objective(NlOpt, &f_callback, this);
}

NlOptWrapper::~NlOptWrapper()
{
	nlopt_destroy(NlOpt);
}

double NlOptWrapper::f_callback(unsigned n, const double* x, double* grad, void* data)
{
	NlOptWrapper* This = reinterpret_cast<NlOptWrapper*>(data);

	return This->NlIface.f(n, x, grad);
}

