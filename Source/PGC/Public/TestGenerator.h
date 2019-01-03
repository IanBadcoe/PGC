// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "LayoutGraph.h"
#include "StructuralGraph.h"
#include "PGCGenerator.h"
#include "NlOptWrapper.h"

#include "TestGenerator.generated.h"

enum class TestConnectorTypes {
	StandardRoad,
};

extern const LayoutGraph::ConnectorDef StandardRoadbed_CD;

class YJunction : public LayoutGraph::Node {
public:
	YJunction();
	virtual ~YJunction() = default;

	// Creates an empty Node of the same type...
	virtual Node* FactoryMethod() const override;

private:
	static const ConnectorArray ConnectorData;
	static const VertexArray VertexData;
	static const PolygonArray PolygonData;
};

class TestGraph : public LayoutGraph::Graph {
public:
	TestGraph() : LayoutGraph::Graph(0.5f) {}

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
