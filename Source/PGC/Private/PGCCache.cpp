#include "PGCCache.h"

#include "Runtime/Core/Public/Serialization/BufferArchive.h"
#include "Runtime/Core/Public/Misc/Paths.h"
#include "Runtime/Core/Public/Misc/FileHelper.h"
#include "Runtime/Core/Public/Serialization/MemoryReader.h"

namespace Cache {

// local types

struct MeshKey {
	FString GeneratorName;
	uint32 GeneratorHash;
	int NumDivisions;
	bool Triangularise;
	PGCDebugMode DM;

	bool operator==(const MeshKey& rhs) const {
		return GeneratorName == rhs.GeneratorName
			&& GeneratorHash == rhs.GeneratorHash
			&& NumDivisions == rhs.NumDivisions
			&& Triangularise == rhs.Triangularise
			&& DM == rhs.DM;
	}
};

static uint32 GetTypeHash(const MeshKey& key) {
	uint32 ret = HashCombine(::GetTypeHash(key.GeneratorName), ::GetTypeHash(key.GeneratorHash));

	ret = HashCombine(ret, ::GetTypeHash(key.NumDivisions));
	ret = HashCombine(ret, ::GetTypeHash(key.Triangularise));
	ret = HashCombine(ret, ::GetTypeHash(key.DM));

	return ret;
}

FArchive& operator<<(FArchive& Ar, MeshKey& mk) {
	Ar << mk.GeneratorName;
	Ar << mk.GeneratorHash;
	Ar << mk.NumDivisions;
	Ar << mk.Triangularise;
	Ar << mk.DM;

	return Ar;
}

struct MeshVal {
	TSharedPtr<Mesh> Geom;
	TSharedPtr<TArray<FPGCNodePosition>> Nodes;
};

FArchive& operator<<(FArchive& Ar, MeshVal& mv) {
	if (!mv.Geom.IsValid())
	{
		mv.Geom = MakeShared<Mesh>();
	}

	if (!mv.Nodes.IsValid())
	{
		mv.Nodes = MakeShared<TArray<FPGCNodePosition>>();
	}

	Ar << *mv.Geom;
	Ar << *mv.Nodes;

	return Ar;
}

// seem to need this as if I put the TSharedPtr directly in the map, then the TMap
// serialization templates want an operator<< for TSharedPtr<IGraph> and however much
// I define one they can't find it
struct IGraphVal {
	TSharedPtr<IGraph> IGraph;
};

FArchive& operator<<(FArchive& Ar, IGraphVal& igv)
{
	if (!igv.IGraph.IsValid())
	{
		igv.IGraph = MakeShared<IGraph>();
	}

	igv.IGraph->Serialize(Ar);

	return Ar;
}

// --

static TMap<uint32, IGraphVal> IGraphCache;
static TMap<MeshKey, MeshVal> MeshCache;
// the idea is we load once, automatically, on first use an a session
static bool IsLoaded = false;

// local methods

static void SaveLoad(FArchive& Ar)
{
	Ar << IGraphCache;
	Ar << MeshCache;
}

static FString SavePath() {
	return FPaths::ConvertRelativePathToFull(FPaths::RootDir()) + "\\PGC\\Data\\Cache.dat";
}

static void Save() {
	FBufferArchive Ar;

	SaveLoad(Ar);

	FFileHelper::SaveArrayToFile(Ar, *SavePath());
}

static void Load() {
	if (IsLoaded)
		return;

	// do this even if we fail, because we don't want to try over and over
	IsLoaded = true;

	TArray<uint8> data;

	if (FFileHelper::LoadFileToArray(data, *SavePath()))
	{
		FMemoryReader FromBinary = FMemoryReader(data, true); //true, free data after done
		FromBinary.Seek(0);

		SaveLoad(FromBinary);
	}
}

// --

TSharedPtr<IGraph> PGCCache::GetIGraph(uint32 hash)
{
	Load();

	if (IGraphCache.Contains(hash))
		return IGraphCache[hash].IGraph;

	return TSharedPtr<IGraph>();
}

void PGCCache::StoreIGraph(uint32 hash, const TSharedPtr<IGraph>& i_graph)
{
	check(!IGraphCache.Contains(hash));

	IGraphCache.Add(hash, IGraphVal{ i_graph });

	Save();
}

TSharedPtr<Mesh> PGCCache::GetMesh(const FString& generator_name, uint32 generator_hash,
	int num_divisions, bool triangularise, PGCDebugMode dm)
{
	Load();

	auto key = MeshKey{ generator_name, generator_hash, num_divisions, triangularise, dm };

	if (MeshCache.Contains(key))
		return MeshCache[key].Geom;

	return TSharedPtr<Mesh>();
}

TSharedPtr<TArray<FPGCNodePosition>> PGCCache::GetMeshNodes(const FString& generator_name, uint32 generator_hash,
	int num_divisions, bool triangularise, PGCDebugMode dm)
{
	Load();

	auto key = MeshKey{ generator_name, generator_hash, num_divisions, triangularise, dm };

	if (MeshCache.Contains(key))
		return MeshCache[key].Nodes;

	return TSharedPtr<TArray<FPGCNodePosition>>();
}

void PGCCache::StoreMesh(const FString& generator_name, uint32 generator_hash,
	int num_divisions, bool triangularise, PGCDebugMode dm,
	const TSharedPtr<Mesh>& mesh, const TSharedPtr<TArray<FPGCNodePosition>>& nodes)
{
	auto key = MeshKey{ generator_name, generator_hash, num_divisions, triangularise, dm };

	check(!MeshCache.Contains(key));

	MeshCache.Add(key, {mesh, nodes});

	Save();
}

}