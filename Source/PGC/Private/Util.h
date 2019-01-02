#pragma once

#include "Mesh.h"

namespace Util {

// temp util to gloss over awkwardness of UVs and edge-types
inline void AddPolyToMesh(TSharedPtr<Mesh> mesh, const TArray<FVector>& verts)
{
	TArray<FVector2D> UVs;
	TArray<PGCEdgeType> EdgeTypes;

	for (int i = 0; i < verts.Num(); i++)
	{
		UVs.Add(FVector2D(0, 0));
		EdgeTypes.Add(PGCEdgeType::Rounded);
	}

	mesh->AddFaceFromVects(verts, UVs, 0, EdgeTypes);
}

// we expect forward and up to be set, right then derives from those
inline void MakeAxisSet(FVector& forward, FVector& right, FVector& up)
{
	forward.Normalize();

	right = FVector::CrossProduct(forward, up);

	right.Normalize();

	up = FVector::CrossProduct(right, forward);

	up.Normalize();
}

inline FTransform MakeTransform(const FVector& pos, FVector local_up, FVector forward)
{
	FVector right;

	MakeAxisSet(forward, right, local_up);

	return FTransform(forward, right, local_up, pos);
}

}