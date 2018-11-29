// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "GameFramework/Actor.h"
#include "Runtime/Core/Public/Math/Vector.h"
#include "Runtime/Core/Public/Containers/Array.h"
#include "Runtime/Core/Public/Templates/UnrealTemplate.h"
#include "Runtime/Core/Public/Templates/SharedPointer.h"
#include "Runtime/Core/Public/Misc/AssertionMacros.h"
#include "Runtime/CoreUObject/Public/UObject/ObjectMacros.h"

#include "PGCCube.h"


#include "Mesh.generated.h"

PRAGMA_DISABLE_OPTIMIZATION

struct MeshFaceRaw {
	TArray<int> VertIdxs;
};

// just the data required for output
struct MeshVertRaw {
	MeshVertRaw() : Pos{ 0, 0, 0 }, UV{ 0, 0 } {}
	MeshVertRaw(FVector pos, FVector2D uv) : Pos{ pos }, UV{ uv } {}

	const MeshVertRaw& operator+=(const MeshVertRaw& rhs)
	{
		Pos += rhs.Pos;
		UV += rhs.UV;

		return *this;
	}

	MeshVertRaw operator/(float rhs) {
		return MeshVertRaw{ Pos / rhs, UV / rhs };
	}

	bool operator==(const MeshVertRaw& rhs) const {
		return Pos == rhs.Pos && UV == rhs.UV;
	}

	FVector Pos;
	FVector2D UV;
};

struct MeshMultiUV : public TMap<int, FVector2D>
{
	const MeshMultiUV& operator+=(const MeshMultiUV& rhs) {
		// there is a theory that we only need to keep UVs that are in both parents
		// but that is awkward if we want to construct an "empty" value and accumulate into it
		// and the extra values should be harmless as nobody should want them...

		for (auto p : rhs)
		{
			if (Contains(p.Key))
			{
				(*this)[p.Key] += p.Value;
			}
			else
			{
				Add(p.Key) = p.Value;
			}
		}

		return *this;
	}

	MeshMultiUV operator+(const MeshMultiUV& rhs) const
	{
		MeshMultiUV ret{ *this };

		ret += rhs;

		return ret;
	}

	MeshMultiUV operator/(float rhs) {
		MeshMultiUV ret;

		for (auto p : *this)
		{
			ret.Add(p.Key) = p.Value / rhs;
		}

		return ret;
	}

	MeshMultiUV operator*(float rhs) {
		MeshMultiUV ret;

		for (auto p : *this)
		{
			ret.Add(p.Key) = p.Value * rhs;
		}

		return ret;
	}
};

struct MeshVertMultiUV {
	MeshVertMultiUV() : Pos{ 0, 0, 0 }{}

	FVector Pos;
	MeshMultiUV UVs;

	MeshVertRaw ToMeshVertRaw(int UVGroup) const {
		return MeshVertRaw{ Pos, UVs[UVGroup] };
	}

	const MeshVertMultiUV& operator+=(const MeshVertMultiUV& rhs)
	{
		Pos += rhs.Pos;

		UVs += rhs.UVs;

		return *this;
	}

	MeshVertMultiUV operator+(const MeshVertMultiUV& rhs)
	{
		MeshVertMultiUV ret{ *this };

		ret += rhs;

		return ret;
	}

	MeshVertMultiUV operator/(float rhs) {
		MeshVertMultiUV ret;
		ret.Pos = Pos / rhs;
		ret.UVs = UVs / rhs;

		return ret;
	}

	MeshVertMultiUV operator*(float rhs) {
		MeshVertMultiUV ret;
		ret.Pos = Pos * rhs;
		ret.UVs = UVs * rhs;

		return ret;
	}
};

struct MeshVert : public MeshVertMultiUV {
	TArray<int> EdgeIdxs;
	TArray<int> FaceIdxs;

	MeshVertMultiUV WorkingNewPos;
};

struct MeshEdge {
	int StartVertIdx = -1;
	int EndVertIdx = -1;

	int ForwardsFaceIdx = -1;
	int BackwardsFaceIdx = -1;

	void AddFace(int face_idx, int start_vert_idx)
	{
		if (start_vert_idx == StartVertIdx)
		{
			check(ForwardsFaceIdx == -1);
			ForwardsFaceIdx = face_idx;
		}
		else
		{
			check(start_vert_idx == EndVertIdx);
			check(BackwardsFaceIdx == -1);

			BackwardsFaceIdx = face_idx;
		}
	}

	MeshVertMultiUV WorkingEdgeVertex;
};

struct MeshFace {
	TArray<int> VertIdxs;
	TArray<int> EdgeIdxs;

	int UVGroup = -1;		///< allow us to respect different UVs at shared vertices

	MeshVertMultiUV WorkingFaceVertex;
};

USTRUCT(BlueprintType)
struct FPGCMeshResult {
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PGC Mesh")
	TArray<FVector> Verts;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PGC Mesh")
	TArray<FVector2D> UVs;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PGC Mesh")
	TArray<int> Triangles;
};

class APGCGenerator;

class Mesh : public TSharedFromThis<Mesh>
{
	TArray<MeshVert> Vertices;
	TArray<MeshEdge> Edges;
	TArray<MeshFace> Faces;

	TArray<MeshVertRaw> BakedVerts;
	TArray<MeshFaceRaw> BakedFaces;

	int NextUVGroup = 0;

	bool FaceUnique(MeshFace face) const;

	int FindEdge(int idx1, int idx2) const;
	int AddFindEdge(int idx1, int idx2);

	int FindVert(const FVector& pos) const;
	int FindVert(const MeshVertRaw& vert, int UVGroup) const;

	int AddFace(MeshFace face);
	int AddVert(MeshVertRaw vert, int UVGroup);
	int AddVert(FVector pos);
	int FindFaceByVertIdxs(const TArray<int>& vert_idxs) const;
	void RemoveFace(int face_idx);

	int AddFaceFromRawVerts(const TArray<MeshVertRaw>& vertices, int UVGroup);
	bool CancelExistingFace(const TArray<FVector>& vertices);
	int AddFaceFromVects(const TArray<FVector>& vertices, const TArray<FVector2D>& uvs, int UVGroup);

	int BakeVertex(const MeshVertRaw& mvr);
	int FindBakedVert(const MeshVertRaw& mvr) const;

	static void RegularizeVertIdxs(TArray<int>& vert_idxs);


public:	
	// Sets default values for this actor's properties
	Mesh() {}

	TSharedPtr<Mesh> Subdivide();

	TSharedPtr<Mesh> SubdivideN(int count);

	void AddCube(int X, int Y, int Z);

	void Bake(FPGCMeshResult& mesh);

	// C++ only
	void CheckConsistent(bool closed);

	void Clear() {
		Vertices.Empty();
		Edges.Empty();
		Faces.Empty();

		NextUVGroup = 0;
	}

#if !UE_BUILD_RELEASE
	void UnitTest();
#endif
};

PRAGMA_ENABLE_OPTIMIZATION