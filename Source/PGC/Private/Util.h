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

}