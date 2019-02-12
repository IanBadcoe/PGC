// Fill out your copyright notice in the Description page of Project Settings.

#include "Mesh.h"

#include "PGCCube.h"

#include "Runtime/Core/Public/Templates/UniquePtr.h"
#include "Runtime/Engine/Classes/Engine/World.h"

PRAGMA_DISABLE_OPTIMIZATION

template <typename T>
const Idx<T> Idx<T>::None{ -1 };

Idx<MeshEdge> Mesh::FindEdge(Idx<MeshVert> vert_idx1, Idx<MeshVert> vert_idx2, Idx<MeshFace> face_idx) const
{
	for (Idx<MeshEdge> i{ 0 }; i < Edges.Num(); i++)
	{
		auto& edge = Edges[i];

		if (edge.Contains(vert_idx1) && edge.Contains(vert_idx2) && edge.Contains(face_idx))
		{
			return i;
		}
	}

	return Idx<MeshEdge>::None;
}

TArray<Idx<MeshEdge>> Mesh::FindAllEdges(Idx<MeshVert> vert_idx1, Idx<MeshVert> vert_idx2) const
{
	TArray<Idx<MeshEdge>> ret;

	for (Idx<MeshEdge> i{ 0 }; i < Edges.Num(); i++)
	{
		auto& edge = Edges[i];

		if (edge.Contains(vert_idx1) && edge.Contains(vert_idx2))
		{
			ret.Push(i);
		}
	}

	return ret;
}

Idx<MeshEdge> Mesh::FindEdge(Idx<MeshVert> vert_idx1, Idx<MeshVert> vert_idx2, bool partial_only) const
{
	for (Idx<MeshEdge> i{ 0 }; i < Edges.Num(); i++)
	{
		auto& edge = Edges[i];

		if (edge.Contains(vert_idx1) && edge.Contains(vert_idx2) && (!partial_only || edge.Contains(Idx<MeshFace>::None)))
		{
			return i;
		}
	}

	return Idx<MeshEdge>::None;
}

Idx<MeshEdge> Mesh::AddFindEdge(Idx<MeshVert> idx1, Idx<MeshVert> idx2)
{
	// do we ever need to propagate the "partial_only" parameter of FindEdge up to the caller?  Not yet...
	auto edge_idx = FindEdge(idx1, idx2, true);

	if (edge_idx != Idx<MeshEdge>::None)
	{
		return edge_idx;
	}

	MeshEdge ne{ ne.StartVertIdx = idx1, ne.EndVertIdx = idx2 };

	Edges.Push(ne);

	Vertices[idx1].EdgeIdxs.Push(Idx<MeshEdge>(Edges.LastIdx()));
	Vertices[idx2].EdgeIdxs.Push(Idx<MeshEdge>(Edges.LastIdx()));

	return Edges.LastIdx();
}

Idx<MeshVert> Mesh::FindVert(const FVector & pos) const
{
	for (Idx<MeshVert> i{ 0 }; i < Vertices.Num(); i++)
	{
		auto& vert = Vertices[i];

		if (vert.Pos == pos)
		{
			return i;
		}
	}

	return Idx<MeshVert>::None;
}

Idx<MeshVert> Mesh::FindVert(const MeshVertRaw& mvr, int UVGroup) const
{
	for (Idx<MeshVert> i{ 0 }; i < Vertices.Num(); i++)
	{
		auto& vert = Vertices[i];

		// ToMeshVertRaw explodes if vert doesn't know about this UVGroup
		if (vert.UVs.Contains(UVGroup) && vert.ToMeshVertRaw(UVGroup).ToleranceCompare(mvr, VertexTolerance))
		{
			return i;
		}
	}

	return Idx<MeshVert>::None;
}

// a closed mesh has no holes, an unclosed on may have faces not added yet...
void Mesh::CheckConsistent(bool closed)
{
	for (Idx<MeshEdge> edge_idx{ 0 }; edge_idx < Edges.Num(); edge_idx++)
	{
		auto e = Edges[edge_idx];

		check(e.StartVertIdx.Valid() && e.StartVertIdx < Vertices.Num());
		check(e.EndVertIdx.Valid() && e.EndVertIdx < Vertices.Num());
		check(e.ForwardFaceIdx < Faces.Num());
		check(e.BackwardsFaceIdx < Faces.Num());
		// we should have both edges if we're closed
		// (redundant edges should have been removed...)
		check(!closed || e.ForwardFaceIdx.Valid());
		check(!closed || e.BackwardsFaceIdx.Valid());
		check(Vertices[e.StartVertIdx].EdgeIdxs.Contains(edge_idx));
		check(Vertices[e.EndVertIdx].EdgeIdxs.Contains(edge_idx));
		check(!e.ForwardFaceIdx.Valid() || Faces[e.ForwardFaceIdx].EdgeIdxs.Contains(edge_idx));
		check(!e.BackwardsFaceIdx.Valid() || Faces[e.BackwardsFaceIdx].EdgeIdxs.Contains(edge_idx));
	}

	for (Idx<MeshVert> vert_idx{ 0 }; vert_idx < Vertices.Num(); vert_idx++)
	{
		auto v = Vertices[vert_idx];

		// we expect one-to-one for edges and faces (true for closed meshes...)
		check(!closed || v.FaceIdxs.Num() == v.EdgeIdxs.Num());

		// if we know about an edge, it should know about us
		for (auto edge_idx : v.EdgeIdxs)
		{
			check(Edges[edge_idx].Contains(vert_idx));
		}

		// if we know about a face, it should know about us
		for (auto face_idx : v.FaceIdxs)
		{
			check(Faces[face_idx].VertIdxs.Contains(vert_idx));
		}
	}

	for (Idx<MeshFace> face_idx{ 0 }; face_idx < Faces.Num(); face_idx++)
	{
		auto& face = Faces[face_idx];

		check(face.VertsAreRegular());

		auto prev_vert_idx = face.VertIdxs.Last();

		for (auto vert_idx : face.VertIdxs)
		{
			check(vert_idx.Valid() && vert_idx < Vertices.Num());

			auto edge_idx = FindEdge(prev_vert_idx, vert_idx, face_idx);
			check(edge_idx != Idx<MeshEdge>::None);

			auto& edge = Edges[edge_idx];

			// should have us as a face on one or the other side
			check(edge.Contains(face_idx));

			// if the edge starts with our previous vert, then we are the forward face of this edge,
			// otherwise we are its backwards face
			check((edge.StartVertIdx == prev_vert_idx) == (edge.ForwardFaceIdx == face_idx));
			check((edge.EndVertIdx == prev_vert_idx) == (edge.BackwardsFaceIdx == face_idx));

			auto v = Vertices[vert_idx];

			check(v.FaceIdxs.Contains(face_idx));

			prev_vert_idx = vert_idx;
		}
	}

}

#ifndef UE_BUILD_RELEASE

TArray<TArray<FVector>> working_configs{
	{							// one cube
		{0, 0, 0}
	},
	{							// face contact
		{0, 0, 0},
		{0, 0, 1},
	},
	{							// edge contact
		{0, 0, 0},
		{0, 1, 1},
	},
	{							// edge contact then fill-in to remerge into one object
		{0, 0, 0},
		{0, 1, 1},
		{0, 1, 0},
	},
	{							// corner contact
		{0, 0, 0},
		{1, 1, 1},
	},
	{							// three in a row
		{0, 0, 0},
		{0, 0, 1},
		{0, 0, 2},
	},
	{							// L shape
		{0, 0, 0},
		{0, 0, 1},
		{0, 1, 1},
	},
	{							// square
		{0, 0, 0},
		{0, 0, 1},
		{0, 1, 1},
		{0, 1, 0},
	},
	{							// 3x3 supplying cubes in an easy order
		{0, 0, 0},
		{0, 0, 1},
		{0, 0, 2},
		{0, 1, 0},
		{0, 1, 1},
		{0, 1, 2},
		{0, 2, 0},
		{0, 2, 1},
		{0, 2, 2},
		{1, 0, 0},
		{1, 0, 1},
		{1, 0, 2},
		{1, 1, 0},
		{1, 1, 1},
		{1, 1, 2},
		{1, 2, 0},
		{1, 2, 1},
		{1, 2, 2},
		{2, 0, 0},
		{2, 0, 1},
		{2, 0, 2},
		{2, 1, 0},
		{2, 1, 1},
		{2, 1, 2},
		{2, 2, 0},
		{2, 2, 1},
		{2, 2, 2},
	},
	{							// 3x3 supplying middle cube last
		{0, 0, 0},
		{0, 0, 1},
		{0, 0, 2},
		{0, 1, 0},
		{0, 1, 1},
		{0, 1, 2},
		{0, 2, 0},
		{0, 2, 1},
		{0, 2, 2},
		{1, 0, 0},
		{1, 0, 1},
		{1, 0, 2},
		{1, 1, 0},
		{1, 1, 2},
		{1, 2, 0},
		{1, 2, 1},
		{1, 2, 2},
		{2, 0, 0},
		{2, 0, 1},
		{2, 0, 2},
		{2, 1, 0},
		{2, 1, 1},
		{2, 1, 2},
		{2, 2, 0},
		{2, 2, 1},
		{2, 2, 2},
		{1, 1, 1},
	},
};

PGCEdgeId edge_test[]{
	PGCEdgeId::BackLeft,
	PGCEdgeId::BackRight,
	PGCEdgeId::FrontLeft,
	PGCEdgeId::FrontRight,
	PGCEdgeId::TopBack,
	PGCEdgeId::TopFront,
	PGCEdgeId::TopLeft,
	PGCEdgeId::TopRight,
	PGCEdgeId::BottomBack,
	PGCEdgeId::BottomFront,
	PGCEdgeId::BottomLeft,
	PGCEdgeId::BottomRight,
};

static void TestOne(const TArray<FVector>& config, int x_from, int y_from, int z_from, bool mirror)
{
	auto mesh = MakeShared<Mesh>(FMath::Cos(FMath::DegreesToRadians(20.0f)));

	int neg = mirror ? -1 : 1;

	for (auto cell : config)
	{
		mesh->AddCube(FPGCCube(cell[x_from] * neg, cell[y_from] * neg, cell[z_from] * neg));
	}

	auto div1 = mesh->Subdivide();
	auto div2 = div1->Subdivide();
}

void Mesh::UnitTest()
{
	for (auto edge_id : edge_test)
	{
		auto mesh = MakeShared<Mesh>(FMath::Cos(FMath::DegreesToRadians(20.0f)));

		FPGCCube cube;
		cube.EdgeTypes[(int)edge_id] = PGCEdgeType::Sharp;

		mesh->AddCube(cube);

		int count_sharp = 0;

		for (const auto& edge : mesh->Edges)
		{
			if (edge.SetType == PGCEdgeType::Sharp)
				count_sharp++;
		}

		check(count_sharp == 1);
	}

	for(auto config : working_configs)
	{
		TestOne(config, 0, 1, 2, false);
		TestOne(config, 0, 2, 1, false);
		TestOne(config, 1, 0, 2, false);
		TestOne(config, 1, 2, 0, false);
		TestOne(config, 2, 0, 1, false);
		TestOne(config, 2, 1, 0, false);

		TestOne(config, 0, 2, 1, true);
		TestOne(config, 0, 1, 2, true);
		TestOne(config, 1, 0, 2, true);
		TestOne(config, 1, 2, 0, true);
		TestOne(config, 2, 0, 1, true);
		TestOne(config, 2, 1, 0, true);
	}
}

#endif

Idx<MeshVert> Mesh::AddVert(MeshVertRaw vert, int UVGRoup)
{
	auto vert_idx = FindVert(vert, UVGRoup);

	// we have it with this UV already set
	if (vert_idx.Valid())
		return vert_idx;

	// do we have it without the UV?
	vert_idx = FindVert(vert.Pos);

	if (!vert_idx.Valid())
	{
		// no, add it
		vert_idx = Vertices.Num();
		MeshVert mv;
		mv.Pos = vert.Pos;
		Vertices.Push(mv);
	}

	// set the UV into it
	Vertices[vert_idx].UVs.Add(UVGRoup) = vert.UV;

	return vert_idx;
}

Idx<MeshVert> Mesh::AddVert(FVector pos)
{
	auto vert_idx = FindVert(pos);

	// we have it with this UV already set
	if (vert_idx.Valid())
		return vert_idx;

	MeshVert mv;
	mv.Pos = pos;
	Vertices.Push(mv);

	return Vertices.LastIdx();
}

Idx<MeshFace> Mesh::FindFaceByVertIdxs(const TArray<Idx<MeshVert>>& vert_idxs) const
{
	for (Idx<MeshFace> face_idx{ 0 }; face_idx < Faces.Num(); face_idx++)
	{
		auto mf = Faces[face_idx];

		if (mf.VertIdxs == vert_idxs)
		{
			return face_idx;
		}
	}

	return Idx<MeshFace>::None;
}

void Mesh::RemoveFace(Idx<MeshFace> face_idx)
{
	for (auto& v : Vertices)
	{
		v.FaceIdxs.Remove(face_idx);

		for(auto& idx : v.FaceIdxs)
		{
			if (idx > face_idx)
			{
				idx--;
			}
		}
	}

	for (auto& e : Edges)
	{
		if (e.ForwardFaceIdx == face_idx)
		{
			e.ForwardFaceIdx = Idx<MeshFace>::None;
		}
		else if (e.ForwardFaceIdx > face_idx)
		{
			e.ForwardFaceIdx--;
		}

		if (e.BackwardsFaceIdx== face_idx)
		{
			e.BackwardsFaceIdx = Idx<MeshFace>::None;
		}
		else if (e.BackwardsFaceIdx > face_idx)
		{
			e.BackwardsFaceIdx--;
		}
	}

	Faces.RemoveAt(face_idx);

	MergePartialEdges();

	CleanUpRedundantEdges();
}

void Mesh::RemoveEdge(Idx<MeshEdge> edge_idx)
{
	auto e = Edges[edge_idx];

	check(!e.ForwardFaceIdx.Valid() && !e.BackwardsFaceIdx.Valid());

	for (auto& v : Vertices)
	{
		v.EdgeIdxs.Remove(edge_idx);

		for (auto& idx : v.EdgeIdxs)
		{
			if (idx > edge_idx)
			{
				idx--;
			}
		}
	}

	for (auto& f : Faces)
	{
		check(!f.EdgeIdxs.Contains(edge_idx));

		for (auto& idx : f.EdgeIdxs)
		{
			if (idx > edge_idx)
			{
				idx--;
			}
		}
	}

	Edges.RemoveAt(edge_idx);
}

void Mesh::RemoveVert(Idx<MeshVert> vert_idx)
{
	auto v = Vertices[vert_idx];

	check(v.EdgeIdxs.Num() == 0);
	check(v.FaceIdxs.Num() == 0);

	for (auto& e : Edges)
	{
		check(e.StartVertIdx != vert_idx);
		check(e.EndVertIdx != vert_idx);

		if (e.StartVertIdx > vert_idx)
		{
			e.StartVertIdx--;
		}
		if (e.EndVertIdx > vert_idx)
		{
			e.EndVertIdx--;
		}
	}

	for (auto& f : Faces)
	{
		check(!f.VertIdxs.Contains(vert_idx));

		for (auto& idx : f.VertIdxs)
		{
			if (idx > vert_idx)
			{
				idx--;
			}
		}
	}

	Vertices.RemoveAt(vert_idx);
}

void Mesh::MergePartialEdges()
{
	for (Idx<MeshEdge> edge_idx{ 0 }; edge_idx < Edges.Num(); edge_idx++)
	{
		auto& edge = Edges[edge_idx];

		if (edge.Contains(Idx<MeshFace>::None))
		{
			auto edges = FindAllEdges(edge.StartVertIdx, edge.EndVertIdx);

			if (edges.Num() > 1)
			{
				for (auto other_edge_idx : edges)
				{
					if (other_edge_idx != edge_idx)
					{
						const auto& other_edge = Edges[other_edge_idx];

						if (other_edge.Contains(Idx<MeshFace>::None))
						{
							check(other_edge_idx > edge_idx);
							MergeEdges(edge_idx, other_edge_idx);
						}
					}
				}
			}
		}
	}
}

// merges from merge_from into merge_to
// merge_from is then removed from the mesh
void Mesh::MergeEdges(Idx<MeshEdge> merge_to, Idx<MeshEdge> merge_from)
{
	check(merge_to != merge_from);

	auto& edge1 = Edges[merge_to];
	auto& edge2 = Edges[merge_from];

	if (edge2.StartVertIdx == edge1.StartVertIdx)
	{
		// if the edges run the same way
		if (edge2.ForwardFaceIdx.Valid())
		{
			check(!edge1.ForwardFaceIdx.Valid());

			Faces[edge2.ForwardFaceIdx].EdgeIdxs.Remove(merge_from);
			Faces[edge2.ForwardFaceIdx].EdgeIdxs.Push(merge_to);
			edge1.ForwardFaceIdx = edge2.ForwardFaceIdx;
			edge2.ForwardFaceIdx = Idx<MeshFace>::None;
		}
		else
		{
			check(edge2.BackwardsFaceIdx.Valid());
			check(!edge1.BackwardsFaceIdx.Valid());

			Faces[edge2.BackwardsFaceIdx].EdgeIdxs.Remove(merge_from);
			Faces[edge2.BackwardsFaceIdx].EdgeIdxs.Push(merge_to);
			edge1.BackwardsFaceIdx = edge2.BackwardsFaceIdx;
			edge2.BackwardsFaceIdx = Idx<MeshFace>::None;
		}
	}
	else
	{
		// if the edges run opposite ways
		if (edge2.ForwardFaceIdx.Valid())
		{
			check(!edge1.BackwardsFaceIdx.Valid());

			Faces[edge2.ForwardFaceIdx].EdgeIdxs.Remove(merge_from);
			Faces[edge2.ForwardFaceIdx].EdgeIdxs.Push(merge_to);
			edge1.BackwardsFaceIdx = edge2.ForwardFaceIdx;
			edge2.ForwardFaceIdx = Idx<MeshFace>::None;
		}
		else
		{
			check(edge2.BackwardsFaceIdx.Valid());
			check(!edge1.ForwardFaceIdx.Valid());

			Faces[edge2.BackwardsFaceIdx].EdgeIdxs.Remove(merge_from);
			Faces[edge2.BackwardsFaceIdx].EdgeIdxs.Push(merge_to);
			edge1.ForwardFaceIdx = edge2.BackwardsFaceIdx;
			edge2.BackwardsFaceIdx = Idx<MeshFace>::None;
		}
	}

	// we follow this with a CleanUpRedundantEdges, so no need for this as we've made 
	// merge_from redundant by unsetting its remaining face index
	//RemoveEdge(merge_from);
}

void Mesh::CleanUpRedundantEdges()
{
	bool any = false;

	for (Idx<MeshEdge> i{ 0 }; i < Edges.Num();)
	{
		const auto& e = Edges[i];

		if (!e.ForwardFaceIdx.Valid() && !e.BackwardsFaceIdx.Valid())
		{
			any = true;
			RemoveEdge(i);
		}
		else
		{
			i++;
		}
	}

	if (any)
	{
		CleanUpRedundantVerts();
	}
}

void Mesh::CleanUpRedundantVerts()
{
	for (Idx<MeshVert> i{ 0 }; i < Vertices.Num();)
	{
		const auto& v = Vertices[i];

		if (v.EdgeIdxs.Num() == 0)
		{
			RemoveVert(i);
		}
		else
		{
			i++;
		}
	}
}

Idx<MeshFace> Mesh::AddFaceFromRawVerts(const TArray<MeshVertRaw>& vertices, int UVGroup, const TArray<PGCEdgeType>& edge_types, int channel)
{
	MeshFace mf(channel);

	for (auto vr : vertices)
	{
		mf.VertIdxs.Push(AddVert(vr, UVGroup));
	}

	TArray<PGCEdgeType> edge_type_copy(edge_types);

	RegularizeVertIdxs(mf.VertIdxs, &edge_type_copy);

	mf.UVGroup = UVGroup;

	return AddFindFace(mf, edge_type_copy);
}

bool Mesh::CancelExistingReverseFace(const TArray<Idx<MeshVert>>& face)
{
	// the reverse face has the same first vert and the other's reverse
	// (the same as reversing the whole lot and then Regularizing again)

	TArray<Idx<MeshVert>> reverse_face{ face[0] };

	for (int i = face.Num() - 1; i >= 1; i--)
	{
		reverse_face.Push(face[i]);
	}

	// if we have the reverse face already, then we cancel it instead to connect the shapes
	auto existing_face_idx = FindFaceByVertIdxs(reverse_face);

	if (existing_face_idx.Valid())
	{
		RemoveFace(existing_face_idx);
		return true;
	}

	return false;
}

bool Mesh::CancelExistingReverseFaceFromVects(const TArray<FVector>& vertices)
{
	TArray<Idx<MeshVert>> vert_idxs;

	for (auto vr : vertices)
	{
		vert_idxs.Push(AddVert(vr));
	}

	RegularizeVertIdxs(vert_idxs, nullptr);

	return CancelExistingReverseFace(vert_idxs);
}

Idx<MeshFace> Mesh::AddFaceFromVects(const TArray<FVector>& vertices,
	const TArray<FVector2D>& uvs, int UVGroup,
	const TArray<PGCEdgeType>& edge_types,
	int channel)
{
	check(vertices.Num() == edge_types.Num());
	check(vertices.Num() == uvs.Num());

	MeshFace mf(channel);

	for (int i = 0; i < vertices.Num(); i++)
	{
		mf.VertIdxs.Push(AddVert(MeshVertRaw(vertices[i], uvs[i]), UVGroup));
	}

	TArray<PGCEdgeType> edge_type_copy(edge_types);

	RegularizeVertIdxs(mf.VertIdxs, &edge_type_copy);

	mf.UVGroup = UVGroup;

	return AddFindFace(mf, edge_type_copy);
}

Idx<MeshVertRaw> Mesh::BakeVertex(const MeshVertRaw& mvr)
{
	auto baked_vert_idx = FindBakedVert(mvr);

	if (baked_vert_idx.Valid())
		return baked_vert_idx;

	BakedVerts.Push(mvr);

	return Idx<MeshVertRaw>(BakedVerts.Num() - 1);
}

Idx<MeshVertRaw> Mesh::FindBakedVert(const MeshVertRaw& mvr) const
{
	for (int i = 0; i < BakedVerts.Num(); i++)
	{
		if (BakedVerts[i].ToleranceCompare(mvr, VertexTolerance))
			return Idx<MeshVertRaw>(i);
	}

	return Idx<MeshVertRaw>::None;
}

void Mesh::BakeChannelsIntoFaceChannel(FPGCMeshResult& mesh, bool insideOut, PGCDebugEdgeType debugEdges, int from_channel, int to_face_channel)
{
	check(BakedFaces.Num() == 0);

	if (mesh.FaceChannels.Num() < to_face_channel + 1)
	{
		mesh.FaceChannels.AddDefaulted(to_face_channel + 1 - mesh.FaceChannels.Num());
	}

	for (const auto& f : Faces)
	{
		if (from_channel == -1 || f.Channel == from_channel)
		{
			MeshFaceRaw mfr;

			for (auto vert_idx : f.VertIdxs)
			{
				mfr.VertIdxs.Push(BakeVertex(Vertices[vert_idx].ToMeshVertRaw(f.UVGroup)));
			}

			BakedFaces.Push(mfr);
		}
	}

	// we are sometimes accumulating BakedVerts over several calls to this function
	// (and we require 1-to-1 between BakedVerts indices and mesh.Verts and mesh.UVs indices)
	// so put over any that are new
	for (int i = mesh.Verts.Num(); i < BakedVerts.Num(); i++)
	{
		const auto& v = BakedVerts[i];

		mesh.Verts.Push(v.Pos);
		mesh.UVs.Push(v.UV);
	}

	for (auto f : BakedFaces)
	{
		check(f.VertIdxs.Num() > 2);

		auto common_vert = f.VertIdxs[0];
		auto prev_vert = f.VertIdxs[1];
		for (int i = 2; i < f.VertIdxs.Num(); i++)
		{
			auto this_vert = f.VertIdxs[i];

			if (insideOut)
			{
				mesh.FaceChannels[to_face_channel].Triangles.Push(common_vert.AsInt());
				mesh.FaceChannels[to_face_channel].Triangles.Push(this_vert.AsInt());
				mesh.FaceChannels[to_face_channel].Triangles.Push(prev_vert.AsInt());
			}
			else
			{
				mesh.FaceChannels[to_face_channel].Triangles.Push(common_vert.AsInt());
				mesh.FaceChannels[to_face_channel].Triangles.Push(prev_vert.AsInt());
				mesh.FaceChannels[to_face_channel].Triangles.Push(this_vert.AsInt());
			}
			prev_vert = this_vert;
		}
	}

	if (debugEdges == PGCDebugEdgeType::Effective)
	{
		for (const auto& edge : Edges)
		{
			check(edge.EffectiveType == PGCEdgeType::Sharp || edge.EffectiveType == PGCEdgeType::Rounded);

			// UV group doesn't matter as we won't be using UVs on the edge drawing anyway
			auto vs = BakeVertex(Vertices[edge.StartVertIdx].ToMeshVertRaw(0));
			auto ve = BakeVertex(Vertices[edge.EndVertIdx].ToMeshVertRaw(0));

			if (edge.EffectiveType == PGCEdgeType::Rounded)
			{
				mesh.RoundedEdges.Push({ vs.AsInt(), ve.AsInt() });
			}
			else
			{
				mesh.SharpEdges.Push({ vs.AsInt(), ve.AsInt() });
			}
		}
	}
	else if (debugEdges == PGCDebugEdgeType::Set)
	{
		for (const auto& edge : Edges)
		{
			check(edge.SetType != PGCEdgeType::Unset);

			// UV group doesn't matter as we won't be using UVs on the edge drawing anyway
			auto vs = BakeVertex(Vertices[edge.StartVertIdx].ToMeshVertRaw(0));
			auto ve = BakeVertex(Vertices[edge.EndVertIdx].ToMeshVertRaw(0));

			if (edge.SetType == PGCEdgeType::Rounded)
			{
				mesh.RoundedEdges.Push({ vs.AsInt(), ve.AsInt() });
			}
			else if (edge.SetType == PGCEdgeType::Sharp)
			{
				mesh.SharpEdges.Push({ vs.AsInt(), ve.AsInt() });
			}
			else
			{
				mesh.AutoEdges.Push({ vs.AsInt(), ve.AsInt() });
			}
		}
	}

	BakedFaces.Empty();
}

TSharedRef<Mesh> Mesh::SplitSharedVerts()
{
	auto ret = MakeShared<Mesh>(*this);

	ret->Clean = true;

	for (Idx<MeshVert> i{ 0 }; i < Vertices.Num(); i++)
	{
		TArray<TArray<Idx<MeshFace>>> pyramids = FindPyramids(i);

		if (pyramids.Num() > 1)
		{
			ret->SplitPyramids(pyramids, i);
		}
	}

	return ret;
}

TArray<TArray<Idx<MeshFace>>> Mesh::FindPyramids(Idx<MeshVert> vert_idx)
{
	auto& v = Vertices[vert_idx];

	TSet<Idx<MeshFace>> all_faces{ v.FaceIdxs };

	TArray<TArray<Idx<MeshFace>>> ret;

	while (all_faces.Num())
	{
		auto first_face{ *all_faces.CreateIterator() };
		auto curr_face = first_face;

		auto prev_edge = Idx<MeshEdge>::None;

		ret.Push(TArray<Idx<MeshFace>>());

		do {
			ret.Last().Push(curr_face);

			check(all_faces.Contains(curr_face));
			all_faces.Remove(curr_face);

			auto f = Faces[curr_face];

			auto curr_edge = Idx<MeshEdge>::None;

			for (const auto& e : f.EdgeIdxs)
			{
				if (e != prev_edge && Edges[e].Contains(vert_idx))
				{
					curr_edge = e;
					break;
				}
			}

			check(curr_edge.Valid());

			prev_edge = curr_edge;

			curr_face = Edges[curr_edge].OtherFace(curr_face);

			// find the two edges 
		} while (curr_face != first_face);
	}

	return ret;
}

void Mesh::SplitPyramids(const TArray<TArray<Idx<MeshFace>>>& pyramids, Idx<MeshVert> vert_idx)
{
	// splitting pyramids generates duplicate vertices, which would break various vertex searches
	// HOWEVER we only do this immediately before a subdivide and that will renders the duplicates unique again

	// leave the first pyramid on the vert we already have
	for (int i = 1; i < pyramids.Num(); i++)
	{
		auto& pyramid = pyramids[i];

		// copy the vert
		{
			MeshVert temp;
			temp.Pos = Vertices[vert_idx].Pos;
			Vertices.Push(temp);
		}
		auto new_vert_idx = Vertices.LastIdx();

		auto& vert = Vertices[new_vert_idx];

		auto& old_vert = Vertices[vert_idx];

		for (auto face_idx : pyramid)
		{
			auto& face = Faces[face_idx];

			old_vert.FaceIdxs.Remove(face_idx);
			vert.FaceIdxs.Push(face_idx);
			vert.UVs.Add(face.UVGroup) = old_vert.UVs[face.UVGroup];

			bool found = false;
			for (auto& j : face.VertIdxs)
			{
				if (j == vert_idx)
				{
					found = true;
					j = new_vert_idx;
					break;
				}
			}

			check(found);

			RegularizeVertIdxs(face.VertIdxs, nullptr);
		}

		int edges_found = 0;

		for (int j = 0; j < old_vert.EdgeIdxs.Num();)
		{
			auto edge_idx = old_vert.EdgeIdxs[j];

			auto& edge = Edges[edge_idx];

			// only need to check one out of the forward and backwards faces
			// since the edges around this vert connect either two or zero faces from this pyramid
			if (pyramid.Contains(edge.ForwardFaceIdx))
			{
				edges_found++;

				if (edge.StartVertIdx == vert_idx)
				{
					old_vert.EdgeIdxs.Remove(edge_idx);
					vert.EdgeIdxs.Push(edge_idx);

					edge.StartVertIdx = new_vert_idx;
				}
				else 
				{
					check(edge.EndVertIdx == vert_idx);

					old_vert.EdgeIdxs.Remove(edge_idx);
					vert.EdgeIdxs.Push(edge_idx);

					edge.EndVertIdx = new_vert_idx;
				}
			}
			else
			{
				j++;
			}
		}

		// we expect the same number of edges and faces in the pyramid
		check(edges_found == pyramid.Num());

		CheckConsistent(true);
	}
}

void Mesh::RegularizeVertIdxs(TArray<Idx<MeshVert>>& vert_idxs, TArray<PGCEdgeType>* edge_types)
{
	// sort the array so that the lowest index is first
	// allows us to search for faces by verts

	auto lowest = vert_idxs[0];
	auto pos = 0;

	for (int i = 1; i < vert_idxs.Num(); i++)
	{
		if (vert_idxs[i] < lowest)
		{
			lowest = vert_idxs[i];
			pos = i;
		}
	}

	if (pos == 0)
		return;

	TArray<Idx<MeshVert>> temp;
	TArray<PGCEdgeType> temp2;

	for (int i = 0; i < vert_idxs.Num(); i++)
	{
		temp.Push(vert_idxs[pos]);
		if (edge_types)
		{
			temp2.Push((*edge_types)[pos]);
		}

		pos = (pos + 1) % vert_idxs.Num();
	}

	if (edge_types)
	{
		*edge_types = std::move(temp2);
	}

	vert_idxs = std::move(temp);
}

Idx<MeshFace> Mesh::AddFindFace(MeshFace face, const TArray<PGCEdgeType>& edge_types)
{
	// enough verts?
	check(face.VertIdxs.Num() > 2);
	// all verts unique?
	check(TSet<Idx<MeshVert>>{face.VertIdxs}.Num() == face.VertIdxs.Num());

	if (CancelExistingReverseFace(face.VertIdxs))
		return Idx<MeshFace>::None;

	auto face_idx = FindFaceByVertIdxs(face.VertIdxs);

	if (face_idx != Idx<MeshFace>::None)
	{
		return face_idx;
	}

	auto prev_vert = face.VertIdxs.Last();
	auto prev_edge_type = edge_types.Last();

	for (int i = 0; i < face.VertIdxs.Num(); i++)
	{
		auto vert_idx = face.VertIdxs[i];

		Vertices[vert_idx].FaceIdxs.Push(Faces.Num());

		auto edge_idx = AddFindEdge(prev_vert, vert_idx);

		face.EdgeIdxs.Push(edge_idx);

		// Have carefully arranged these edge_types into the same order as the verts.
		// Sharp edges from either existing or incoming edges take precedence.
		Edges[edge_idx].SetType = MergeEdgeTypes(Edges[edge_idx].SetType, prev_edge_type);
		Edges[edge_idx].AddFace(Faces.Num(), prev_vert);

		prev_vert = vert_idx;
		prev_edge_type = edge_types[i];
	}

	Faces.Push(face);

	return Faces.LastIdx();
}


// (untested) copy-over code that could be used in the following for faces that are already triangles
// (doesn't happen at present, hence untested...)
// copying over could be usefully encapsulated into a Mesh member function
//
//// just copy over as is already a triangle
//MeshFace mf(f.Channel);
//
//auto v0 = Vertices[f.VertIdxs[0]].ToMeshVertRaw(f.UVGroup);
//auto v1 = Vertices[f.VertIdxs[1]].ToMeshVertRaw(f.UVGroup);
//auto v2 = Vertices[f.VertIdxs[2]].ToMeshVertRaw(f.UVGroup);
//
//const auto& e0 = Edges[FindEdge(f.VertIdxs[0], f.VertIdxs[1], false)];
//const auto& e1 = Edges[FindEdge(f.VertIdxs[1], f.VertIdxs[2], false)];
//const auto& e2 = Edges[FindEdge(f.VertIdxs[2], f.VertIdxs[0], false)];
//
//ret->AddFaceFromRawVerts({ v0, v1, v2 },
//	f.UVGroup,
//	{ e0.SetType, e1.SetType, e2.SetType },
//	f.Channel);

TSharedPtr<Mesh> Mesh::Triangularise()
{
	ResolveEffectiveEdgeTypes();

	check(Clean);

	auto ret = MakeShared<Mesh>(FMath::Cos(FMath::DegreesToRadians(20.0f)));

	for (const auto& f : Faces)
	{
		check(f.VertIdxs.Num() > 2);

		MeshVertRaw fv;

		for (auto v : f.VertIdxs)
		{
			fv += Vertices[v].ToMeshVertRaw(f.UVGroup);
		}

		fv = fv / f.VertIdxs.Num();

		auto prev_vert = f.VertIdxs.Last();

		for (auto v : f.VertIdxs)
		{
			check(prev_vert != v);

			const MeshVertRaw& from = Vertices[prev_vert].ToMeshVertRaw(f.UVGroup);
			const MeshVertRaw& to = Vertices[v].ToMeshVertRaw(f.UVGroup);

			check(!(from == to));
			check(!(from == fv));
			check(!(to == fv));

			const auto& edge_idx = FindEdge(prev_vert, v, false);

			check(edge_idx.Valid());

			// edges introduced in the middle of what was a single (roughly planar) face should be Rounded, I guess...
			ret->AddFaceFromRawVerts({ from, to, fv },
				f.UVGroup,
				{ Edges[edge_idx].SetType, PGCEdgeType::Rounded, PGCEdgeType::Rounded },
				f.Channel);

			prev_vert = v;
		}
	}

	// we could check for closure if we knew the input was also closed...
	ret->CheckConsistent(false);

	return ret;
}

TSharedPtr<Mesh> Mesh::Subdivide()
{
	CheckConsistent(true);

	TSharedRef<Mesh> work_on = AsShared();

	// if we've had any geometry added manually, we're not clean
	// if we were generated procedurally (say by SplitSharedVerts or returned from here), then we're clean and don't need any fixing-up...
	if (!Clean)
	{
		work_on = SplitSharedVerts();
	}

	return work_on->SubdivideInner();
}

TSharedPtr<Mesh> Mesh::SubdivideInner()
{
	check(Clean);

	auto ret = MakeShared<Mesh>(CosAutoSharpAngle);

	ResolveEffectiveEdgeTypes();

	for (auto& f : Faces)
	{
		MeshVertRaw fv;

		for (auto v : f.VertIdxs)
		{
			fv += Vertices[v].ToMeshVertRaw(f.UVGroup);
		}

		fv = fv / f.VertIdxs.Num();

		// set this up to represent our position and the UVs for _only_ this face
		f.WorkingFaceVertex.Pos = fv.Pos;
		f.WorkingFaceVertex.UVs.Empty();
		f.WorkingFaceVertex.UVs.Add(f.UVGroup) = fv.UV;
	}

	for (auto& e : Edges)
	{
		if (e.EffectiveType == PGCEdgeType::Rounded)
		{
			e.WorkingEdgeVertex.Pos = (Vertices[e.StartVertIdx].Pos + Vertices[e.EndVertIdx].Pos
				+ Faces[e.ForwardFaceIdx].WorkingFaceVertex.Pos + Faces[e.BackwardsFaceIdx].WorkingFaceVertex.Pos) / 4;
		}
		else
		{
			e.WorkingEdgeVertex.Pos = (Vertices[e.StartVertIdx].Pos + Vertices[e.EndVertIdx].Pos) / 2;
		}

		e.WorkingEdgeVertex.UVs = (Vertices[e.StartVertIdx].UVs + Vertices[e.EndVertIdx].UVs) / 2;
	}

	for (Idx<MeshVert> vert_idx{ 0 }; vert_idx < Vertices.Num(); vert_idx++)
	{
		auto& v = Vertices[vert_idx];

		TArray<MeshEdge> sharp_edges;

		for (auto edge_idx : v.EdgeIdxs)
		{
			const auto& edge = Edges[edge_idx];

			if (edge.EffectiveType == PGCEdgeType::Sharp)
			{
				sharp_edges.Add(edge);
			}
		}

		if (sharp_edges.Num() < 2)
		{
			auto n = v.FaceIdxs.Num();

			FVector F{ 0, 0, 0 };

			for (auto face_idx : v.FaceIdxs)
			{
				F += Faces[face_idx].WorkingFaceVertex.Pos;
			}

			F = F / n;

			FVector R{ 0, 0, 0 };

			for (auto edge_idx : v.EdgeIdxs)
			{
				R += Edges[edge_idx].WorkingEdgeVertex.Pos;
			}

			// assuming the number of faces == the number of edges, which is true for closed meshes, may need a special rule for the edges if this is ever not true...
			R = R / n;

			v.WorkingNewPos.Pos = (v.Pos * (n - 3) + R * 2 + F) / n;
		}
		else if (sharp_edges.Num() == 2)
		{
			const auto& ov0 = Vertices[sharp_edges[0].OtherVert(vert_idx)];
			const auto& ov1 = Vertices[sharp_edges[1].OtherVert(vert_idx)];

			v.WorkingNewPos.Pos = v.Pos * 0.75f + ov0.Pos * 0.125f + ov1.Pos * 0.125f;
		}
		else
		{
			v.WorkingNewPos.Pos = v.Pos;
		}

		// on a vert we keep the UVs as they were, since the position is going to pull in but still wants to be the same point in texture-space
		v.WorkingNewPos.UVs = v.UVs;
	}

	for (Idx<MeshFace> face_idx{ 0 }; face_idx < Faces.Num(); face_idx++)
	{
		const auto& f = Faces[face_idx];
		auto n = f.VertIdxs.Num();

		for (int i = 0; i < n; i++)
		{
			auto vert_idx = f.VertIdxs[i];
			auto prev_vert_idx = f.VertIdxs[(i + n - 1) % n];
			auto next_vert_idx = f.VertIdxs[(i + 1) % n];

			auto prev_edge_idx = FindEdge(prev_vert_idx, vert_idx, face_idx);
			check(prev_edge_idx.Valid());

			auto next_edge_idx = FindEdge(vert_idx, next_vert_idx, face_idx);
			check(next_edge_idx.Valid());

			auto& v1 = Vertices[vert_idx].WorkingNewPos;
			auto& v2 = Edges[next_edge_idx].WorkingEdgeVertex;
			auto& v3 = f.WorkingFaceVertex;
			auto& v4 = Edges[prev_edge_idx].WorkingEdgeVertex;

			// edges introduced in the middle of what was a single (roughly planar) face should be Rounded, I guess...
			ret->AddFaceFromRawVerts({
				v1.ToMeshVertRaw(f.UVGroup),
				v2.ToMeshVertRaw(f.UVGroup),
				v3.ToMeshVertRaw(f.UVGroup),
				v4.ToMeshVertRaw(f.UVGroup)
				}, f.UVGroup,
				{
					Edges[next_edge_idx].SetType,
					PGCEdgeType::Rounded,
					PGCEdgeType::Rounded,
					Edges[prev_edge_idx].SetType,
				},
				f.Channel);
		}
	}

	ret->CheckConsistent(true);

	return ret;
}

void Mesh::ResolveEffectiveEdgeTypes()
{
	for (auto& e : Edges)
	{
		if (e.EffectiveType == PGCEdgeType::Unset)
		{
			if (e.SetType != PGCEdgeType::Auto)
			{
				e.EffectiveType = e.SetType;
			}
			else
			{
				CalcEffectiveType(e);
			}
		}
	}
}

void Mesh::CalcEffectiveType(MeshEdge& edge)
{
	FVector e1normal = CalcNonplanarFaceNormal(Faces[edge.ForwardFaceIdx]);
	FVector e2normal = CalcNonplanarFaceNormal(Faces[edge.BackwardsFaceIdx]);

	auto cos = FVector::DotProduct(e1normal, e2normal);

	if (cos < CosAutoSharpAngle)
	{
		edge.EffectiveType = PGCEdgeType::Sharp;
	}
	else
	{
		edge.EffectiveType = PGCEdgeType::Rounded;
	}
}

FVector Mesh::CalcNonplanarFaceNormal(const MeshFace& face)
{
	FVector sum{ 0, 0, 0 };

	auto prev_vert_idx = face.VertIdxs.Last();

	for (const auto& vert_idx : face.VertIdxs)
	{
		sum += FVector::CrossProduct(Vertices[prev_vert_idx].Pos, Vertices[vert_idx].Pos);

		prev_vert_idx = vert_idx;
	}

	return sum.GetSafeNormal();
}

TSharedPtr<Mesh> Mesh::SubdivideN(int count)
{
	auto ret = AsShared();

	for (int i = 0; i < count; i++)
	{
		auto temp = ret->Subdivide();

		ret = temp.ToSharedRef();
	}

	return ret;
}

void Mesh::AddCube(const FPGCCube& cube)
{
	Clean = false;

	enum VertNames {
		LFB,
		LFT,
		LBB,
		LBT,
		RFB,
		RFT,
		RBB,
		RBT,
	};

	FVector verts[8] {
		{(float)cube.X - 0.5f, (float)cube.Y - 0.5f, (float)cube.Z - 0.5f},
		{(float)cube.X - 0.5f, (float)cube.Y - 0.5f, (float)cube.Z + 0.5f},
		{(float)cube.X - 0.5f, (float)cube.Y + 0.5f, (float)cube.Z - 0.5f},
		{(float)cube.X - 0.5f, (float)cube.Y + 0.5f, (float)cube.Z + 0.5f},
		{(float)cube.X + 0.5f, (float)cube.Y - 0.5f, (float)cube.Z - 0.5f},
		{(float)cube.X + 0.5f, (float)cube.Y - 0.5f, (float)cube.Z + 0.5f},
		{(float)cube.X + 0.5f, (float)cube.Y + 0.5f, (float)cube.Z - 0.5f},
		{(float)cube.X + 0.5f, (float)cube.Y + 0.5f, (float)cube.Z + 0.5f}
	};

	TArray<FVector2D> uvs {
		{0, 0},
		{0, 1},
		{1, 1},
		{1, 0}
	};

	TArray<TArray<FVector>> faces {
		{ verts[RFB], verts[RBB], verts[LBB], verts[LFB] },
		{ verts[RFB], verts[RFT], verts[RBT], verts[RBB] },
		{ verts[LBB], verts[RBB], verts[RBT], verts[LBT] },
		{ verts[LFT], verts[LFB], verts[LBB], verts[LBT] },
		{ verts[RFT], verts[RFB], verts[LFB], verts[LFT] },
		{ verts[RFT], verts[LFT], verts[LBT], verts[RBT] }
	};

	TArray<TArray<PGCEdgeType>> edge_types {
		{ cube.EdgeTypes[(int)PGCEdgeId::BottomRight],	cube.EdgeTypes[(int)PGCEdgeId::BottomBack],		cube.EdgeTypes[(int)PGCEdgeId::BottomLeft],	cube.EdgeTypes[(int)PGCEdgeId::BottomFront],	},
		{ cube.EdgeTypes[(int)PGCEdgeId::FrontRight],	cube.EdgeTypes[(int)PGCEdgeId::TopRight],		cube.EdgeTypes[(int)PGCEdgeId::BackRight],	cube.EdgeTypes[(int)PGCEdgeId::BottomRight],	},
		{ cube.EdgeTypes[(int)PGCEdgeId::BottomBack],	cube.EdgeTypes[(int)PGCEdgeId::BackRight],		cube.EdgeTypes[(int)PGCEdgeId::TopBack],	cube.EdgeTypes[(int)PGCEdgeId::BackLeft],		},
		{ cube.EdgeTypes[(int)PGCEdgeId::FrontLeft],	cube.EdgeTypes[(int)PGCEdgeId::BottomLeft],		cube.EdgeTypes[(int)PGCEdgeId::BackLeft],	cube.EdgeTypes[(int)PGCEdgeId::TopLeft],		},
		{ cube.EdgeTypes[(int)PGCEdgeId::FrontRight],	cube.EdgeTypes[(int)PGCEdgeId::BottomFront],	cube.EdgeTypes[(int)PGCEdgeId::FrontLeft],	cube.EdgeTypes[(int)PGCEdgeId::TopFront],		},
		{ cube.EdgeTypes[(int)PGCEdgeId::TopFront],		cube.EdgeTypes[(int)PGCEdgeId::TopLeft],		cube.EdgeTypes[(int)PGCEdgeId::TopBack],	cube.EdgeTypes[(int)PGCEdgeId::TopRight],		},
	};

	for (int i = 0; i < 6; i++)
	{
		// add channel argument(s) to this function if we want a choice here...
		AddFaceFromVects(faces[i], uvs, NextUVGroup++, edge_types[i], 0);
		CheckConsistent(false);
	}

	CheckConsistent(true);
}

void Mesh::BakeAllChannelsIntoOne(FPGCMeshResult& mesh, bool insideOut, PGCDebugEdgeType debugEdges)
{
	ResolveEffectiveEdgeTypes();

	check(BakedVerts.Num() == 0);

	BakeChannelsIntoFaceChannel(mesh, insideOut, debugEdges, -1, 0);

	BakedVerts.Empty();
}

void Mesh::BakeChannels(FPGCMeshResult& mesh, bool insideOut, PGCDebugEdgeType debugEdges, int start_channel, int end_channel)
{
	ResolveEffectiveEdgeTypes();

	check(BakedVerts.Num() == 0);

	for(int i = start_channel; i <= end_channel; i++)
	{
		BakeChannelsIntoFaceChannel(mesh, insideOut, debugEdges, i, i);
	}

	BakedVerts.Empty();
}


bool MeshVertRaw::ToleranceCompare(const MeshVertRaw& other, float tolerance) const
{
	// may need separate coord and UV tolerances if the scales get very different...
	return FVector::DistSquared(Pos, other.Pos) < tolerance * tolerance
		&& FVector2D::DistSquared(UV, other.UV) < tolerance * tolerance;
}

PRAGMA_ENABLE_OPTIMIZATION
