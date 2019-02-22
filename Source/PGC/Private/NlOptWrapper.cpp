#include "NlOptWrapper.h"

#include "Runtime/Core/Public/Templates/SharedPointer.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"

PRAGMA_DISABLE_OPTIMIZATION

NlOptWrapper::NlOptWrapper(const TSharedPtr<NlOptIface> iface)
	: NlIface(iface)
{
}

NlOptWrapper::~NlOptWrapper()
{
}

bool NlOptWrapper::RunOptimization(bool use_limits, int loggingFreq, double precision, double* out_energy)
{
	LoggingFreq = loggingFreq;
	First = true;

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
	// NLOPT_LN_NELDERMEAD - converged really quickly into a bad local minimum

	// conclusion: SubPlex is best, but if I could do a simpler optimisation on the starting config first it might work
	// more reliably/faster (Done now: see SetupOptFunction)

	auto ret = RunOptimization(NLOPT_LN_SBPLX, 1000000, use_limits, precision, out_energy);

	if (loggingFreq != -1)
	{
		if (ret)
		{
			UE_LOG(LogTemp, Warning, TEXT("-------------------------------------"));
			UE_LOG(LogTemp, Warning, TEXT("Converged"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("-------------------------------------"));
			UE_LOG(LogTemp, Warning, TEXT("Not Converged"));
		}

		Log("(final)");
	}

	timeUtc = FDateTime::UtcNow();
	int64 end = timeUtc.ToUnixTimestamp() * 1000 + timeUtc.GetMillisecond();

	if (loggingFreq != -1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Time elapsed: %ldms"), end - start);
		UE_LOG(LogTemp, Warning, TEXT("-------------------------------------"));
	}

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

	static int64 last_update = -1000;

	FDateTime timeUtc = FDateTime::UtcNow();
	int64 now = timeUtc.ToUnixTimestamp() * 1000 + timeUtc.GetMillisecond();

	if (ret < BestEnergy || First)
	{
		BestEnergy = ret;
		BestEnergyComponents = NlIface->GetLastEnergyTerms();
	}

	if (LoggingFreq == -1)
	{
		First = false;
		return ret;
	}

	// don't log too often as it really slows us down...
	if (First || now - last_update >= LoggingFreq)
	{
		Log(First ? "(initial)" : "");

		last_update = now;
		First = false;
	}

	return ret;
}

bool NlOptWrapper::RunOptimization(nlopt_algorithm alg, int steps, bool use_limits, double precision, double* out_energy)
{
	nlopt_opt NlOpt = nlopt_create(alg, NlIface->GetSize());
	nlopt_set_min_objective(NlOpt, &f_callback, this);
	nlopt_set_ftol_rel(NlOpt, precision);
	nlopt_set_ftol_abs(NlOpt, precision);
	nlopt_set_maxeval(NlOpt, steps);

	TArray<double> initial_step;
	initial_step.AddDefaulted(NlIface->GetSize());

	NlIface->GetInitialStepSize(initial_step.GetData(), NlIface->GetSize());
	nlopt_set_initial_step(NlOpt, initial_step.GetData());

	if (use_limits)
	{
		TArray<double> upper, lower;
		upper.AddDefaulted(NlIface->GetSize());
		lower.AddDefaulted(NlIface->GetSize());

		NlIface->GetLimits(lower.GetData(), upper.GetData(), NlIface->GetSize());
		nlopt_set_lower_bounds(NlOpt, lower.GetData());
		nlopt_set_upper_bounds(NlOpt, upper.GetData());
	}

	TArray<double> state;
	state.AddDefaulted(NlIface->GetSize());
	NlIface->GetState(state.GetData(), NlIface->GetSize());

	double val = 0.0;

	auto res = nlopt_optimize(NlOpt, state.GetData(), out_energy ? out_energy : &val);

	NlIface->SetState(state.GetData(), NlIface->GetSize());

	nlopt_destroy(NlOpt);

	return res != NLOPT_MAXEVAL_REACHED;
}

void NlOptWrapper::Log(const char* note)
{
	UE_LOG(LogTemp, Warning, TEXT("%sTarget function best seen: %f"), *FString(note), BestEnergy);
	//		This->NlIface->print_histo();

	auto names = NlIface->GetEnergyTermNames();

	for (int i = 0; i < names.Num(); i++)
	{
		UE_LOG(LogTemp, Warning, TEXT("Energy: %s: %f"), *names[i], BestEnergyComponents[i]);
	}
}

PRAGMA_ENABLE_OPTIMIZATION
