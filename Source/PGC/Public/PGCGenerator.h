// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "Runtime/Core/Public/Templates/SharedPointer.h"
#include "Mesh.h"

#include "PGCGenerator.generated.h"

// This class does not need to be modified.
UINTERFACE(BlueprintType, meta = ( CannotImplementInterfaceInBlueprint ))
class UPGCGenerator : public UInterface
{
	GENERATED_BODY()
};


UENUM()
enum class PGCDebugMode : uint8 {
	Normal,
	Skeleton,
	IntermediateSkeleton,
	RawIntermediateSkeleton
};


/**
 * 
 */
class PGC_API IPGCGenerator
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	
	// use in C++ only
	// conceptually const, may cache internally
	virtual void MakeMesh(TSharedPtr<Mesh> mesh, const TSharedPtr<TArray<FPGCNodePosition>> Nodes,
		PGCDebugMode dm) const = 0;

	virtual uint32 SettingsHash() const = 0;
	virtual FString GetName() const = 0;
};
