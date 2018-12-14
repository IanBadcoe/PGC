#include "LayoutGraph.h"

#pragma optimize ("", off)

void LayoutGraph::Graph::MakeMesh(TSharedPtr<Mesh> mesh) const
{
	for (const auto& node : Nodes)
	{
		node->AddToMesh(mesh);
	}
}

// temp util to gloss over awkwardness of UVs and edge-types
static void AddPolyToMesh(TSharedPtr<Mesh> mesh, const TArray<FVector>& verts)
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

void LayoutGraph::Node::AddToMesh(TSharedPtr<Mesh> mesh)
{
	for (const auto& pol : Polygons)
	{
		TArray<FVector> verts;

		for (const auto& idx : pol.VertexIndices)
		{
			verts.Add(GetTransformedVert(idx));
		}

		AddPolyToMesh(mesh, verts);
	}

	// fill-in any conntectors that aren't connected
	for (int i = 0; i < Connectors.Num(); i++)
	{
		if (!Edges[i].IsValid())
		{
			TArray<FVector> verts;
	
			for (int j = 0; j < Connectors[i].Definition.Profile.Num(); j++)
			{
				verts.Add(GetTransformedVert({i, j}));
			}

			AddPolyToMesh(mesh, verts);
		}
	}
}

FVector LayoutGraph::Node::GetTransformedVert(const Polygon::Idx& idx)
{
	if (idx.Connector == -1)
	{
		return Position.TransformPosition(Vertices[idx.Vertex]);
	}
	else
	{
		const ConnectorDef& def = Connectors[idx.Connector].Definition;

		// an untransformed connector faces down X and it upright w.r.t Z
		// so its width (internal X) is mapped onto Y and its height (internal Y) onto Z
		FVector temp{ 0.0f, def.Profile[idx.Vertex].X, def.Profile[idx.Vertex].Y };

		FTransform tot_trans = Connectors[idx.Connector].Transform * Position;

		//FVector temp2 = Connectors[idx.Connector].Transform.TransformPosition(temp);

		//FVector temp3 = Position.TransformVector(temp2);

		return tot_trans.TransformPosition(temp);
	}
}

inline LayoutGraph::ConnectorInst::ConnectorInst(const ConnectorDef& definition, const FVector& pos, const FVector& normal, const FVector& up)
	: Definition(definition)
{
	FVector right = FVector::CrossProduct(up, normal);

	// untransformed has the normal on X, the right on Y and Z up...
	Transform = FTransform(normal, right, up, pos);
}

#pragma optimize ("", on)
