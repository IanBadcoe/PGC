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

/**
 * 
 */
class PGC_API IPGCGenerator
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Needs Refinement", Keywords = "PGC, procedural"), Category = "PGC")
		virtual bool NeedsRefinement() = 0;
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Refine", Keywords = "PGC, procedural"), Category = "PGC")
		virtual void Refine() = 0;

	// use in C++ only
	virtual void MakeMesh(TSharedPtr<Mesh> mesh) = 0;

};
