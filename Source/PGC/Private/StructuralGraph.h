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
		const bool Rolling;

		SEdge(TWeakPtr<SNode> fromNode, TWeakPtr<SNode> toNode, double d0, bool rolling);

		TWeakPtr<SNode> OtherNode(const SNode* n) const
		{
			if (FromNode.Pin().Get() == n)
			{
				return ToNode;
			}

			check(n == ToNode.Pin().Get());
			return FromNode;
		}

		TWeakPtr<SNode> OtherNode(const TWeakPtr<SNode>& n) const
		{
			return OtherNode(n.Pin().Get());
		}
	};

	class SNode {
	public:
		TArray<TWeakPtr<SEdge>> Edges;

		const int Idx;

		const TSharedPtr<LayoutGraph::ParameterisedProfile> Profile;

		mutable FTransform CachedTransform;
		mutable bool Flipped;
		mutable bool Rolled;

		float Radius = 0.0f;

		SNode(int idx, const TSharedPtr<LayoutGraph::ParameterisedProfile> profile) : Idx(idx), Profile(profile), Flipped(false), Rolled(false)
		{
		}

		void FindRadius() {
			if (Profile.IsValid())
			{
				Radius = Profile->Radius();
			}
			else
			{
				for (const auto& e : Edges)
				{
					auto ep = e.Pin();

					// take our longest connector position
					Radius = FMath::Max(Radius, (Position - ep->OtherNode(this).Pin()->Position).Size());
				}

				// and back non-connected stuff off a little further
				Radius *= 1.5f;
			}
		}

		SNode(const SNode&) = delete;
		const SNode& operator=(const SNode&) = delete;
		virtual ~SNode() = default;

		void SetPosition(const FVector& pos)
		{
			Position = pos;
		}

		void SetUp(const FVector& up)
		{
			Up = up;
			Up.Normalize();
			RawUp = up;
		}

		void SetForward(const FVector& forward)
		{
			Forward = forward;
		}

		const FVector& GetPosition() const {
			return Position;
		}

		const FVector& GetUp() const {
			return Up;
		}

		const FVector& GetRawUp() const {
			return RawUp;
		}

		const FVector& GetForward() const {
			return Forward;
		}

		void AddToMesh(TSharedPtr<Mesh> mesh);

	private:
		FVector Position;
		FVector Up;
		FVector RawUp;			// this one may not be normalised, but is needed by optimisation for gradient calculations
		FVector Forward;
	};

	class SGraph {
		void RefreshTransforms() const;

	public:
		SGraph(TSharedPtr<LayoutGraph::Graph> input);

		// connect "from" to "to" directly with an edge and no regard to geometry...
		void Connect(const TSharedPtr<SNode> n1, const TSharedPtr<SNode> n2, double D0, bool flipping);
		// connect "from" to "to" via "Divs" intermediate back-to-back nodes
		void ConnectAndFillOut(const TSharedPtr<SNode> from_n, TSharedPtr<SNode> from_c,
			const TSharedPtr<SNode> to_n, TSharedPtr<SNode> to_c,
			int divs, int twists,
			float D0, const TArray<TSharedPtr<LayoutGraph::ParameterisedProfile>>& profiles);

		int FindNodeIdx(const TSharedPtr<SNode>& node) const;

		void MakeMesh(TSharedPtr<Mesh> mesh, bool skeleton_only);

		TArray<TSharedPtr<SNode>> Nodes;
		TArray<TSharedPtr<SEdge>> Edges;
	};
}
