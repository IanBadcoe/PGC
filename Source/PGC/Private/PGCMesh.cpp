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

FPGCMeshResult UPGCMesh::Generate(int NumDivisions, bool InsideOut)
{
	NumDivisions = 0; /// DEBUGGING! <<<

	auto divided_mesh = InitialMesh->SubdivideN(NumDivisions);

	FPGCMeshResult ret;

	divided_mesh->Bake(ret, InsideOut);

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

