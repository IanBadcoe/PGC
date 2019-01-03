#pragma once

#include "Runtime/Core/Public/Templates/SharedPointer.h"
#include "Runtime/Core/Public/Templates/SharedPointer.h"
#include "Core/Public/Templates/TypeHash.h"

#include "Mesh.h"
#include "NlOptWrapper.h"

#include "LayoutGraph.h"
#include "SplineUtil.h"

namespace StructuralGraph {
	class SNode;

	class SEdge {
	public:
		const TWeakPtr<SNode> FromNode;
		const TWeakPtr<SNode> ToNode;

		const double D0;
		const bool Flipping;

		SEdge(TWeakPtr<SNode> fromNode, TWeakPtr<SNode> toNode, double d0, bool flipping);
	};

	class SNode {
	public:
		TArray<TWeakPtr<SEdge>> Edges;

		FVector Position;
		FVector Up;
		FVector Forwards;

		int Idx;

		TSharedPtr<LayoutGraph::Node> LayoutNode;
		const LayoutGraph::ConnectorDef* Profile;

		SNode(int idx, const LayoutGraph::ConnectorDef* profile) : Idx(idx), Profile(profile) {}

		SNode(const SNode&) = delete;
		const SNode& operator=(const SNode&) = delete;
		virtual ~SNode() = default;

		void SetPosition(const FVector& pos, const FVector& up) { Position = pos; Up = up; }

		void AddToMesh(TSharedPtr<Mesh> mesh) {
			if (LayoutNode.IsValid())
			{
				LayoutNode->AddToMesh(mesh);
			}
		}
	};

	class SGraph {
		mutable TMap<const SNode*, FTransform> CachedTransforms;

		void RefreshTransforms() const;

	public:
		SGraph(TSharedPtr<LayoutGraph::Graph> input);

		// connect "from" to "to" directly with an edge and no regard to geometry...
		void Connect(const TSharedPtr<SNode> n1, const TSharedPtr<SNode> n2, double D0, bool flipping);
		// connect "from" to "to" via "Divs" intermediate back-to-back nodes
		void ConnectAndFillOut(const TSharedPtr<SNode> from_n, TSharedPtr<SNode> from_c,
			const TSharedPtr<SNode> to_n, TSharedPtr<SNode> to_c,
			int divs, int twists,
			float D0, const LayoutGraph::ConnectorDef* profile);

		int FindNodeIdx(const TSharedPtr<SNode>& node) const;

		void MakeMesh(TSharedPtr<Mesh> mesh);

		TArray<TSharedPtr<SNode>> Nodes;
		TArray<TSharedPtr<SEdge>> Edges;
	};

	struct JoinIdxs {
		int i;
		int j;
		double D0;

		static inline friend uint32 GetTypeHash(const JoinIdxs& j) {
			return ::GetTypeHash(j.i) ^ ::GetTypeHash(j.j);
		}

		bool operator==(const JoinIdxs& rhs) const {
			return i == rhs.i && j == rhs.j;
		}
	};

	struct AngleIdxs {
		int i;
		int j;
		int k;

		static inline friend uint32 GetTypeHash(const AngleIdxs& a) {
			return ::GetTypeHash(a.i) ^ ::GetTypeHash(a.j) ^ ::GetTypeHash(a.k);
		}

		bool operator==(const AngleIdxs& rhs) const {
			return i == rhs.i && j == rhs.j && k == rhs.k;
		}
	};

	class OptFunction : public NlOptIface {
		TSharedPtr<SGraph> G;
		TSet<JoinIdxs> Connected;
		TSet<AngleIdxs> Angles;

		void ApplyParams(const double* x, int n);

		double UnconnectedNodeNodeVal(const FVector& p1, const FVector& p2, float D0) const;
		// i is the index of one of the parameters of pGrad
		double UnconnectedNodeNodeGrad(int i, const FVector& pGrad, const FVector& pOther, float D0) const;

		double ConnectedNodeNodeVal(const FVector& p1, const FVector& p2, float D0) const;
		// i is the index of one of the parameters of pGrad
		double ConnectedNodeNodeGrad(int i, const FVector& pGrad, const FVector& pOther, float D0) const;

		// R = distance, D = optimal distance, N = power
		double LeonardJonesVal(double R, double D, int N) const;
		double LeonardJonesGrad(double R, double D, int N) const;

	public:
		OptFunction(TSharedPtr<SGraph> g);
		virtual ~OptFunction() = default;

		// Inherited via NlOptIface
		int GetSize() const;
		virtual double f(int n, const double * x, double * grad) override;
		virtual void GetInitialStepSize(double* steps, int n) const override;
		virtual void GetState(double* x, int n) const override;
		virtual void SetState(const double* x, int n) override;
	};

}
