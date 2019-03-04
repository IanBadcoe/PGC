// Fill out your copyright notice in the Description page of Project Settings.

#include "TestGenerator.h"

PRAGMA_DISABLE_OPTIMIZATION

// Sets default values
ATestGenerator::ATestGenerator()
{
	TopologicalGraph = MakeShared<TestGraph>();
	// assuming for the moment this is cheap...
	TopologicalGraph->Generate();
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

uint32 ATestGenerator::SettingsHash() const
{
	return HashCombine(TopologicalGraph->GetTypeHash(), GetTypeHash(RStream.GetCurrentSeed()));
}

void ATestGenerator::MakeMesh(TSharedPtr<Mesh> mesh, TSharedPtr<TArray<FPGCNodePosition>> Nodes) const
{
	// we won't change our' actual random stream, because the usual UE mechanaism for resetting it
	// doesn't apply when we're invoked from some other Actor getting editted
	FRandomStream throwaway_rstream(RStream);

	TSharedPtr<const StructuralGraph::ProfileSource> ProfileSource = MakeShared<const TestProfileSource>(FRandomStream(throwaway_rstream.RandHelper(INT_MAX)));

	auto StructuralGraph = MakeShared<StructuralGraph::SGraph>(TopologicalGraph, ProfileSource,
		FRandomStream(throwaway_rstream.RandHelper(INT_MAX)));

	if (StructuralGraph->DM != StructuralGraph::SGraph::DebugMode::IntermediateSkeleton)
	{
		auto OptimizerInterface = MakeShared<Opt::OptFunction>(StructuralGraph, 1.0, 1.0, 100.0, 100.0, 10.0, 10.0);
		
		auto Optimizer = MakeShared<NlOptWrapper>(OptimizerInterface);
		Optimizer->RunOptimization(true, 1000, 1e-3, 100000, nullptr);
	}

	StructuralGraph->MakeMesh(mesh);

	for (const auto& n : StructuralGraph->Nodes)
	{
		Nodes->Emplace(n->Position, n->CachedUp);
	}
}

TestProfileSource::TestProfileSource(const FRandomStream& random_stream)
	: RStream(random_stream)
{
	//// tunnels
	//AddRoadbed("SquareTunnel", MakeShared<ParameterisedRoadbedShape>(1, 1, 0.5f, 0.5f, 0, -1));
	//AddRoadbed("RoundTunnel", MakeShared<ParameterisedRoadbedShape>(1, 1, 0.5f, 0.5f, 3, 9));
	//AddRoadbed("RoundTunnel", MakeShared<ParameterisedRoadbedShape>(1, 1, 0.5f, 0.5f, 3, 9));

	//// canyons
	//AddRoadbed("SquareCanyon", MakeShared<ParameterisedRoadbedShape>(1, 1, 0.35f, 0.35f, 0, -1));
	//AddRoadbed("RoundCanyon", MakeShared<ParameterisedRoadbedShape>(1, 1, 0.35f, 0.35f, 3, 9));
	//AddRoadbed("RoundCanyonB", MakeShared<ParameterisedRoadbedShape>(1, 1, 0.35f, 0.35f, 4, 8));

	//AddRoadbed("TallThinSquareCanyon", MakeShared<ParameterisedRoadbedShape>(1.5, 1.5, 0.35f, 0.35f, 0, -1));
	//AddRoadbed("TallThinRoundCanyon", MakeShared<ParameterisedRoadbedShape>(1.5, 1.5, 0.35f, 0.35f, 3, 9));
	//AddRoadbed("TallThinRoundCanyonB", MakeShared<ParameterisedRoadbedShape>(1.5, 1.5, 0.35f, 0.35f, 4, 8));

	//// U-shapes
	//AddRoadbed("SquareU", MakeShared<ParameterisedRoadbedShape>(1, 1, 0, 0, 0, -1));
	//AddRoadbed("SquareLowU", MakeShared<ParameterisedRoadbedShape>(0.5f, 0.5f, 0, 0, 0, -1));
	//AddRoadbed("SquareShallowU", MakeShared<ParameterisedRoadbedShape>(0.1f, 0.1f, 0, 0, 0, -1));
	//AddRoadbed("RoundU", MakeShared<ParameterisedRoadbedShape>(1, 1, 0, 0, 4, 8));
	//AddRoadbed("RoundLowU", MakeShared<ParameterisedRoadbedShape>(0.5f, 0.5f, 0, 0, 4, 8));
	//AddRoadbed("RoundShallowU", MakeShared<ParameterisedRoadbedShape>(0.1f, 0.1f, 0, 0, 4, 8));

	//// C-shapes
	//AddRoadbed("SquareC", MakeShared<ParameterisedRoadbedShape>(1, 0, 1, 0, 0, -1));
	//AddRoadbed("SquareShortTopC", MakeShared<ParameterisedRoadbedShape>(1, 0, 0.5f, 0, 0, -1));
	//AddRoadbed("RoundC", MakeShared<ParameterisedRoadbedShape>(1, 0, 1, 0, 3, 7));
	//AddRoadbed("RoundShortTopC", MakeShared<ParameterisedRoadbedShape>(1, 0, 0.5f, 0, 3, 7));

	//// L-shapes
	//AddRoadbed("SquareL", MakeShared<ParameterisedRoadbedShape>(1, 0, 0, 0, 0, -1));
	//AddRoadbed("SquareShortL", MakeShared<ParameterisedRoadbedShape>(0.5f, 0, 0, 0, 0, -1));
	//AddRoadbed("RoundL", MakeShared<ParameterisedRoadbedShape>(1, 0, 0, 0, 5, 6));
	//AddRoadbed("RoundShortL", MakeShared<ParameterisedRoadbedShape>(0.5f, 0, 0, 0, 5, 6));

	// test only!!!
	AddRoadbed("test1", MakeShared<Profile::ParameterisedRoadbedShape>(1.0f, 1.0f, 0.5f, 0.5f, 6, 11, 5, 7), true, { 3.0f });
	AddRoadbed("test2", MakeShared<Profile::ParameterisedRoadbedShape>(1.0f, 1.0f, 0, 0, 6, 11, 5, 7), true, { 3.0f });
}

TSharedPtr<Profile::ParameterisedProfile> TestProfileSource::GetProfile() const
{
	return Profiles[RStream.RandRange(0, Profiles.Num() - 1)];
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
		auto rand_key = RStream.RandRange(1, keys.Num() - 2);

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