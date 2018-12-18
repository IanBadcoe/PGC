#pragma once

extern "C" {
#include "nlopt/nlopt.h"
}

class NlOptWrapper
{
	nlopt_opt NlOpt;

public:
	NlOptWrapper(int n);
	~NlOptWrapper();
};

