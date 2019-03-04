// Fill out your copyright notice in the Description page of Project Settings.

#include "PGCMesh.h"

PRAGMA_DISABLE_OPTIMIZATION

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

void UPGCMesh::Generate(int NumDivisions, bool Triangularise, PGCDebugMode dm)
{
	auto checksum = Generator->SettingsHash();
	auto gname = Generator->GetName();

	PGCMeshCache::CacheKey key{ gname, checksum, NumDivisions, Triangularise, dm };

	if (!Cache.Contains(key))
	{
		RealGenerate(key, NumDivisions, Triangularise, dm);
	}

	CurrentMesh = Cache[key].Geom;
	CurrentNodes = Cache[key].Nodes;
}

void UPGCMesh::RealGenerate(PGCMeshCache::CacheKey key, int NumDivisions, bool Triangularise,
	PGCDebugMode dm)
{
	bool need_another_divide = false;
	if (Triangularise)
	{
		Generate(NumDivisions, false, dm);
	}
	else if (NumDivisions > 0)
	{
		Generate(NumDivisions - 1, false, dm);
		need_another_divide = true;
	}
	else
	{
		auto out_mesh = MakeShared<Mesh>(FMath::Cos(FMath::DegreesToRadians(20.0f)));
		auto out_nodes = MakeShared<TArray<FPGCNodePosition>>();

		Generator->MakeMesh(out_mesh, out_nodes, dm);
		Cache.Add(PGCMeshCache::CacheKey{ key.GeneratorName, key.GeneratorConfigChecksum, 0, false, dm }, { out_mesh, out_nodes });

		return;
	}

	PGCMeshCache::CacheKey parent_key{ key.GeneratorName, key.GeneratorConfigChecksum, NumDivisions - 1, false, dm };

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

FPGCMeshResult UPGCMesh::GenerateMergeChannels(int NumDivisions, bool InsideOut, bool Triangularise, PGCDebugEdgeType DebugEdges,
	PGCDebugMode dm)
{
	Generate(NumDivisions, Triangularise, dm);

	FPGCMeshResult ret;

	CurrentMesh->BakeAllChannelsIntoOne(ret, InsideOut, DebugEdges);

	ret.Nodes = *CurrentNodes;

	return ret;
}

FPGCMeshResult UPGCMesh::GenerateChannels(int NumDivisions, bool InsideOut, bool Triangularise,
	PGCDebugEdgeType DebugEdges,
	PGCDebugMode dm,
	int StartChannel, int EndChannel)
{
	Generate(NumDivisions, Triangularise, dm);

	FPGCMeshResult ret;

	CurrentMesh->BakeChannels(ret, InsideOut, DebugEdges, StartChannel, EndChannel);

	ret.Nodes = *CurrentNodes;

	return ret;
}

PRAGMA_ENABLE_OPTIMIZATION
