#pragma once

#include "Runtime/Core/Public/Templates/SharedPointer.h"
#include "Runtime/Core/Public/Templates/SharedPointer.h"
#include "Core/Public/Templates/TypeHash.h"

#include "Mesh.h"
#include "NlOptWrapper.h"

#include "LayoutGraph.h"
#include "SplineUtil.h"
#include "ParameterisedProfile.h"
#include "IntermediateGraph.h"

namespace StructuralGraph {
	class SNode;

	using INode = IntermediateGraph::INode<TSharedPtr<SNode>>;
	using IEdge = IntermediateGraph::IEdge<TSharedPtr<SNode>>;

	struct IGraphMetaData
	{
		// a collection of data we need to pass to and from the IGraph before we build our own full connections
		struct ConnCurve {
			TSharedPtr<SNode> FromConn;
			TSharedPtr<SNode> ToConn;
			TSharedPtr<INode> Intermediate1Node;
			TSharedPtr<INode> Intermediate2Node;

			TSharedPtr<LayoutGraph::Edge> Edge;
		};

		TArray<ConnCurve> IntermediatePoints;
		double Energy;
	};
	using IGraph = IntermediateGraph::IGraph<TSharedPtr<SNode>, IGraphMetaData>;

	class SEdge {
	public:
		TWeakPtr<SNode> FromNode;
		TWeakPtr<SNode> ToNode;

		const double D0;

		SEdge(TWeakPtr<SNode> fromNode, TWeakPtr<SNode> toNode, double d0);

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
		struct OrdCon {
			TSharedPtr<SNode> ConNode;
			float Angle;
			FVector Axis;
			bool Flipped;

			bool UpsideDown = false;

			int QuartersMap[4]{ 3, 2, 1, 0 };		// if the connector is Flipped and/or Rolled its quarters change relative location
		};

	public:
		TArray<TWeakPtr<SEdge>> Edges;

		enum class Type {
			Junction,			// three types of node (so far) "Junction" is the centre of an original node from the layout graph
			JunctionConnector,	// "JunctionConnector" are the extra nodes we place around a junction to replace the layout connectors
			Connection			// "Connection" are the nodes inserted along each edge
		};

		const Type MyType;

		const TSharedPtr<Profile::ParameterisedProfile> Profile;

		mutable FTransform CachedTransform;

		float Radius = 0.0f;

		FVector Position;
		float Rotation;			// from the parent's up vector, projected into our "forward" plane
		FVector CachedUp;		// our up, initially just set, but after "MakeIntoDAG" calculated from parent up and "Rotation"
		FVector Forward;

		TSharedPtr<SNode> Parent;		// for the purposes of the DAG only...

		SNode(const TSharedPtr<Profile::ParameterisedProfile> profile, Type type) : Profile(profile), MyType(type)
		{
		}

		void FindRadius();

		SNode(const SNode&) = delete;
		const SNode& operator=(const SNode&) = delete;
		virtual ~SNode() = default;

		void AddToMesh(TSharedPtr<Mesh> mesh) const;
		void AddCeilingToMesh(TSharedPtr<Mesh> mesh, const TArray<OrdCon>& connectors, bool top) const;

		// after initial layout, forward is defined as a function of our, our parent's and other connected node positions
		void RecalcForward();
		void CalcRotation();
		void ApplyRotation();

		const FVector ProjectParentUp() const;
	};

	class ProfileSource {
	public:
		virtual TSharedPtr<Profile::ParameterisedProfile> GetProfile() const = 0;
		virtual TArray<TSharedPtr<Profile::ParameterisedProfile>> GetCompatibleProfileSequence(TSharedPtr<Profile::ParameterisedProfile> from,
			TSharedPtr<Profile::ParameterisedProfile> to, int steps) const = 0;
	};

	class SGraph {
	private:
		void RefreshTransforms() const;

		// for the purpose of propagating up vectors when optimizing, we need every node in the graph to have a unambiguous parent
		// to achieve this we:
		// 1) use Nodes[0] as the root
		// 2) set a parent pointer in all nodes except that one
		// 3) move the parent connection to be the first connection (where there is one...)
		// 4) reverse the parent <-> child connection, if appropriate, so that the forwards direction is towards the child
		// 5) recalculate the node forwards vectors for all nodes (which is part of what we want to do when interpreting
		//    optimization parameters, and hence part of the point of doing this...)
		void MakeIntoDAG();

		void MakeIntoDagInner(TSharedPtr<SNode> node, TSet<TSharedPtr<SNode>>& closed, TArray<TSharedPtr<SNode>>& new_order);

		void MakeMeshReal(TSharedPtr<Mesh> mesh) const;
		void MakeMeshSkeleton(TSharedPtr<Mesh> mesh) const;

		void CalcEdgeStartParams(const TSharedPtr<SNode>& from_c, const TSharedPtr<SNode>& to_c,
			const TSharedPtr<SNode>& from_n, const TSharedPtr<SNode>& to_n, float length, FVector& out1, FVector& out2);

		static double OptimizeIGraph(TSharedPtr<IGraph> graph, double precision, bool final);

		enum class DebugMode {
			Normal,
			Skeleton,
			IntermediateSkeleton
		};

		DebugMode DM = DebugMode::IntermediateSkeleton;

	public:
		SGraph(TSharedPtr<LayoutGraph::Graph> input, ProfileSource* profile_source);

		TSharedPtr<IGraph> IntermediateOptimize(TSharedPtr<LayoutGraph::Graph> input);

		// connect "from" to "to" directly with an edge and no regard to geometry...
		void Connect(const TSharedPtr<SNode> n1, const TSharedPtr<SNode> n2, double D0);
		// connect "from" to "to" via "Divs" intermediate back-to-back nodes
		void ConnectAndFillOut(TSharedPtr<SNode> from_c, TSharedPtr<SNode> to_c,
			const FVector& int1, const FVector& int2, 
			int divs, int twists,
			float D0, const TArray<TSharedPtr<Profile::ParameterisedProfile>>& profiles);

		int FindNodeIdx(const TWeakPtr<SNode>& node) const;

		void MakeMesh(TSharedPtr<Mesh> mesh) const;

		TArray<TSharedPtr<SNode>> Nodes;
		TArray<TSharedPtr<SEdge>> Edges;

		const ProfileSource* _ProfileSource;
	};
}
