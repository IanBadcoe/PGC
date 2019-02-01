#include "ParameterisedProfile.h"

PRAGMA_DISABLE_OPTIMIZATION

using namespace Profile;

ParameterisedProfile::ParameterisedProfile(float width,
	const TArray<float> barriers,
	const TArray<float> overhangs,
	const TArray<bool> outgoingSharps)
	: Width{ width },
	  BarrierHeights(barriers),
	  OverhangWidths(overhangs),
	  OutgoingSharp(outgoingSharps),
	  AbsoluteBound{ 0, 0 }
{
	check(barriers.Num() == 4);
	check(overhangs.Num() == 4);
	check(outgoingSharps.Num() == 24);

	CalcAbsoluteBound();

	CheckConsistent();
}

ParameterisedProfile::ParameterisedProfile(float width, const TSharedPtr<ParameterisedRoadbedShape>& top, const TSharedPtr<ParameterisedRoadbedShape>& bottom)
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

float Profile::ParameterisedProfile::Diff(const TSharedPtr<ParameterisedProfile>& other) const
{
	auto ret = 0.0f;

	for (int i = 0; i < NumVerts; i++)
	{
		ret += (this->GetPoint(i) - other->GetPoint(i)).Size();
	}

	return ret;
}

static inline float interp(float from, float to, float frac)
{
	check(frac >= 0 && frac <= 1);

	return from + (to - from) * frac;
}

TSharedPtr<ParameterisedProfile> Profile::ParameterisedProfile::Interp(TSharedPtr<ParameterisedProfile> other, float frac) const
{
	for (int i = 0; i < 4; i++)
	{
		if ((OverhangWidths[i] == 0) != (other->OverhangWidths[i] == 0))
		{
			if (frac < 0.5)
			{
				return RawInterp(SafeIntermediate(other), frac * 2);
			}

			return SafeIntermediate(other)->RawInterp(other, frac * 2 - 1);
		}
	}

	return RawInterp(other, frac);
}

TSharedPtr<ParameterisedProfile> Profile::ParameterisedProfile::RawInterp(TSharedPtr<ParameterisedProfile> other, float frac) const
{
	return MakeShared<ParameterisedProfile>(
		interp(Width, other->Width, frac),
		TArray<float> {
			interp(BarrierHeights[0], other->BarrierHeights[0], frac),
			interp(BarrierHeights[1], other->BarrierHeights[1], frac),
			interp(BarrierHeights[2], other->BarrierHeights[2], frac),
			interp(BarrierHeights[3], other->BarrierHeights[3], frac),
		},
		TArray<float> {
			interp(OverhangWidths[0], other->OverhangWidths[0], frac),
			interp(OverhangWidths[1], other->OverhangWidths[1], frac),
			interp(OverhangWidths[2], other->OverhangWidths[2], frac),
			interp(OverhangWidths[3], other->OverhangWidths[3], frac),
		},
		TArray<bool>(frac < 0.5f ? OutgoingSharp : other->OutgoingSharp)
	);
}

TSharedPtr<ParameterisedProfile> Profile::ParameterisedProfile::SafeIntermediate(TSharedPtr<ParameterisedProfile> other) const
{
	return MakeShared<ParameterisedProfile>(
		(Width + other->Width) / 2,
		TArray<float> {
			FMath::Max(BarrierHeights[0], other->BarrierHeights[0]),
			FMath::Max(BarrierHeights[1], other->BarrierHeights[1]),
			FMath::Max(BarrierHeights[2], other->BarrierHeights[2]),
			FMath::Max(BarrierHeights[3], other->BarrierHeights[3]),
		},
		TArray<float> {
			FMath::Min(OverhangWidths[0], other->OverhangWidths[0]),
			FMath::Min(OverhangWidths[1], other->OverhangWidths[1]),
			FMath::Min(OverhangWidths[2], other->OverhangWidths[2]),
			FMath::Min(OverhangWidths[3], other->OverhangWidths[3]),
		},
		TArray<bool>(other->OutgoingSharp)
	);
}

PRAGMA_ENABLE_OPTIMIZATION