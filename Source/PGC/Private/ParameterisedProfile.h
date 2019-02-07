#pragma once

namespace Profile {

// so that we can build a parameterized profile from separate top and bottom halves
// since width needs to be the same for both halves, still specify that only for ParamaterizedProfile
class ParameterisedRoadbedShape {
	//
	//  1---------2           9---------10
	//  |         |           |         |
	//  |         |           |         |
	//  |         |           |         |
	//  |    4----3           8----7    |
	//  |    |                     |    |
	//  |    |                     |    |
	//  |    |                     |    |
	//  |    5---------------------6    |
	//  |                               |
	//  |                               |
	//  |                               |
	//  0                               11
	//
	// Smoothing works as follows:
	// 0 and 11 are always smooth (between top and bottom, nevcer drivable (when using this class)
	// Start/EndSmoothIdx specify a sub-range between 1 and 11 (inclusive of start) to be smoothed
	// (set to empty range (say 0 -> -1) to disable

	// when interacting with the ParameterizedProfile, we use our heights and overhangs here as fractions
	// and multiply them by the desired width

	int StartSmoothIdx;
	int EndSmoothIdx;

	float LeftBarrierHeight;
	float RightBarrierHeight;

	float LeftOverhang;
	float RightOverhang;

public:
	ParameterisedRoadbedShape(float leftBarrier, float rightBarrier,
		float leftOverhang, float rightOverhang,
		int startSmooth,
		int endSmooth)
		: StartSmoothIdx(startSmooth), EndSmoothIdx(endSmooth),
		  LeftBarrierHeight(leftBarrier), RightBarrierHeight(rightBarrier),
		  LeftOverhang(leftOverhang), RightOverhang(rightOverhang)
	{}
	ParameterisedRoadbedShape(const ParameterisedRoadbedShape& rhs) = default;
	ParameterisedRoadbedShape& operator=(const ParameterisedRoadbedShape& rhs) = default;

	float GetBarrierHeight(int idx) const {
		check(idx >= 0 && idx < 2);

		return idx == 0 ? LeftBarrierHeight : RightBarrierHeight;
	}

	float GetOverhang(int idx) const {
		check(idx >= 0 && idx < 2);

		return idx == 0 ? LeftOverhang : RightOverhang;
	}

	bool GetSmooth(int idx) const {
		check(idx >= 0 && idx < 12);

		if (idx == 0 || idx == 11)
			return true;

		return idx >= StartSmoothIdx && idx < EndSmoothIdx;
	}

	TSharedPtr<ParameterisedRoadbedShape> Mirrored() const {
		return MakeShared<ParameterisedRoadbedShape>
		(
			RightBarrierHeight, LeftBarrierHeight,
			RightOverhang, LeftOverhang,
			12 - EndSmoothIdx, 12 - StartSmoothIdx
		);
	}

	bool operator==(const ParameterisedRoadbedShape& rhs) const;

	bool operator!=(const ParameterisedRoadbedShape& rhs) const {
		return !(*this == rhs);
	}

	bool IsEmptySmoothRange() const {
		return StartSmoothIdx >= EndSmoothIdx;
	}
};

template<typename T, int N>
class TFixedSizeArray : public TArray<T, TFixedAllocator<N>> {
public:
	TFixedSizeArray()
	{
		AddDefaulted(N);
	}
	TFixedSizeArray(const TFixedSizeArray& rhs) = default;
	TFixedSizeArray& operator=(const TFixedSizeArray& rhs) = default;

	TFixedSizeArray(const TArray<T>& rhs) : TArray(rhs) { check(rhs.Num() == N); }
	TFixedSizeArray& operator=(const TArray& rhs)
	{
		check(rhs.Num() == N);

		static_cast<TArray<T>(*this) = rhs;

		return *this;
	}
};

class ParameterisedProfile {
public:
	static constexpr int VertsPerQuarter = 6;
	static constexpr int NumVerts = VertsPerQuarter * 4;

	enum class VertTypes {
		RoadbedInner = 0,
		BarrierTopInner = 1,
		OverhangEndInner = 2,
		OverhangEndOuter = 3,
		BarrierTopOuter = 4,
		RoadbedOuter = 5
	};

private:
	//               <--overhang
	//                 length-><-1.0->
	//            ^  3---------------4 ^
	//        1.0 |  |               | |
	//            |  |               | |
	//            v  2--------1      | |
	//                        |      | (barrier height)
	// -----------------------0      | |
	// <--  width/2) -->             | |
	//                               | |
	//              	             5 v

	float Width;										// width of roadbed
	TFixedSizeArray<float, 4> BarrierHeights;			// height of side-barriers: upper-right, lower-right, lower-left, upper-left
	TFixedSizeArray<float, 4> OverhangWidths;			// width of overhangs: upper-right, lower-right, lower-left, upper-left
	TFixedSizeArray<bool, NumVerts> OutgoingSharp;		// whether any edge leaving this vert for the next profile should be sharp
	TFixedSizeArray<bool, NumVerts> IsDrivableEdge;		// whether the edge following this surface defines a drivable surface

	FVector2D AbsoluteBound;							// +ve corner of a rectangle large enough to just contain all

	static const bool UsesBarrierHeight[NumVerts];
	static const bool UsesOverhangWidth[NumVerts];
	static const bool IsXOuter[NumVerts];
	static const bool IsYOuter[NumVerts];

	TSharedPtr<ParameterisedProfile> RawInterp(TSharedPtr<ParameterisedProfile> other, float frac) const;

	// when interpolating, need to do wall-height and overhang changes in a particular order
	// so that we don't end up with an overhang on wall it won't fit...
	TSharedPtr<ParameterisedProfile> SafeIntermediate(TSharedPtr<ParameterisedProfile> other) const;

	void CalcDrivable();
	void CalcDrivable(int start_edge);

public:
	ParameterisedProfile(float width,
		const TArray<float> barriers,
		const TArray<float> overhangs,
		const TArray<bool> outgoingSharps);
	ParameterisedProfile(const ParameterisedProfile& rhs) = default;
	ParameterisedProfile& operator=(const ParameterisedProfile& rhs) = default;

	ParameterisedProfile(float width, const TSharedPtr<ParameterisedRoadbedShape>& top, const TSharedPtr<ParameterisedRoadbedShape>& bottom);

	void CheckConsistent() const;

	void CalcAbsoluteBound();

	static int GetQuarterIdx(int idx)
	{
		return idx / VertsPerQuarter;
	}

	FVector2D GetPoint(int idx) const;

	FVector GetTransformedVert(int vert_idx, const FTransform& total_trans) const;
	FVector GetTransformedVert(VertTypes type, int quarter_idx, const FTransform& total_trans) const;

	float Radius() const { return AbsoluteBound.Size(); }

	float Diff(const TSharedPtr<ParameterisedProfile>& other) const;

	TSharedPtr<ParameterisedProfile> Interp(TSharedPtr<ParameterisedProfile> other, float frac) const;

	PGCEdgeType GetOutgoungEdgeType(int i) {
		return OutgoingSharp[i] ? PGCEdgeType::Sharp : PGCEdgeType::Rounded;
	}

	// querying about an edge using its _preceding_ vertex
	bool IsDrivable(int i) const {
		return IsDrivableEdge[i];
	}
};

}