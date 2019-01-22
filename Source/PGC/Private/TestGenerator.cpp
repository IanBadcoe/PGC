// Fill out your copyright notice in the Description page of Project Settings.

#include "TestGenerator.h"

using namespace LayoutGraph;
using namespace StructuralGraph;

const static TSharedPtr<ParameterisedProfile> FullRoadbedProfile = MakeShared<ParameterisedProfile>
(
	4.0f,
	0.0f, 6.0f, 6.0f, 1.0f,
	0.0f, 2.0f, 2.0f, 0.0f
);

const static TSharedPtr<ParameterisedProfile> HalfClosed = MakeShared<ParameterisedProfile>
(
	6.0f,
	1.0f, 6.0f, 6.0f, 1.0f,
	0.0f, 3.0f, 3.0f, 0.0f
);

YJunction::YJunction(const FVector & pos, const FVector & rot)
	: YJunction(TArray<TSharedPtr<ParameterisedProfile >> { FullRoadbedProfile, FullRoadbedProfile, FullRoadbedProfile }, pos, rot)
{
}

// Sets default values
ATestGenerator::ATestGenerator()
{
}

void ATestGenerator::EnsureGraphs()
{
	if (!TopologicalGraph.IsValid())
	{
		TopologicalGraph = MakeShared<TestGraph>();
		TopologicalGraph->Generate();
	}

	if (!StructuralGraph.IsValid())
	{
		StructuralGraph = MakeShared<SGraph>(TopologicalGraph);
	}
}

void ATestGenerator::EnsureOptimizer()
{
	EnsureGraphs();

	if (!Optimizer.IsValid())
	{
		check(!OptimizerInterface.IsValid());

		OptimizerInterface = MakeShared<Opt::OptFunction>(StructuralGraph);

		Optimizer = MakeShared<NlOptWrapper>(OptimizerInterface);
	}
}

// Called when the game starts or when spawned
void ATestGenerator::BeginPlay()
{
	Super::BeginPlay();
	
}

void ATestGenerator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// ...
}

void ATestGenerator::MakeMesh(TSharedPtr<Mesh> mesh)
{
	EnsureGraphs();

	EnsureOptimizer();

	//int sz = OptimizerInterface->GetSize();
	//TArray<double> state;
	//state.AddDefaulted(sz);
	//OptimizerInterface->GetState(state.GetData(), sz);
	//OptimizerInterface->SetState(state.GetData(), sz);

	if (Optimizer->RunOptimization())
	{
		UE_LOG(LogTemp, Warning, TEXT("Converged"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Not converged"));
	}

	StructuralGraph->MakeMesh(mesh, false);
}

bool ATestGenerator::NeedsRefinement()
{
	return true;
}

void ATestGenerator::Refine()
{
	//EnsureOptimizer();

	//Optimizer->RunOptimization();
}

void TestGraph::Generate()
{
	Nodes.Add(MakeShared<YJunction>(FVector{ 0, 0, 0 }, FVector{ 0, 0, 90 }));
	Nodes.Add(MakeShared<YJunction>(FVector{ 0, 100, 0 }, FVector{ 0, 0, -90 }));

	Connect(0, 0, 1, 0, 20, 0);
	Connect(0, 1, 0, 2, 20, 0);
	Connect(1, 2, 1, 1, 20, 0);

	//Nodes.Add(MakeShared<Node>(TArray<TSharedPtr<ParameterisedProfile>>{ HalfClosed, HalfClosed, HalfClosed, HalfClosed, HalfClosed, HalfClosed, }, FVector::ZeroVector, FVector::ZeroVector));
	//Nodes.Add(MakeShared<Node>(TArray<TSharedPtr<ParameterisedProfile>>{ HalfClosed, HalfClosed, HalfClosed, HalfClosed, HalfClosed, HalfClosed, }, FVector(0, 0, 20), FVector(180, 0, 0)));

	//Nodes.Add(MakeShared<YJunction>(FVector{ 50, 40, 0 }, FVector::ZeroVector));
	//Nodes.Add(MakeShared<YJunction>(FVector{ 50, 20, 0 }, FVector::ZeroVector));

	//Connect(0, 1, 2, 1, 20, 0);
	//Connect(0, 2, 3, 1, 20, 0);
	//Connect(0, 4, 0, 5, 20, 0);
	//Connect(1, 1, 1, 2, 20, 0);
	//Connect(1, 4, 1, 5, 20, 0);
	//Connect(0, 0, 1, 3, 20, 1);
	//Connect(0, 3, 1, 0, 20, 0);
	//Connect(2, 0, 2, 2, 20, 0);
	//Connect(3, 0, 3, 2, 20, 0);
}
