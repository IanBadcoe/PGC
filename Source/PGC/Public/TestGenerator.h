// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "LayoutGraph.h"
#include "PGCGenerator.h"

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
	void Generate();
};

UCLASS(BlueprintType)
class PGC_API ATestGenerator : public AActor, public IPGCGenerator
{
	GENERATED_BODY()
	
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

};
