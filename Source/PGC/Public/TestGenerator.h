// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "LayoutGraph.h"
#include "StructuralGraph.h"
#include "OptFunction.h"
#include "PGCGenerator.h"
#include "NlOptWrapper.h"

#include <typeinfo>

#include "TestGenerator.generated.h"

class YJunction : public LayoutGraph::Node {
public:
	YJunction(const FVector& pos, const FVector& rot)
		: Node(3, pos, rot) {
	}
	virtual ~YJunction() = default;
};

class TestGraph : public LayoutGraph::Graph {
public:
	TestGraph() : LayoutGraph::Graph(5.0f) {}

	void Generate();
};

class TestProfileSource : public StructuralGraph::ProfileSource {
public:
	TestProfileSource(const FRandomStream& random_stream);
	virtual ~TestProfileSource() = default;

	// Inherited via ProfileSource
	virtual TSharedPtr<Profile::ParameterisedProfile> GetProfile() const override;
	virtual TArray<TSharedPtr<Profile::ParameterisedProfile>> GetCompatibleProfileSequence(TSharedPtr<Profile::ParameterisedProfile> from, TSharedPtr<Profile::ParameterisedProfile> to, int steps) const override;

	void AddRoadbed(FString name, TSharedPtr<Profile::ParameterisedRoadbedShape> roadbed,
		bool suppress_mirroring = false, TArray<float> sizes = {});

	FRandomStream RStream;

private:
	TMap<FString, TSharedPtr<Profile::ParameterisedRoadbedShape>> Roadbeds;
	TArray<TSharedPtr<Profile::ParameterisedProfile>> Profiles;
};

UCLASS(BlueprintType)
class PGC_API ATestGenerator : public AActor, public IPGCGenerator
{
	GENERATED_BODY()

	TSharedPtr<TestGraph> TopologicalGraph;

public:
	// Sets default values for this actor's properties
	ATestGenerator();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PGC Mesh")
	FRandomStream RStream;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Inherited via IPGCGenerator
	virtual void MakeMesh(TSharedPtr<Mesh> mesh, const TSharedPtr<TArray<FPGCNodePosition>> Nodes) const override;
	virtual uint32 SettingsHash() const override;
	virtual FString GetName() const override { return "ATestGenerator"; }
};
