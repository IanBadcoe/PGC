// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGCCube.generated.h"

USTRUCT(BlueprintType)
struct FPGCCube
{
	GENERATED_USTRUCT_BODY()
	
public:	
	// Sets default values for this actor's properties
	FPGCCube();

protected:

public:	
	
	UPROPERTY(EditAnywhere)
	int CellCoords[3];
};
