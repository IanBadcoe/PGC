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

	TScriptInterface<IPGCGenerator> Generator;

	TSharedPtr<Mesh> CurrentMesh;
	TSharedPtr<TArray<FPGCNodePosition>> CurrentNodes;

	void Generate(int NumDivisions, bool Triangularise, PGCDebugMode dm);
	void RealGenerate(const FString& generator_name, uint32 generator_hash,
		int NumDivisions, bool Triangularise, PGCDebugMode dm);

public:	
	// Sets default values for this component's properties
	UPGCMesh();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set GeneratorName", Keywords = "PGC, procedural"), Category = "PGC")
		void SetGenerator(const TScriptInterface<IPGCGenerator>& gen);
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Generate", Keywords = "PGC, procedural"), Category = "PGC")
		FPGCMeshResult GenerateMergeChannels(int NumDivisions, bool InsideOut, bool Triangularise, PGCDebugEdgeType DebugEdges,
			PGCDebugMode dm);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Generate Channels", Keywords = "PGC, procedural"), Category = "PGC")
		FPGCMeshResult GenerateChannels(int NumDivisions, bool InsideOut, bool Triangularise, PGCDebugEdgeType DebugEdges,
			PGCDebugMode dm,
			int StartChannel, int EndChannel);

};
