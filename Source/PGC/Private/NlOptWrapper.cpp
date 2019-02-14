#include "NlOptWrapper.h"

#include "Runtime/Core/Public/Templates/SharedPointer.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"

NlOptWrapper::NlOptWrapper(const TSharedPtr<NlOptIface> iface)
	: NlIface(iface)
{
}

NlOptWrapper::~NlOptWrapper()
{
}

bool NlOptWrapper::RunOptimization()
{
	//TArray<double> state;
	//state.AddDefaulted(NlIface->GetSize());
	//NlIface->GetState(state.GetData(), state.Num());

	//for (int i = 0; i < state.Num(); i++)
	//{
	//	state[i] += FMath::FRand() * 0.4 - 0.2;
	//}

	//NlIface->SetState(state.GetData(), state.Num());

	FDateTime timeUtc = FDateTime::UtcNow();
	int64 start = timeUtc.ToUnixTimestamp() * 1000 + timeUtc.GetMillisecond();

	// NLOPT_LN_SBPLX - 42s - unreliable
	// NLOPT_GN_ISRES - nothing
	// NLOPT_LN_COBYLA - 28s - failed to straighten edge
	// NLOPT_LN_COBYLA - with bounds - 90s - failed to straighten edge
	// NLOPT_LN_BOBYQA - failed to straighten
	// NLOPT_LN_PRAXIS
	// NLOPT_GN_DIRECT - took ages, never improved it, returned mangled
	// NLOPT_GN_DIRECT_L - likewise
	// NLOPT_GN_ISRES - needs bounds - failed to unfold edge
	// NLOPT_GN_CRS2_LM - 62s - making good progress but not converged after 1000000 steps
	// NLOPT_GN_ESCH - failed to start?

	auto ret = RunOptimization(NLOPT_LN_SBPLX, 1000000, true);

	timeUtc = FDateTime::UtcNow();
	int64 end = timeUtc.ToUnixTimestamp() * 1000 + timeUtc.GetMillisecond();

	UE_LOG(LogTemp, Warning, TEXT("Time elapsed: %ldms"), end - start);

	//if (RunOptimization(NLOPT_LN_SBPLX, 10000))
	//{
	//	return true;
	//}

	//UE_LOG(LogTemp, Warning, TEXT("Simplex Completed"));

	//return RunOptimization(NLOPT_LD_SLSQP, 10000);

	return ret;
}

double NlOptWrapper::f_callback(unsigned n, const double* x, double* grad, void* data)
{
	NlOptWrapper* This = reinterpret_cast<NlOptWrapper*>(data);

	return This->f_callback_inner(n, x, grad);
}

double NlOptWrapper::f_callback_inner(unsigned n, const double * x, double * grad)
{
	auto ret = NlIface->f(n, x, grad);

	static auto best = 0.0f;
	static auto first = true;

	if (first || ret < best) {
		static int64 last_update = -1000;

		FDateTime timeUtc = FDateTime::UtcNow();
		int64 now = timeUtc.ToUnixTimestamp() * 1000 + timeUtc.GetMillisecond();

		// don't log too often as it really slows us down...
		if (now - last_update > 1000)
		{
			best = ret;
			first = false;

			UE_LOG(LogTemp, Warning, TEXT("Target function best seen: %f"), best);
			//		This->NlIface->print_histo();

			auto names = NlIface->GetEnergyTermNames();
			auto energies = NlIface->GetLastEnergyTerms();

			for (int i = 0; i < names.Num(); i++)
			{
				UE_LOG(LogTemp, Warning, TEXT("Energy: %s: %f"), *names[i], energies[i]);
			}

			last_update = now;
		}
	}

	return ret;
}

bool NlOptWrapper::RunOptimization(nlopt_algorithm alg, int steps, bool use_limits)
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

	if (use_limits)
	{
		NlIface->GetLimits(lower.GetData(), upper.GetData(), NlIface->GetSize());
		nlopt_set_lower_bounds(NlOpt, lower.GetData());
		nlopt_set_upper_bounds(NlOpt, upper.GetData());
	}

	TArray<double> state;
	state.AddDefaulted(NlIface->GetSize());
	NlIface->GetState(state.GetData(), NlIface->GetSize());

	double val = 0.0;

	auto res = nlopt_optimize(NlOpt, state.GetData(), &val);

	NlIface->SetState(state.GetData(), NlIface->GetSize());

	nlopt_destroy(NlOpt);

	return res != NLOPT_MAXEVAL_REACHED;
}

