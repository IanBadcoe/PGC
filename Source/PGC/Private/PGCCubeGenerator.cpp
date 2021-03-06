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

void APGCCubeGenerator::MakeMesh(TSharedPtr<Mesh> mesh, const TSharedPtr<TArray<FPGCNodePosition>> Nodes,
	PGCDebugMode /*dm*/) const
{
	for (auto& cube : Cubes)
	{
		Nodes->Emplace(FVector{(float)cube.X, (float)cube.Y, (float)cube.Z}, FVector{0.0f, 0.0f, 1.0f});
		mesh->AddCube(cube);
	}

	mesh->CheckConsistent(true);
}

uint32 APGCCubeGenerator::SettingsHash() const
{
	uint32 ret = 0;

	for (const auto& c : Cubes)
	{
		ret = HashCombine(ret, c.GetTypeHash());
	}

	return ret;
}

PRAGMA_ENABLE_OPTIMIZATION
