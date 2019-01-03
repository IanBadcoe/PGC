// Fill out your copyright notice in the Description page of Project Settings.

#include "PGCCubeGenerator.h"

#include "Mesh.h"
#include "Runtime/Engine/Classes/Engine/World.h"

PRAGMA_DISABLE_OPTIMIZATION

// Sets default values
APGCCubeGenerator::APGCCubeGenerator()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void APGCCubeGenerator::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void APGCCubeGenerator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void APGCCubeGenerator::MakeMesh(TSharedPtr<Mesh> mesh)
{
	for (auto& cube : Cubes)
	{
		mesh->AddCube(cube);
	}

	mesh->CheckConsistent(true);
}

bool APGCCubeGenerator::NeedsRefinement()
{
	return false;
}

void APGCCubeGenerator::Refine()
{
	check(false);
}

PRAGMA_ENABLE_OPTIMIZATION

