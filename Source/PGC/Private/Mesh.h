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

template <typename T> class TArrayIdx;

template <typename T>
class Idx {
	friend class TArrayIdx<T>;
	explicit operator int() const { return I; }

public:
	explicit Idx(int i) : I(i) {}

	bool Valid() const { return I != -1; }

	bool operator<(const Idx& rhs) const { return I < rhs.I; }
	bool operator>(const Idx& rhs) const { return I > rhs.I; }
	bool operator<=(const Idx& rhs) const { return I <= rhs.I; }
	bool operator>=(const Idx& rhs) const { return I >= rhs.I; }
	bool operator==(const Idx& rhs) const { return I == rhs.I; }
	bool operator!=(const Idx& rhs) const { return I != rhs.I; }

	Idx operator++() { I++; return *this; }
	Idx operator++(int) { Idx temp{ *this }; I++; return temp; }
	Idx operator--() { I--; return *this; }
	Idx operator--(int) { Idx temp{ *this }; I--; return temp; }

	// for very rare conversion out to other systems
	int AsInt() const { return I; }

	const static Idx None;

	inline friend uint32 GetTypeHash(const Idx idx)
	{
		return GetTypeHash(idx.I);
	}

private:
	int I;
};

struct MeshVertRaw;

template <typename Element>
class TArrayIdx : private TArray<Element> {
public:
	ElementType& operator[](Idx<Element> Index) {
		check(Index.Valid());
		return static_cast<TArray<Element>&>(*this)[(int)Index];
	}
	const ElementType& operator[](Idx<Element> Index) const {
		check(Index.Valid());
		return static_cast<const TArray<Element>&>(*this)[(int)Index];
	}

	Idx<Element> Num() const { return Idx<Element>(TArray::Num()); }
	Idx<Element> LastIdx() const { return Idx<Element>(TArray::Num() - 1); }

	void Empty() { TArray::Empty(); }

	void Push(const Element& elem) { TArray::Push(elem); }
	void Push(Element&& elem) { TArray::Push(elem); }

	bool Contains(const Element& value) const { return TArray::Contains(value); }

	void RemoveAt(Idx<Element> Index) { TArray::RemoveAt((int)Index); }

private:
	FORCEINLINE friend RangedForIteratorType      begin(TArrayIdx& Array) { return RangedForIteratorType(Array.ArrayNum, Array.GetData()); }
	FORCEINLINE friend RangedForConstIteratorType begin(const TArrayIdx& Array) { return RangedForConstIteratorType(Array.ArrayNum, Array.GetData()); }
	FORCEINLINE friend RangedForIteratorType      end(TArrayIdx& Array) { return RangedForIteratorType(Array.ArrayNum, Array.GetData() + static_cast<TArray&>(Array).Num()); }
	FORCEINLINE friend RangedForConstIteratorType end(const TArrayIdx& Array) { return RangedForConstIteratorType(Array.ArrayNum, Array.GetData() + static_cast<const TArray&>(Array).Num()); }
};

struct MeshFaceRaw {
	TArray<Idx<MeshVertRaw>> VertIdxs;
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

	//bool operator==(const MeshVertRaw& rhs) const {
	//	return Pos == rhs.Pos && UV == rhs.UV;
	//}

	bool ToleranceCompare(const MeshVertRaw& other, float tolerance) const;

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

struct MeshEdge;
struct MeshFace;

struct MeshVert : public MeshVertMultiUV {
	TArray<Idx<MeshEdge>> EdgeIdxs;
	TArray<Idx<MeshFace>> FaceIdxs;

	MeshVertMultiUV WorkingNewPos;
};

struct MeshEdge {
	Idx<MeshVert> StartVertIdx = Idx<MeshVert>::None;
	Idx<MeshVert> EndVertIdx = Idx<MeshVert>::None;

	Idx<MeshFace> ForwardsFaceIdx = Idx<MeshFace>::None;
	Idx<MeshFace> BackwardsFaceIdx = Idx<MeshFace>::None;

	PGCEdgeType Type = PGCEdgeType::Rounded;

	void AddFace(Idx<MeshFace> face_idx, Idx<MeshVert> start_vert_idx)
	{
		if (start_vert_idx == StartVertIdx)
		{
			check(!ForwardsFaceIdx.Valid());
			ForwardsFaceIdx = face_idx;
		}
		else
		{
			check(start_vert_idx == EndVertIdx);
			check(!BackwardsFaceIdx.Valid());

			BackwardsFaceIdx = face_idx;
		}
	}

	bool Contains(Idx<MeshVert> vert_idx) const {
		return StartVertIdx == vert_idx || EndVertIdx == vert_idx;
	}

	bool Contains(Idx<MeshFace> face_idx) const {
		return ForwardsFaceIdx == face_idx || BackwardsFaceIdx == face_idx;
	}

	Idx<MeshFace> OtherFace(Idx<MeshFace> face_idx) const
	{
		check(Contains(face_idx));

		return face_idx == BackwardsFaceIdx ? ForwardsFaceIdx : BackwardsFaceIdx;
	}

	Idx<MeshVert> OtherVert(Idx<MeshVert> vert_idx) const
	{
		check(Contains(vert_idx));

		return vert_idx == StartVertIdx ? EndVertIdx : StartVertIdx;
	}

	MeshVertMultiUV WorkingEdgeVertex;
};

struct MeshFace {
	TArray<Idx<MeshVert>> VertIdxs;
	TArray<Idx<MeshEdge>> EdgeIdxs;

	int UVGroup = -1;		///< allow us to respect different UVs at shared vertices

	bool VertsAreRegular() const {
		for (int i = 1; i < VertIdxs.Num(); i++)
		{
			if (VertIdxs[i] < VertIdxs[0])
				return false;
		}

		return true;
	}

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

class Mesh : public TSharedFromThis<Mesh>
{
	TArrayIdx<MeshVert> Vertices;
	TArrayIdx<MeshEdge> Edges;
	TArrayIdx<MeshFace> Faces;

	TArray<MeshVertRaw> BakedVerts;
	TArray<MeshFaceRaw> BakedFaces;

	int NextUVGroup = 0;
	float VertexTolerance = 0.001f;	// currently working on meshes that are multiples of 1 in size, so 0.001 should be plenty

	bool Clean = true;				// when we add geometry, we may generate inappropriately shared verts
									// this signals to clean that up

	// "face_idx" allows disambiguation when there's more than one edge between the same two verts
	// (happens in edge-edge overlap of cubes)
	Idx<MeshEdge> FindEdge(Idx<MeshVert> vert_idx1, Idx<MeshVert> vert_idx2, Idx<MeshFace> face_idx) const;
	TArray<Idx<MeshEdge>> FindAllEdges(Idx<MeshVert> vert_idx1, Idx<MeshVert> vert_idx2) const;
	// "partial_only" means only return an edge with a face-slot that isn't filled
	Idx<MeshEdge> FindEdge(Idx<MeshVert> idx1, Idx<MeshVert> idx2, bool partial_only) const;
	Idx<MeshEdge> AddFindEdge(Idx<MeshVert> idx1, Idx<MeshVert> idx2);

	Idx<MeshVert> FindVert(const FVector& pos) const;
	Idx<MeshVert> FindVert(const MeshVertRaw& vert, int UVGroup) const;

	Idx<MeshFace> AddFindFace(MeshFace face, const TArray<PGCEdgeType>& edge_types);
	Idx<MeshVert> AddVert(MeshVertRaw vert, int UVGroup);
	Idx<MeshVert> AddVert(FVector pos);
	Idx<MeshFace> FindFaceByVertIdxs(const TArray<Idx<MeshVert>>& vert_idxs) const;
	void RemoveFace(Idx<MeshFace> face_idx);
	void RemoveEdge(Idx<MeshEdge> edge_idx);		///< it must not be in use by any faces
	void RemoveVert(Idx<MeshVert> vert_idx);					///< it must not be in use by any edges (or faces)
	void MergePartialEdges();
	void MergeEdges(Idx<MeshEdge> edge_idx1, Idx<MeshEdge> edge_idx2);
	void CleanUpRedundantEdges();
	void CleanUpRedundantVerts();

	Idx<MeshFace> AddFaceFromRawVerts(const TArray<MeshVertRaw>& vertices, int UVGroup, const TArray<PGCEdgeType>& edge_types);
	bool CancelExistingFace(const TArray<FVector>& vertices);

	Idx<MeshVertRaw> BakeVertex(const MeshVertRaw& mvr);
	Idx<MeshVertRaw> FindBakedVert(const MeshVertRaw& mvr) const;

	TSharedRef<Mesh> SplitSharedVerts();		///< edges and faces should only share a vert if they form a single "pyramid" with that vert as the point, when two 
									            ///< pyramids share a vert we split the vert
	TArray<TArray<Idx<MeshFace>>> FindPyramids(Idx<MeshVert> vert_idx);
	void SplitPyramids(const TArray<TArray<Idx<MeshFace>>>& pyramids, Idx<MeshVert> vert_idx);

	// if we cyclically re-order the edges, then we can need to reorder the edge_types because during construction
	// we find the edges from the verts
	static void RegularizeVertIdxs(TArray<Idx<MeshVert>>& vert_idxs, TArray<PGCEdgeType>* edge_types);

	TSharedPtr<Mesh> SubdivideInner();

public:	
	// Sets default values for this actor's properties
	Mesh() {}

	TSharedPtr<Mesh> Subdivide();

	TSharedPtr<Mesh> SubdivideN(int count);

	// where existing edges are duplicated with incoming ones
	// the result is a sharp edge if either edge is sharp
	// (this is poor, but only in the same way that building by cubes is poor in the first instance)
	void AddCube(const FPGCCube& cube);

	Idx<MeshFace> AddFaceFromVects(const TArray<FVector>& vertices, const TArray<FVector2D>& uvs, int UVGroup, const TArray<PGCEdgeType>& edge_types);

	void Bake(FPGCMeshResult& mesh, bool insideOut);

	// C++ only
	void CheckConsistent(bool closed);

	void Clear() {
		Vertices.Empty();
		Edges.Empty();
		Faces.Empty();

		NextUVGroup = 0;

		Clean = true;
	}

#ifndef UE_BUILD_RELEASE
	static void UnitTest();
#endif
};

PRAGMA_ENABLE_OPTIMIZATION