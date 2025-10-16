#pragma once

// AABB type used by multiple files
struct AABB {
    float minX, minY, minZ;
    float maxX, maxY, maxZ;
};

// Prototypes that match the implementations in lightDemo.cpp
AABB computePackageAABB();
AABB computeDeliveryAABB(float center[3]);
AABB computeDroneAABB();
bool checkCollision(const AABB& a, const AABB& b);