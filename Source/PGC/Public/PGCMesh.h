// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Components/ActorComponent.h"
#include "Mesh.h"
#include "PGCGenerator.h"

#include "PGCMesh.generated.h"


namespace PGCMeshCache
{

struct CacheKey {
	FString GeneratorName;
	uint32 GeneratorConfigChecksum;
	int NumDivisions;
	bool Triangularise;

	bool operator==(const CacheKey& rhs) const {
		return GeneratorName == rhs.GeneratorName
			&& GeneratorConfigChecksum == rhs.GeneratorConfigChecksum
			&& NumDivisions == rhs.NumDivisions
			&& Triangularise == rhs.Triangularise;
	}
};

static uint32 GetTypeHash(const CacheKey& key) {
	uint32 ret = HashCombine(::GetTypeHash(key.GeneratorName), ::GetTypeHash(key.GeneratorConfigChecksum));

	ret = HashCombine(ret, ::GetTypeHash(key.NumDivisions));
	ret = HashCombine(ret, ::GetTypeHash(key.Triangularise));

	return ret;
}

struct CacheVal {
	TSharedPtr<Mesh> Geom;
	TSharedPtr<TArray<FPGCNodePosition>> Nodes;
};

}

UCLASS(BlueprintType)
class PGC_API UPGCMesh : public UActorComponent
{
	GENERATED_BODY()

	static TMap<PGCMeshCache::CacheKey, PGCMeshCache::CacheVal> Cache;

	TScriptInterface<IPGCGenerator> Generator;

	TSharedPtr<Mesh> CurrentMesh;
	TSharedPtr<TArray<FPGCNodePosition>> CurrentNodes;

	void Generate(int NumDivisions, bool Triangularise);
	void RealGenerate(PGCMeshCache::CacheKey key, int NumDivisions, bool Triangularise);

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
		FPGCMeshResult GenerateMergeChannels(int NumDivisions, bool InsideOut, bool Triangularise, PGCDebugEdgeType DebugEdges);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Generate Channels", Keywords = "PGC, procedural"), Category = "PGC")
		FPGCMeshResult GenerateChannels(int NumDivisions, bool InsideOut, bool Triangularise, PGCDebugEdgeType DebugEdges,
			int StartChannel, int EndChannel);

};
