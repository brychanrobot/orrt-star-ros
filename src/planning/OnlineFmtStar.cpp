#include "OnlineFmtStar.hpp"
#include <stdio.h>
#include <cstdlib>
#include "../planning-utils/geom/Coord.hpp"
#include "../planning-utils/geom/utils.hpp"

using namespace std;

OnlineFmtStar::OnlineFmtStar(shared_ptr<vector<vector<bool>>> obstacleHash, shared_ptr<vector<shared_ptr<Rect>>> obstacleRects, double maxSegment, int width, int height,
                             bool usePseudoRandom, shared_ptr<Coord> start, double pruneRadius, double percentCoverage)
    : SamplingPlanner(obstacleHash, obstacleRects, maxSegment, width, height, usePseudoRandom, start, pruneRadius, percentCoverage) {
	this->name = "ofmtstar";
	this->open.push(this->root);

	for (int n = 0; n < this->nodeAddThreshold; n++) {
		auto point = this->randomOpenAreaPoint();

		auto node = make_shared<RrtNode>(point, nullptr, numeric_limits<double>::max());

		this->rtree.insert(RtreeValue(point.getBoostPoint(), node));
		this->numNodes++;
	}
}

bool OnlineFmtStar::isDoneBuilding() { return this->open.empty(); }

void OnlineFmtStar::sampleAndAdd() {
	auto bestOpenNode = this->open.top();
	this->open.pop();

	vector<RtreeValue> neighbors;
	this->getNeighbors(bestOpenNode->coord, this->rewireNeighborhood, neighbors);

	for (auto neighbor_tuple : neighbors) {
		auto neighbor = neighbor_tuple.second;
		if (neighbor->status == Status::Unvisited) {
			shared_ptr<RrtNode> bestParent;
			double bestCost;
			findBestOpenNeighbor(neighbor, bestParent, bestCost);
			if (bestParent && !this->lineIntersectsObstacle(neighbor->coord, bestParent->coord)) {
				bestParent->addChild(neighbor, bestCost);
				neighbor->status = Status::Open;
				neighbor->heuristic = neighbor->cumulativeCost;
				this->open.push(neighbor);
			}
		}
	}

	// bestOpenNode->printChildren();

	bestOpenNode->status = Status::Closed;
}

void OnlineFmtStar::findBestOpenNeighbor(shared_ptr<RrtNode> &node, shared_ptr<RrtNode> &bestNeighbor, double &bestCost) {
	vector<RtreeValue> neighbor_tuples;
	this->getNeighbors(node->coord, this->rewireNeighborhood, neighbor_tuples);

	double bestCumulativeCost = std::numeric_limits<double>::max();
	for (auto neighbor_tuple : neighbor_tuples) {
		auto neighbor = neighbor_tuple.second;
		auto cost = this->getCost(neighbor, node);
		if (neighbor->status == Status::Open && cost + neighbor->cumulativeCost < bestCumulativeCost) {
			bestCost = cost;
			bestCumulativeCost = cost + neighbor->cumulativeCost;
			bestNeighbor = neighbor;
		}
	}
}
