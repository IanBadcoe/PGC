// Fill out your copyright notice in the Description page of Project Settings.

#include "TestGenerator.h"

using namespace LayoutGraph;
using namespace StructuralGraph;

const static ConnectorDef::ProfileArray StandardRoadbedProfileData
{
	ProfileVert { FVector2D { -2,  0.5 }, PGCEdgeType::Rounded },
	ProfileVert { FVector2D {  2,  0.5 }, PGCEdgeType::Sharp   },
	ProfileVert { FVector2D {  2, -0.5 }, PGCEdgeType::Rounded },
	ProfileVert { FVector2D { -2, -0.5 }, PGCEdgeType::Sharp   },
};

const ConnectorDef StandardRoadbed_CD
{
	TestConnectorTypes::StandardRoad,
	StandardRoadbedProfileData
};

const YJunction::ConnectorArray YJunction::ConnectorData
{
	// speeds shouldn't come from here?  should be set by generation needs?  pass into ctor?  just overwrite later?
	MakeShared<ConnectorInst>(StandardRoadbed_CD, FVector{ 0, -3.46f, 0 }, FVector{ 0, -1, 0 }, FVector{ 0, 0, 1 }),
	MakeShared<ConnectorInst>(StandardRoadbed_CD, FVector{ 3, 1.73f, 0 }, FVector{ 0.866f, 0.5f, 0 }, FVector{ 0, 0, 1 }),
	MakeShared<ConnectorInst>(StandardRoadbed_CD, FVector{ -3, 1.73f, 0 }, FVector{ -0.866f, 0.5f, 0 }, FVector{ 0, 0, 1 }),
};
const YJunction::VertexArray YJunction::VertexData
{
	//FVector { 3, -1.73f, 0.5f },
	//FVector { 3, -1.73f, -0.5f },
	//FVector { 0, 3.46f, 0.5f },
	//FVector { 0, 3.46f, -0.5f },
	//FVector { -3, -1.73f, 0.5f },
	//FVector { -3, -1.73f, -0.5f },
};
const YJunction::PolygonArray YJunction::PolygonData
{
	Polygon { {  0, 1 }, /*{ -1, 0 },*/ {  1, 0 }, {  1, 3 }, /*{ -1, 1 },*/ {  0, 2 }, },			// between C0 and C1
	Polygon { {  1, 1 }, /*{ -1, 2 },*/ {  2, 0 }, {  2, 3 }, /*{ -1, 3 },*/ {  1, 2 }, },			// between C1 and C2
	Polygon { {  2, 1 }, /*{ -1, 4 },*/ {  0, 0 }, {  0, 3 }, /*{ -1, 5 },*/ {  2, 2 }, },			// between C2 and C0
	Polygon { {  0, 1 }, {  0, 0 }, /*{ -1, 4 },*/
			  {  2, 1 }, {  2, 0 }, /*{ -1, 2 },*/
			  {  1, 1 }, {  1, 0 }, /*{ -1, 0 },*/ },			// top
	Polygon { {  0, 3 }, {  0, 2 }, /*{ -1, 1 },*/
			  {  1, 3 }, {  1, 2 }, /*{ -1, 3 },*/
			  {  2, 3 }, {  2, 2 }, /*{ -1, 5 },*/ },			// bottom
};

YJunction::YJunction()
	: Node(ConnectorData, VertexData, PolygonData)
{
}

Node * YJunction::FactoryMethod() const
{
	return new YJunction();
}

// Sets default values
ATestGenerator::ATestGenerator()
{
}

void ATestGenerator::EnsureGraphs()
{
	if (!TopologicalGraph.IsValid())
	{
		TopologicalGraph = MakeShared<TestGraph>();
		TopologicalGraph->Generate();
	}

	if (!StructuralGraph.IsValid())
	{
		StructuralGraph = MakeShared<SGraph>(TopologicalGraph);
	}
}

void ATestGenerator::EnsureOptimizer()
{
	EnsureGraphs();

	if (!Optimizer.IsValid())
	{
		check(!OptimizerInterface.IsValid());

		OptimizerInterface = MakeShared<OptFunction>(StructuralGraph);

		Optimizer = MakeShared<NlOptWrapper>(OptimizerInterface);
	}
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
	EnsureGraphs();

	StructuralGraph->MakeMesh(mesh);
}

bool ATestGenerator::NeedsSteps()
{
	return true;
}

void ATestGenerator::Step()
{
	EnsureOptimizer();

	Optimizer->Optimize();
}

void TestGraph::Generate()
{
	Nodes.Add(MakeShared<YJunction>());
	Nodes.Add(MakeShared<YJunction>());
	//Nodes.Add(MakeShared<YJunction>());
	//Nodes.Add(MakeShared<YJunction>());

	Nodes[1]->Position.SetLocation(FVector(0, -20, 0));
	Nodes[1]->Position.SetRotation(FQuat(FVector(0, 0, 1), PI));
	//Nodes[2]->Position.SetLocation(FVector(20, 0, 0));
	//Nodes[3]->Position.SetLocation(FVector(0, 20, 10));

	//Connect(0, 0, 1, 0, 10, 100.0f, 100.0f);
	//Connect(0, 1, 1, 2, 10, 100.0f, 100.0f);
	Connect(1, 1, 0, 2, 20, 100.0f, 100.0f);
	//Connect(0, 1, 2, 1, 10, 100.0f, 100.0f);
	//Connect(0, 2, 3, 2, 10, 100.0f, 100.0f);
	//Connect(2, 2, 1, 2, 10, 100.0f, 100.0f);
	//Connect(2, 0, 3, 0, 10, 100.0f, 100.0f);
	//Connect(3, 1, 1, 1, 10, 100.0f, 100.0f);
}
