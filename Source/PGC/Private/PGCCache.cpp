#include "PGCCache.h"

namespace Cache {

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

struct SGraphVal {
	TSharedPtr<Mesh> Geom;
	TSharedPtr<TArray<FPGCNodePosition>> Nodes;
};

// --

static TMap<uint32, TSharedPtr<IGraph>> IGraphCache;
static TMap<MeshKey, SGraphVal> MeshCache;

// --

TSharedPtr<IGraph> PGCCache::GetIGraph(uint32 hash)
{
	if (IGraphCache.Contains(hash))
		return IGraphCache[hash];

	return TSharedPtr<IGraph>();
}

void PGCCache::StoreIGraph(uint32 hash, const TSharedPtr<IGraph>& i_graph)
{
	check(!IGraphCache.Contains(hash));

	IGraphCache.Add(hash, i_graph);
}

TSharedPtr<Mesh> PGCCache::GetMesh(const FString& generator_name, uint32 generator_hash,
	int num_divisions, bool triangularise, PGCDebugMode dm)
{
	auto key = MeshKey{ generator_name, generator_hash, num_divisions, triangularise, dm };

	if (MeshCache.Contains(key))
		return MeshCache[key].Geom;

	return TSharedPtr<Mesh>();
}

TSharedPtr<TArray<FPGCNodePosition>> PGCCache::GetMeshNodes(const FString& generator_name, uint32 generator_hash,
	int num_divisions, bool triangularise, PGCDebugMode dm)
{
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
}

}