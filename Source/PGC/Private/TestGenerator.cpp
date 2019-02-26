// Fill out your copyright notice in the Description page of Project Settings.

#include "TestGenerator.h"

PRAGMA_DISABLE_OPTIMIZATION

// namespaces are not supported by UnrealHeaderTool...
// and we have uberized builds, so don't want usings outside a namespace...

// class statics

TMap<FString, TSharedPtr<Profile::ParameterisedRoadbedShape>> TestProfileSource::Roadbeds;
TArray <TSharedPtr<Profile::ParameterisedProfile>> TestProfileSource::Profiles;

static bool s_inited = false;

static void Init()
{
	if (s_inited) return;

	s_inited = true;

	//// tunnels
	//TestProfileSource::AddRoadbed("SquareTunnel", MakeShared<ParameterisedRoadbedShape>(1, 1, 0.5f, 0.5f, 0, -1));
	//TestProfileSource::AddRoadbed("RoundTunnel", MakeShared<ParameterisedRoadbedShape>(1, 1, 0.5f, 0.5f, 3, 9));
	//TestProfileSource::AddRoadbed("RoundTunnel", MakeShared<ParameterisedRoadbedShape>(1, 1, 0.5f, 0.5f, 3, 9));

	//// canyons
	//TestProfileSource::AddRoadbed("SquareCanyon", MakeShared<ParameterisedRoadbedShape>(1, 1, 0.35f, 0.35f, 0, -1));
	//TestProfileSource::AddRoadbed("RoundCanyon", MakeShared<ParameterisedRoadbedShape>(1, 1, 0.35f, 0.35f, 3, 9));
	//TestProfileSource::AddRoadbed("RoundCanyonB", MakeShared<ParameterisedRoadbedShape>(1, 1, 0.35f, 0.35f, 4, 8));

	//TestProfileSource::AddRoadbed("TallThinSquareCanyon", MakeShared<ParameterisedRoadbedShape>(1.5, 1.5, 0.35f, 0.35f, 0, -1));
	//TestProfileSource::AddRoadbed("TallThinRoundCanyon", MakeShared<ParameterisedRoadbedShape>(1.5, 1.5, 0.35f, 0.35f, 3, 9));
	//TestProfileSource::AddRoadbed("TallThinRoundCanyonB", MakeShared<ParameterisedRoadbedShape>(1.5, 1.5, 0.35f, 0.35f, 4, 8));

	//// U-shapes
	//TestProfileSource::AddRoadbed("SquareU", MakeShared<ParameterisedRoadbedShape>(1, 1, 0, 0, 0, -1));
	//TestProfileSource::AddRoadbed("SquareLowU", MakeShared<ParameterisedRoadbedShape>(0.5f, 0.5f, 0, 0, 0, -1));
	//TestProfileSource::AddRoadbed("SquareShallowU", MakeShared<ParameterisedRoadbedShape>(0.1f, 0.1f, 0, 0, 0, -1));
	//TestProfileSource::AddRoadbed("RoundU", MakeShared<ParameterisedRoadbedShape>(1, 1, 0, 0, 4, 8));
	//TestProfileSource::AddRoadbed("RoundLowU", MakeShared<ParameterisedRoadbedShape>(0.5f, 0.5f, 0, 0, 4, 8));
	//TestProfileSource::AddRoadbed("RoundShallowU", MakeShared<ParameterisedRoadbedShape>(0.1f, 0.1f, 0, 0, 4, 8));

	//// C-shapes
	//TestProfileSource::AddRoadbed("SquareC", MakeShared<ParameterisedRoadbedShape>(1, 0, 1, 0, 0, -1));
	//TestProfileSource::AddRoadbed("SquareShortTopC", MakeShared<ParameterisedRoadbedShape>(1, 0, 0.5f, 0, 0, -1));
	//TestProfileSource::AddRoadbed("RoundC", MakeShared<ParameterisedRoadbedShape>(1, 0, 1, 0, 3, 7));
	//TestProfileSource::AddRoadbed("RoundShortTopC", MakeShared<ParameterisedRoadbedShape>(1, 0, 0.5f, 0, 3, 7));

	//// L-shapes
	//TestProfileSource::AddRoadbed("SquareL", MakeShared<ParameterisedRoadbedShape>(1, 0, 0, 0, 0, -1));
	//TestProfileSource::AddRoadbed("SquareShortL", MakeShared<ParameterisedRoadbedShape>(0.5f, 0, 0, 0, 0, -1));
	//TestProfileSource::AddRoadbed("RoundL", MakeShared<ParameterisedRoadbedShape>(1, 0, 0, 0, 5, 6));
	//TestProfileSource::AddRoadbed("RoundShortL", MakeShared<ParameterisedRoadbedShape>(0.5f, 0, 0, 0, 5, 6));

	// test only!!!
	TestProfileSource::AddRoadbed("test1", MakeShared<Profile::ParameterisedRoadbedShape>(1.0f, 1.0f, 0.5f, 0.5f, 6, 11, 5, 7), true, { 3.0f });
	TestProfileSource::AddRoadbed("test2", MakeShared<Profile::ParameterisedRoadbedShape>(1.0f, 1.0f, 0, 0, 6, 11, 5, 7), true, { 3.0f });
}

// Sets default values
ATestGenerator::ATestGenerator()
{
	Init();
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
		StructuralGraph = MakeShared<StructuralGraph::SGraph>(TopologicalGraph, &ProfileSource);
	}
}

void ATestGenerator::EnsureOptimizer()
{
	EnsureGraphs();

	if (!Optimizer.IsValid())
	{
		check(!OptimizerInterface.IsValid());

		OptimizerInterface = MakeShared<Opt::OptFunction>(StructuralGraph, 1.0, 1.0, 100.0, 100.0, 10.0, 10.0);

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

void ATestGenerator::MakeMesh(TSharedPtr<Mesh> mesh, TArray<FPGCNodePosition>& Nodes)
{
	EnsureGraphs();

	EnsureOptimizer();

	//int sz = OptimizerInterface->GetSize();
	//TArray<double> state;
	//state.AddDefaulted(sz);
	//OptimizerInterface->GetState(state.GetData(), sz);
	//OptimizerInterface->SetState(state.GetData(), sz);

	if (StructuralGraph->DM != StructuralGraph::SGraph::DebugMode::IntermediateSkeleton)
	{
		Optimizer->RunOptimization(true, 1000, 1e-3, nullptr);
	}

	StructuralGraph->MakeMesh(mesh);

	for (const auto& n : StructuralGraph->Nodes)
	{
		Nodes.Emplace(n->Position, n->CachedUp);
	}
}

TSharedPtr<Profile::ParameterisedProfile> TestProfileSource::GetProfile() const
{
	return Profiles[FMath::RandRange(0, Profiles.Num() - 1)];
}

TArray<TSharedPtr<Profile::ParameterisedProfile>> TestProfileSource::GetCompatibleProfileSequence(
	TSharedPtr<Profile::ParameterisedProfile> from,
	TSharedPtr<Profile::ParameterisedProfile> to,
	int steps) const
{
	// must have room for at least the start and end-points
	check(steps >= 2);

	auto here_num = steps / 5;

	TArray<TSharedPtr<Profile::ParameterisedProfile>> keys;

	keys.Add(from);

	for (int i = 0; i < here_num - 1; i++)
	{
		keys.Add(GetProfile());
	}

	keys.Add(to);

	for (int i = 0; i < 20; i++)
	{
		auto rand_key = FMath::RandRange(1, keys.Num() - 2);

		auto profile = GetProfile();

		auto prev_diff = keys[rand_key]->Diff(keys[rand_key - 1]) + keys[rand_key]->Diff(keys[rand_key + 1]);
		auto new_diff = profile->Diff(keys[rand_key - 1]) + profile->Diff(keys[rand_key + 1]);

		if (new_diff < prev_diff)
		{
			keys[rand_key] = profile;
		}
	}

	TArray<TSharedPtr<Profile::ParameterisedProfile>> ret;

	for (int i = 0; i < steps; i++)
	{
		auto key = i / 5;
		auto frac = (i - (key * 5)) / 4.0f;

		ret.Add(keys[key]->Interp(keys[key + 1], frac));
	}

	return ret;
}

void TestProfileSource::AddRoadbed(FString name, TSharedPtr<Profile::ParameterisedRoadbedShape> roadbed,
	bool suppress_mirroring /* = false */, TArray<float> sizes /* = {} */)
{
	auto here_sizes = sizes.Num() ? sizes : TArray<float>{ 3.0f, 5.0f, 8.0f };

	for (auto width : here_sizes)
	{
		//for (const auto& other : Roadbeds)
		//{
		//	Profiles.Add(MakeShared<ParameterisedProfile>(width, roadbed, other.Value));
		//	Profiles.Add(MakeShared<ParameterisedProfile>(width, other.Value, roadbed));
		//}

		Profiles.Add(MakeShared<Profile::ParameterisedProfile>(width, roadbed, roadbed));
	}

	Roadbeds.Add(name) = roadbed;

	if (!suppress_mirroring)
	{
		auto mirrored = roadbed->Mirrored();

		if (*mirrored != *roadbed)
		{
			AddRoadbed(name + " (mirrored)", mirrored, true);
		}
	}
}

void TestGraph::Generate()
{
	// NODE ROTATIONS ARE IN THE ORDER X, Y, Z and DEGREES
	Nodes.Add(MakeShared<YJunction>(FVector{ 0, 0, 0 }, FVector{ 0, 0, 0 }));
	Nodes.Add(MakeShared<YJunction>(FVector{ 30, 0, 0 }, FVector{ 0, 0, 180 }));
	Nodes.Add(MakeShared<YJunction>(FVector{ -10, 20, 0 }, FVector{ 0, 0, 180 }));
	Nodes.Add(MakeShared<YJunction>(FVector{ -10, -20, 0 }, FVector{ 0, 0, 180 }));

	Connect(0, 0, 1, 0, 20, 1);
	Connect(0, 1, 2, 0, 20, 0);
	Connect(0, 2, 3, 0, 20, 0);
	Connect(1, 1, 2, 1, 20, 0);
	Connect(1, 2, 3, 1, 20, 0);
	Connect(2, 2, 3, 2, 20, 0);

	//Nodes.Add(MakeShared<YJunction>(FVector{ 0, 0, 0 }, FVector{ 0, 0, 0 }));
	//Nodes.Add(MakeShared<YJunction>(FVector{ 0, 0, 100 }, FVector{ 0, 0, 0 }));

	//Connect(0, 1, 1, 1, 20, 1);
	//Connect(0, 2, 1, 2, 20, 0);
	//Connect(0, 0, 1, 0, 20, 0);

	////Nodes.Add(MakeShared<Node>(TArray<TSharedPtr<ParameterisedProfile>>{ HalfClosed, HalfClosed, HalfClosed, HalfClosed, HalfClosed, HalfClosed, }, FVector::ZeroVector, FVector::ZeroVector));
	////Nodes.Add(MakeShared<Node>(TArray<TSharedPtr<ParameterisedProfile>>{ HalfClosed, HalfClosed, HalfClosed, HalfClosed, HalfClosed, HalfClosed, }, FVector(0, 0, 20), FVector(180, 0, 0)));

	////Nodes.Add(MakeShared<YJunction>(FVector{ 50, 40, 0 }, FVector::ZeroVector));
	////Nodes.Add(MakeShared<YJunction>(FVector{ 50, 20, 0 }, FVector::ZeroVector));

	////Connect(0, 1, 2, 1, 20, 0);
	////Connect(0, 2, 3, 1, 20, 0);
	////Connect(0, 4, 0, 5, 20, 0);
	////Connect(1, 1, 1, 2, 20, 0);
	////Connect(1, 4, 1, 5, 20, 0);
	////Connect(0, 0, 1, 3, 20, 1);
	////Connect(0, 3, 1, 0, 20, 0);
	////Connect(2, 0, 2, 2, 20, 0);
	////Connect(3, 0, 3, 2, 20, 0);
}

PRAGMA_ENABLE_OPTIMIZATION