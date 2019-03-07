// Fill out your copyright notice in the Description page of Project Settings.

#include "PGCMesh.h"

#include "PGCCache.h"

PRAGMA_DISABLE_OPTIMIZATION


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

	auto mesh = Cache::PGCCache::GetMesh(gname, checksum, NumDivisions, Triangularise, dm);

	if (!mesh.IsValid())
	{
		RealGenerate(gname, checksum, NumDivisions, Triangularise, dm);
	}

	CurrentMesh = Cache::PGCCache::GetMesh(gname, checksum, NumDivisions, Triangularise, dm);
	CurrentNodes = Cache::PGCCache::GetMeshNodes(gname, checksum, NumDivisions, Triangularise, dm);
}

void UPGCMesh::RealGenerate(const FString& generator_name, uint32 generator_hash,
	int NumDivisions, bool Triangularise, PGCDebugMode dm)
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
		Cache::PGCCache::StoreMesh(generator_name, generator_hash, 0, false, dm, out_mesh, out_nodes);

		return;
	}

	auto out_mesh = Cache::PGCCache::GetMesh(generator_name, generator_hash, NumDivisions - 1, false, dm);
	check(out_mesh.IsValid());

	// surfaces with normals differing by more than 20 degrees to be set sharp when
	// using PGCEdgeType::Auto
	auto out_nodes = Cache::PGCCache::GetMeshNodes(generator_name, generator_hash, NumDivisions - 1, false, dm);

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

	Cache::PGCCache::StoreMesh(generator_name, generator_hash, NumDivisions, Triangularise, dm, out_mesh, out_nodes);
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
