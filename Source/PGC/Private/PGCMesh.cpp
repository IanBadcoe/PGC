// Fill out your copyright notice in the Description page of Project Settings.

#include "PGCMesh.h"


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

FPGCMeshResult UPGCMesh::GenerateMergeChannels(int NumDivisions, bool InsideOut, bool Triangularise, PGCDebugEdgeType DebugEdges)
{
	TSharedPtr<Mesh> out_mesh = InitialMesh;
	
	if (NumDivisions > 0)
	{
		out_mesh = out_mesh->SubdivideN(NumDivisions);
	}
	
	if (Triangularise)	// do we ever want subdivision _and_ triangularisation?  No reason why not, but not needed it yet...
	{
		// can have some seriously non-planar faces without subdivision,
		// this splits them around a central point, rather than fan from an random edge vertex
		out_mesh = out_mesh->Triangularise();
	}

	FPGCMeshResult ret;

	out_mesh->BakeAllChannelsIntoOne(ret, InsideOut, DebugEdges);

	return ret;
}

FPGCMeshResult UPGCMesh::GenerateChannels(int NumDivisions, bool InsideOut, bool Triangularise,
	PGCDebugEdgeType DebugEdges,
	int StartChannel, int EndChannel)
{
	TSharedPtr<Mesh> out_mesh = InitialMesh;

	if (NumDivisions > 0)
	{
		out_mesh = out_mesh->SubdivideN(NumDivisions);
	}
	
	if (Triangularise)
	{
		// can have some seriously non-planar faces without subdivision,
		// this splits them around a central point, rather than fan from an random edge vertex
		out_mesh = out_mesh->Triangularise();
	}

	FPGCMeshResult ret;

	out_mesh->BakeChannels(ret, InsideOut, DebugEdges, StartChannel, EndChannel);

	return ret;
}

bool UPGCMesh::NeedsRefinement()
{
	if (Generator.GetObject())
		return Generator->NeedsRefinement();

	return false;
}

void UPGCMesh::Refine()
{
	if (Generator.GetObject())
		Generator->Refine();
}

