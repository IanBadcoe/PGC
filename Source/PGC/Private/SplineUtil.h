#pragma once

class SplineUtil {
public:
	static FVector Hermite(float t, FVector P1, FVector P2, FVector P1Dir, FVector P2Dir) {
		float t2 = t * t;
		float t3 = t * t * t;

		float h1 = 2 * t3 - 3 * t2 + 1;
		float h2 = -2 * t3 + 3 * t2;
		float h3 = t3 - 2 * t2 + t;
		float h4 = t3 - t2;

		return h1 * P1 + h2 * P2 + h3 * P1Dir + h4 * P2Dir;
	}

	// NOT unit length...
	static FVector HermiteTangent(float t, FVector P1, FVector P2, FVector P1Dir, FVector P2Dir) {
		float t2 = t * t;
		float t3 = t * t * t;

		float h1 = 6 * t2 - 6 * t;
		float h2 = -6 * t2 + 6 * t;
		float h3 = 3 * t2 - 4 * t + 1;
		float h4 = 3 * t2 - 2 * t;

		return h1 * P1 + h2 * P2 + h3 * P1Dir + h4 * P2Dir;
	}

	static FVector CubicBezier(float t, FVector P0, FVector P1, FVector P2, FVector P3) {
		float omt = 1 - t;

		return omt * omt * omt * P0
			+ 3 * t * omt * omt * P1
			+ 3 * t * t * omt * P2
			+ t * t * t * P3;
	}

	// NOT unit length...
	static FVector CubicBezierTangent(float t, FVector P0, FVector P1, FVector P2, FVector P3) {
		float omt = 1 - t;

		return -3 * P0 * omt * omt
			+ P1 * (3 * omt * omt - 6 * omt * t)
			+ P2 * (6 * omt * t - 3 * t * t)
			+ 3 * P3 * t * t;
	}
};


