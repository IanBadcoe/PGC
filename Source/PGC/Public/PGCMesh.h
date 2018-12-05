// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Components/ActorComponent.h"
#include "Mesh.h"
#include "PGCGenerator.h"

#include "PGCMesh.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PGC_API UPGCMesh : public UActorComponent
{
	GENERATED_BODY()

	TSharedPtr<Mesh> InitialMesh{ MakeShared<Mesh>() };

public:	
	// Sets default values for this component's properties
	UPGCMesh();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable)
	void SetInputMesh(APGCGenerator* gen)
	{
		InitialMesh->Clear();
		gen->MakeMesh(InitialMesh);
	}
	
	UFUNCTION(BlueprintCallable)
	FPGCMeshResult Generate(int NumDivisions, bool InsideOut);
};
