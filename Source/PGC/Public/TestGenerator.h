// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "LayoutGraph.h"
#include "StructuralGraph.h"
#include "PGCGenerator.h"
#include "NlOptWrapper.h"

#include "TestGenerator.generated.h"

using namespace LayoutGraph;

enum class TestConnectorTypes {
	StandardRoad,
};

class YJunction : public LayoutGraph::Node {
public:
	YJunction(const FVector& pos, const FVector& rot);
	YJunction(TArray<TSharedPtr<ParameterisedProfile>> profiles, const FVector& pos, const FVector& rot)
		: Node(profiles, pos, rot) {
		check(profiles.Num() == 3);
	}
	virtual ~YJunction() = default;
};

class TestGraph : public LayoutGraph::Graph {
public:
	TestGraph() : LayoutGraph::Graph(1.5f) {}

	void Generate();
};

UCLASS(BlueprintType)
class PGC_API ATestGenerator : public AActor, public IPGCGenerator
{
	GENERATED_BODY()

	TSharedPtr<TestGraph> TopologicalGraph;
	TSharedPtr<StructuralGraph::SGraph> StructuralGraph;

	TSharedPtr<NlOptIface> OptimizerInterface;
	TSharedPtr<NlOptWrapper> Optimizer;

	void EnsureGraphs();
	void EnsureOptimizer();

public:	
	// Sets default values for this actor's properties
	ATestGenerator();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Inherited via IPGCGenerator
	virtual void MakeMesh(TSharedPtr<Mesh> mesh);


	// Inherited via IPGCGenerator
	virtual bool NeedsRefinement() override;

	virtual void Refine() override;

};
