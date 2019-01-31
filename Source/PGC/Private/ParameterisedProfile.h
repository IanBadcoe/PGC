#pragma once

namespace Profile {

// so that we can build a parameterized profile from separate top and bottom halves
// since width needs to be the same for both halves, still specify that only for ParamaterizedProfile
class ParameterizedRoadbedShape {
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
	ParameterizedRoadbedShape(float leftBarrier, float rightBarrier,
		float leftOverhang, float rightOverhang,
		int startSmooth,
		int endSmooth)
		: StartSmoothIdx(startSmooth), EndSmoothIdx(endSmooth),
		LeftBarrierHeight(leftBarrier), RightBarrierHeight(rightBarrier),
		LeftOverhang(leftOverhang), RightOverhang(rightOverhang)
	{}
	ParameterizedRoadbedShape(const ParameterizedRoadbedShape& rhs) = default;
	ParameterizedRoadbedShape& operator=(const ParameterizedRoadbedShape& rhs) = default;

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

	ParameterizedRoadbedShape Mirrored() const {
		return ParameterizedRoadbedShape
		{
			RightBarrierHeight, LeftBarrierHeight,
			RightOverhang, LeftOverhang,
			11 - EndSmoothIdx, 11 - StartSmoothIdx
		};
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

	float Width;					// width of roadbed
	float BarrierHeights[4];		// height of side-barriers: upper-right, lower-right, lower-left, upper-left
	float OverhangWidths[4];		// width of overhangs: upper-right, lower-right, lower-left, upper-left
	bool OutgoingSharp[24];			// whether any edge leaving this vert for the next profile should be sharp

	FVector2D AbsoluteBound;		// +ve corner of a rectangle large enough to just contain all

	static const bool UsesBarrierHeight[NumVerts];
	static const bool UsesOverhangWidth[NumVerts];
	static const bool IsXOuter[NumVerts];
	static const bool IsYOuter[NumVerts];

public:
	ParameterisedProfile(float width,
		TArray<float> barriers,
		TArray<float> overhangs,
		TArray<bool> outgoingSharps);
	ParameterisedProfile(const ParameterisedProfile& rhs) = default;
	ParameterisedProfile& operator=(const ParameterisedProfile& rhs) = default;

	ParameterisedProfile(float width, const TSharedPtr<ParameterizedRoadbedShape>& top, const TSharedPtr<ParameterizedRoadbedShape>& bottom);

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
};

}