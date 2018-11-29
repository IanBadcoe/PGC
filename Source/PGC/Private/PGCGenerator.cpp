// Fill out your copyright notice in the Description page of Project Settings.

#include "PGCGenerator.h"

#include "Mesh.h"
#include "Runtime/Engine/Classes/Engine/World.h"

PRAGMA_DISABLE_OPTIMIZATION

// Sets default values
APGCGenerator::APGCGenerator()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void APGCGenerator::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void APGCGenerator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void APGCGenerator::MakeMesh(TSharedPtr<Mesh> mesh)
{
	for (auto& cube : Cubes)
	{
		mesh->AddCube(cube.CellCoords[0], cube.CellCoords[1], cube.CellCoords[2]);
	}

	mesh->CheckConsistent(true);
}

PRAGMA_ENABLE_OPTIMIZATION

