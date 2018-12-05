// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGCCube.generated.h"

UENUM()
enum class PGCEdgeId {
	TopFront,
	TopLeft,
	TopBack,
	TopRight,
	FrontLeft,
	BackLeft,
	BackRight,
	FrontRight,
	BottomFront,
	BottomLeft,
	BottomBack,
	BottomRight,

	MAX
};

UENUM()
enum class PGCEdgeType {
	Rounded,
	Sharp
};

USTRUCT(BlueprintType)
struct FPGCCube
{
	GENERATED_USTRUCT_BODY()
	
public:	
	// Sets default values for this actor's properties
	FPGCCube();
	FPGCCube(int x, int y, int z);

protected:

public:	
	
	UPROPERTY(EditAnywhere)
	int X;

	UPROPERTY(EditAnywhere)
	int Y;

	UPROPERTY(EditAnywhere)
	int Z;

	UPROPERTY(EditAnywhere)
	PGCEdgeType Sharp[PGCEdgeId::MAX];
};
