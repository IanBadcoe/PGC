// Fill out your copyright notice in the Description page of Project Settings.

#include "TestGenerator.h"

using namespace LayoutGraph;
using namespace StructuralGraph;

// tunnels

const static TSharedPtr<ParameterizedRoadbedShape> SquareTunnel = MakeShared<ParameterizedRoadbedShape>
(
	1, 1,
	0.5f, 0.5f,
	0, -1
);

const static TSharedPtr<ParameterizedRoadbedShape> RoundTunnel = MakeShared<ParameterizedRoadbedShape>
(
	1, 1,
	0.5f, 0.5f,
	3, 9
);

// canyons

const static TSharedPtr<ParameterizedRoadbedShape> SquareCanyon = MakeShared<ParameterizedRoadbedShape>
(
	1, 1,
	0.35f, 0.35f,
	0, -1
);

const static TSharedPtr<ParameterizedRoadbedShape> RoundCanyon = MakeShared<ParameterizedRoadbedShape>
(
	1, 1,
	0.35f, 0.35f,
	3, 9
);

const static TSharedPtr<ParameterizedRoadbedShape> RoundCanyonB = MakeShared<ParameterizedRoadbedShape>
(
	1, 1,
	0.35f, 0.35f,
	4, 8
);

// U-shapes

const static TSharedPtr<ParameterizedRoadbedShape> SquareU = MakeShared<ParameterizedRoadbedShape>
(
	1, 1,
	0, 0,
	0, -1
);

const static TSharedPtr<ParameterizedRoadbedShape> SquareLowU = MakeShared<ParameterizedRoadbedShape>
(
	0.5f, 0.5f,
	0, 0,
	0, -1
);

const static TSharedPtr<ParameterizedRoadbedShape> SquareShallowU = MakeShared<ParameterizedRoadbedShape>
(
	0.1f, 0.1f,
	0, 0,
	0, -1
);


const static TSharedPtr<ParameterisedProfile> DefaultRoadbedX2 = MakeShared<ParameterisedProfile>
(
	4.0f,
	TArray<float> { 1.0f, 1.0f, 1.0f, 1.0f },
	TArray<float> { 0.0f, 0.0f, 0.0f, 0.0f },
	TArray<bool> {
		false, false, false, false, false, false, 
		false, false, false, false, false, false, 
		false, false, false, false, false, false, 
		false, false, false, false, false, false
	}
);

//const static TSharedPtr<ParameterisedProfile> HalfClosed = MakeShared<ParameterisedProfile>
//(
//	6.0f,
//	1.0f, 6.0f, 6.0f, 1.0f,
//	0.0f, 3.0f, 3.0f, 0.0f
//);
//
//const static TSharedPtr<ParameterisedProfile> LeftCs = MakeShared<ParameterisedProfile>
//(
//	6.0f,
//	0.0f, 0.0f, 6.0f, 6.0f,
//	0.0f, 0.0f, 6.0f, 6.0f
//);

YJunction::YJunction(const FVector & pos, const FVector & rot)
	: YJunction(TArray<TSharedPtr<ParameterisedProfile >> { DefaultRoadbedX2, DefaultRoadbedX2, DefaultRoadbedX2 }, pos, rot)
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

		OptimizerInterface = MakeShared<Opt::OptFunction>(StructuralGraph, 1.0, 1.0, 100.0);

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

void Connect(TestGraph* graph, int nodeFrom, int nodeFromConnector, int nodeTo, int nodeToConnector,
	int divs, int twists,
	float width, 
	TArray<TSharedPtr<ParameterizedRoadbedShape>> top = TArray<TSharedPtr<ParameterizedRoadbedShape>>{},
	TArray<TSharedPtr<ParameterizedRoadbedShape>> bottom = TArray<TSharedPtr<ParameterizedRoadbedShape>>{})
{
	TArray<TSharedPtr<ParameterisedProfile>> profiles;

	if (top.Num() == 0)
	{
		top.Push(SquareShallowU);
	}

	if (bottom.Num() == 0)
	{
		bottom.Push(SquareShallowU);
	}

	auto max_count = FMath::Max3(top.Num(), bottom.Num(), 3);

	for (int i = 0; i < max_count; i++)
	{
		int top_p = (float)i / max_count * top.Num();
		int bottom_p = (float)i / max_count * bottom.Num();

		const auto top_rb = top[top_p];
		const auto bottom_rb = bottom[top_p];

		profiles.Emplace(MakeShared<ParameterisedProfile>(width, top_rb, bottom_rb));
	}

	graph->Connect(nodeFrom, nodeFromConnector, nodeTo, nodeToConnector, divs, twists, profiles);
}

void TestGraph::Generate()
{
	Nodes.Add(MakeShared<YJunction>(FVector{ 0, 0, 0 }, FVector{ 0, 0, 0 }));
	Nodes.Add(MakeShared<YJunction>(FVector{ 0, 0, 100 }, FVector{ 0, 0, 0 }));

	::Connect(this, 0, 1, 1, 1, 20, 1, 4.0f);
	::Connect(this, 0, 2, 1, 2, 20, 0, 6.0f);
	::Connect(this, 0, 0, 1, 0, 20, 0, 3.0f);

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
