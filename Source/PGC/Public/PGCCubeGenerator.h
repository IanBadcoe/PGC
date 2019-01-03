// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Runtime/Core/Public/Containers/Array.h"
#include "Runtime/Core/Public/Templates/SharedPointer.h"

#include "PGCCube.h"
#include "PGCGenerator.h"

#include "PGCCubeGenerator.generated.h"

class Mesh;

UCLASS(BlueprintType)
class PGC_API APGCCubeGenerator : public AActor, public IPGCGenerator
{
	GENERATED_BODY()

public:	
	// Sets default values for this actor's properties
	APGCCubeGenerator();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere)
	TArray<FPGCCube> Cubes;

	virtual void MakeMesh(TSharedPtr<Mesh> mesh) override;
	virtual bool NeedsRefinement() override;
	virtual void Refine() override;
};
