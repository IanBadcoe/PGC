#include "NlOptWrapper.h"



NlOptWrapper::NlOptWrapper(int n)
{
	NlOpt = nlopt_create(NLOPT_LD_SLSQP, n);
}


NlOptWrapper::~NlOptWrapper()
{
	nlopt_destroy(NlOpt);
}
