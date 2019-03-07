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


#include "Mesh.generated.h"

PRAGMA_DISABLE_OPTIMIZATION

struct FPGCCube;

UENUM()
enum class PGCEdgeType {
	Rounded,
	Sharp,
	Auto,		///< Make sharp if initial angle > some threshold, otherwise smooth
				///< when merging with other sorts of edge, the other takes presidence
	Unset		///< a default for "EffectiveType" so we can assert it has been set
};

// Anything trumps Unset
// Anything except Unset trumps Auto
// Sharp trumps Rounded
inline PGCEdgeType MergeEdgeTypes(const PGCEdgeType& e1, const PGCEdgeType& e2)
{
	if (e1 == PGCEdgeType::Unset)
		return e2;

	if (e2 == PGCEdgeType::Unset)
		return e1;

	if (e1 == PGCEdgeType::Auto)
		return e2;

	if (e2 == PGCEdgeType::Auto)
		return e1;

	if (e1 == PGCEdgeType::Rounded)
		return e2;

	return e1;
}

template <typename T> class TArrayIdx;

template <typename T>
class Idx {
	friend class TArrayIdx<T>;
	friend FArchive& operator<<(FArchive&, Idx& idx);

	explicit operator int() const { return I; }

public:
	Idx() : I(-1) {}
	explicit Idx(int i) : I(i) {}

	bool Valid() const { return I != -1; }

	bool operator<(const Idx& rhs) const { return I < rhs.I; }
	bool operator>(const Idx& rhs) const { return I > rhs.I; }
	bool operator<=(const Idx& rhs) const { return I <= rhs.I; }
	bool operator>=(const Idx& rhs) const { return I >= rhs.I; }
	bool operator==(const Idx& rhs) const { return I == rhs.I; }
	bool operator!=(const Idx& rhs) const { return I != rhs.I; }

	Idx operator++() { I++; return *this; }
	Idx operator++(int) { check(Valid());  Idx temp{ *this }; I++; return temp; }
	Idx operator--() { I--; return *this; }
	Idx operator--(int) { check(Valid()); Idx temp{ *this }; I--; return temp; }

	// for very rare conversion out to other systems
	int AsInt() const { return I; }

	const static Idx None;

	inline friend uint32 GetTypeHash(const Idx idx)
	{
		return GetTypeHash(idx.I);
	}

	inline friend FArchive& operator<<(FArchive& Ar, Idx& idx) {
		Ar << idx.I;

		return Ar;
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

	bool operator==(const MeshVertRaw& rhs) const {
		return Pos == rhs.Pos && UV == rhs.UV;
	}

//	bool ToleranceCompare(const MeshVertRaw& other, float tolerance) const;

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
		// -1 means just use any UVGroup we have...
		// (used for debug line drawing, where we don't have any UVs...)
		if (UVGroup == -1)
			UVGroup = UVs.CreateConstIterator()->Key;

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

inline FArchive& operator<<(FArchive& Ar, MeshVert& mv) {
	Ar << mv.EdgeIdxs;
	Ar << mv.FaceIdxs;

	return Ar;
}

struct MeshEdge {
	Idx<MeshVert> StartVertIdx = Idx<MeshVert>::None;
	Idx<MeshVert> EndVertIdx = Idx<MeshVert>::None;

	Idx<MeshFace> ForwardFaceIdx = Idx<MeshFace>::None;
	Idx<MeshFace> BackwardsFaceIdx = Idx<MeshFace>::None;

	PGCEdgeType SetType = PGCEdgeType::Unset;					///< we need to start unset here, because we make edges with a default value and then merge-in the real one
	PGCEdgeType EffectiveType = PGCEdgeType::Unset;				///< same as SetType except that "Auto" has been resolved into one of Rounded or Sharp

	void AddFace(Idx<MeshFace> face_idx, Idx<MeshVert> start_vert_idx)
	{
		if (start_vert_idx == StartVertIdx)
		{
			check(!ForwardFaceIdx.Valid());
			ForwardFaceIdx = face_idx;
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
		return ForwardFaceIdx == face_idx || BackwardsFaceIdx == face_idx;
	}

	Idx<MeshFace> OtherFace(Idx<MeshFace> face_idx) const
	{
		check(Contains(face_idx));

		return face_idx == BackwardsFaceIdx ? ForwardFaceIdx : BackwardsFaceIdx;
	}

	Idx<MeshVert> OtherVert(Idx<MeshVert> vert_idx) const
	{
		check(Contains(vert_idx));

		return vert_idx == StartVertIdx ? EndVertIdx : StartVertIdx;
	}

	MeshVertMultiUV WorkingEdgeVertex;
};

inline FArchive& operator<<(FArchive& Ar, MeshEdge& me) {
	Ar << me.StartVertIdx;
	Ar << me.EndVertIdx;
	Ar << me.ForwardFaceIdx;
	Ar << me.BackwardsFaceIdx;
	Ar << me.SetType;
	Ar << me.EffectiveType;

	return Ar;
}

struct MeshFace {
	TArray<Idx<MeshVert>> VertIdxs;
	TArray<Idx<MeshEdge>> EdgeIdxs;

	int UVGroup = -1;		///< allow us to respect different UVs at shared vertices
	int Channel;

	// transient...
	MeshVertMultiUV WorkingFaceVertex;

	bool VertsAreRegular() const {
		for (int i = 1; i < VertIdxs.Num(); i++)
		{
			if (VertIdxs[i] < VertIdxs[0])
				return false;
		}

		return true;
	}

	MeshFace() : Channel(-1) {}
	MeshFace(int channel) : Channel(channel) {}
};

inline FArchive& operator<<(FArchive& Ar, MeshFace& mf)
{
	Ar << mf.VertIdxs;
	Ar << mf.EdgeIdxs;
	Ar << mf.UVGroup;
	Ar << mf.Channel;

	return Ar;
}

USTRUCT(BlueprintType)
struct FPGCDebugEdge {
	GENERATED_USTRUCT_BODY()

	FPGCDebugEdge()
		: StartVertex(-1), EndVertex(-1) {}
	FPGCDebugEdge(int start, int end)
		: StartVertex(start), EndVertex(end) {}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PGC Mesh")
	int StartVertex;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PGC Mesh")
	int EndVertex;
};

// purely because the wrapping system doesn't support array of array
USTRUCT(BlueprintType)
struct FPGCTriangleSet {
	GENERATED_USTRUCT_BODY()
		
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PGC Mesh")
	TArray<int> Triangles;
};

USTRUCT(BlueprintType)
struct FPGCNodePosition {
	GENERATED_USTRUCT_BODY()

	FPGCNodePosition() = default;
	FPGCNodePosition(const FVector& pos, const FVector& up) : Position(pos), UpVector(up) {}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PGC Mesh")
	FVector Position;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PGC Mesh")
	FVector UpVector;
};

inline FArchive& operator<<(FArchive& Ar, FPGCNodePosition& pos)
{
	Ar << pos.Position;
	Ar << pos.UpVector;

	return Ar;
}

USTRUCT(BlueprintType)
struct FPGCMeshResult {
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PGC Mesh")
	TArray<FVector> Verts;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PGC Mesh")
	TArray<FVector2D> UVs;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PGC Mesh")
	TArray<FPGCTriangleSet> FaceChannels;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PGC Mesh")
	TArray<FPGCDebugEdge> SharpEdges;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PGC Mesh")
	TArray<FPGCDebugEdge> RoundedEdges;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PGC Mesh")
	TArray<FPGCDebugEdge> AutoEdges;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PGC Mesh")
	TArray<FPGCNodePosition> Nodes;
};

UENUM(BlueprintType)
enum class PGCDebugEdgeType : uint8 {
	None,
	Effective,
	Set
};

class Mesh : public TSharedFromThis<Mesh>
{
	friend FArchive& operator<<(FArchive&, Mesh&);

	TArrayIdx<MeshVert> Vertices;
	TArrayIdx<MeshEdge> Edges;
	TArrayIdx<MeshFace> Faces;

	TArray<MeshVertRaw> BakedVerts;
	TArray<MeshFaceRaw> BakedFaces;

	int NextUVGroup = 0;
	float VertexTolerance = 0.001f;	// currently working on meshes that are multiples of 1 in size, so 0.001 should be plenty
	const float CosAutoSharpAngle;

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
	Idx<MeshVert> FindVert(const FVector& pos, int UVGroup) const;

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

	Idx<MeshFace> AddFaceFromRawVerts(const TArray<MeshVertRaw>& vertices, int UVGroup, const TArray<PGCEdgeType>& edge_types, int channel);

	// the function of these was driven by the AddFace requirement of finding and canceling an existing face
	// which is the reverse of these, so the arguments here take a face winding the opposite wat to the one we are removing
	// if we need a "forward" version of either of these it would be trivial to write...
	bool CancelExistingReverseFace(const TArray<Idx<MeshVert>>& face);
	bool CancelExistingReverseFaceFromVects(const TArray<FVector>& vertices);

	Idx<MeshVertRaw> BakeVertex(const MeshVertRaw& mvr);
	Idx<MeshVertRaw> FindBakedVert(const MeshVertRaw& mvr) const;
	// take the faces tagged "from_channel" and bake them into the FaceChannel "to_face_channel" in the array
	// *SPECIAL* to put all channels into one, supply -1 as "from_channel"
	void BakeChannelsIntoFaceChannel(FPGCMeshResult& mesh, bool insideOut, PGCDebugEdgeType debugEdges, int from_channel, int to_face_channel);

	TSharedRef<Mesh> SplitSharedVerts();		///< edges and faces should only share a vert if they form a single "pyramid" with that vert as the point, when two 
									            ///< pyramids share a vert we split the vert
	TArray<TArray<Idx<MeshFace>>> FindPyramids(Idx<MeshVert> vert_idx);
	void SplitPyramids(const TArray<TArray<Idx<MeshFace>>>& pyramids, Idx<MeshVert> vert_idx);

	// if we cyclically re-order the edges, then we can need to reorder the edge_types because during construction
	// we find the edges from the verts
	static void RegularizeVertIdxs(TArray<Idx<MeshVert>>& vert_idxs, TArray<PGCEdgeType>* edge_types);

	TSharedPtr<Mesh> SubdivideInner();

	void ResolveEffectiveEdgeTypes();
	void CalcEffectiveType(MeshEdge& edge);
	FVector CalcNonplanarFaceNormal(const MeshFace& face);

public:	
	// Sets default values for this actor's properties
	Mesh(float cosAutoSharpAngleDegrees) : CosAutoSharpAngle(cosAutoSharpAngleDegrees) {
		// initialised?
		check(CosAutoSharpAngle != -2);
	}
	Mesh() : CosAutoSharpAngle(-2) {}

	// if we are viewing a starting mesh, some faces can be very non-planar, and the arbitrary meshing of Bake isn't easy to look at
	// so can do a pass of this, but on a divided mesh all faces should be smaller and flatter and that not matter...
	TSharedPtr<Mesh> Triangularise();

	TSharedPtr<Mesh> Subdivide();

	TSharedPtr<Mesh> SubdivideN(int count);

	// where existing edges are duplicated with incoming ones
	// the result is a sharp edge if either edge is sharp
	// (this is poor, but only in the same way that building by cubes is poor in the first instance)
	void AddCube(const FPGCCube& cube);

	Idx<MeshFace> AddFaceFromVects(const TArray<FVector>& vertices, const TArray<FVector2D>& uvs,
		int UVGroup, const TArray<PGCEdgeType>& edge_types, int channel);

	void BakeAllChannelsIntoOne(FPGCMeshResult& mesh, bool insideOut, PGCDebugEdgeType debugEdges);
	void BakeChannels(FPGCMeshResult& mesh, bool insideOut, PGCDebugEdgeType debugEdges, int start_channel, int end_channel);

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

FArchive& operator<<(FArchive& Ar, Mesh& mesh);

PRAGMA_ENABLE_OPTIMIZATION
