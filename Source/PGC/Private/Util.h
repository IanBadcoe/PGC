#pragma once

#include "Mesh.h"

#pragma optimize ("", off)

namespace Util {

// temp util to gloss over awkwardness of UVs and edge-types
inline void AddPolyToMesh(TSharedPtr<Mesh> mesh, TArray<FVector> verts)
{
	auto prev_vert = verts.Last();

	// parameterized profiles can generate degenerate edges, remove any duplicated verts
	for (int i = 0; i < verts.Num();)
	{
		if (verts[i] == prev_vert)
		{
			verts.RemoveAt(i);
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
	TArray<PGCEdgeType> EdgeTypes;

	for (int i = 0; i < verts.Num(); i++)
	{
		UVs.Add(FVector2D(0, 0));
		EdgeTypes.Add(PGCEdgeType::Rounded);
	}

	mesh->AddFaceFromVects(verts, UVs, 0, EdgeTypes);
}

// temp util to gloss over awkwardness of UVs and edge-types
inline void AddPolyToMeshReversed(TSharedPtr<Mesh> mesh, const TArray<FVector>& verts)
{
	TArray<FVector> reversed;

	for (int i = verts.Num() - 1; i >= 0; i--)
	{
		reversed.Push(verts[i]);
	}

	AddPolyToMesh(mesh, reversed);
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
inline float SignedAngle(const FVector& from, const FVector& to, const FVector& axis)
{
	auto from_proj_norm = ProjectOntoPlane(from, axis).GetSafeNormal();
	auto to_proj_norm = ProjectOntoPlane(to, axis).GetSafeNormal();

	auto angle = FMath::Acos(FVector::DotProduct(from_proj_norm, to_proj_norm));

	auto sign_check = FVector::CrossProduct(from_proj_norm, to_proj_norm);

	if (FVector::DotProduct(sign_check, axis) < 0)
	{
		angle = -angle;
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

}

#pragma optimize ("", on)
