#pragma once

#include "Mesh.h"
#include "GVector.h"

#include "NlOptWrapper.h"


PRAGMA_DISABLE_OPTIMIZATION

namespace Util {

// temp util to gloss over awkwardness of UVs and edge-types
inline void AddPolyToMesh(TSharedPtr<Mesh> mesh, TArray<FVector> verts, TArray<PGCEdgeType> edge_types, int channel)
{
	check(verts.Num() == edge_types.Num());

	auto prev_vert = verts.Last();
	auto& prev_edge_type = edge_types.Last();

	// parameterized profiles can generate degenerate edges, remove any duplicated verts
	for (int i = 0; i < verts.Num();)
	{
		if (verts[i] == prev_vert)
		{
			verts.RemoveAt(i);

			if (edge_types[i] == PGCEdgeType::Sharp || prev_edge_type == PGCEdgeType::Sharp)
			{
				// is this right?  my thinking is that if either of the two edges was sharp, then
				// the merged one should be (consider if this is the end of a wall, the wall
				// top corners should not go soft just because they are merging back into the floor...
				prev_edge_type = PGCEdgeType::Sharp;
			}

			edge_types.RemoveAt(i);
		}
		else
		{
			prev_vert = verts[i];
			i++;
		}
	}

	// 	if there are not enough unique verts left, ignore this face (it will lie in some other face's edge)
	if (verts.Num() < 3)
		return;

	TArray<FVector2D> UVs;

	for (int i = 0; i < verts.Num(); i++)
	{
		UVs.Add(FVector2D(0, 0));
	}

	mesh->AddFaceFromVects(verts, UVs, 0, edge_types, channel);
}

// temp util to gloss over awkwardness of UVs and edge-types
inline void AddPolyToMeshReversed(TSharedPtr<Mesh> mesh, const TArray<FVector>& verts, const TArray<PGCEdgeType> edge_types, int channel)
{
	TArray<FVector> verts_reversed;
	TArray<PGCEdgeType> edge_types_reversed;

	const auto sz = verts.Num();

	for (int i = sz - 1; i >= 0; i--)
	{
		verts_reversed.Push(verts[i]);
		// because edge_types are for the following edge, when we reverse the verts
		// the "following" edge is what was the preceding one
		// and the edge types move by one
		edge_types_reversed.Push(edge_types[(i - 1 + sz) % sz]);
	}

	AddPolyToMesh(mesh, verts_reversed, edge_types_reversed, channel);
}

inline void AddPolyToMesh(TSharedPtr<Mesh> mesh, TArray<FVector> verts, PGCEdgeType all_edge_types, int channel)
{
	TArray<PGCEdgeType> edge_types;

	for (const auto& vert: verts)
	{
		edge_types.Push(all_edge_types);
	}

	return AddPolyToMesh(mesh, verts, edge_types, channel);
}

inline void AddPolyToMeshReversed(TSharedPtr<Mesh> mesh, TArray<FVector> verts, PGCEdgeType all_edge_types, int channel)
{
	TArray<PGCEdgeType> edge_types;

	for (const auto& vert : verts)
	{
		edge_types.Push(all_edge_types);
	}

	return AddPolyToMeshReversed(mesh, verts, edge_types, channel);
}

// we expect forward and up to be set, right then derives from those
inline void MakeAxisSet(FVector& forward, FVector& right, FVector& up)
{
	forward.Normalize();

	right = FVector::CrossProduct(up, forward);

	right.Normalize();

	up = FVector::CrossProduct(forward, right);

	up.Normalize();
}

inline FTransform MakeTransform(const FVector& pos, FVector local_up, FVector forward)
{
	FVector right;

	MakeAxisSet(forward, right, local_up);

	auto ret = FTransform(forward, right, local_up, pos);

	check(FVector::Dist(ret.GetScale3D(), FVector(1, 1, 1)) < 1.0E-6);

	return ret;
}

inline FVector ProjectOntoPlane(const FVector& vect, const FVector& plane_normal)
{
	check(plane_normal.IsUnit());

	auto proj_dist = FVector::DotProduct(plane_normal, vect);

	return vect - proj_dist * plane_normal;
}

// measures the angle to "to" from "from" about the axis axis...
// projects "to" and "from" into the axis plane,
//
// return in radians, looking down "axis" clockwise is a +ve angle
//
// keep_positive:
//   true: outputs 0 -> 2PI
//   false: outputs -PI -> PI
inline float SignedAngle(const FVector& from, const FVector& to, const FVector& axis, bool keep_positive)
{
	auto from_proj_norm = ProjectOntoPlane(from, axis).GetSafeNormal();
	auto to_proj_norm = ProjectOntoPlane(to, axis).GetSafeNormal();

	auto angle = FMath::Acos(FVector::DotProduct(from_proj_norm, to_proj_norm));

	auto sign_check = FVector::CrossProduct(from_proj_norm, to_proj_norm);

	if (FVector::DotProduct(sign_check, axis) < 0)
	{
		angle = -angle;
	}

	if (keep_positive && angle < 0)
	{
		angle += 2 * PI;
	}

	return angle;
}

inline FQuat AxisAngleToQuaternion(FVector Axis, float Angle)
{
	FQuat q;

	q.X = Axis.X * sin(Angle / 2);
	q.Y = Axis.Y * sin(Angle / 2);
	q.Z = Axis.Z * sin(Angle / 2);
	q.W = cos(Angle / 2);

	return q;
}

inline FVector NewellPolyNormal(const TArray<FVector>& verts)
{
	auto prev_vert = verts.Last();
	FVector sum{ 0, 0, 0 };

	for (const auto& vert : verts)
	{
		sum += FVector::CrossProduct(prev_vert, vert);

		prev_vert = vert;
	}

	return sum.GetSafeNormal();
}

// modified from : http://geomalgorithms.com/a07-_distance.html
// dist3D_Segment_to_Segment(): get the 3D minimum distance between 2 segments
//    Input:  two 3D line segments S1 and S2
//    Return: the shortest distance between S1 and S2
double
dist3D_Segment_to_Segment(GVector P0, GVector P1, GVector Q0, GVector Q1)
{
	GVector   u = P1 - P0;
	GVector   v = Q1 - Q0;
	GVector   w = P0 - Q0;
	auto      a = GVector::DotProduct(u, u);         // always >= 0
	auto      b = GVector::DotProduct(u, v);
	auto      c = GVector::DotProduct(v, v);         // always >= 0
	auto      d = GVector::DotProduct(u, w);
	auto      e = GVector::DotProduct(v, w);
	auto      D = a * c - b * b;        // always >= 0
	double    sc, sN, sD = D;       // sc = sN / sD, default sD = D >= 0
	double    tc, tN, tD = D;       // tc = tN / tD, default tD = D >= 0

	static const auto SMALL_NUM = 1E-6;

	// compute the line parameters of the two closest points
	if (D < SMALL_NUM) { // the lines are almost parallel
		sN = 0.0;         // force using point P0 on segment S1
		sD = 1.0;         // to prevent possible division by 0.0 later
		tN = e;
		tD = c;
	}
	else {                 // get the closest points on the infinite lines
		sN = (b*e - c * d);
		tN = (a*e - b * d);
		if (sN < 0.0) {        // sc < 0 => the s=0 edge is visible
			sN = 0.0;
			tN = e;
			tD = c;
		}
		else if (sN > sD) {  // sc > 1  => the s=1 edge is visible
			sN = sD;
			tN = e + b;
			tD = c;
		}
	}

	if (tN < 0.0) {            // tc < 0 => the t=0 edge is visible
		tN = 0.0;
		// recompute sc for this edge
		if (-d < 0.0)
			sN = 0.0;
		else if (-d > a)
			sN = sD;
		else {
			sN = -d;
			sD = a;
		}
	}
	else if (tN > tD) {      // tc > 1  => the t=1 edge is visible
		tN = tD;
		// recompute sc for this edge
		if ((-d + b) < 0.0)
			sN = 0;
		else if ((-d + b) > a)
			sN = sD;
		else {
			sN = (-d + b);
			sD = a;
		}
	}
	// finally do the division to get sc and tc
	sc = (abs(sN) < SMALL_NUM ? 0.0 : sN / sD);
	tc = (abs(tN) < SMALL_NUM ? 0.0 : tN / tD);

	// get the difference of the two closest points
	GVector   dP = w + (u * sc) - (v * tc);  // =  S1(sc) - S2(tc)

	return dP.Size();   // return the closest distance
}

struct ParamPair {
	double s;
	double t;
};

inline double ParametricRayPointDiff(
	const GVector& P0, const GVector& u, double s,
	const GVector& Q0, const GVector& v, double t)
{
	auto p = P0 + u * s;
	auto q = Q0 + v * t;
	return (p - q).Size();
}

template <typename T>
T Clamp(T val, T min, T max) {
	return val < min ? min : val > max ? max : val;
}

inline ParamPair FindSegSegClosestPointParamsSlow(const GVector& P0, const GVector& u, const GVector& Q0, const GVector& v)
{
	struct OF : NlOptIface {
		GVector P0;
		GVector u;
		GVector Q0;
		GVector v;

		double s = 0;
		double t = 0;
	public:
		virtual ~OF() {}

		virtual double f(int n, const double* x, double* grad) override {
			return ParametricRayPointDiff(P0, u, x[0], Q0, v, x[1]);
		}
		virtual int GetSize() const override { return 2; }
		virtual void GetInitialStepSize(double* steps, int n) const override {
			steps[0] = 0.1;
			steps[1] = 0.1;
		}
		virtual void GetLimits(double* lower, double* upper, int n) const override {
			lower[0] = 0;
			lower[1] = 0;

			upper[0] = 1;
			upper[1] = 1;
		}
		virtual void GetState(double* x, int n) const override {
			x[0] = s;
			x[1] = t;
		}
		virtual void SetState(const double* x, int n) override {
			s = x[0];
			t = x[1];
		}
		virtual TArray<FString> GetEnergyTermNames() const override
		{
			return {};
		}
		virtual TArray<double> GetLastEnergyTerms() const override {
			return {};
		}
	};

	auto of = MakeShared<OF>();

	of->P0 = P0;
	of->u = u;
	of->Q0 = Q0;
	of->v = v;

	NlOptWrapper nlow(of);

	// need extra precision for this to work...
	nlow.RunOptimization(true, -1, 1e-20, nullptr);

	return { (float)of->s, (float)of->t };
}

// find the separation between two line segments, each segment defined by a point at either end
inline double SegmentSegmentDistance(const GVector& P0, const GVector& P1, const GVector& Q0, const GVector& Q1)
{
	auto dist = dist3D_Segment_to_Segment(P0, P1, Q0, Q1);

#if 0
	auto u = P1 - P0;
	auto v = Q1 - Q0;

	auto check_params = FindSegSegClosestPointParamsSlow(P0, u, Q0, v);

	// we can't check distance at the found params
	// because for very-nearly parallel rays the optimizing approach can shoot off towards infinity and find a very close
	// result, but the algebraic solution will notice they are parallel and approximate to avoid numerical instability
	// so instead let's clamp both to the segment range before checking

	// can't check the returned params, because for parallel rays they underdetermined...
	// but can check we get points at the same separation...

	auto check_p = P0 + u * check_params.s;
	auto check_q = Q0 + v * check_params.t;

	auto check_dist = (check_p - check_q).Size();

	check(FMath::Abs(dist - check_dist) < 2e-4);
#endif

	return dist;
}

}

PRAGMA_ENABLE_OPTIMIZATION