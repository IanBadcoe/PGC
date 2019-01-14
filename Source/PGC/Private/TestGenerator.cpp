// Fill out your copyright notice in the Description page of Project Settings.

#include "TestGenerator.h"

using namespace LayoutGraph;
using namespace StructuralGraph;

const static TSharedPtr<ParameterisedProfile> FullRoadbedProfile = MakeShared<ParameterisedProfile>
(
	4.0f,
	0.0f, 4.0f, 6.0f, 1.0f,
	0.0f, 1.0f, 1.0f, 0.0f
);

const YJunction::ConnectorArray YJunction::ConnectorData
{
	MakeShared<ConnectorInst>(FullRoadbedProfile, FVector{ 0, -3.46f, 0 }, FVector{ 0, -1, 0 }, FVector{ 0, 0, 1 }),
	MakeShared<ConnectorInst>(FullRoadbedProfile, FVector{ 3, 1.73f, 0 }, FVector{ 0.866f, 0.5f, 0 }, FVector{ 0, 0, 1 }),
	MakeShared<ConnectorInst>(FullRoadbedProfile, FVector{ -3, 1.73f, 0 }, FVector{ -0.866f, 0.5f, 0 }, FVector{ 0, 0, 1 }),
};

YJunction::YJunction()
	: YJunction({})
{
}

YJunction::YJunction(TSharedPtr<ParameterisedProfile> profiles[3])
	: Node(ConnectorData)
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

		OptimizerInterface = MakeShared<OptFunction>(StructuralGraph);

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

	StructuralGraph->MakeMesh(mesh);
}

bool ATestGenerator::NeedsRefinement()
{
	return true;
}

void ATestGenerator::Refine()
{
	EnsureOptimizer();

	Optimizer->Optimize();
}

void TestGraph::Generate()
{
	Nodes.Add(MakeShared<YJunction>());
	Nodes.Add(MakeShared<YJunction>());
	//Nodes.Add(MakeShared<YJunction>());
	//Nodes.Add(MakeShared<YJunction>());

	Nodes[1]->Position.SetLocation(FVector(0, -40, 10));
	Nodes[1]->Position.SetRotation(FQuat(FVector(0, 0, 1), PI));
	//Nodes[2]->Position.SetLocation(FVector(20, 0, 0));
	//Nodes[3]->Position.SetLocation(FVector(0, 20, 10));

	TSharedPtr<ParameterisedProfile> test_profile = MakeShared<ParameterisedProfile>(
		2.0f,
		0.0f, 4.0f, 6.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f
	);

	Connect(0, 0, 1, 0, 20, 0, TArray<TSharedPtr<ParameterisedProfile>> { FullRoadbedProfile, test_profile, FullRoadbedProfile, });
	Connect(0, 1, 1, 2, 20, 1);
	Connect(1, 1, 0, 2, 20, -1);
	//Connect(0, 1, 2, 1, 10);
	//Connect(0, 2, 3, 2, 10);
	//Connect(2, 2, 1, 2, 10);
	//Connect(2, 0, 3, 0, 10);
	//Connect(3, 1, 1, 1, 10);
}
