#include "ParameterisedProfile.h"

using namespace Profile;

ParameterisedProfile::ParameterisedProfile(float width,
	TArray<float> barriers,
	TArray<float> overhangs,
	TArray<bool> outgoingSharps)
	: Width{ width }, AbsoluteBound{ 0, 0 }
{
	check(barriers.Num() == 4);
	check(overhangs.Num() == 4);
	check(outgoingSharps.Num() == 24);

	for (int i = 0; i < 4; i++)
	{
		BarrierHeights[i] = barriers[i] * Width;
		OverhangWidths[i] = overhangs[i] * Width;
	}

	for (int i = 0; i < NumVerts; i++)
	{
		OutgoingSharp[i] = outgoingSharps[i];
	}

	CalcAbsoluteBound();

	CheckConsistent();
}

ParameterisedProfile::ParameterisedProfile(float width, const TSharedPtr<ParameterizedRoadbedShape>& top, const TSharedPtr<ParameterizedRoadbedShape>& bottom)
	: Width(width)
{
	BarrierHeights[0] = top->GetBarrierHeight(1) * Width;
	BarrierHeights[1] = bottom->GetBarrierHeight(0) * Width;
	BarrierHeights[2] = bottom->GetBarrierHeight(1) * Width;
	BarrierHeights[3] = top->GetBarrierHeight(0) * Width;

	OverhangWidths[0] = top->GetOverhang(1) * Width;
	OverhangWidths[1] = bottom->GetOverhang(0) * Width;
	OverhangWidths[2] = bottom->GetOverhang(1) * Width;
	OverhangWidths[3] = top->GetOverhang(0) * Width;

	for (int i = 0; i < VertsPerQuarter; i++)
	{
		OutgoingSharp[i + VertsPerQuarter * 0] = top->GetSmooth(i + VertsPerQuarter);
		OutgoingSharp[i + VertsPerQuarter * 1] = bottom->GetSmooth(i);
		OutgoingSharp[i + VertsPerQuarter * 2] = bottom->GetSmooth(i + VertsPerQuarter);
		OutgoingSharp[i + VertsPerQuarter * 3] = top->GetSmooth(i);
	}

	CalcAbsoluteBound();

	CheckConsistent();
}

void ParameterisedProfile::CheckConsistent() const {
	check(Width > 0);

	for (int i = 0; i < 4; i++)
	{
		// no -ve values
		check(OverhangWidths[i] >= 0);
		check(BarrierHeights[i] >= 0);

		// cannot have an overhang unless the barrier is high enough
		check(OverhangWidths[i] == 0 || BarrierHeights[i] >= 1);

		const auto other_half_idx = 3 - i;

		// the overhangs do not collide
		check(OverhangWidths[i] + OverhangWidths[other_half_idx] <= Width || FMath::Abs(BarrierHeights[i] - BarrierHeights[other_half_idx]) >= 1);
	}
}

inline void ParameterisedProfile::CalcAbsoluteBound()
{
	for (int i = 0; i < NumVerts; i++)
	{
		AbsoluteBound = FVector2D{
			FMath::Max(AbsoluteBound.X, FMath::Abs(GetPoint(i).X)),
			FMath::Max(AbsoluteBound.Y, FMath::Abs(GetPoint(i).Y))
		};
	}
}

const bool ParameterisedProfile::UsesBarrierHeight[ParameterisedProfile::NumVerts]{
	false, true, true, true, true, false,
	false, true, true, true, true, false,
	false, true, true, true, true, false,
	false, true, true, true, true, false,
};

const bool ParameterisedProfile::UsesOverhangWidth[ParameterisedProfile::NumVerts]{
	false, false, true, true, false, false,
	false, false, true, true, false, false,
	false, false, true, true, false, false,
	false, false, true, true, false, false,
};

const bool ParameterisedProfile::IsXOuter[ParameterisedProfile::NumVerts]{
	false, false, false, false, true, true,
	true, true, false, false, false, false,
	false, false, false, false, true, true,
	true, true, false, false, false, false,
};

const bool ParameterisedProfile::IsYOuter[ParameterisedProfile::NumVerts]{
	false, false, false, true, true, false,
	false, true, true, false, false, false,
	false, false, false, true, true, false,
	false, true, true, false, false, false,
};

FVector2D ParameterisedProfile::GetPoint(int idx) const {
	const auto quarter = GetQuarterIdx(idx);

	float x{ Width / 2 };
	float y{ 0.5f };

	if (UsesBarrierHeight[idx])
		y += BarrierHeights[quarter];

	if (IsXOuter[idx] && BarrierHeights[quarter] > 0)
		x += 1;

	if (UsesOverhangWidth[idx])
		x -= OverhangWidths[quarter];

	if (IsYOuter[idx] && OverhangWidths[quarter])
		y += 1;

	if (quarter > 1)
		x = -x;

	if (quarter == 1 || quarter == 2)
		y = -y;

	return FVector2D{ x, y };
}

FVector ParameterisedProfile::GetTransformedVert(int vert_idx, const FTransform & trans) const
{
	const auto point = GetPoint(vert_idx);

	// an untransformed connector faces down X and it upright w.r.t Z
	// so its width (internal X) is mapped onto Y and its height (internal Y) onto Z
	FVector temp{ 0.0f, point.X, point.Y };

	return trans.TransformPosition(temp);
}

FVector ParameterisedProfile::GetTransformedVert(VertTypes type, int quarter_idx, const FTransform & trans) const
{
	int idx = quarter_idx * VertsPerQuarter;

	if (quarter_idx & 1)
	{
		idx = idx + 5 - (int)type;
	}
	else
	{
		idx = idx + (int)type;
	}

	return GetTransformedVert(idx, trans);
}



