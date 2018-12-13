// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "LayoutGraph.h"

#include "TestGenerator.generated.h"

enum class TestConnectorTypes {
	StandardRoad,
};

using ConnectorDefT = LayoutGraph::ConnectorDef<TestConnectorTypes>;
using ConnectorInstT = LayoutGraph::ConnectorInst<TestConnectorTypes>;
using NodeT = LayoutGraph::Node<TestConnectorTypes>;

class StandardRoadbed : public ConnectorDefT {
public:
	StandardRoadbed()
		: ConnectorDefT(TestConnectorTypes::StandardRoad,
			TArray<FVector2D> {
				{ -2, 0.5 },
				{ -1, 0.5 },
				{ 0, 0.5 },
				{ 1, 0.5 },
				{ 2, 0.5 },
				{ 2, -0.5 },
				{ 1, -0.5 },
				{ 0, -0.5 },
				{ -1, -0.5 },
				{ -2, -0.5 },
			}) {}

	static const StandardRoadbed Instance;
};

class YJunction : public NodeT {
	YJunction()
		: NodeT(TArray<ConnectorInstT>{
			ConnectorInstT(StandardRoadbed::Instance, FVector{ 0, -0.866f, 0 }, FVector{ -1, 0, 0 }),
			ConnectorInstT(StandardRoadbed::Instance, FVector{ 0.5f, 0.433f, 0 }, FVector{ 0.866f, 0.5f, 0 }),
			ConnectorInstT(StandardRoadbed::Instance, FVector{ 0.5f, -0.433f, 0 }, FVector{ -0.866f, 0.5f, 0 }),
	}) {}
};

UCLASS()
class PGC_API ATestGenerator : public AActor
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

	
	
};
