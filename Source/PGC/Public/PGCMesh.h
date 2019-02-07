// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Components/ActorComponent.h"
#include "Mesh.h"
#include "PGCGenerator.h"

#include "PGCMesh.generated.h"


UCLASS(BlueprintType)
class PGC_API UPGCMesh : public UActorComponent
{
	GENERATED_BODY()

	TSharedPtr<Mesh> InitialMesh{ MakeShared<Mesh>() };

	TScriptInterface<IPGCGenerator> Generator;

public:	
	// Sets default values for this component's properties
	UPGCMesh();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set Generator", Keywords = "PGC, procedural"), Category = "PGC")
	void SetGenerator(const TScriptInterface<IPGCGenerator>& gen)
	{
		Generator = gen;

		InitialMesh->Clear();

		Generator->MakeMesh(InitialMesh);
	}
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Generate", Keywords = "PGC, procedural"), Category = "PGC")
		FPGCMeshResult Generate(int NumDivisions, bool InsideOut);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Generate Channels", Keywords = "PGC, procedural"), Category = "PGC")
		TArray<FPGCMeshResult> GenerateChannels(int NumDivisions, bool InsideOut,
			int start_channel, int end_channel);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Needs Refinement", Keywords = "PGC, procedural"), Category = "PGC")
		bool NeedsRefinement();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Refine", Keywords = "PGC, procedural"), Category = "PGC")
		void Refine();

};
