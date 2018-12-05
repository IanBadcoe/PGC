// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Runtime/Core/Public/Containers/Array.h"
#include "Runtime/Core/Public/Templates/SharedPointer.h"
#include "PGCCube.h"



#include "PGCGenerator.generated.h"

class Mesh;

UCLASS()
class PGC_API APGCGenerator : public AActor
{
	GENERATED_BODY()

public:	
	// Sets default values for this actor's properties
	APGCGenerator();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere)
	TArray<FPGCCube> Cubes;

	void MakeMesh(TSharedPtr<Mesh> mesh);
};
