#pragma once

#include "gsl/multimin/gsl_multimin.h"

class GSLMinimizer
{
	const gsl_multimin_fdfminimizer_type *T;
	gsl_multimin_fdfminimizer *s;

public:
	GSLMinimizer();
	~GSLMinimizer();
};

