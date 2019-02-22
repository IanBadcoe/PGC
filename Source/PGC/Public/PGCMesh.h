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

	// surfaces with normals differing by more than 20 degrees to be set sharp when
	// using PGCEdgeType::Auto
	TSharedPtr<Mesh> InitialMesh{ MakeShared<Mesh>(FMath::Cos(FMath::DegreesToRadians(20.0f))) };

	TArray<FPGCNodePosition> Nodes;

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

		Generator->MakeMesh(InitialMesh, Nodes);
	}
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Generate", Keywords = "PGC, procedural"), Category = "PGC")
		FPGCMeshResult GenerateMergeChannels(int NumDivisions, bool InsideOut, bool Triangularise, PGCDebugEdgeType DebugEdges);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Generate Channels", Keywords = "PGC, procedural"), Category = "PGC")
		FPGCMeshResult GenerateChannels(int NumDivisions, bool InsideOut, bool Triangularise, PGCDebugEdgeType DebugEdges,
			int StartChannel, int EndChannel);

};
