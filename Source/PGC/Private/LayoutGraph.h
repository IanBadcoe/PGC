#pragma once

#include "Runtime/Core/Public/Templates/SharedPointer.h"
#include "Runtime/Core/Public/Templates/SharedPointer.h"

#include "Mesh.h"
#include "NlOptWrapper.h"

namespace LayoutGraph {
	class Node;
	class ConnectorInst;

	struct ParameterisedProfile {
		static constexpr int VertsPerQuarter = 6;
		static constexpr int NumVerts = VertsPerQuarter * 4;

		//   3----4
		//   |    |
		//   2-1  |
		//     |  |
		//  ---0  |
		//		  |
		//		  |
		//		  5

		enum class VertTypes {
			RoadbedInner = 0,
			BarrierTopInner = 1,
			OverhangEndInner = 2,
			OverhangEndOuter = 3,
			BarrierTopOuter = 4,
			RoadbedOuter = 5
		};

		const float Width;					// width of roadbed
		const float BarrierHeights[4];		// height of side-barriers: upper-right, lower-right, lower-left, upper-left
		const float OverhangWidths[4];		// width of overhangs: upper-right, lower-right, lower-left, upper-left

		//const PGCEdgeType FollowingEdgeType[24];
		//const PGCEdgeType ExtrudedEdgeType[24];

		static const bool UsesBarrierHeight[NumVerts];
		static const bool UsesOverhangWidth[NumVerts];
		static const bool IsXOuter[NumVerts];
		static const bool IsYOuter[NumVerts];

		ParameterisedProfile(float width,
			float b0, float b1, float b2, float b3,
			float o0, float o1, float o2, float o3)
			: Width{ width }
			, BarrierHeights{ b0, b1, b2, b3 }
			, OverhangWidths{ o0, o1, o2, o3 }
		{
			CheckConsistent();
		}
		ParameterisedProfile(const ParameterisedProfile& rhs) = default;

		void CheckConsistent() const;

		// "rolled" means rotated in-plane 180 around centre, which maps quarter 0 -> 2 etc...
		bool IsCompatible(const ParameterisedProfile& other, bool rolled) const {
			// the only thing we cannot blend between is an overhang where there isn't even a barrier
			for (int i = 0; i < 4; i++)
			{
				const auto other_idx = rolled ? i ^ 2 : i;

				if (OverhangWidths[i] > 0 && other.BarrierHeights[other_idx] == 0)
					return false;

				if (BarrierHeights[i] == 0 && other.OverhangWidths[other_idx] > 0)
					return false;
			}

			return true;
		}

		static int GetQuarterIdx(int idx)
		{
			return idx / VertsPerQuarter;
		}

		FVector2D GetPoint(int idx) const;

		FVector GetTransformedVert(int vert_idx, const FTransform& total_trans) const;
		FVector GetTransformedVert(VertTypes type, int quarter_idx, const FTransform& total_trans) const;
	};

	class Edge {
	public:
		const TWeakPtr<Node> FromNode;
		const TWeakPtr<Node> ToNode;
		const TWeakPtr<ConnectorInst> FromConnector;
		const TWeakPtr<ConnectorInst> ToConnector;

		int Divs;					// how many points along the spline required for smoothness, swivels etc...
		int Twists;					// how many +ve or -ve half-twists to make along the edge

		// empty means use profile of start node
		// otherwise divide these profiles evenly over the length of the connection
		TArray <TSharedPtr<ParameterisedProfile>> Profiles;

			Edge(TWeakPtr<Node> fromNode, TWeakPtr<Node> toNode,
				TWeakPtr<ConnectorInst> fromConnector, TWeakPtr<ConnectorInst> toConnector);
	};

	class ConnectorInst {
	public:
		const TSharedPtr<ParameterisedProfile> Profile;

		FTransform Transform;

		ConnectorInst(const TSharedPtr<ParameterisedProfile>& profile, const FVector& pos, FVector forward, FVector up);
	};

	class Node {
	public:
		using ConnectorArray = TArray<TSharedRef<ConnectorInst>>;

		TArray<TWeakPtr<Edge>> Edges;
		const ConnectorArray Connectors;

		FTransform Position;

		Node(const ConnectorArray& connectors)
			: Connectors(connectors)
		{
			// every connector is potentially the start of an edge
			Edges.AddDefaulted(Connectors.Num());
		}

		Node() = delete;
		Node(const Node&) = delete;
		const Node& operator=(const Node&) = delete;
		virtual ~Node() = default;

		int FindConnectorIdx(const TSharedPtr<ConnectorInst>& conn) const;
	};

	// built-in node type, used to connect edges end-to-end when filling-out their geometry
	class BackToBack : public Node {
	public:
		BackToBack(const TSharedPtr<ParameterisedProfile>& profile);
		virtual ~BackToBack() = default;

	private:
		static const ConnectorArray ConnectorData;
	};

	class Graph {
	public:
		const float SegLength;

		Graph(float segLength);

//		void MakeMesh(TSharedPtr<Mesh> mesh) const;
		// connect "from" to "to" directly with an edge and no regard to geometry...
		void Connect(int nodeFrom, int nodeFromconnector,
			int nodeTo, int nodeToconnector,
			int divs = 10, int twists = 0,
			const TArray<TSharedPtr<ParameterisedProfile>>& profiles = TArray<TSharedPtr<ParameterisedProfile>>());

		int FindNodeIdx(const TSharedPtr<Node>& node) const;

		const TArray<TSharedPtr<Node>>& GetNodes() const { return Nodes; }
		const TArray<TSharedPtr<Edge>>& GetEdges() const { return Edges; }

	protected:
		TArray<TSharedPtr<Node>> Nodes;
		TArray<TSharedPtr<Edge>> Edges;
	};
}
