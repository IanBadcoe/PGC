// Fill out your copyright notice in the Description page of Project Settings.

#include "PGCMesh.h"


//class static
TMap<PGCMeshCache::CacheKey, PGCMeshCache::CacheVal> UPGCMesh::Cache;

// --

// Sets default values for this component's properties
UPGCMesh::UPGCMesh()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}


// Called when the game starts
void UPGCMesh::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UPGCMesh::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UPGCMesh::SetGenerator(const TScriptInterface<IPGCGenerator>& gen)
{
	Generator = gen;
}

void UPGCMesh::Generate(int NumDivisions, bool Triangularise)
{
	auto checksum = Generator->SettingsHash();
	auto gname = Generator->GetName();

	PGCMeshCache::CacheKey key{ gname, checksum, NumDivisions, Triangularise };

	if (!Cache.Contains(key))
	{
		RealGenerate(key, NumDivisions, Triangularise);
	}

	CurrentMesh = Cache[key].Geom;
	CurrentNodes = Cache[key].Nodes;
}

void UPGCMesh::RealGenerate(PGCMeshCache::CacheKey key, int NumDivisions, bool Triangularise)
{
	bool need_another_divide = false;
	if (Triangularise)
	{
		Generate(NumDivisions, false);
	}
	else if (NumDivisions > 0)
	{
		Generate(NumDivisions - 1, false);
		need_another_divide = true;
	}
	else
	{
		auto out_mesh = MakeShared<Mesh>(FMath::Cos(FMath::DegreesToRadians(20.0f)));
		auto out_nodes = MakeShared<TArray<FPGCNodePosition>>();

		Generator->MakeMesh(out_mesh, out_nodes);
		Cache.Add(PGCMeshCache::CacheKey{ key.GeneratorName, key.GeneratorConfigChecksum, 0, false }, { out_mesh, out_nodes });

		return;
	}

	PGCMeshCache::CacheKey parent_key{ key.GeneratorName, key.GeneratorConfigChecksum, NumDivisions - 1, false };

	check(Cache.Contains(parent_key));

	// surfaces with normals differing by more than 20 degrees to be set sharp when
	// using PGCEdgeType::Auto
	auto out_mesh = Cache[parent_key].Geom;
	auto out_nodes = Cache[parent_key].Nodes;

	if (need_another_divide)
	{
		out_mesh = out_mesh->Subdivide();
	}

	if (Triangularise)
	{
		// can have some seriously non-planar faces without subdivision,
		// this splits them around a central point, rather than fan from an random edge vertex
		out_mesh = out_mesh->Triangularise();
	}

	Cache.Add(key, PGCMeshCache::CacheVal{ out_mesh, out_nodes });
}

FPGCMeshResult UPGCMesh::GenerateMergeChannels(int NumDivisions, bool InsideOut, bool Triangularise, PGCDebugEdgeType DebugEdges)
{
	Generate(NumDivisions, Triangularise);

	FPGCMeshResult ret;

	CurrentMesh->BakeAllChannelsIntoOne(ret, InsideOut, DebugEdges);

	ret.Nodes = *CurrentNodes;

	return ret;
}

FPGCMeshResult UPGCMesh::GenerateChannels(int NumDivisions, bool InsideOut, bool Triangularise,
	PGCDebugEdgeType DebugEdges,
	int StartChannel, int EndChannel)
{
	Generate(NumDivisions, Triangularise);

	FPGCMeshResult ret;

	CurrentMesh->BakeChannels(ret, InsideOut, DebugEdges, StartChannel, EndChannel);

	ret.Nodes = *CurrentNodes;

	return ret;
}
