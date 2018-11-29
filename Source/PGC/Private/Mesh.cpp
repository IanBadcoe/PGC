// Fill out your copyright notice in the Description page of Project Settings.

#include "Mesh.h"

#include "Runtime/Core/Public/Templates/UniquePtr.h"
#include "Runtime/Engine/Classes/Engine/World.h"

PRAGMA_DISABLE_OPTIMIZATION

inline bool Mesh::FaceUnique(MeshFace face) const
{
	return FindFaceByVertIdxs(face.VertIdxs) == -1;
}

int Mesh::FindEdge(int idx1, int idx2) const
{
	for (int i = 0; i < Edges.Num(); i++)
	{
		auto& edge = Edges[i];

		if ((edge.StartVertIdx == idx1 && edge.EndVertIdx == idx2)
			|| (edge.EndVertIdx == idx1 && edge.StartVertIdx == idx2))
		{
			return i;
		}
	}

	return -1;
}

int Mesh::AddFindEdge(int idx1, int idx2)
{
	int edge_idx = FindEdge(idx1, idx2);

	if (edge_idx != -1)
		return edge_idx;

	MeshEdge ne{ ne.StartVertIdx = idx1, ne.EndVertIdx = idx2 };

	Vertices[idx1].EdgeIdxs.Push(Edges.Num());
	Vertices[idx2].EdgeIdxs.Push(Edges.Num());

	Edges.Push(ne);

	return Edges.Num() - 1;
}

int Mesh::FindVert(const FVector & pos) const
{
	for (int i = 0; i < Vertices.Num(); i++)
	{
		auto& vert = Vertices[i];

		if (vert.Pos == pos)
		{
			return i;
		}
	}

	return -1;
}

int Mesh::FindVert(const MeshVertRaw& mvr, int UVGroup) const
{
	for (int i = 0; i < Vertices.Num(); i++)
	{
		auto& vert = Vertices[i];

		// ToMeshVertRaw explodes if vert doesn't know about this UVGroup
		if (vert.UVs.Contains(UVGroup) && vert.ToMeshVertRaw(UVGroup) == mvr)
		{
			return i;
		}
	}

	return -1;
}

// a closed mesh has no holes, an unclosed on may have faces not added yet...
void Mesh::CheckConsistent(bool closed)
{
	for (int edge_idx = 0; edge_idx < Edges.Num(); edge_idx++)
	{
		auto e = Edges[edge_idx];

		check(e.StartVertIdx >= 0 && e.StartVertIdx < Vertices.Num());
		check(e.EndVertIdx >= 0 && e.EndVertIdx < Vertices.Num());
		check(e.ForwardsFaceIdx < Faces.Num());
		check(e.BackwardsFaceIdx < Faces.Num());
		check(!closed || e.ForwardsFaceIdx != -1);
		check(!closed || e.BackwardsFaceIdx != -1);

		check(Vertices[e.StartVertIdx].EdgeIdxs.Contains(edge_idx));
		check(Vertices[e.EndVertIdx].EdgeIdxs.Contains(edge_idx));
		check(e.ForwardsFaceIdx == -1 || Faces[e.ForwardsFaceIdx].EdgeIdxs.Contains(edge_idx));
		check(e.BackwardsFaceIdx == -1 || Faces[e.BackwardsFaceIdx].EdgeIdxs.Contains(edge_idx));
	}

	for (int vert_idx = 0; vert_idx < Vertices.Num(); vert_idx++)
	{
		auto v = Vertices[vert_idx];

		// we expect one-to-one for edges and faces (true for closed meshes...)
		check(!closed || v.FaceIdxs.Num() == v.EdgeIdxs.Num());

		// if we know about an edge, it should know about us
		for (auto edge_idx : v.EdgeIdxs)
		{
			check(Edges[edge_idx].StartVertIdx == vert_idx || Edges[edge_idx].EndVertIdx == vert_idx);
		}

		// if we know about a face, it should know about us
		for (auto face_idx : v.FaceIdxs)
		{
			check(Faces[face_idx].VertIdxs.Contains(vert_idx));
		}
	}

	for (int face_idx = 0; face_idx < Faces.Num(); face_idx++)
	{
		auto& face = Faces[face_idx];

		auto prev_vert_idx = face.VertIdxs.Last();

		for (auto vert_idx : face.VertIdxs)
		{
			check(vert_idx >= 0 && vert_idx < Vertices.Num());

			int edge_idx = FindEdge(prev_vert_idx, vert_idx);
			check(edge_idx != -1);

			auto& edge = Edges[edge_idx];

			// should have us as a face on one or the other side
			check(edge.ForwardsFaceIdx == face_idx || edge.BackwardsFaceIdx == face_idx);

			// if the edge starts with our previous vert, then we are the forwards face of this edge,
			// otherwise we are its backwards face
			check((edge.StartVertIdx == prev_vert_idx) == (edge.ForwardsFaceIdx == face_idx));

			auto v = Vertices[vert_idx];

			check(v.FaceIdxs.Contains(face_idx));

			prev_vert_idx = vert_idx;
		}
	}

}

void Mesh::UnitTest()
{
}

int Mesh::AddVert(MeshVertRaw vert, int UVGRoup)
{
	int vert_idx = FindVert(vert, UVGRoup);

	// we have it with this UV already set
	if (vert_idx != -1)
		return vert_idx;

	// do we have it without the UV?
	vert_idx = FindVert(vert.Pos);

	if (vert_idx == -1)
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

int Mesh::AddVert(FVector pos)
{
	int vert_idx = FindVert(pos);

	// we have it with this UV already set
	if (vert_idx != -1)
		return vert_idx;

	MeshVert mv;
	mv.Pos = pos;
	Vertices.Push(mv);

	return Vertices.Num() - 1;
}

int Mesh::FindFaceByVertIdxs(const TArray<int>& vert_idxs) const
{
	for (int face_idx = 0; face_idx < Faces.Num(); face_idx++)
	{
		auto mf = Faces[face_idx];

		if (mf.VertIdxs == vert_idxs)
		{
			return face_idx;
		}
	}

	return -1;
}

void Mesh::RemoveFace(int face_idx)
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
		if (e.ForwardsFaceIdx == face_idx)
		{
			e.ForwardsFaceIdx = -1;
		}
		else if (e.ForwardsFaceIdx > face_idx)
		{
			e.ForwardsFaceIdx--;
		}


		if (e.BackwardsFaceIdx== face_idx)
		{
			e.BackwardsFaceIdx = -1;
		}
		else if (e.BackwardsFaceIdx > face_idx)
		{
			e.BackwardsFaceIdx--;
		}
	}

	Faces.RemoveAt(face_idx);
}

int Mesh::AddFaceFromRawVerts(const TArray<MeshVertRaw>& vertices, int UVGroup)
{
	MeshFace fm;

	for (auto vr : vertices)
	{
		fm.VertIdxs.Push(AddVert(vr, UVGroup));
	}

	RegularizeVertIdxs(fm.VertIdxs);

	fm.UVGroup = UVGroup;

	return AddFace(fm);
}

bool Mesh::CancelExistingFace(const TArray<FVector>& vertices)
{
	TArray<int> vert_idxs;

	for (auto vr : vertices)
	{
		vert_idxs.Push(AddVert(vr));
	}

	RegularizeVertIdxs(vert_idxs);

	// the reverse face has the same first vert and the other's reverse
	// (the same as reversing the whole lot and then Regularizing again)

	TArray<int> reverse_face{ vert_idxs[0] };

	for (int i = vert_idxs.Num() - 1; i >= 1; i--)
	{
		reverse_face.Push(vert_idxs[i]);
	}

	// if we have the reverse face already, then we cancel it instead to connect the shapes
	int existing_face_idx = FindFaceByVertIdxs(reverse_face);

	if (existing_face_idx != -1)
	{
		RemoveFace(existing_face_idx);
		return true;
	}

	return false;
}

int Mesh::AddFaceFromVects(const TArray<FVector>& vertices, const TArray<FVector2D>& uvs, int UVGroup)
{
	MeshFace fm;

	for (int i = 0; i < vertices.Num(); i++)
	{
		fm.VertIdxs.Push(AddVert(MeshVertRaw(vertices[i], uvs[i]), UVGroup));
	}

	fm.UVGroup = UVGroup;

	return AddFace(fm);
}

int Mesh::BakeVertex(const MeshVertRaw& mvr)
{
	int baked_vert_idx = FindBakedVert(mvr);

	if (baked_vert_idx != -1)
		return baked_vert_idx;

	BakedVerts.Push(mvr);

	return BakedVerts.Num() - 1;
}

int Mesh::FindBakedVert(const MeshVertRaw& mvr) const
{
	for (int i = 0; i < BakedVerts.Num(); i++)
	{
		if (BakedVerts[i] == mvr)
			return i;
	}

	return -1;
}

void Mesh::RegularizeVertIdxs(TArray<int>& vert_idxs)
{
	// sort the array so that the lowest index is first
	// allows us to search for faces by verts

	int lowest = vert_idxs[0];
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

	TArray<int> temp;

	for (int i = 0; i < vert_idxs.Num(); i++)
	{
		temp.Push(vert_idxs[pos]);

		pos = (pos + 1) % vert_idxs.Num();
	}

	vert_idxs = std::move(temp);
}

int Mesh::AddFace(MeshFace face)
{
	check(FaceUnique(face));

	auto prev_vert = face.VertIdxs.Last();

	for (auto vert_idx : face.VertIdxs)
	{
		Vertices[vert_idx].FaceIdxs.Push(Faces.Num());

		int edge_idx = AddFindEdge(prev_vert, vert_idx);

		face.EdgeIdxs.Push(edge_idx);

		Edges[edge_idx].AddFace(Faces.Num(), prev_vert);

		prev_vert = vert_idx;
	}

	Faces.Push(face);

	return Faces.Num() - 1;
}

TSharedPtr<Mesh> Mesh::Subdivide()
{
	CheckConsistent(true);

	auto ret = MakeShared<Mesh>();

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
		e.WorkingEdgeVertex.Pos = (Vertices[e.StartVertIdx].Pos + Vertices[e.EndVertIdx].Pos
			+ Faces[e.ForwardsFaceIdx].WorkingFaceVertex.Pos + Faces[e.BackwardsFaceIdx].WorkingFaceVertex.Pos) / 4;

		e.WorkingEdgeVertex.UVs = (Vertices[e.StartVertIdx].UVs + Vertices[e.EndVertIdx].UVs) / 2;
	}

	for (auto& v : Vertices)
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

		// on a vert we keep the UVs as they were, since the position is going to pull in but still wants to be the same point in texture-space
		v.WorkingNewPos.UVs = v.UVs;
	}

	for (const auto f : Faces)
	{
		auto n = f.VertIdxs.Num();

		for (int i = 0; i < n; i++)
		{
			auto vert_idx = f.VertIdxs[i];
			auto prev_vert_idx = f.VertIdxs[(i + n - 1) % n];
			auto next_vert_idx = f.VertIdxs[(i + 1) % n];

			auto prev_edge_idx = FindEdge(prev_vert_idx, vert_idx);
			check(prev_edge_idx != -1);

			auto next_edge_idx = FindEdge(vert_idx, next_vert_idx);
			check(next_edge_idx != -1);

			auto& v1 = Vertices[vert_idx].WorkingNewPos;
			auto& v2 = Edges[next_edge_idx].WorkingEdgeVertex;
			auto& v3 = f.WorkingFaceVertex;
			auto& v4 = Edges[prev_edge_idx].WorkingEdgeVertex;

			ret->AddFaceFromRawVerts({
				v1.ToMeshVertRaw(f.UVGroup),
				v2.ToMeshVertRaw(f.UVGroup),
				v3.ToMeshVertRaw(f.UVGroup),
				v4.ToMeshVertRaw(f.UVGroup)
				}, f.UVGroup);
		}
	}

	ret->CheckConsistent(true);

	return ret;
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

void Mesh::AddCube(int X, int Y, int Z)
{
	FVector verts[8] {
		{(float)X, (float)Y, (float)Z},
		{(float)X, (float)Y, (float)Z + 1},
		{(float)X, (float)Y + 1, (float)Z},
		{(float)X, (float)Y + 1, (float)Z + 1},
		{(float)X + 1, (float)Y, (float)Z},
		{(float)X + 1, (float)Y, (float)Z + 1},
		{(float)X + 1, (float)Y + 1, (float)Z},
		{(float)X + 1, (float)Y + 1, (float)Z + 1}
	};

	TArray<FVector2D> uvs {
		{0, 0},
		{0, 1},
		{1, 1},
		{1, 0}
	};

	TArray<TArray<FVector>> faces{
		{ verts[4], verts[6], verts[2], verts[0] },
		{ verts[4], verts[5], verts[7], verts[6] },
		{ verts[2], verts[6], verts[7], verts[3] },
		{ verts[1], verts[0], verts[2], verts[3] },
		{ verts[5], verts[4], verts[0], verts[1] },
		{ verts[5], verts[1], verts[3], verts[7] },
	};

	TArray<bool> need_add;

	// when adding a cube, if it's face is the inverse of an existing face, then we delete that
	// (and do not add this one, to make the cubes connect
	for (const auto f : faces)
	{
		need_add.Push(!CancelExistingFace(f));
		CheckConsistent(false);
	}

	int i = 0;

	for (const auto f : faces)
	{
		if (need_add[i++])
		{
			AddFaceFromVects(f, uvs, NextUVGroup++);
			CheckConsistent(false);
		}
	}
}

void Mesh::Bake(FPGCMeshResult& mesh)
{
	for (const auto& f : Faces)
	{
		MeshFaceRaw mfr;

		for (auto vert_idx : f.VertIdxs)
		{
			mfr.VertIdxs.Push(BakeVertex(Vertices[vert_idx].ToMeshVertRaw(f.UVGroup)));
		}

		BakedFaces.Push(mfr);
	}

	for (auto v : BakedVerts)
	{
		mesh.Verts.Push(v.Pos * 100.0f);
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

			mesh.Triangles.Push(common_vert);
			mesh.Triangles.Push(prev_vert);
			mesh.Triangles.Push(this_vert);

			prev_vert = this_vert;
		}
	}

	BakedFaces.Empty();
	BakedVerts.Empty();
}

PRAGMA_ENABLE_OPTIMIZATION