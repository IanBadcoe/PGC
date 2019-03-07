#pragma once

#include "StructuralGraph.h"
#include "PGCGenerator.h"

namespace Cache {
using IGraph = StructuralGraph::IGraph;

class PGCCache
{
public:
	PGCCache() = delete;
	~PGCCache() = delete;

	static TSharedPtr<IGraph> GetIGraph(uint32 hash);
	static void StoreIGraph(uint32 hash, const TSharedPtr<IGraph>& i_graph);

	static TSharedPtr<Mesh> GetMesh(const FString& generator_name, uint32 generator_hash,
		int num_divisions, bool triangularise, PGCDebugMode dm);
	static TSharedPtr<TArray<FPGCNodePosition>> GetMeshNodes(const FString& generator_name, uint32 generator_hash,
		int num_divisions, bool triangularise, PGCDebugMode dm);
	static void StoreMesh(const FString& generator_name, uint32 generator_hash,
		int num_divisions, bool triangularise, PGCDebugMode dm,
		const TSharedPtr<Mesh>& mesh, const TSharedPtr<TArray<FPGCNodePosition>>& s_nodes);
};

}