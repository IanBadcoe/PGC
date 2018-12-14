// Fill out your copyright notice in the Description page of Project Settings.

#include "TestGenerator.h"

using namespace LayoutGraph;

const static TArray<FVector2D> StandardRoadbedProfileData
{
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
};

const ConnectorDef StandardRoadbed_CD
{
	TestConnectorTypes::StandardRoad,
	StandardRoadbedProfileData
};

const YJunction::ConnectorArray YJunction::ConnectorData
{
	ConnectorInst(StandardRoadbed_CD, FVector{ 0, -3.46f, 0 }, FVector{ 0, -1, 0 }, FVector{ 0, 0, 1 }),
	ConnectorInst(StandardRoadbed_CD, FVector{ 3, 1.73f, 0 }, FVector{ 0.866f, 0.5f, 0 }, FVector{ 0, 0, 1 }),
	ConnectorInst(StandardRoadbed_CD, FVector{ -3, 1.73f, 0 }, FVector{ -0.866f, 0.5f, 0 }, FVector{ 0, 0, 1 }),
};
const YJunction::VertexArray YJunction::VertexData
{
	FVector { 3, -1.73f, 0.5f },
	FVector { 3, -1.73f, -0.5f },
	FVector { 0, 3.46f, 0.5f },
	FVector { 0, 3.46f, -0.5f },
	FVector { -3, -1.73f, 0.5f },
	FVector { -3, -1.73f, -0.5f },
};
const YJunction::PolygonArray YJunction::PolygonData
{
	Polygon { {  0, 4 }, { -1, 0 }, {  1, 0 }, {  1, 9 }, { -1, 1 }, {  0, 5 }, },			// between C0 and C1
	Polygon { {  1, 4 }, { -1, 2 }, {  2, 0 }, {  2, 9 }, { -1, 3 }, {  1, 5 }, },			// between C1 and C2
	Polygon { {  2, 4 }, { -1, 4 }, {  0, 0 }, {  0, 9 }, { -1, 5 }, {  2, 5 }, },			// between C2 and C0
	Polygon { {  0, 4 }, {  0, 3 }, {  0, 2 }, {  0, 1 }, {  0, 0 }, { -1, 4 },
			  {  2, 4 }, {  2, 3 }, {  2, 2 }, {  2, 1 }, {  2, 0 }, { -1, 2 },
			  {  1, 4 }, {  1, 3 }, {  1, 2 }, {  1, 1 }, {  1, 0 }, { -1, 0 }, },			// top
	Polygon { {  0, 9 }, {  0, 8 }, {  0, 7 }, {  0, 6 }, {  0, 5 }, { -1, 1 },
			  {  1, 9 }, {  1, 8 }, {  1, 7 }, {  1, 6 }, {  1, 5 }, { -1, 3 },
			  {  2, 9 }, {  2, 8 }, {  2, 7 }, {  2, 6 }, {  2, 5 }, { -1, 5 }, },			// bottom
};

YJunction::YJunction()
	: Node(ConnectorData, VertexData, PolygonData)
{
}

// Sets default values
ATestGenerator::ATestGenerator()
{
}

// Called when the game starts or when spawned
void ATestGenerator::BeginPlay()
{
	Super::BeginPlay();
	
}

void ATestGenerator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// ...
}

void ATestGenerator::MakeMesh(TSharedPtr<Mesh> mesh)
{
	TestGraph g;

	g.Generate();

	g.MakeMesh(mesh);
}

void TestGraph::Generate()
{
	Nodes.Add(MakeShared<YJunction>());
}
