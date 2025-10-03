//
// AVT 2025: Texturing with Phong Shading and Text rendered with TrueType library
// The text rendering was based on https://dev.to/shreyaspranav/how-to-render-truetype-fonts-in-opengl-using-stbtruetypeh-1p5k
// You can also learn an alternative with FreeType text: https://learnopengl.com/In-Practice/Text-Rendering
// This demo was built for learning purposes only.
// Some code could be severely optimised, but I tried to
// keep as simple and clear as possible.


#include <math.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <array>
#include <cmath>

// include GLEW to access OpenGL 3.3 functions
#include <GL/glew.h>

// GLUT is the toolkit to interface with the OS
#include <GL/freeglut.h>

#include <IL/il.h>

#include "renderer.h"
#include "shader.h"
#include "mathUtility.h"
#include "model.h"
#include "texture.h"
#include <algorithm>

#include <vector>
#include <utility>  // for std::pair
#include <ctime>    // for seeding rand()

using namespace std;

#define CAPTION "AVT - Lab Assignment 1"
int WindowHandle = 0;
int WinX = 1024, WinY = 768;

unsigned int FrameCount = 0;

//File with the font
const string fontPathFile = "fonts/arial.ttf";

//Object of class gmu (Graphics Math Utility) to manage math and matrix operations
gmu mu;

//Object of class renderer to manage the rendering of meshes and ttf-based bitmap text
Renderer renderer;
	
// Camera Position
float camX, camY, camZ;

// Camera Spherical Coordinates
float alpha = 57.0f, beta = 18.0f;
float r = 45.0f;

// Key states arrays
bool keyStates[256] = { false };
bool specialKeyStates[256] = { false };


// Camera modes
enum CameraMode { FOLLOW, THIRD, TOP_ORTHO, TOP_PERSPECTIVE };
CameraMode cameraMode = THIRD;

// Third-person/orbit camera parameters (tweak to taste)
float camOrbitRadius    = 7.5f;   // distance from drone
float camOrbitHeight    = 3.0f;    // vertical offset above drone
float camOrbitSpeed     = 0.8f;    // radians per second (positive = orbit CCW)
float camOrbitAngle     = 3.14159265f;    // current angle (radians)
float camOrbitLookHeight = 1.5f;   // where camera looks relative to drone.y
bool camOrbitFollowYaw = true; // camera rotates with drone yaw (child-like)

// Orbit control
const float CAM_ORBIT_SENSITIVITY_X = -0.01f; // radians per pixel horizontal
const float CAM_ORBIT_SENSITIVITY_Y = 0.05f; // world units per pixel vertical
const float CAM_ORBIT_MIN_HEIGHT = 1.0f;
const float CAM_ORBIT_MAX_HEIGHT = 80.0f;

// Drone Scale
const float DRONE_WIDTH = 1.25f/2.0f;
const float DRONE_DEPTH = 0.75f/2.0f;
const float DRONE_HEIGHT = 0.15f/2.0f;

// Buildings Scale
const float BUILDING_WIDTH = 10.0f;
const float BUILDING_DEPTH = 10.0f;

// Dome parameters
const float DOME_RADIUS = 100.0f;
const float DOME_CENTER_X = 35.0f;
const float DOME_CENTER_Y = 0.0f;
const float DOME_CENTER_Z = 35.0f;
const float DOME_TRANSPARENCY = 0.75f;

// Mouse Tracking Variables
int startX, startY, tracking = 0;

// Frame counting and FPS computation
long myTime,timebase = 0,frame = 0;
char s[32];

float lightPos[4] = {4.0f, 15.0f, 2.0f, 1.0f};

// Directional light
float dirLightDir[4] = { -0.2f, -1.0f, -0.3f, 0.0f }; // directional light (w=0)

// -- Light counts (single place to change) --
#define NUM_POINT_LIGHTS 7
#define NUM_SPOT_LIGHTS  4

// Runtime constants (use these when an `int` is required)
const int kNumPointLights = NUM_POINT_LIGHTS;
const int kNumSpotLights  = NUM_SPOT_LIGHTS;

// Point lights
float pointLightPos[NUM_POINT_LIGHTS][4] = {
    { 5.0f,  5.0f,  5.0f, 1.0f },
    {-5.0f,  5.0f,  5.0f, 1.0f },
    { 5.0f,  5.0f, -5.0f, 1.0f },
    {-5.0f,  5.0f, -5.0f, 1.0f },
    { 0.0f, 10.0f,  0.0f, 1.0f },
    { 0.0f,  2.0f,  5.0f, 1.0f },
	{ 15.0f,  15.0f,  5.0f, 1.0f }	
};

// Spotlights
float spotLightPos[NUM_SPOT_LIGHTS][4] = {
    { 25.0f, 12.0f, -10.0f, 1.0f },
    { 5.0f, 12.0f, -10.0f, 1.0f },
	{ 25.0f, 12.0f, 20.0f, 1.0f },
    { 5.0f, 12.0f, 20.0f, 1.0f }
};
float spotLightDir[NUM_SPOT_LIGHTS][3] = {
    { 0.0f, -1.0f, 0.5f },
    { 0.0f, -1.0f, 0.5f },
	{ 0.0f, -1.0f, -0.5f },
    { 0.0f, -1.0f, -0.5f }
};


//Spotlight
bool spotlight_mode = false;
float coneDir[4] = { 0.0f, -0.0f, -1.0f, 0.0f };

bool fontLoaded = false;

float cubeHeights[6][6]; // Store heights for each cube
std::vector<std::pair<int,int>> openAreas;

struct vec3 {
	float x, y, z;
	vec3() : x(0), y(0), z(0) {}
	vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
	vec3 operator+(const vec3& b) const { return vec3(x + b.x, y + b.y, z + b.z); }
	vec3 operator-(const vec3& b) const { return vec3(x - b.x, y - b.y, z - b.z); }
};

// Helper: 3x3 matrix struct
struct Mat3 {
	float m[3][3];
	vec3 operator*(const vec3& v) const {
		return vec3(
			m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
			m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
			m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z
		);
	}
};

vector<vec3> drone_parts_position = {
	{-0.625f, 0.0f, -0.375f},   // Main body (cube)
	{0.625f, 0.0f, 0.5f},
	{0.625f, 0.0f, -0.5f},
	{-0.625f, 0.0f, -0.5f},
	{-0.625f, 0.0f, 0.5f}
};

struct Drone {
    float pos[3] = {1.0f, 10.0f, -2.0f}; // Initial position
	float dir[3] = {0.0f, 1.0f, 0.0f}; // Initial direction (pointing up)
	float velocity[3] = {0.0f, 0.0f, 0.0f}; // Initial velocity
	float collisionVel[3] = {0.0f, 0.0f, 0.0f}; // Velocity of collision normal
    float throttle = 0.0f;             // current throttle level
    float yaw = 0.0f;      // Rotation around Y (degrees)
    float pitch = 0.0f;    // Forward/backward tilt (degrees)
    float roll = 0.0f;     // Left/right tilt (degrees)
};
Drone drone;

struct Bird {
    float pos[3];
    float velocity[3];
    float rotation; // degrees
    float rotSpeed; // degrees/sec
    Bird(float x, float y, float z, float vx, float vy, float vz, float rs)
        : rotation(0.0f), rotSpeed(rs) {
        pos[0] = x; pos[1] = y; pos[2] = z;
        velocity[0] = vx; velocity[1] = vy; velocity[2] = vz;
    }
};
std::vector<Bird> birds;

struct AABB {
	float minX, minY, minZ;
	float maxX, maxY, maxZ;
};

/// ::::::::::::::::::::::::::::::::::::::::::::::::CALLBACK FUNCIONS:::::::::::::::::::::::::::::::::::::::::::::::::://///

void timer(int value)
{
	std::ostringstream oss;
	oss << CAPTION << ": " << FrameCount << " FPS @ (" << WinX << "x" << WinY << ")";
	std::string s = oss.str();
	glutSetWindow(WindowHandle);
	glutSetWindowTitle(s.c_str());
    FrameCount = 0;
    glutTimerFunc(1000, timer, 0);
}

void changeSize(int w, int h) {

	float ratio;
	// Prevent a divide by zero, when window is too short
	if(h == 0)
		h = 1;
	// set the viewport to be the entire window
	glViewport(0, 0, w, h);
	// set the projection matrix
	ratio = (1.0f * w) / h;
	mu.loadIdentity(gmu::PROJECTION);
	mu.perspective(53.13f, ratio, 0.1f, 1000.0f);
}

// -----------------------------------------------------------
//
// Update Birds State
//

struct BirdData {
    float pos[3];
    float velocity[3];
    float rotSpeed;
};

// Helper function to generate a new bird spawn around the drone
BirdData spawnBird(float minDistFromDrone, float maxDistFromDrone) {
    BirdData b;

    // Random angles for uniform direction on sphere
    float theta = 2.0f * 3.14159265f * (rand() / (float)RAND_MAX); 
    float cosPhi = 2.0f * (rand() / (float)RAND_MAX) - 1.0f;      
    float sinPhi = sqrtf(1.0f - cosPhi * cosPhi);

    // Radius sampled uniformly in spherical shell volume
    float u = rand() / (float)RAND_MAX;
    float minR3 = minDistFromDrone * minDistFromDrone * minDistFromDrone;
    float maxR3 = maxDistFromDrone * maxDistFromDrone * maxDistFromDrone;
    float r_samp = cbrtf(u * (maxR3 - minR3) + minR3);

    // Convert spherical → cartesian, offset by drone position
    b.pos[0] = drone.pos[0] + r_samp * sinPhi * cosf(theta);
    b.pos[1] = drone.pos[1] + r_samp * cosPhi;
    b.pos[2] = drone.pos[2] + r_samp * sinPhi * sinf(theta);

    // Inward direction
    float dir[3] = { drone.pos[0] - b.pos[0], drone.pos[1] - b.pos[1], drone.pos[2] - b.pos[2] };
    float len = sqrt(dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2]);
    dir[0] /= len; dir[1] /= len; dir[2] /= len;

    // Random spread (~30°)
    float spreadAngle = 0.5236f; 
    float randAngle = (rand() / (float)RAND_MAX) * spreadAngle;

    float axis[3] = { -dir[1], dir[0], 0 };
    float axisLen = sqrt(axis[0]*axis[0] + axis[1]*axis[1] + axis[2]*axis[2]);
    if (axisLen > 0.0001f) {
        axis[0] /= axisLen; axis[1] /= axisLen; axis[2] /= axisLen;
    } else {
        axis[0] = 1; axis[1] = 0; axis[2] = 0;
    }

    float cosA = cos(randAngle);
    float sinA = sin(randAngle);
    float dot = dir[0]*axis[0] + dir[1]*axis[1] + dir[2]*axis[2];
    float newDir[3];
    newDir[0] = cosA * dir[0] + sinA * (axis[1]*dir[2] - axis[2]*dir[1]) + (1-cosA)*dot*axis[0];
    newDir[1] = cosA * dir[1] + sinA * (axis[2]*dir[0] - axis[0]*dir[2]) + (1-cosA)*dot*axis[1];
    newDir[2] = cosA * dir[2] + sinA * (axis[0]*dir[1] - axis[1]*dir[0]) + (1-cosA)*dot*axis[2];

    // Velocity
    float speed = 10.0f;
    b.velocity[0] = speed * newDir[0];
    b.velocity[1] = speed * newDir[1];
    b.velocity[2] = speed * newDir[2];

    // Rotation speed
    b.rotSpeed = 90.0f * ((rand() / (float)RAND_MAX) - 0.5f);

    return b;
}


// Initialize birds with random positions,velocities, and rotation speeds
void initBirds(int numBirds) {
    birds.clear();
    const float minDist = 30.0f;
    const float maxDist = 60.0f;

    for (int i = 0; i < numBirds; ++i) {
        BirdData d = spawnBird(minDist, maxDist);
        birds.emplace_back(d.pos[0], d.pos[1], d.pos[2],
                           d.velocity[0], d.velocity[1], d.velocity[2],
                           d.rotSpeed);
    }
}


// Update bird positions and rotations
void updateBirds(float dt) {
    const float maxDistFromDrone = 60.0f; // radius around drone

    // Speed multiplier after 30 seconds
    int elapsedMs = glutGet(GLUT_ELAPSED_TIME);
    float speedMultiplier = (elapsedMs > 30000) ? 1.3f : 1.0f;

    for (auto& b : birds) {
        // Update position and rotation
        for (int i = 0; i < 3; ++i)
            b.pos[i] += b.velocity[i] * dt * speedMultiplier;
        b.rotation += b.rotSpeed * dt;
        if (b.rotation > 360.0f) b.rotation -= 360.0f;
        if (b.rotation < 0.0f) b.rotation += 360.0f;

        float dx = b.pos[0] - drone.pos[0];
        float dy = b.pos[1] - drone.pos[1];
        float dz = b.pos[2] - drone.pos[2];
        float dist = sqrt(dx*dx + dy*dy + dz*dz);
		if (dist > maxDistFromDrone) {
			BirdData d = spawnBird(30.0f, maxDistFromDrone);
			b.pos[0] = d.pos[0]; b.pos[1] = d.pos[1]; b.pos[2] = d.pos[2];
			b.velocity[0] = d.velocity[0]; b.velocity[1] = d.velocity[1]; b.velocity[2] = d.velocity[2];
			b.rotation = 0.0f;
			b.rotSpeed = d.rotSpeed;
		}

		// Calculate collision
		float dist_x = drone.pos[0] - b.pos[0];
		float dist_y = drone.pos[1] - b.pos[1];
		float dist_z = drone.pos[2] - b.pos[2];
    	if (std::sqrt(dist_x*dist_x + dist_y*dist_y + dist_z*dist_z) <= 1.0f) {
			drone = Drone(); // Reset drone
		} 
    }
}


// ------------------------------------------------------------
//
// Collision Detection
//

// Build rotation matrix from yaw (Y), pitch (Z), roll (X) in degrees
Mat3 getDroneRotationMatrix(float yaw, float pitch, float roll) {
	const float DEG2RAD = 3.14159265f / 180.0f;
	float y = yaw * DEG2RAD;
	float p = pitch * DEG2RAD;
	float r = roll * DEG2RAD;

	// Yaw (Y axis)
	Mat3 Ry = { {
		{ cos(y), 0, sin(y) },
		{ 0,      1, 0      },
		{ -sin(y),0, cos(y) }
	} };
	// Pitch (Z axis)
	Mat3 Rz = { {
		{ cos(p), -sin(p), 0 },
		{ sin(p),  cos(p), 0 },
		{ 0,       0,      1 }
	} };
	// Roll (X axis)
	Mat3 Rx = { {
		{ 1, 0,      0     },
		{ 0, cos(r), -sin(r)},
		{ 0, sin(r),  cos(r)}
	} };

	// Combined: R = Ry * Rz * Rx (Y * Z * X)
	Mat3 R = Ry;
	// R = Ry * Rz
	Mat3 temp;
	for (int i = 0;i < 3;++i) for (int j = 0;j < 3;++j) {
		temp.m[i][j] = 0;
		for (int k = 0;k < 3;++k) temp.m[i][j] += Ry.m[i][k] * Rz.m[k][j];
	}
	// R = (Ry*Rz) * Rx
	Mat3 result;
	for (int i = 0;i < 3;++i) for (int j = 0;j < 3;++j) {
		result.m[i][j] = 0;
		for (int k = 0;k < 3;++k) result.m[i][j] += temp.m[i][k] * Rx.m[k][j];
	}
	return result;
}

// Compute the rotated AABB for the drone
AABB computeDroneAABB() {
	// Local half-sizes
	float halfWidth = DRONE_WIDTH / 2.0f + 0.1f;
	float halfHeight = DRONE_HEIGHT / 2.0f;
	float halfDepth = DRONE_DEPTH / 2.0f + 0.1f;

	// 8 corners in local space
	std::array<vec3, 8> localCorners = { {
		vec3(-halfWidth, -halfHeight, -halfDepth),
		vec3(halfWidth, -halfHeight, -halfDepth),
		vec3(-halfWidth,  halfHeight, -halfDepth),
		vec3(halfWidth,  halfHeight, -halfDepth),
		vec3(-halfWidth, -halfHeight,  halfDepth),
		vec3(halfWidth, -halfHeight,  halfDepth),
		vec3(-halfWidth,  halfHeight,  halfDepth),
		vec3(halfWidth,  halfHeight,  halfDepth)
	} };

	// Get rotation matrix from drone's yaw, pitch, roll
	Mat3 R = getDroneRotationMatrix(drone.yaw, drone.pitch, drone.roll);

	// Transform corners to world space
	vec3 pos(drone.pos[0], drone.pos[1], drone.pos[2]);
	std::array<vec3, 8> worldCorners;
	for (int i = 0; i < 8; ++i)
		worldCorners[i] = R * localCorners[i] + pos;

	// Find min/max
	float minX = worldCorners[0].x, maxX = worldCorners[0].x;
	float minY = worldCorners[0].y, maxY = worldCorners[0].y;
	float minZ = worldCorners[0].z, maxZ = worldCorners[0].z;
	for (int i = 1; i < 8; ++i) {
		minX = std::min(minX, worldCorners[i].x);
		maxX = std::max(maxX, worldCorners[i].x);
		minY = std::min(minY, worldCorners[i].y);
		maxY = std::max(maxY, worldCorners[i].y);
		minZ = std::min(minZ, worldCorners[i].z);
		maxZ = std::max(maxZ, worldCorners[i].z);
	}

	AABB box;
	box.minX = minX; box.maxX = maxX;
	box.minY = minY; box.maxY = maxY;
	box.minZ = minZ; box.maxZ = maxZ;
	return box;
}

AABB computeBuildingAABB(int i, int j) {
    AABB box;
    float width = 10.0f;
    float depth = 10.0f;
    float height = cubeHeights[i][j];

    float x = -20.0f + i * 20.0f;
    float z = -20.0f + j * 20.0f;

    box.minX = x;
    box.maxX = x + width;
    box.minY = 0.0f;
    box.maxY = height;
    box.minZ = z;
    box.maxZ = z + depth;
    return box;
}

bool checkCollision(const AABB &a, const AABB &b) {
    return (a.minX <= b.maxX && a.maxX >= b.minX) &&
           (a.minY <= b.maxY && a.maxY >= b.minY) &&
           (a.minZ <= b.maxZ && a.maxZ >= b.minZ);
}

void computeNormalAfterCollision(float buildingPos[3]) {
	float collision_vector[3] = {fabs(drone.pos[0] - buildingPos[0]), 0.0f, fabs(drone.pos[2] - buildingPos[2])};
	int direction_index = 0;

	if (collision_vector[0] < BUILDING_WIDTH/2.0f && collision_vector[2] < BUILDING_DEPTH/2.0f) {
		direction_index = 1; // Y axis
	} else {
		direction_index = collision_vector[0] <= collision_vector[2] ? 2 : 0; // Z or X axis
	}
	
	printf("Collision Vector: (%.2f, %.2f, %.2f)\n", collision_vector[0], collision_vector[1], collision_vector[2]);
	printf("Direction of Impact: %d\n", direction_index);

	drone.collisionVel[direction_index] = - drone.velocity[direction_index] * 2.0f;
	printf("Collision Velocity: (%.2f, %.2f, %.2f)\n", drone.collisionVel[0], drone.collisionVel[1], drone.collisionVel[2]);
}

void handleCollisions() {	
	AABB droneBox = computeDroneAABB();

	// Check against ground (y = 0.0f)
	if (droneBox.minY <= 0.0f) {
		std::cout << "Collision with ground\n";
		drone.collisionVel[1] = - drone.velocity[1] * 2.0f;
		return;
	}

	bool hasCollided = false;

	// Check against buildings
	for (int i = 0; i < 6; ++i) {
		if(hasCollided) break;
		for (int j = 0; j < 6; ++j) {
			bool skip = false;
			for (auto &p : openAreas) {
				if (p.first == i && p.second == j) {
					skip = true;
					break;
				}
			}
			if (skip || ((i == 1 || i == 2) && j == 1)) continue;

			AABB buildingBox = computeBuildingAABB(i, j);
			if (checkCollision(droneBox, buildingBox)) {
				std::cout << "Collision with building at (" << i << "," << j << ")\n";
				float buildingPos[3] = {-15.0f + i * 20.0f, 0.0f, -15.0f + j * 20.0f};
				computeNormalAfterCollision(buildingPos);
				return;
			}
		}
	}
}


// ------------------------------------------------------------
//
// Update Drone State
//

void updateDroneState(float dt) {
	// Parameters (tweak to taste)
    const float maxThrottle = 15.0f;   // upward accel when throttleCmd == 1
    const float maxTilt = 46.0f;            // degrees: maximum pitch/roll allowed
	const float dampingFactor = 0.992f; // Reduce velocity by 50% on collision

	
	// Limits
	if (drone.yaw > 360.0f || drone.yaw < -360.0f) drone.yaw = 0.0f;

	if (drone.pitch > maxTilt) drone.pitch = maxTilt;
	if (drone.pitch < -maxTilt) drone.pitch = -maxTilt;

	if (drone.roll  > maxTilt) drone.roll  = maxTilt;
	if (drone.roll  < -maxTilt) drone.roll  = -maxTilt;

	if (drone.throttle > maxThrottle) drone.throttle = maxThrottle;
	if (drone.throttle < -maxThrottle) drone.throttle = -maxThrottle;

	float pitchRad = drone.pitch * 3.14159265f / 180.0f;
	float rollRad  = drone.roll  * 3.14159265f / 180.0f;
	float yawRad   = drone.yaw   * 3.14159265f / 180.0f; 

	const float maxTiltRad = maxTilt * 3.14159265f / 180.0f;

    if (dt <= 0.0f) return;

	drone.dir[0] = cos(yawRad) * (- sin(pitchRad) / sin(maxTiltRad)) + sin(yawRad) * (sin(rollRad) / sin(maxTiltRad));
	drone.dir[1] = cos(3.14159f / 2 * fabs(pitchRad / maxTiltRad)) * cos(3.14159f / 2 * fabs(rollRad / maxTiltRad));
	drone.dir[2] = cos(yawRad) * (sin(rollRad) / sin(maxTiltRad)) + sin(yawRad) * (sin(pitchRad) / sin(maxTiltRad));

	drone.velocity[0] = (10 + drone.throttle) * drone.dir[0];
	drone.velocity[1] = (drone.throttle == 0.0f) ? drone.velocity[1] * dampingFactor : drone.throttle * drone.dir[1];
	drone.velocity[2] = (10 + drone.throttle) * drone.dir[2];

	// Check for collisions 
	handleCollisions();

	// Integrate position
	drone.pos[0] += (drone.velocity[0] + drone.collisionVel[0]) * dt;
    drone.pos[1] += (drone.velocity[1] + drone.collisionVel[1]) * dt;
    drone.pos[2] += (drone.velocity[2] + drone.collisionVel[2]) * dt;

	drone.collisionVel[0] *= dampingFactor; if (fabs(drone.collisionVel[0]) < 0.01f) drone.collisionVel[0] = 0.0f;
	drone.collisionVel[1] *= dampingFactor; if (fabs(drone.collisionVel[1]) < 0.01f) drone.collisionVel[1] = 0.0f;
	drone.collisionVel[2] *= dampingFactor; if (fabs(drone.collisionVel[2]) < 0.01f) drone.collisionVel[2] = 0.0f;
}


// ------------------------------------------------------------
//
// Events from the Keyboard
//

void applyKeyInputs(float dt) {
    // Normal keys
    if (keyStates['w'] || keyStates['W']) {
        drone.throttle +=  15.0f * dt;
    }
    if (keyStates['s'] || keyStates['S']) {
        drone.throttle -=  15.0f * dt;
    }
    if (keyStates['a'] || keyStates['A']) {
        drone.yaw += 60.0f * dt;
    }
    if (keyStates['d'] || keyStates['D']) {
        drone.yaw -= 60.0f * dt;
    }

    // Special keys
    if (specialKeyStates[GLUT_KEY_UP]) {
        drone.pitch -= 60.0f * dt;
    }
    if (specialKeyStates[GLUT_KEY_DOWN]) {
        drone.pitch += 60.0f * dt;
    }
    if (specialKeyStates[GLUT_KEY_LEFT]) {
        drone.roll  -= 60.0f * dt;
    }
    if (specialKeyStates[GLUT_KEY_RIGHT]) {
        drone.roll  += 60.0f * dt;
    }
}

void processKeysDown(unsigned char key, int xx, int yy) {
	keyStates[key] = true;

    switch (key) {
        case 27: glutLeaveMainLoop(); break;
        case 'c':
            printf("Camera Spherical Coordinates (%f, %f, %f)\n", alpha, beta, r);
            break;
        case 'l':
            spotlight_mode = !spotlight_mode;
            printf("Spotlight toggled: %d\n", spotlight_mode);
            break;
        case 'r':
            alpha = 57.0f; beta = 18.0f; r = 45.0f;
            camX = r * sin(alpha * 3.14f / 180.0f) * cos(beta * 3.14f / 180.0f);
            camZ = r * cos(alpha * 3.14f / 180.0f) * cos(beta * 3.14f / 180.0f);
            camY = r * sin(beta * 3.14f / 180.0f);
            drone = Drone(); // reset drone state
            break;
        case 'm': glEnable(GL_MULTISAMPLE); break;
        case 'n': glDisable(GL_MULTISAMPLE); break;
        case '1': cameraMode = THIRD; printf("Camera mode: THIRD PERSON ORBIT\n"); break;
        case '2': cameraMode = TOP_ORTHO; printf("Camera mode: TOP ORTHO\n"); break;
        case '3': cameraMode = TOP_PERSPECTIVE; printf("Camera mode: TOP PERSPECTIVE\n"); break;
        case '4': cameraMode = FOLLOW; printf("Camera mode: FOLLOW\n"); break;
    }
}

void processKeysUp(unsigned char key, int xx, int yy) {
	keyStates[key] = false;

	switch (key) {
		case 'w': case 'W': case 's': case 'S':
			drone.throttle = 0.0f; break;
	}
}

void processSpecialKeysDown(int key, int xx, int yy) {
    specialKeyStates[key] = true;
}

void processSpecialKeysUp(int key, int xx, int yy) {
    specialKeyStates[key] = false;
}


// ------------------------------------------------------------
//
// Mouse Events
//

void processMouseButtons(int button, int state, int xx, int yy)
{
	// start tracking the mouse
	if (state == GLUT_DOWN)  {
		startX = xx;
		startY = yy;
		if (button == GLUT_LEFT_BUTTON)
			tracking = 1;
		else if (button == GLUT_RIGHT_BUTTON)
			tracking = 2;
	}

	//stop tracking the mouse
	else if (state == GLUT_UP) {
		if (tracking == 1) {
			alpha -= (xx - startX);
			beta += (yy - startY);
		}
		else if (tracking == 2) {
			r += (yy - startY) * 0.01f;
			if (r < 0.1f)
				r = 0.1f;
		}
		tracking = 0;
	}
}
///////////////////////////////////////////////////////////////////////
// Print point lights (world coords) and drone world position
void printPointLightsAndDrone()
{
    printf("=== Scene positions (world coords) ===\n");
    printf("Drone pos: (%.3f, %.3f, %.3f)\n",
           drone.pos[0], drone.pos[1], drone.pos[2]);

    for (int i = 0; i < NUM_POINT_LIGHTS; ++i) {
        printf("PointLight[%d] world pos: (%.3f, %.3f, %.3f)\n",
               i, pointLightPos[i][0], pointLightPos[i][1], pointLightPos[i][2]);
    }
    fflush(stdout);
}
///////////////////////////////////////////////////////////////////////

// Track mouse motion while buttons are pressed

void processMouseMotion(int xx, int yy)
	{
		int deltaX = - xx + startX;
		int deltaY =    yy - startY;

		// LEFT button dragging
		if (tracking == 1) {
			if (cameraMode == THIRD) {
				// Horizontal drag -> orbit angle (radians)
				float angleDelta = deltaX * CAM_ORBIT_SENSITIVITY_X;
				camOrbitAngle += angleDelta;

				// keep angle in [-pi, pi] or [0, 2pi]
				const float TWO_PI = 6.28318530718f;
				if (camOrbitAngle >= TWO_PI) camOrbitAngle -= TWO_PI;
				if (camOrbitAngle < 0.0f) camOrbitAngle += TWO_PI;

				// Vertical drag -> change orbit height
				float heightDelta = deltaY * CAM_ORBIT_SENSITIVITY_Y;
				camOrbitHeight += heightDelta;
				if (camOrbitHeight < CAM_ORBIT_MIN_HEIGHT) camOrbitHeight = CAM_ORBIT_MIN_HEIGHT;
				if (camOrbitHeight > CAM_ORBIT_MAX_HEIGHT) camOrbitHeight = CAM_ORBIT_MAX_HEIGHT;

				// update start positions for smooth dragging
				startX = xx;
				startY = yy;

				// recompute camX/camY/camZ not necessary — renderSim() computes camera from camOrbit* each frame
				return;
			}

			// existing "orbit with alpha/beta" behavior for other modes
			float alphaAux = alpha + (- xx + startX);
			float betaAux  = beta  + (yy - startY);
			float rAux = r;

			if (betaAux > 85.0f)
				betaAux = 85.0f;
			else if (betaAux < -85.0f)
				betaAux = -85.0f;

			camX = rAux * sin(alphaAux * 3.14f / 180.0f) * cos(betaAux * 3.14f / 180.0f);
			camZ = rAux * cos(alphaAux * 3.14f / 180.0f) * cos(betaAux * 3.14f / 180.0f);
			camY = rAux * sin(betaAux * 3.14f / 180.0f);
		}
		// RIGHT button dragging: zoom (keep as before for THIRD as well)
		else if (tracking == 2) {
			float alphaAux = alpha;
			float betaAux = beta;
			float rAux = r + (deltaY * 0.01f);
			if (rAux < 0.1f)
				rAux = 0.1f;

			camX = rAux * sin(alphaAux * 3.14f / 180.0f) * cos(betaAux * 3.14f / 180.0f);
			camZ = rAux * cos(alphaAux * 3.14f / 180.0f) * cos(betaAux * 3.14f / 180.0f);
			camY = rAux * sin(betaAux * 3.14f / 180.0f);

			// update start coords so drag is incremental
			startX = xx;
			startY = yy;
		}
	//  uncomment this if not using an idle or refresh func
	//	glutPostRedisplay();
	}

void mouseWheel(int wheel, int direction, int x, int y) {

	r += direction * 0.1f;
	if (r < 0.1f)
		r = 0.1f;

	camX = r * sin(alpha * 3.14f / 180.0f) * cos(beta * 3.14f / 180.0f);
	camZ = r * cos(alpha * 3.14f / 180.0f) * cos(beta * 3.14f / 180.0f);
	camY = r *   						     sin(beta * 3.14f / 180.0f);
}


// ------------------------------------------------------------
//
// Render stuff
//

void drawBirds(dataMesh& data) {
    data.meshID = 0; // Sphere mesh
    for (const auto& b : birds) {
        mu.pushMatrix(gmu::MODEL);
        mu.translate(gmu::MODEL, b.pos[0], b.pos[1], b.pos[2]);
        mu.rotate(gmu::MODEL, b.rotation, 0, 1, 0); // Rotate around Y axis
        mu.scale(gmu::MODEL, 1.0f, 0.5f, 1.0f); // Sphere size
        mu.computeDerivedMatrix(gmu::PROJ_VIEW_MODEL);
        mu.computeNormalMatrix3x3();
        data.texMode = 0;
        data.vm = mu.get(gmu::VIEW_MODEL),
        data.pvm = mu.get(gmu::PROJ_VIEW_MODEL);
        data.normal = mu.getNormalMatrix();
        renderer.renderMesh(data);
        mu.popMatrix(gmu::MODEL);
    }
}

void drawDrone(dataMesh &data) {
    mu.pushMatrix(gmu::MODEL);
	mu.translate(gmu::MODEL, drone.pos[0], drone.pos[1], drone.pos[2]);
	mu.rotate(gmu::MODEL, drone.yaw,   0, 1, 0);
    mu.rotate(gmu::MODEL, drone.pitch, 0, 0, 1);
    mu.rotate(gmu::MODEL, drone.roll,  1, 0, 0);

	for (int i = 0; i < 5; i++) {
		if (i==0) {
			data.meshID = 0;
		} else {
			data.meshID = 3;
		}
		mu.pushMatrix(gmu::MODEL);
		mu.translate(gmu::MODEL, drone_parts_position[i].x / 2.0f, drone_parts_position[i].y / 2.0f, drone_parts_position[i].z / 2.0f);
		if (i == 0) {
			mu.scale(gmu::MODEL, DRONE_WIDTH, DRONE_HEIGHT, DRONE_DEPTH);
		} else if (i > 0 && i < 3) {
			mu.scale(gmu::MODEL, 0.25f, 0.0025f, 0.25f);
		} else {
			mu.scale(gmu::MODEL, 0.3125f, 0.025f, 0.3125f);
		}

		mu.computeDerivedMatrix(gmu::PROJ_VIEW_MODEL);
		mu.computeNormalMatrix3x3();

		data.texMode = 4;   //modulate diffuse color with texel color
		data.vm = mu.get(gmu::VIEW_MODEL),
		data.pvm = mu.get(gmu::PROJ_VIEW_MODEL);
		data.normal = mu.getNormalMatrix();
		renderer.renderMesh(data);

		mu.popMatrix(gmu::MODEL);
	}
	mu.popMatrix(gmu::MODEL);
}

// Place point lights at the top of the N tallest buildings
void updatePointLightsFromBuildings()
{
    struct B { int i, j; float h; float x, z; };
    std::vector<B> list;
    list.reserve(6*6);

    const float gridOriginX = -20.0f;
    const float gridOriginZ = -20.0f;
    const float gridSpacing   = 20.0f;

    const float cubeWidth  = 10.0f; // scale X
    const float cubeDepth  = 10.0f; // scale Z

    // Build list of all buildings
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 6; ++j) {
			
			bool skip = false;
			for (auto &p : openAreas) {
				if (p.first == i && p.second == j) {
					skip = true;
					break;
				}
			}
			if (skip || ((i == 1 || i == 2) && j == 1)) continue; // skip this iteration

            float h = cubeHeights[i][j];
            float x = gridOriginX + i * gridSpacing;
            float z = gridOriginZ + j * gridSpacing;
            list.push_back({i, j, h, x, z});
        }
    }

    // sort descending by height
    std::sort(list.begin(), list.end(), [](const B &a, const B &b){
        return a.h > b.h;
    });

    // How much above the roof to place the light
    const float roofOffset = 1.0f;

    // If your cube mesh's base is at y=0 instead, use y = h + roofOffset.
    for (int k = 0; k < NUM_POINT_LIGHTS - 1; ++k) {
        if (k < (int)list.size()) {
            const B &b = list[k];
            float topY = b.h + roofOffset;
            pointLightPos[k][0] = b.x + cubeWidth / 2.0f; // center of cube in X
            pointLightPos[k][1] = topY;
            pointLightPos[k][2] = b.z + cubeDepth / 2.0f; // center of cube in Z
            pointLightPos[k][3] = 1.0f;
        } else {
            // fallback: turn off remaining lights by placing far away / at ground
            pointLightPos[k][0] = 0.0f;
            pointLightPos[k][1] = -10000.0f;
            pointLightPos[k][2] = 0.0f;
            pointLightPos[k][3] = 1.0f;
        }
    }
}

void renderSim(void) {
	FrameCount++;

	// frame delta time for camera updates
	static int lastFrameTime = glutGet(GLUT_ELAPSED_TIME);
	int nowFrame = glutGet(GLUT_ELAPSED_TIME);
	float frameDt = (nowFrame - lastFrameTime) * 0.001f; // seconds
	lastFrameTime = nowFrame;

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	renderer.activateRenderMeshesShaderProg(); // use the required GLSL program to draw the meshes with illumination
	
	//Associate Texture Units to Objects Texture
	renderer.setTexUnit(0, 0);
	renderer.setTexUnit(1, 1);
	renderer.setTexUnit(2, 2);
	renderer.setTexUnit(3, 3);

	// load identity matrices
	mu.loadIdentity(gmu::VIEW);
	mu.loadIdentity(gmu::MODEL);
	
	switch (cameraMode) {
		case THIRD:
			{
				// Compose final orbit angle as: drone yaw (in radians) + user offset (camOrbitAngle)
				const float DEG2RAD = 3.14159265f / 180.0f;
				float yawRad = drone.yaw * DEG2RAD;

				static float smoothYaw = 0.0f;
				const float smoothFactor = 0.08f; // larger = snappier, smaller = smoother
				smoothYaw = smoothYaw + (yawRad - smoothYaw) * smoothFactor;
				float finalAngle = camOrbitAngle - (camOrbitFollowYaw ? smoothYaw : 0.0f);

				// compute camera offset in drone-local world coords (orbit around Y)
				float ox = camOrbitRadius * cosf(finalAngle);
				float oz = camOrbitRadius * sinf(finalAngle);

				// world-space camera position = drone position + offset
				float camWorldX = drone.pos[0] + ox;
				float camWorldY = drone.pos[1] + camOrbitHeight;
				float camWorldZ = drone.pos[2] + oz;

				// where to look: the drone's position (selfie style)
				float lookX = drone.pos[0];
				float lookY = drone.pos[1] + camOrbitLookHeight;
				float lookZ = drone.pos[2];

				mu.lookAt(camWorldX, camWorldY, camWorldZ,
						lookX, lookY, lookZ,
						0, 1, 0);

				mu.loadIdentity(gmu::PROJECTION);
				mu.perspective(53.13f, (1.0f * WinX) / WinY, 0.1f, 1000.0f);
			}
			break;


		case TOP_ORTHO:
			mu.lookAt(35, 50, 35, 
					  35, 0,  35, 
					  0, 0, -1);
			mu.loadIdentity(gmu::PROJECTION);
			mu.ortho(-100, 100, -100, 100, -100, 100);
			break;
		
		case TOP_PERSPECTIVE:
			mu.lookAt(35, 150, 35, 
					  35, 0,  35, 
					  0, 0, -1);
			mu.loadIdentity(gmu::PROJECTION);
			mu.perspective(53.13f, (1.0f * WinX) / WinY, 0.1f, 1000.0f);
			break;

		case FOLLOW:
			mu.lookAt(drone.pos[0] + camX, drone.pos[1] + camY,  drone.pos[2] + camZ, 
					  drone.pos[0] + 1.0f, drone.pos[1] + 0.25f, drone.pos[2] + 1.0f, 
					  0, 1, 0);
			mu.loadIdentity(gmu::PROJECTION);
			mu.perspective(53.13f, (1.0f * WinX) / WinY, 0.1f, 1000.0f);
			break;
	}

	float lposAux[4];
	mu.multMatrixPoint(gmu::VIEW, lightPos, lposAux);   //lightPos definido em World Coord so is converted to eye space
	renderer.setLightPos(lposAux);

	// --- Directional light ---
	// dirLightDir is a direction (w = 0.0f). Use multMatrixPoint with w==0 to transform vectors
	float dirEye4[4] = { dirLightDir[0], dirLightDir[1], dirLightDir[2], dirLightDir[3] };
	float dAux[4];
	mu.multMatrixPoint(gmu::VIEW, dirEye4, dAux); // transforms ignoring translation because w==0

	// prepare 3-component arrays for the renderer API
	float dirEye3[3] = { dAux[0], dAux[1], dAux[2] };
	float dirAmb[3]  = { 0.05f, 0.05f, 0.05f };
	float sunIntensity = 1.5f; // 1.0 = same, 2.0 = twice as bright
	float dirDiff[3] = { 0.40f * sunIntensity, 0.40f * sunIntensity, 0.40f * sunIntensity };
	float dirSpec[3] = { 0.70f * sunIntensity, 0.70f * sunIntensity, 0.70f * sunIntensity };

	// Query currently bound program (safe even if renderer hides program id)
	GLint prog = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &prog);

	// Query some key uniform locations used by the new shader
	GLint loc_dir = glGetUniformLocation(prog, "dirLight.direction");
	GLint loc_numP = glGetUniformLocation(prog, "numPointLights");
	GLint loc_numS = glGetUniformLocation(prog, "numSpotLights");

	// If the shader is active and locations valid, set them explicitly as a fallback
	if (prog != 0) {
		if (loc_dir >= 0) {
			float sunDir[3] = { -0.7f, -0.2f, -0.7f };
			glUniform3fv(loc_dir, 1, sunDir);
		}

		// ensure shader knows how many point/spot lights we plan to use
		if (loc_numP >= 0) glUniform1i(loc_numP, kNumPointLights); // 7 point lights
		if (loc_numS >= 0) glUniform1i(loc_numS, kNumSpotLights); // 4 spot lights
	}

	// ensure shader program active (you already call activateRenderMeshesShaderProg() above)
	renderer.setDirectionalLight(dirEye3, dirAmb, dirDiff, dirSpec);

	// --- Point lights ---
	for (int i = 0; i < NUM_POINT_LIGHTS; i++) {
	    float pAux4[4];
	    mu.multMatrixPoint(gmu::VIEW, pointLightPos[i], pAux4); // world->eye
	    float pEye3[3] = { pAux4[0], pAux4[1], pAux4[2] };

		float intensity = 10.0f;

	    float pAmb[3]  = { 0.02f, 0.02f, 0.02f };
	    float pDiff[3] = { 0.60f * intensity, 0.50f * intensity, 0.40f * intensity};
	    float pSpec[3] = { 0.50f * intensity, 0.50f * intensity, 0.50f * intensity};

	    // attenuation constants (example)
	    float constant  = 1.0f;
	    float linear    = 0.09f;
	    float quadratic = 0.032f;
		
	    renderer.setPointLight(i, pEye3, pAmb, pDiff, pSpec, constant, linear, quadratic);
	}

	// --- Spotlights ---
	for (int i = 0; i < NUM_SPOT_LIGHTS; i++) {
	    float sAux4[4];
	    mu.multMatrixPoint(gmu::VIEW, spotLightPos[i], sAux4); // world->eye
	    float sPos3[3] = { sAux4[0], sAux4[1], sAux4[2] };

	    // spot direction is already a 3-float array in world space; transform it as a vector (w=0)
	    float sDirWorld4[4] = { spotLightDir[i][0], spotLightDir[i][1], spotLightDir[i][2], 0.0f };
	    float sDirEye4[4];
	    mu.multMatrixPoint(gmu::VIEW, sDirWorld4, sDirEye4);
	    float sDir3[3] = { sDirEye4[0], sDirEye4[1], sDirEye4[2] };

		float intensity = 5.0f; // >1.0 makes it brighter, <1.0 makes it dimmer

	    float sAmb[3]  = { 0.0f, 0.0f, 0.0f };
	    float sDiff[3] = { 1.0f * intensity, 1.0f * intensity, 1.0f * intensity };
	    float sSpec[3] = { 0.8f * intensity, 0.8f * intensity, 0.8f * intensity };

	    // spot cutoffs (use cos of angle). Example: inner=12.5deg outer=17.5deg
	    const float PI = 3.14159265f;
	    float innerCutDeg = 12.5f, outerCutDeg = 17.5f;
	    float innerCutCos = cosf(innerCutDeg * PI / 180.0f);
	    float outerCutCos = cosf(outerCutDeg * PI / 180.0f);

	    // attenuation (same as point lights or tweak)
	    float sConstant  = 1.0f;
	    float sLinear    = 0.09f;
	    float sQuadratic = 0.032f;

	    renderer.setSpotLight(i, sPos3, sDir3, innerCutCos, outerCutCos, sAmb, sDiff, sSpec, sConstant, sLinear, sQuadratic);
	}

	// Draw the terrain - myMeshes[6] contains the Quad object
	mu.pushMatrix(gmu::MODEL);
	mu.translate(gmu::MODEL, 0.0f, 0.0f, 0.0f);
	mu.scale(gmu::MODEL, 30.0f, 0.1f, 30.0f);
	mu.rotate(gmu::MODEL, -90.0f, 1.0f, 0.0f, 0.0f); // Rotate quad from XY to XZ plane

	mu.computeDerivedMatrix(gmu::PROJ_VIEW_MODEL);
	mu.computeNormalMatrix3x3();

	dataMesh data;

	data.meshID = 6; // For the terrain (last mesh)
	data.texMode = 5; // "two-texture" path in mesh.frag
	data.vm = mu.get(gmu::VIEW_MODEL),
	data.pvm = mu.get(gmu::PROJ_VIEW_MODEL);
	data.normal = mu.getNormalMatrix();

	// set terrain tiling
	if (prog != 0) {
		GLint loc_tile1 = glGetUniformLocation(prog, "terrainTile1");
		GLint loc_tile2 = glGetUniformLocation(prog, "terrainTile2");
		if (loc_tile1 >= 0) glUniform2f(loc_tile1, 64.0f, 64.0f);
		if (loc_tile2 >= 0) glUniform2f(loc_tile2, 32.0f, 32.0f);

	}

	renderer.renderMesh(data);
	mu.popMatrix(gmu::MODEL);

	// Draw the Buildings
	data.meshID = 0; // For the cube (myMeshes[0])
	for (int i=0; i<6; ++i) {  // Draw the other objects in the scene (myMeshes[0] to myMeshes[5])
		for (int j=0; j<6; ++j) {

			bool skip = false;
			for (auto &p : openAreas) {
				if (p.first == i && p.second == j) {
					skip = true;
					break;
				}
			}
			if (skip || ((i == 1 || i == 2) && j == 1)) continue; // skip this iteration

			data.texMode = 0; //no texturing
			mu.pushMatrix(gmu::MODEL);
			mu.translate(gmu::MODEL, -20.0f + i * 20.0f, 0.0f, -20.0f + j * 20.0f);
			mu.scale(gmu::MODEL, BUILDING_WIDTH, cubeHeights[i][j], BUILDING_DEPTH);

			mu.computeDerivedMatrix(gmu::PROJ_VIEW_MODEL);
			mu.computeNormalMatrix3x3();

			data.texMode = 1;   //modulate diffuse color with texel color
			data.vm = mu.get(gmu::VIEW_MODEL),
			data.pvm = mu.get(gmu::PROJ_VIEW_MODEL);
			data.normal = mu.getNormalMatrix();
			renderer.renderMesh(data);

			mu.popMatrix(gmu::MODEL);
		}
	}

	// Update and draw birds
	static int lastBirdTime = glutGet(GLUT_ELAPSED_TIME);
	int nowBird = glutGet(GLUT_ELAPSED_TIME);
	float dtBird = (nowBird - lastBirdTime) * 0.001f;
	lastBirdTime = nowBird;
	updateBirds(dtBird);

	drawBirds(data);

	//Update Drone State
	static int lastTime = glutGet(GLUT_ELAPSED_TIME);
    int now = glutGet(GLUT_ELAPSED_TIME);
    float dt = (now - lastTime) * 0.001f;
	lastTime = now;
    applyKeyInputs(dt);
	updateDroneState(dt);

	drawDrone(data);

	// put this after drawDrone(data) and before text rendering
	renderer.activateRenderMeshesShaderProg(); // ensure shader bound

	// configure translucency
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE); // don't write to depth buffer for translucent objects

	// Decide whether to render inner faces (if camera inside) or outer faces.
	// (Optional: compute camera world pos depending on cameraMode — here is a simple approximation)
	float camWX=0.f, camWY=0.f, camWZ=0.f;
	if (cameraMode == THIRD) {
		const float DEG2RAD = 3.14159265f / 180.0f;
		float yawRad = drone.yaw * DEG2RAD;
		// use camOrbitAngle / camOrbitRadius from your code
		float finalAngle = camOrbitAngle - (camOrbitFollowYaw ? yawRad : 0.0f);
		float ox = camOrbitRadius * cosf(finalAngle);
		float oz = camOrbitRadius * sinf(finalAngle);
		camWX = drone.pos[0] + ox;
		camWY = drone.pos[1] + camOrbitHeight;
		camWZ = drone.pos[2] + oz;
	} else if (cameraMode == FOLLOW) {
		camWX = drone.pos[0] + camX;
		camWY = drone.pos[1] + camY;
		camWZ = drone.pos[2] + camZ;
	} else { // TOP modes: approximate
		camWX = 0.0f; camWY = 50.0f; camWZ = 0.0f;
	}

	// camera distance test
	float dx = camWX - DOME_CENTER_X;
	float dy = camWY - DOME_CENTER_Y;
	float dz = camWZ - DOME_CENTER_Z;
	float camDist = sqrtf(dx*dx + dy*dy + dz*dz);
	bool cameraInside = (camDist < DOME_RADIUS);

	// set culling to draw appropriate faces
	glEnable(GL_CULL_FACE);
	if (cameraInside) {
		glCullFace(GL_FRONT); // draw back faces so inner surface is visible
	} else {
		glCullFace(GL_BACK);  // draw front faces (normal outer view)
	}

	// transform + render dome (meshID 2 in your buildScene)
	mu.pushMatrix(gmu::MODEL);
	mu.translate(gmu::MODEL, DOME_CENTER_X, DOME_CENTER_Y, DOME_CENTER_Z);
	mu.scale(gmu::MODEL, DOME_RADIUS, DOME_RADIUS, DOME_RADIUS);
	mu.computeDerivedMatrix(gmu::PROJ_VIEW_MODEL);
	mu.computeNormalMatrix3x3();

	data.meshID = 2;  
	data.texMode = 0;  
	data.vm  = mu.get(gmu::VIEW_MODEL);
	data.pvm = mu.get(gmu::PROJ_VIEW_MODEL);
	data.normal = mu.getNormalMatrix();

	// set alpha uniform (make sure mesh.frag has 'uniform float uAlpha;')
	GLint progID = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &progID);
	if (progID != 0) {
		GLint loc_uAlpha = glGetUniformLocation(progID, "uAlpha");
		if (loc_uAlpha >= 0) glUniform1f(loc_uAlpha, DOME_TRANSPARENCY); // 0.0 = fully transparent, 1.0 = opaque
	}

	// actually draw it
	renderer.renderMesh(data);
	mu.popMatrix(gmu::MODEL);

	// restore GL state
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glCullFace(GL_BACK); // restore default
	if (progID != 0) {
		GLint loc_uAlpha = glGetUniformLocation(progID, "uAlpha");
		if (loc_uAlpha >= 0) glUniform1f(loc_uAlpha, 1.0f); // reset alpha
	}

	//Render text (bitmap fonts) in screen coordinates. So use ortoghonal projection with viewport coordinates.
	//Each glyph quad texture needs just one byte color channel: 0 in background and 1 for the actual character pixels. Use it for alpha blending
	//text to be rendered in last place to be in front of everything
	
	if(fontLoaded) {
		glDisable(GL_DEPTH_TEST);
		TextCommand textCmd = { "Yaw: " + std::to_string(drone.yaw) + 
								"\n\nPitch: " + std::to_string(drone.pitch) + 
								"\n\nRoll: " + std::to_string(drone.roll) +
								"\n\n\nThrottle: " + std::to_string(drone.throttle) +
								"\n\n\nDirection: (" + std::to_string(drone.dir[0]) + ", " + std::to_string(drone.dir[1]) + ", " + std::to_string(drone.dir[2]) + ")" +
								"\n\n\nVelocity: (" + std::to_string(drone.velocity[0]) + ", " + std::to_string(drone.velocity[1]) + ", " + std::to_string(drone.velocity[2]) + ")" +
								"\n\n\nPosition: (" + std::to_string(drone.pos[0]) + ", " + std::to_string(drone.pos[1]) + ", " + std::to_string(drone.pos[2]) + ")", 
								{30, 650}, 0.3f };
		//the glyph contains transparent background colors and non-transparent for the actual character pixels. So we use the blending
		glEnable(GL_BLEND);  
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		int m_viewport[4];
		glGetIntegerv(GL_VIEWPORT, m_viewport);

		//viewer at origin looking down at  negative z direction

		mu.loadIdentity(gmu::MODEL);
		mu.loadIdentity(gmu::VIEW);
		mu.pushMatrix(gmu::PROJECTION);
		mu.loadIdentity(gmu::PROJECTION);
		mu.ortho(m_viewport[0], m_viewport[0] + m_viewport[2] - 1, m_viewport[1], m_viewport[1] + m_viewport[3] - 1, -1, 1);
		mu.computeDerivedMatrix(gmu::PROJ_VIEW_MODEL);
		textCmd.pvm = mu.get(gmu::PROJ_VIEW_MODEL);
		renderer.renderText(textCmd);
		mu.popMatrix(gmu::PROJECTION);
		glDisable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);
		
	}
	
	glutSwapBuffers();
}


//
// Scene building with basic geometry
//

void buildScene()
{
	//Texture Object definition
	renderer.TexObjArray.texture2D_Loader("assets/building.jpg");
	renderer.TexObjArray.texture2D_Loader("assets/marstex.jpg");
	renderer.TexObjArray.texture2D_Loader("assets/noise2.jpg");
	renderer.TexObjArray.texture2D_Loader("assets/drone.jpg");


	//Scene geometry with triangle meshes

	MyMesh amesh;

	float amb[] = { 0.2f, 0.15f, 0.1f, 1.0f };
	float diff[] = { 0.8f, 0.6f, 0.4f, 1.0f };
	float spec[] = { 0.8f, 0.8f, 0.8f, 1.0f };

	float amb1[]  = { 0.20f, 0.20f, 0.20f, 1.0f };
	float diff1[] = { 0.60f, 0.60f, 0.60f, 1.0f };
	float spec1[] = { 0.30f, 0.30f, 0.30f, 1.0f };

	float emissive[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	float shininess = 100.0f;
	int texcount = 0;


	// create geometry and VAO of the cube
	amesh = createCube();
	memcpy(amesh.mat.ambient, amb1, 4 * sizeof(float));
	memcpy(amesh.mat.diffuse, diff1, 4 * sizeof(float));
	memcpy(amesh.mat.specular, spec1, 4 * sizeof(float));
	memcpy(amesh.mat.emissive, emissive, 4 * sizeof(float));
	amesh.mat.shininess = shininess;
	amesh.mat.texCount = texcount;
	renderer.myMeshes.push_back(amesh);

	// create geometry and VAO of the pawn
	amesh = createPawn();
	memcpy(amesh.mat.ambient, amb, 4 * sizeof(float));
	memcpy(amesh.mat.diffuse, diff, 4 * sizeof(float));
	memcpy(amesh.mat.specular, spec, 4 * sizeof(float));
	memcpy(amesh.mat.emissive, emissive, 4 * sizeof(float));
	amesh.mat.shininess = shininess;
	amesh.mat.texCount = texcount;
	renderer.myMeshes.push_back(amesh);

	// create geometry and VAO of the sphere
	amesh = createSphere(1.0f, 20);
	memcpy(amesh.mat.ambient, amb, 4 * sizeof(float));
	memcpy(amesh.mat.diffuse, diff, 4 * sizeof(float));
	memcpy(amesh.mat.specular, spec, 4 * sizeof(float));
	memcpy(amesh.mat.emissive, emissive, 4 * sizeof(float));
	amesh.mat.shininess = shininess;
	amesh.mat.texCount = texcount;
	renderer.myMeshes.push_back(amesh);

	// create geometry and VAO of the cylinder
	amesh = createCylinder(1.5f, 0.5f, 20);
	memcpy(amesh.mat.ambient, amb, 4 * sizeof(float));
	memcpy(amesh.mat.diffuse, diff, 4 * sizeof(float));
	memcpy(amesh.mat.specular, spec, 4 * sizeof(float));
	memcpy(amesh.mat.emissive, emissive, 4 * sizeof(float));
	amesh.mat.shininess = shininess;
	amesh.mat.texCount = texcount;
	renderer.myMeshes.push_back(amesh);

	// create geometry and VAO of the cone
	amesh = createCone(2.5f, 1.2f, 20);
	memcpy(amesh.mat.ambient, amb, 4 * sizeof(float));
	memcpy(amesh.mat.diffuse, diff, 4 * sizeof(float));
	memcpy(amesh.mat.specular, spec, 4 * sizeof(float));
	memcpy(amesh.mat.emissive, emissive, 4 * sizeof(float));
	amesh.mat.shininess = shininess;
	amesh.mat.texCount = texcount;
	renderer.myMeshes.push_back(amesh);

	// create geometry and VAO of the torus
	amesh = createTorus(0.5f, 1.5f, 20, 20);
	memcpy(amesh.mat.ambient, amb, 4 * sizeof(float));
	memcpy(amesh.mat.diffuse, diff, 4 * sizeof(float));
	memcpy(amesh.mat.specular, spec, 4 * sizeof(float));
	memcpy(amesh.mat.emissive, emissive, 4 * sizeof(float));
	amesh.mat.shininess = shininess;
	amesh.mat.texCount = texcount;
	renderer.myMeshes.push_back(amesh);


	// create geometry and VAO of the terrain
	amesh = createQuad(30.0f, 30.0f);
	memcpy(amesh.mat.ambient, amb, 4 * sizeof(float));
	memcpy(amesh.mat.diffuse, diff, 4 * sizeof(float));
	memcpy(amesh.mat.specular, spec, 4 * sizeof(float));
	memcpy(amesh.mat.emissive, emissive, 4 * sizeof(float));
	amesh.mat.shininess = shininess;
	amesh.mat.texCount = 2;
	renderer.myMeshes.push_back(amesh);

	//The truetypeInit creates a texture object in TexObjArray for storing the fontAtlasTexture
	
	fontLoaded = renderer.truetypeInit(fontPathFile);
	if (!fontLoaded)
		cerr << "Fonts not loaded\n";
	else 
		cerr << "Fonts loaded\n";

	printf("\nNumber of Texture Objects is %d\n\n", renderer.TexObjArray.getNumTextureObjects());

	// set the camera position based on its spherical coordinates
	camX = r * sin(alpha * 3.14f / 180.0f) * cos(beta * 3.14f / 180.0f);
	camZ = r * cos(alpha * 3.14f / 180.0f) * cos(beta * 3.14f / 180.0f);
	camY = r * sin(beta * 3.14f / 180.0f);
}

// ------------------------------------------------------------
//
// Main function
//

int main(int argc, char **argv) {

//  GLUT initialization
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DEPTH|GLUT_DOUBLE|GLUT_RGBA|GLUT_MULTISAMPLE);

	glutInitContextVersion (4, 3);
	glutInitContextProfile (GLUT_CORE_PROFILE );
	glutInitContextFlags(GLUT_FORWARD_COMPATIBLE | GLUT_DEBUG);

	glutInitWindowPosition(100,100);
	glutInitWindowSize(WinX, WinY);
	WindowHandle = glutCreateWindow(CAPTION);



//  Callback Registration
	glutDisplayFunc(renderSim);
	glutReshapeFunc(changeSize);

	glutTimerFunc(0, timer, 0);
	glutIdleFunc(renderSim);  // Use it for maximum performance
	//glutTimerFunc(0, refresh, 0);    //use it to to get 60 FPS

//	Mouse and Keyboard Callbacks
	glutKeyboardFunc(processKeysDown);
	glutKeyboardUpFunc(processKeysUp);
	glutSpecialFunc(processSpecialKeysDown);
	glutSpecialUpFunc(processSpecialKeysUp);

	glutMouseFunc(processMouseButtons);
	glutMotionFunc(processMouseMotion);
	glutMouseWheelFunc ( mouseWheel ) ;
	

//	return from main loop
	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);

//	Init GLEW
	glewExperimental = GL_TRUE;
	glewInit();

	// some GL settings
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glEnable(GL_MULTISAMPLE);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	printf ("Vendor: %s\n", glGetString (GL_VENDOR));
	printf ("Renderer: %s\n", glGetString (GL_RENDERER));
	printf ("Version: %s\n", glGetString (GL_VERSION));
	printf ("GLSL: %s\n", glGetString (GL_SHADING_LANGUAGE_VERSION));

	/* Initialization of DevIL */
	if (ilGetInteger(IL_VERSION_NUM) < IL_VERSION)
	{
		printf("wrong DevIL version \n");
		exit(0);
	}
	ilInit();

	buildScene();

	for (int i = 0; i < 6; ++i) {
		for (int j = 0; j < 6; ++j) {
			cubeHeights[i][j] = 20.0f + 15.0f * (rand() / (float)RAND_MAX); // random between 5 and 10
		}
	}

	srand(static_cast<unsigned>(time(0))); // seed RNG

	for (int i = 0; i < 3; i++) {
		int x = 1 + rand() % 4;  // 1 to 4
		int y = 1 + rand() % 4;
		openAreas.push_back(std::make_pair(x, y));
	}

	printf("Open areas at:\n");
	for (const auto& p : openAreas) {
		printf("(%d, %d)\n", p.first, p.second);
	}

	updatePointLightsFromBuildings();
	printPointLightsAndDrone(); // print initial positions for debugging

	if(!renderer.setRenderMeshesShaderProg("shaders/mesh.vert", "shaders/mesh.frag") || 
		!renderer.setRenderTextShaderProg("shaders/ttf.vert", "shaders/ttf.frag"))
	return(1);

	// Initialize birds
	initBirds(30);

	//  GLUT main loop
	glutMainLoop();

	return(0);
}
