#pragma once

#include "Runtime/Core/Public/Math/Vector.h"

// a clone of UE4's FVector, but double-precision, and with only the bits I have needed filled in...
class GVector
{
	double X;
	double Y;
	double Z;

public:
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

	double Size() const {
		return sqrt(X * X + Y * Y + Z * Z);
	}

	static double DotProduct(const GVector& a, const GVector& b) {
		return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
	}
};

