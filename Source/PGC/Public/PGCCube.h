// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Mesh.h"


#include "PGCCube.generated.h"

UENUM()
enum class PGCEdgeId : uint8 {
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
	PGCEdgeType EdgeTypes[PGCEdgeId::MAX];

	uint32 GetTypeHash() const;
};
