#include "SamplingPlanner.hpp"

#include <stdio.h>
#include <cstdlib>
#include <ctime>
#include <limits>
#include <vector>

#include "geom/Coord.hpp"
#include "geom/utils.hpp"

using namespace std;

SamplingPlanner::SamplingPlanner(vector<vector<bool>> *obstacleHash, vector<Rect *> *obstacleRects, double maxSegment, int width, int height,
                                 bool usePseudoRandom)
    : Planner(obstacleHash, obstacleRects, width, height, usePseudoRandom) {
	this->maxSegment = maxSegment;
	this->rewireNeighborhood = maxSegment * 6;
	this->nodeAddThreshold = 0.02 * width * height;

	this->root->status = Status::Open;
	this->rtree.insert(RtreeValue(this->root->coord, this->root));

	this->rtree.insert(RtreeValue(this->endNode->coord, this->endNode));
}

void SamplingPlanner::sampleWithRewire() {
	auto point = this->randomOpenAreaPoint();
	vector<Node *> neighbors;
	Node *bestNeighbor;
	this->findBestNeighborWithoutCost(point, bestNeighbor, neighbors);
	for (auto neighbor : neighbors) {
		if (neighbor->status == Status::Closed && neighbor != bestNeighbor) {
			auto cost = this->getCost(bestNeighbor, neighbor);
			if (bestNeighbor->cumulativeCost + cost < neighbor->cumulativeCost &&
			    !this->lineIntersectsObstacle(neighbor->coord, bestNeighbor->coord)) {
				neighbor->rewire(bestNeighbor, cost);
			}
		}
	}
}

void SamplingPlanner::moveStart(double dx, double dy) {
	if (dx != 0 || dy != 0) {
		Coord point(clamp(this->root->coord.x() + dx, 0, this->width - 1), clamp(this->root->coord.y() + dy, 0, this->height - 1));

		if (!this->obstacleHash->at((int)point.y()).at((int)point.x())) {
			auto newRoot = new Node(point, NULL, 0.0);
			newRoot->status = Status::Closed;
			this->rtree.insert(RtreeValue(newRoot->coord, newRoot));

			auto rtr_cost = this->getCost(newRoot, this->root);

			this->root->rewire(newRoot, rtr_cost);
			this->root = newRoot;

			// maybe this doesn't really add value
			std::vector<RtreeValue> neighbor_tuples;
			this->getNeighbors(newRoot->coord, this->rewireNeighborhood, neighbor_tuples);
			for (auto neighbor_tuple : neighbor_tuples) {
				auto neighbor = neighbor_tuple.second;
				if (neighbor != newRoot && neighbor->status == Status::Closed && !this->lineIntersectsObstacle(newRoot->coord, neighbor->coord)) {
					auto cost = this->getCost(newRoot, neighbor);
					neighbor->rewire(newRoot, cost);
				}
			}
		}
	}
}

void SamplingPlanner::replan(Coord &newEndpoint) {
	this->endNode = this->getNearestNeighbor(newEndpoint);
	this->refreshBestPath();
}

Node *SamplingPlanner::getNearestNeighbor(Coord &p) {
	vector<RtreeValue> result;
	this->rtree.query(boost::geometry::index::nearest((point)p, 1), back_inserter(result));
	return result[0].second;
}

void SamplingPlanner::getNeighbors(Coord center, double radius, vector<RtreeValue> &results) {
	box query_box(point(center.x() - radius, center.y() - radius), point(center.x() + radius, center.y() + radius));
	this->rtree.query(boost::geometry::index::intersects(query_box), back_inserter(results));
}

void SamplingPlanner::findBestNeighborWithoutCost(Coord point, Node *&bestNeighbor, vector<Node *> &neighbors) {
	vector<RtreeValue> neighbor_tuples;
	this->getNeighbors(point, this->rewireNeighborhood, neighbor_tuples);

	double bestCumulativeCost = 9999999999999999;  /// std::numeric_limits<double>::max();
	// double bestCost = std::numeric_limits<double>::max();

	for (auto neighbor_tuple : neighbor_tuples) {
		auto neighbor = neighbor_tuple.second;
		neighbors.push_back(neighbor);
		// auto cost = this->getCost(neighbor, node);
		if (neighbor->status == Status::Closed && neighbor->cumulativeCost < bestCumulativeCost) {
			// bestCost = cost;
			bestCumulativeCost = neighbor->cumulativeCost;
			bestNeighbor = neighbor;
		}
	}
}

void SamplingPlanner::findBestNeighbor(Coord point, Node *&bestNeighbor, double &bestCost, vector<Node *> &neighbors, vector<double> &neighborCosts) {
	vector<RtreeValue> neighbor_tuples;
	this->getNeighbors(point, this->rewireNeighborhood, neighbor_tuples);

	double bestCumulativeCost = 999999999999999;  // std::numeric_limits<double>::max();
	// double bestCost = std::numeric_limits<double>::max();

	for (auto neighbor_tuple : neighbor_tuples) {
		auto neighbor = neighbor_tuple.second;
		neighbors.push_back(neighbor);
		auto cost = this->getCost(neighbor->coord, point);
		neighborCosts.push_back(cost);
		if (neighbor->status == Status::Closed && (neighbor->cumulativeCost + cost < bestCumulativeCost)) {
			bestCost = cost;
			bestCumulativeCost = neighbor->cumulativeCost + cost;
			bestNeighbor = neighbor;
		}
	}
}