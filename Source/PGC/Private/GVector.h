#pragma once

#include "Runtime/Core/Public/Math/Vector.h"

// a clone of UE4's FVector, but double-precision, and with only the bits I have needed filled in...
class GVector
{
public:
	double X;
	double Y;
	double Z;

	GVector();
	explicit GVector(const FVector& fv) : X(fv.X), Y(fv.Y), Z(fv.Z) {}
	GVector(double x, double y, double z) : X(x), Y(y), Z(z) {}

	~GVector();

	GVector operator-(const GVector& rhs) const {
		return GVector{ X - rhs.X, Y - rhs.Y, Z - rhs.Z, };
	}

	GVector operator+(const GVector& rhs) const {
		return GVector{ X + rhs.X, Y + rhs.Y, Z + rhs.Z, };
	}

	GVector operator*(double rhs) const {
		return GVector{ X * rhs, Y * rhs, Z * rhs, };
	}

	GVector operator/(double rhs) const {
		return GVector{ X / rhs, Y / rhs, Z / rhs, };
	}

	double SizeSquared() const {
		return X * X + Y * Y + Z * Z;
	}

	double Size() const {
		return sqrt(SizeSquared());
	}

	static double DotProduct(const GVector& a, const GVector& b) {
		return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
	}

	GVector GetSafeNormal() const {
		const auto SquareSum = X * X + Y * Y + Z * Z;

		// Not sure if it's safe to add tolerance in there. Might introduce too many errors
		if (SquareSum == 1.f)
		{
			return *this;
		}
		else if (SquareSum < 1e-12)
		{
			return ZeroVector;
		}

		const auto Scale = sqrt(SquareSum);
		return GVector(X*Scale, Y*Scale, Z*Scale);
	}

	bool IsNormalized() const {
		return (fabs(1.0 - SizeSquared()) < 0.001);
	}

	static const GVector ZeroVector;
};

