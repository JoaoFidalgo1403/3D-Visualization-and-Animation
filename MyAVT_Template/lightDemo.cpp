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

// assimp include files. These three are usually needed.
#include "assimp/Importer.hpp"	//OO version Header!
#include "assimp/scene.h"

// Use Very Simple Libs
#include "VSShaderlib.h"
#include "AVTmathLib.h"

#include "Texture_Loader.h"
#include "meshFromAssimp.h"

using namespace std;

#define CAPTION "AVT - Lab Assignment 1"
int WindowHandle = 0;
int WinX = 1024, WinY = 768;

unsigned int FrameCount = 0;
bool isPaused = false;

// --- To import models ---
Assimp::Importer droneImporter;

// the global Assimp scene object
const aiScene* droneScene;

// scale factor for the Assimp model to fit in the window
float droneImportedScale;

// --- Drone Model Data ---
GLuint* droneTextureIds;       // texture array for the drone (filled by createMeshFromAssimp)
bool droneModelLoaded = false;

char model_dir[50];  //initialized by the user input at the console

bool normalMapKey = TRUE;

//External array storage defined in AVTmathLib.cpp

/// The storage for matrices
extern float mMatrix[COUNT_MATRICES][16];
extern float mCompMatrix[COUNT_COMPUTED_MATRICES][16];

/// The normal matrix
extern float mNormal3x3[9];


// --- Game state (battery/distance/score) ---
float batteryLevel = 1.0f;                        // 1.0 == full battery
const float BATTERY_DRAIN_AT_FULL = 0.02f;        // fraction drained per second at full throttle (tweak)
const float COLLISION_BATTERY_PENALTY = 0.20f;    // lose 1/5 of total battery on collisions
float distanceTraveled = 0.0f;                    // meters (world units)
int scorePoints = 0;

bool isGameOver = false;

float prevDronePos[3] = { 0.0f, 0.0f, 0.0f };

const float GAME_MAX_THROTTLE = 15.0f;            // must match updateDroneState's maxThrottle
const float DRONE_FALL_VELOCITY = -6.0f;          // m/s downward when battery depleted

// INVULNERABILITY after collisions
const float COLLISION_INVULN_DURATION = 2.0f; // seconds of invulnerability after taking collision damage
const float DRONE_BLINK_PERIOD = 0.12f;      // visual blink period while invulnerable

// runtime
float collisionInvulnTimer = 0.0f; // counts down; >0 => invulnerable

//File with the font
const string fontPathFile = "fonts/arial.ttf";

//Object of class gmu (Graphics Math Utility) to manage math and matrix operations
gmu mu;

//Object of class renderer to manage the rendering of meshes and ttf-based bitmap text
Renderer renderer;

// Object of class model to manage the loading of meshes from OBJ files
bool fontLoaded = false;

// --- Utility structures and functions ---

// 3D Vector
struct vec3 {
	float x, y, z;
	vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};

// PI
const float PI = 3.14159265f;
	
// --- Camera parameters ---

// Camera Position
float camX, camY, camZ;

// Camera Spherical Coordinates
float alpha = 57.0f, beta = 18.0f;
float r = 45.0f;

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

// --- Mouse and Keyboard control ---

// Mouse Tracking Variables
int startX, startY, tracking = 0;

// Frame counting and FPS computation
long myTime,timebase = 0,frame = 0;
char s[32];

// Key states arrays
bool keyStates[256] = { false };
bool specialKeyStates[256] = { false };

// --- Birds Parameters ---
float const BIRD_MAX_SPAWN_RADIUS = 95.0f;
float const BIRD_MIN_SPAWN_RADIUS = 70.0f;
float const BIRD_MAX_DISTANCE = 100.0f;

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

// --- Drone Parameters ---
const float DRONE_WIDTH = 1.25f/2.0f;
const float DRONE_DEPTH = 0.75f/2.0f;
const float DRONE_HEIGHT = 0.15f/2.0f;
const float DRONE_MEASURE[3] = { DRONE_WIDTH, DRONE_HEIGHT, DRONE_DEPTH };

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


// --- Package state ---
struct Package {
    bool active = false;      // is a package present in the world?
    bool pickedUp = false;    // is it currently attached to the drone?
    int src_i = -1, src_j = -1;
    int dst_i = -1, dst_j = -1;
	float localPos[3]; // local position relative to drone when attached
    float pos[3];             // world coords of package (when on roof or attached)
} currentPackage;


// --- City parameters ---
const float CITY_CENTER[3] = {35.0f, 0.0f, 35.0f};

// Buildings Scale
const float BUILDING_WIDTH = 10.0f;
const float BUILDING_DEPTH = 10.0f;

float cubeHeights[6][6]; // Store heights for each cube
std::vector<std::pair<int,int>> openAreas;

// Dome parameters
const float DOME_RADIUS = 100.0f;
const float DOME_TRANSPARENCY = 0.75f;

// --- Light parameters ---

float lightPos[4] = {4.0f, 15.0f, 2.0f, 1.0f};

// Directional light
float dirLightDir[4] = { -1.0f, -0.5f, -0.3f, 0.0f }; // directional light (w=0)

// Light counts
#define NUM_POINT_LIGHTS 7
#define NUM_SPOT_LIGHTS  4

// Runtime constants
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
float spotLightOffset[NUM_SPOT_LIGHTS][4] = {
    { 2.0f, 0.0f, -2.0f, 0.0f },
	{ 2.0f, 0.0f,  2.0f, 0.0f },
	{ -2.0f, 0.0f, -2.0f, 0.0f },
	{ -2.0f, 0.0f,  2.0f, 0.0f }
};

bool night_mode = false;
bool plight_mode = true;
bool headlights_mode = true;
bool fog_mode = true;

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
// Package Handling
//

// check whether building (i,j) is valid for package placement
static bool isValidBuildingCell(int i, int j) {
    if (i < 0 || i >= 6 || j < 0 || j >= 6) return false;
    // skip open areas
    for (auto &p : openAreas) if (p.first == i && p.second == j) return false;
    // skip special blocked cell (your code already skipped these)
    if ((i == 1 || i == 2) && j == 1) return false;
    return true;
}

// get world coords for building roof center
static void buildingRoofCenter(int i, int j, float out[3]) {
    float x = -20.0f + i * 20.0f;
    float z = -20.0f + j * 20.0f;
    out[0] = x + BUILDING_WIDTH * 0.5f;
    out[2] = z + BUILDING_DEPTH * 0.5f;
    out[1] = cubeHeights[i][j]; // top of roof
}

void spawnPackage() {
    // choose random valid source and destination (distinct)
    int s_i, s_j, d_i, d_j;
    const int MAX_TRIES = 200;
    int tries = 0;
    do {
        s_i = rand() % 6; s_j = rand() % 6; ++tries;
    } while ((!isValidBuildingCell(s_i, s_j)) && tries < MAX_TRIES);

    tries = 0;
    do {
        d_i = rand() % 6; d_j = rand() % 6; ++tries;
    } while (((!isValidBuildingCell(d_i, d_j)) || (d_i == s_i && d_j == s_j)) && tries < MAX_TRIES);

    if (!isValidBuildingCell(s_i, s_j) || !isValidBuildingCell(d_i, d_j) || (s_i == d_i && s_j == d_j)) {
        // fallback: no package this time
        currentPackage.active = false;
        return;
    }

    currentPackage.active = true;
    currentPackage.pickedUp = false;
    currentPackage.src_i = s_i; currentPackage.src_j = s_j;
    currentPackage.dst_i = d_i; currentPackage.dst_j = d_j;

    float center[3];
    buildingRoofCenter(s_i, s_j, center);
    currentPackage.pos[0] = center[0];
    currentPackage.pos[2] = center[2];
    currentPackage.pos[1] = center[1] + 2.0f; // a little above roof

	// default local offset relative to drone when picked up (drone-local coords)
    currentPackage.localPos[0] = - 0.2f;
    currentPackage.localPos[1] = -0.4f;
    currentPackage.localPos[2] = - 0.2f; 

    printf("[PACKAGE] spawned at (%d,%d) -> dest (%d,%d) at (%.1f, %.1f, %.1f)\n",
           s_i, s_j, d_i, d_j,
           currentPackage.pos[0], currentPackage.pos[1], currentPackage.pos[2]);
}

void checkPackagePickupAndDelivery() {
    if (!currentPackage.active) return;
	// if not picked up: check drone proximity to package (package is static on roof)
    if (!currentPackage.pickedUp) {
        float dx = drone.pos[0] - currentPackage.pos[0];
        float dy = drone.pos[1] - currentPackage.pos[1];
        float dz = drone.pos[2] - currentPackage.pos[2];
        float dist = sqrtf(dx*dx + dy*dy + dz*dz);
        const float pickupRadius = 2.0f; // tweak
        if (dist <= pickupRadius && !isGameOver) {
            currentPackage.pickedUp = true;
       
            printf("[PACKAGE] picked up (attached, localOffset = %.2f, %.2f, %.2f)\n",
                   currentPackage.localPos[0], currentPackage.localPos[1], currentPackage.localPos[2]);
        }
    } else {
        // package is attached: compute its world position from drone MODEL transform
        float local4[4] = { currentPackage.localPos[0], currentPackage.localPos[1], currentPackage.localPos[2], 1.0f };
        float world4[4];
        mu.pushMatrix(gmu::MODEL);
        mu.translate(gmu::MODEL, drone.pos[0], drone.pos[1], drone.pos[2]);
        mu.rotate(gmu::MODEL, drone.yaw, 0, 1, 0);
        mu.rotate(gmu::MODEL, drone.pitch, 0, 0, 1);
        mu.rotate(gmu::MODEL, drone.roll, 1, 0, 0);
        mu.multMatrixPoint(gmu::MODEL, local4, world4); // MODEL -> world
        mu.popMatrix(gmu::MODEL);

        // update stored world position (so draw/other code can use it)
        currentPackage.pos[0] = world4[0];
        currentPackage.pos[1] = world4[1];
        currentPackage.pos[2] = world4[2];

        // check delivery proximity to destination roof center using the computed world pos
        float dstCenter[3];
        buildingRoofCenter(currentPackage.dst_i, currentPackage.dst_j, dstCenter);
        // measure horizontal distance and vertical closeness - allow some tolerance
        float dx = currentPackage.pos[0] - dstCenter[0];
        float dz = currentPackage.pos[2] - dstCenter[2];
        float dy = currentPackage.pos[1] - (dstCenter[1] + 1.0f); // package target just above roof
        float horizDist = sqrtf(dx*dx + dz*dz);
        const float deliverHorizRadius = 2.5f;
        const float deliverVertTol = 3.0f; // allow delivering from above within this vertical tolerance

        if (horizDist <= deliverHorizRadius && fabsf(drone.pos[1] - dstCenter[1]) <= deliverVertTol) {
            // delivered!
            int gained = (int)roundf(batteryLevel * 100.0f); // points proportional to remaining battery
            scorePoints += gained;
            batteryLevel = 1.0f; // refill battery on delivery
            printf("[PACKAGE] delivered to (%d,%d). +%d points. Battery refilled.\n",
                   currentPackage.dst_i, currentPackage.dst_j, gained);

            // spawn a new package immediately
            currentPackage.active = false;
            currentPackage.pickedUp = false;
            spawnPackage();
        }
    }
}


// ------------------------------------------------------------
//
// Game State
//

// Reset game state (call on startup and on restart 'R')
void resetGameState() {
    batteryLevel = 1.0f;
    distanceTraveled = 0.0f;
    scorePoints = 0;
	collisionInvulnTimer = 0.0f;
    isGameOver = false;

    // reset drone
    drone = Drone();

    // set prev position for distance accumulation
    prevDronePos[0] = drone.pos[0];
    prevDronePos[1] = drone.pos[1];
    prevDronePos[2] = drone.pos[2];
	spawnPackage();

    // clear key states so no sticky input
    memset(keyStates, 0, sizeof(keyStates));
    memset(specialKeyStates, 0, sizeof(specialKeyStates));
}

// Game over trigger: stop inputs and start fall
void triggerGameOver() {
    if (isGameOver) return;
    isGameOver = true;
    // clear inputs
    memset(keyStates, 0, sizeof(keyStates));
    memset(specialKeyStates, 0, sizeof(specialKeyStates));
    printf("Battery depleted: entering fall state.\n");
}

// --- battery loss reasons & logging helper ---
enum BatteryLossReason { LOSS_THROTTLE, LOSS_BIRD, LOSS_BUILDING, LOSS_GROUND };

static const char* reasonToStr(BatteryLossReason r) {
    switch (r) {
        case LOSS_THROTTLE: return "THROTTLE";
        case LOSS_BIRD:     return "BIRD";
        case LOSS_BUILDING: return "BUILDING";
        case LOSS_GROUND:   return "GROUND";
        default:            return "UNKNOWN";
    }
}

// universal battery apply + logging
void applyBatteryLoss(float amountFraction, BatteryLossReason reason) {
    if (amountFraction <= 0.0f) return;
    float before = batteryLevel;
    batteryLevel -= amountFraction;
    if (batteryLevel < 0.0f) batteryLevel = 0.0f;

    // print human friendly data: amount in percent and remaining battery
    int amtPct = (int)roundf(amountFraction * 100.0f);
    int remaining = (int)roundf(batteryLevel * 100.0f);
    printf("[BATTERY] -%d%% (%s) -> remaining %d%%\n", amtPct, reasonToStr(reason), remaining);
    fflush(stdout);

    if (batteryLevel <= 0.0f) {
        triggerGameOver();
    }
}

void updateBatteryAndDistance(float dt) {
    if (isGameOver) return;

    // throttle normalized [0..1] - uses absolute throttle magnitude
    float throttleNorm = fminf(1.0f, fabsf(drone.throttle) / GAME_MAX_THROTTLE);

    // drain
	float drain = BATTERY_DRAIN_AT_FULL * throttleNorm * dt; // fraction-per-second * throttle * dt seconds
    if (batteryLevel > 0.0f) applyBatteryLoss(drain, LOSS_THROTTLE);

    // accumulate distance using world displacement
    float dx = drone.pos[0] - prevDronePos[0];
    float dy = drone.pos[1] - prevDronePos[1];
    float dz = drone.pos[2] - prevDronePos[2];
    float step = sqrtf(dx*dx + dy*dy + dz*dz);
    distanceTraveled += step;

    prevDronePos[0] = drone.pos[0];
    prevDronePos[1] = drone.pos[1];
    prevDronePos[2] = drone.pos[2];

    // check battery empty
    if (batteryLevel <= 0.0f) {
        triggerGameOver();
    }
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
BirdData spawnBird() {
    BirdData b;

    // Random angles for uniform direction on sphere
    float theta = 2.0f * 3.14159265f * (rand() / (float)RAND_MAX); 
    float cosPhi = 2.0f * (rand() / (float)RAND_MAX) - 1.0f;      
    float sinPhi = sqrtf(1.0f - cosPhi * cosPhi);

    // Radius sampled uniformly in spherical shell volume
    float u = rand() / (float)RAND_MAX;
    float minR3 = BIRD_MIN_SPAWN_RADIUS * BIRD_MIN_SPAWN_RADIUS * BIRD_MIN_SPAWN_RADIUS;
    float maxR3 = BIRD_MAX_SPAWN_RADIUS * BIRD_MAX_SPAWN_RADIUS * BIRD_MAX_SPAWN_RADIUS;
    float r_samp = cbrtf(u * (maxR3 - minR3) + minR3);

    // Convert spherical → cartesian, offset by drone position
    b.pos[0] = CITY_CENTER[0] + r_samp * sinPhi * cosf(theta);
    b.pos[1] = CITY_CENTER[1] + r_samp * cosPhi;
	if (b.pos[1] < 0.0f) b.pos[1] = -b.pos[1]; // Ensure above ground
    b.pos[2] = CITY_CENTER[2] + r_samp * sinPhi * sinf(theta);

    // Inward direction
    float dir[3] = { CITY_CENTER[0] - b.pos[0], CITY_CENTER[1] - b.pos[1], CITY_CENTER[2] - b.pos[2] };
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
    float speed = 2.0f;
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

    for (int i = 0; i < numBirds; ++i) {
        BirdData d = spawnBird();
        birds.emplace_back(d.pos[0], d.pos[1], d.pos[2],
                           d.velocity[0], d.velocity[1], d.velocity[2],
                           d.rotSpeed);
    }
}

// Update bird positions and rotations
void updateBirds(float dt) {
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

        float dx = b.pos[0] - CITY_CENTER[0];
        float dy = b.pos[1] - CITY_CENTER[1];
        float dz = b.pos[2] - CITY_CENTER[2];
        float dist = sqrt(dx*dx + dy*dy + dz*dz);
		if (dist > BIRD_MAX_DISTANCE || b.pos[1] < 0.0f) {
			BirdData d = spawnBird();
			b.pos[0] = d.pos[0]; b.pos[1] = d.pos[1]; b.pos[2] = d.pos[2];
			b.velocity[0] = d.velocity[0]; b.velocity[1] = d.velocity[1]; b.velocity[2] = d.velocity[2];
			b.rotation = 0.0f;
			b.rotSpeed = d.rotSpeed;
		}

		// Calculate collision
		float dist_x = CITY_CENTER[0] - b.pos[0];
		float dist_y = CITY_CENTER[1] - b.pos[1];
		float dist_z = CITY_CENTER[2] - b.pos[2];
		if (std::sqrt(dist_x*dist_x + dist_y*dist_y + dist_z*dist_z) <= 1.0f) {
			// apply battery penalty for hitting a bird and give a small bounce effect
			if (collisionInvulnTimer <= 0.0f) {
        		applyBatteryLoss(COLLISION_BATTERY_PENALTY, LOSS_BIRD);
        		collisionInvulnTimer = COLLISION_INVULN_DURATION;
				drone.collisionVel[1] = - drone.velocity[1] * 1.5f;
				if (batteryLevel <= 0.0f) triggerGameOver();
			}
		}

    }
}

// ------------------------------------------------------------
//
// Collision Detection
//

AABB computeDroneAABB() {
    AABB box;
    float halfWidth = DRONE_WIDTH / 2.0f + 0.1f;
    float halfHeight = DRONE_HEIGHT / 2.0f;
    float halfDepth = DRONE_DEPTH / 2.0f + 0.1f;

    // Center at drone.pos
    box.minX = drone.pos[0] - halfWidth;
    box.maxX = drone.pos[0] + halfWidth;
    box.minY = drone.pos[1] - halfHeight;
    box.maxY = drone.pos[1] + halfHeight;
    box.minZ = drone.pos[2] - halfDepth;
    box.maxZ = drone.pos[2] + halfDepth;
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
		if (collisionInvulnTimer <= 0.0f) {
			applyBatteryLoss(COLLISION_BATTERY_PENALTY, LOSS_GROUND);
			collisionInvulnTimer = COLLISION_INVULN_DURATION;
			if (batteryLevel <= 0.0f) triggerGameOver();
		}
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

				if (collisionInvulnTimer <= 0.0f) {
					applyBatteryLoss(COLLISION_BATTERY_PENALTY, LOSS_BUILDING);
					collisionInvulnTimer = COLLISION_INVULN_DURATION;
					if (batteryLevel <= 0.0f) triggerGameOver();
				}
			}
		}
	}
}


// ------------------------------------------------------------
//
// Update Drone State
//

float wrapDegrees(float a) {
		a = fmodf(a + 180.0f, 360.0f);
		if (a < 0.0f) a += 360.0f;
		return a - 180.0f;
	}

void updateDroneState(float dt) {
	// Parameters (tweak to taste)
    const float maxThrottle = 15.0f;   // upward accel when throttleCmd == 1
    const float maxTilt = 46.0f;            // degrees: maximum pitch/roll allowed
	const float dampingFactor = 0.992f; // Reduce velocity by 50% on collision

	// Limits
	drone.yaw = wrapDegrees(drone.yaw);

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

	// decrement invulnerability timer
	if (collisionInvulnTimer > 0.0f) {
		collisionInvulnTimer -= dt;
		if (collisionInvulnTimer < 0.0f) collisionInvulnTimer = 0.0f;
	}

}


// ------------------------------------------------------------
//
// Events from the Keyboard
//

void applyKeyInputs(float dt) {
	if (isGameOver) return;

    // Normal keys
    if (keyStates['w'] || keyStates['W']) {
        drone.throttle +=  30.0f * dt;
    }
    if (keyStates['s'] || keyStates['S']) {
        drone.throttle -=  30.0f * dt;
    }
    if (keyStates['a'] || keyStates['A']) {
        drone.yaw += 120.0f * dt;
    }
    if (keyStates['d'] || keyStates['D']) {
        drone.yaw -= 120.0f * dt;
    }

    // Special keys
    if (specialKeyStates[GLUT_KEY_UP]) {
        drone.pitch -= 120.0f * dt;
    }
    if (specialKeyStates[GLUT_KEY_DOWN]) {
        drone.pitch += 120.0f * dt;
    }
    if (specialKeyStates[GLUT_KEY_LEFT]) {
        drone.roll  -= 120.0f * dt;
    }
    if (specialKeyStates[GLUT_KEY_RIGHT]) {
        drone.roll  += 120.0f * dt;
    }
}

void processKeysDown(unsigned char key, int xx, int yy) {
	keyStates[key] = true;

    switch (key) {
        case 27: glutLeaveMainLoop(); break;;
		case 'f':
			if (!fog_mode) {
				fog_mode = true;
				printf("Fog mode enabled\n");
			}
			else {
				fog_mode = false;
				printf("Fog mode disabled\n");
			}
			break;
		case 'n':
            if (!night_mode) {
				night_mode = true;
				printf("Directional Light disabled. Night Mode enabled\n");
			}
			else {
				night_mode = false;
				printf("Night Mode disabled. Directional Light enabled\n");
			}
			break;
        case 'c':
            if (!plight_mode) {
				plight_mode = true;
				printf("Point Light Mode enabled\n");
			}
			else {
				plight_mode = false;
				printf("Point Light Mode disabled\n");
			}
			break;
		case 'h':
            if (!headlights_mode) {
				headlights_mode = true;
				printf("Headlights On\n");
			}
			else {
				headlights_mode = false;
				printf("Headlights off");
			}
			break;
		case 'p':
			isPaused = !isPaused;
			if (isPaused) {
				memset(keyStates, 0, sizeof(keyStates));
				memset(specialKeyStates, 0, sizeof(specialKeyStates));
				printf("Game paused\n");
			} else {
				printf("Game resumed\n");
			}
			break;			
        case 'r':
            alpha = 57.0f; beta = 18.0f; r = 45.0f;
            camX = r * sin(alpha * 3.14f / 180.0f) * cos(beta * 3.14f / 180.0f);
            camZ = r * cos(alpha * 3.14f / 180.0f) * cos(beta * 3.14f / 180.0f);
            camY = r * sin(beta * 3.14f / 180.0f);
			// reset game state (drone, battery, distance, score)
   			resetGameState();
			printf("Game restarted/reset (R pressed)\n");
            break;
        case 'x': glEnable(GL_MULTISAMPLE); break;
        case 'z': glDisable(GL_MULTISAMPLE); break;
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

// lightDemo.cpp
// lightDemo.cpp
// Render an Assimp node hierarchy using the currently bound mesh shader program.
//
// allMeshes   : renderer.myMeshes (global vector).
// textureIds  : array created by LoadGLTexturesTUs(...) for THIS scene.
// baseIndex   : starting index in allMeshes where THIS model's meshes begin (e.g., droneModelStartIndex).
void aiRecursive_render(const aiNode* nd,
                        std::vector<struct MyMesh>& myMeshes,
                        GLuint*& textureIds)
{
    // --- Node transform (Assimp) -> mu MODEL stack ---
    aiMatrix4x4 m = nd->mTransformation;
    m.Transpose();

    mu.pushMatrix(gmu::MODEL);

    float aux[16];
    memcpy(aux, &m, sizeof(float) * 16);

    mu.multMatrix(gmu::MODEL, aux);

    // --- Draw all meshes for this node ---
    for (unsigned int n = 0; n < nd->mNumMeshes; ++n) {

        // MATERIAL (use the currently bound mesh program)
        GLint prog = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &prog);

        GLint loc;
        loc = glGetUniformLocation(prog, "mat.ambient");
        if (loc >= 0) glUniform4fv(loc, 1, myMeshes[nd->mMeshes[n]].mat.ambient);

        loc = glGetUniformLocation(prog, "mat.diffuse");
        if (loc >= 0) glUniform4fv(loc, 1, myMeshes[nd->mMeshes[n]].mat.diffuse);

        loc = glGetUniformLocation(prog, "mat.specular");
        if (loc >= 0) glUniform4fv(loc, 1, myMeshes[nd->mMeshes[n]].mat.specular);

        loc = glGetUniformLocation(prog, "mat.emissive");
        if (loc >= 0) glUniform4fv(loc, 1, myMeshes[nd->mMeshes[n]].mat.emissive);

        loc = glGetUniformLocation(prog, "mat.shininess");
        if (loc >= 0) glUniform1f(loc, myMeshes[nd->mMeshes[n]].mat.shininess);

        loc = glGetUniformLocation(prog, "mat.texCount");
        if (loc >= 0) glUniform1i(loc, myMeshes[nd->mMeshes[n]].mat.texCount);


		unsigned int diffMapCount = 0;

		// Reset material flags for each mesh
		glUniform1i(renderer.getNormalMapLoc(), false);
		glUniform1i(renderer.getSpecularMapLoc(), false);
		glUniform1ui(renderer.getDiffMapCountLoc(), 0);

		unsigned int meshIdx = nd->mMeshes[n];

		// Only loop if the material claims textures
		if (myMeshes[meshIdx].mat.texCount != 0) {
			for (unsigned int i = 0; i < myMeshes[meshIdx].mat.texCount; ++i) {
				
				//Activate a TU with a Texture Object
				GLuint texId = myMeshes[meshIdx].texUnits[i];  // actual GL texture ID

				// Bind according to type; diffuse supports up to 2 maps
				if (myMeshes[meshIdx].texTypes[i] == DIFFUSE) {
					if (diffMapCount == 0) {
						diffMapCount++;
						renderer.setTexUnit(4, texId);  // first diffuse on unit 4
					} else if (diffMapCount == 1) {
						diffMapCount++;
						renderer.setTexUnit(5, texId);  // second diffuse on unit 5
					} else {
						printf("Only supports a Material with a maximum of 2 diffuse textures\n");
					}
				} else if (myMeshes[meshIdx].texTypes[i] == SPECULAR) {
					renderer.setTexUnit(6, texId);      // specular map on unit 6
					glUniform1i(renderer.getSpecularMapLoc(), GL_TRUE);
				} else if (myMeshes[meshIdx].texTypes[i] == NORMALS) {
					renderer.setTexUnit(7, texId);      // normal map on unit 7
					glUniform1i(renderer.getNormalMapLoc(), GL_TRUE);
				} else {
					printf("Texture Map not supported\n");
				}
			}
			// After the loop, tell the fragment shader how many diffuse maps are bound
			glUniform1ui(renderer.getDiffMapCountLoc(), diffMapCount);
		}


        // --- MATRICES from mu (this fixes the “spawns at package” bug) ---
        mu.computeDerivedMatrix(gmu::PROJ_VIEW_MODEL);
        // Use your global uniform IDs that were set when the mesh program was bound
        glUniformMatrix4fv(renderer.getVmLoc(),   1, GL_FALSE, mu.get(gmu::VIEW_MODEL));
        glUniformMatrix4fv(renderer.getPvmLoc(),  1, GL_FALSE, mu.get(gmu::PROJ_VIEW_MODEL));
        mu.computeNormalMatrix3x3();
        glUniformMatrix3fv(renderer.getNormalLoc(), 1, GL_FALSE, mu.getNormalMatrix());

        // --- Draw ---
        glBindVertexArray(myMeshes[nd->mMeshes[n]].vao);
        glDrawElements(myMeshes[nd->mMeshes[n]].type, myMeshes[nd->mMeshes[n]].numIndexes, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    // Recurse
    for (unsigned int c = 0; c < nd->mNumChildren; ++c)
        aiRecursive_render(nd->mChildren[c], myMeshes, textureIds);

    mu.popMatrix(gmu::MODEL);
}





void drawPackage(dataMesh &data) {
    if (!currentPackage.active) return;

   // If attached -> draw using drone's MODEL transform so it rotates with the drone.
    mu.pushMatrix(gmu::MODEL);
    if (currentPackage.pickedUp) {
        mu.translate(gmu::MODEL, drone.pos[0], drone.pos[1], drone.pos[2]);
        mu.rotate(gmu::MODEL, drone.yaw,   0, 1, 0);
        mu.rotate(gmu::MODEL, drone.pitch, 0, 0, 1);
        mu.rotate(gmu::MODEL, drone.roll,  1, 0, 0);
        // local offset where package is attached (drone-local)
        mu.translate(gmu::MODEL, currentPackage.localPos[0], currentPackage.localPos[1], currentPackage.localPos[2]);
    } else {
        mu.translate(gmu::MODEL, currentPackage.pos[0], currentPackage.pos[1], currentPackage.pos[2]);
    }
    mu.scale(gmu::MODEL, 0.4f, 0.4f, 0.4f); // package size
    mu.computeDerivedMatrix(gmu::PROJ_VIEW_MODEL);
    mu.computeNormalMatrix3x3();

    data.meshID = 0; // Cube mesh
    data.texMode = 0;
    data.vm = mu.get(gmu::VIEW_MODEL);
    data.pvm = mu.get(gmu::PROJ_VIEW_MODEL);
    data.normal = mu.getNormalMatrix();
	renderer.setIsModel(false);
    renderer.renderMesh(data);

    mu.popMatrix(gmu::MODEL);
}


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
		renderer.setIsModel(false);
        renderer.renderMesh(data);
        mu.popMatrix(gmu::MODEL);
    }
}


void drawDrone(dataMesh &data) {
    // isolate from prior draws
    mu.pushMatrix(gmu::MODEL);
    mu.loadIdentity(gmu::MODEL);

    if (renderer.droneMeshes.size() > 0 && droneScene != nullptr) {
        // world transform for the drone
        mu.translate(gmu::MODEL, drone.pos[0], drone.pos[1], drone.pos[2]);
        mu.rotate(gmu::MODEL, drone.yaw,   0, 1, 0);
        mu.rotate(gmu::MODEL, drone.pitch, 0, 0, 1);
        mu.rotate(gmu::MODEL, drone.roll,  1, 0, 0);

        float augment = 1.0f;
        mu.scale(gmu::MODEL,
                 augment * droneImportedScale,
                 augment * droneImportedScale,
                 augment * droneImportedScale);

        // render Assimp model relative to current MODEL
		renderer.setIsModel(true);
        aiRecursive_render(droneScene->mRootNode, renderer.droneMeshes, droneTextureIds);
		renderer.setIsModel(false);

        mu.popMatrix(gmu::MODEL);   // balance the first push
        return;
    }

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
		renderer.setIsModel(false);
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

    for (int k = 0; k < NUM_POINT_LIGHTS; ++k) {
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

	static int lastFrameTime = glutGet(GLUT_ELAPSED_TIME);
	int nowFrame = glutGet(GLUT_ELAPSED_TIME);
	float frameDt = (nowFrame - lastFrameTime) * 0.001f; // seconds
	lastFrameTime = nowFrame;

	if (!isPaused && !isGameOver) {
		applyKeyInputs(frameDt);
		updateDroneState(frameDt);
	}

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
				// smoothing yaw with shortest-path angular interpolation
				const float DEG2RAD = 3.14159265f / 180.0f;
				static float smoothYaw = 0.0f; // persistent across frames
				const float smoothFactor = 0.08f; // your previous value


				float yawRad = drone.yaw * DEG2RAD;


				// compute signed shortest angular difference in [-pi, +pi]
				auto shortestAngularDifference = [](float target, float current) {
				float diff = target - current;
				const float PI = 3.14159265f;
				const float TWO_PI = 2.0f * PI;
				while (diff <= -PI) diff += TWO_PI;
				while (diff > PI) diff -= TWO_PI;
				return diff;
				};


				float diff = shortestAngularDifference(yawRad, smoothYaw);
				smoothYaw += diff * smoothFactor; // move along shortest arc


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

	//Light settings
	renderer.setNightMode(night_mode);
	renderer.setPLightMode(plight_mode);
	renderer.setHeadlightsMode(headlights_mode);

	//Fog Mode
	renderer.setFogMode(fog_mode);

	// --- Directional light ---
	// dirLightDir is a direction (w = 0.0f). Use multMatrixPoint with w==0 to transform vectors
	float dirEye4[4] = { dirLightDir[0], dirLightDir[1], dirLightDir[2], dirLightDir[3] };
	float dAux[4];
	mu.multMatrixPoint(gmu::VIEW, dirEye4, dAux); // transforms ignoring translation because w==0

	// prepare 3-component arrays for the renderer API
	float dirEye3[3] = { dAux[0], dAux[1], dAux[2] };
	float dirAmb[3]  = { 0.2f, 0.2f, 0.2f };  // Increased ambient light
	float sunIntensity = 2.0f; // 1.0 = same, 2.0 = twice as bright
	float dirDiff[3] = { 0.6f * sunIntensity, 0.6f * sunIntensity, 0.6f * sunIntensity };  // Increased diffuse
	float dirSpec[3] = { 0.8f * sunIntensity, 0.8f * sunIntensity, 0.8f * sunIntensity };  // Increased specular

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

	// --- Spotlights  ---
	for (int i = 0; i < NUM_SPOT_LIGHTS; ++i) {

		mu.pushMatrix(gmu::MODEL);
		mu.translate(gmu::MODEL, drone.pos[0], drone.pos[1], drone.pos[2]);
		mu.rotate(gmu::MODEL, drone.yaw,   0, 1, 0);
		mu.rotate(gmu::MODEL, drone.pitch, 0, 0, 1);
		mu.rotate(gmu::MODEL, drone.roll,  1, 0, 0);

		// --- compute spotlight world position from local offset (w = 1) ---
		float localOffset[4] = { spotLightOffset[i][0], spotLightOffset[i][1], spotLightOffset[i][2], 1.0f };
		float worldPos4[4];
		mu.multMatrixPoint(gmu::MODEL, localOffset, worldPos4); // MODEL -> world
		float sAux4[4];
		mu.multMatrixPoint(gmu::VIEW, worldPos4, sAux4);        // world -> eye
		float sPos3[3] = { sAux4[0], sAux4[1], sAux4[2] };

		// Rotation by +90 deg about Z: (x,y) -> (-y, x)
		float localForward[4] = { 0.0f, 1.0f, 0.0f, 0.0f }; // w=0 because it's a direction
		float localRotated[4] = {
			-localForward[1],  // new X = -y
			localForward[0],  // new Y = x
			localForward[2],  // Z unchanged
			0.0f
		};

		// Transform rotated local direction -> world -> eye (use w=0)
		float worldDir4[4];
		mu.multMatrixPoint(gmu::MODEL, localRotated, worldDir4); // MODEL->world (w still 0)
		float eyeDir4[4];
		mu.multMatrixPoint(gmu::VIEW, worldDir4, eyeDir4);       // world->eye (w=0 stays 0)

		// Front or back
		float sDir3[3] = { -eyeDir4[0], -eyeDir4[1], -eyeDir4[2] };
		if (i >= 2) {
			sDir3[0] =  eyeDir4[0];
			sDir3[1] =  eyeDir4[1];
			sDir3[2] =  eyeDir4[2];
		}
		
		float len = sqrtf(sDir3[0]*sDir3[0] + sDir3[1]*sDir3[1] + sDir3[2]*sDir3[2]);
		if (len > 1e-6f) { sDir3[0] /= len; sDir3[1] /= len; sDir3[2] /= len; }

		// restore MODEL stack
		mu.popMatrix(gmu::MODEL);

		// --- rest of your spotlight parameters (unchanged) ---
		float intensity = 5.0f;
		float sAmb[3]  = { 1.0f, 1.0f, 1.0f };
		float sDiff[3] = { 1.0f * intensity, 1.0f * intensity, 1.0f * intensity };
		float sSpec[3] = { 0.8f * intensity, 0.8f * intensity, 0.8f * intensity };

		if (i >= 2) {
			intensity = 2.0f;
			sAmb[0]  = 1.0f; sAmb[1]  = 0.0f; sAmb[2]  = 0.0f;
			sDiff[0] = 2.0f * intensity; sDiff[1] = 0.0f; sDiff[2] = 0.0f;
		}
		const float PI = 3.14159265358979f;
		float innerCutDeg = 12.5f, outerCutDeg = 17.5f;
		float innerCutCos = cosf(innerCutDeg * PI / 180.0f);
		float outerCutCos = cosf(outerCutDeg * PI / 180.0f);

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

	renderer.setIsModel(false);
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

			// highlight package source/destination: temporarily modify the cube's diffuse color
			float savedDiffuse[4];
			memcpy(savedDiffuse, renderer.myMeshes[0].mat.diffuse, sizeof(savedDiffuse));

			if (currentPackage.active && !currentPackage.pickedUp && i == currentPackage.src_i && j == currentPackage.src_j) {
				float col[4] = {0.2f, 1.0f, 0.2f, 1.0f}; // green = source
				memcpy(renderer.myMeshes[0].mat.diffuse, col, sizeof(col));
			} else if (currentPackage.active && currentPackage.dst_i == i && currentPackage.dst_j == j) {
				float col[4] = {1.0f, 0.2f, 0.2f, 1.0f}; // red = destination
				memcpy(renderer.myMeshes[0].mat.diffuse, col, sizeof(col));
			}
			
			renderer.setIsModel(false);
			renderer.renderMesh(data);

			// restore original diffuse so other objects not affected
			memcpy(renderer.myMeshes[0].mat.diffuse, savedDiffuse, sizeof(savedDiffuse));

			mu.popMatrix(gmu::MODEL);
		}
	}

	// Update timers and states
	static int lastBirdTime = glutGet(GLUT_ELAPSED_TIME);
	int nowBird = glutGet(GLUT_ELAPSED_TIME);
	float dtBird = (nowBird - lastBirdTime) * 0.001f;
	lastBirdTime = nowBird;
	if (!isPaused) updateBirds(dtBird);

	static int lastTime = glutGet(GLUT_ELAPSED_TIME);
	int now = glutGet(GLUT_ELAPSED_TIME);
	float dt = (now - lastTime) * 0.001f;
	lastTime = now;

	// Check game state and interactions first
	checkPackagePickupAndDelivery();

	// Draw objects in correct order and ensure model matrix is reset for each

	drawBirds(data);  // Background elements first
	drawPackage(data);  // Then package 
	drawDrone(data);  // Draw drone last so it's on top

	if (!isPaused && !isGameOver) {
		updateBatteryAndDistance(dt);
	} else if (!isPaused && isGameOver) {
		if (drone.pos[1] <= 0.0f) {
			drone.pos[1] = 0.0f;
			drone.velocity[0] = 0.0f;
			drone.velocity[1] = 0.0f;
			drone.velocity[2] = 0.0f;
		} else drone.pos[1] += DRONE_FALL_VELOCITY * dt;
	} 
	

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
	float dx = camWX - CITY_CENTER[0];
	float dy = camWY - CITY_CENTER[1];
	float dz = camWZ - CITY_CENTER[2];
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
	mu.translate(gmu::MODEL, CITY_CENTER[0], CITY_CENTER[1], CITY_CENTER[2]);
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
	renderer.setIsModel(false);
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
		char distBuf[32];
		snprintf(distBuf, sizeof(distBuf), "%.2f", distanceTraveled);
		TextCommand textCmd = { std::string("Distance Flown: ") + distBuf +
								"\n\nScore: " + std::to_string(scorePoints),
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

		// --- Game Over centered message ---
		if (isGameOver) {
			// simple center approximation: place text at center using WinX/WinY
			TextCommand goCmd;
			goCmd.str = std::string("GAME OVER\nPress R to restart");
			goCmd.size = 0.9f;
			// center position: bottom-left origin for renderText, do a simple estimate
			goCmd.position[0] = (float)(WinX/2 - 160);
			goCmd.position[1] = (float)(WinY/2 - 20);
			goCmd.pvm = mu.get(gmu::PROJ_VIEW_MODEL);
			goCmd.color[0] = 1.0f; goCmd.color[1] = 0.2f; goCmd.color[2] = 0.2f; goCmd.color[3] = 1.0f;
			renderer.renderText(goCmd);
		}


		if (isPaused) {
			TextCommand pauseCmd = { "PAUSED", { WinX/2 - 160, WinY/2 - 20 }, 1.0f};
			pauseCmd.pvm = mu.get(gmu::PROJ_VIEW_MODEL);
			renderer.renderText(pauseCmd);
		}

		// --- battery HUD (top-left) ---
		float barX = 20.0f;
		float barY = (float)(m_viewport[3] - 60);
		float barW = 200.0f;
		float barH = 12.0f;

		renderer.activateRenderMeshesShaderProg();

		// get currently bound program
		GLint curProg = 0;
		glGetIntegerv(GL_CURRENT_PROGRAM, &curProg);
		if (curProg == 0) {
			printf("No program bound — activateRenderMeshesShaderProg didn't bind.\n");
		}

		// get uniform locations (cache these in init for better perf)
		GLint locIsHud   = glGetUniformLocation((GLuint)curProg, "is_Hud");
		GLint locHudColor= glGetUniformLocation((GLuint)curProg, "uHudColor");

		if (locIsHud != -1) {
			glProgramUniform1i((GLuint)curProg, locIsHud, 1); // enable HUD mode
		} else {
			printf("Warning: is_Hud loc == -1\n");
		}

		glDisable(GL_CULL_FACE);

		// --- draw background rect (unlit) ---
		float bgCol[4] = {0.15f, 0.15f, 0.15f, 0.9f};
		if (locHudColor != -1) {
			glProgramUniform4fv((GLuint)curProg, locHudColor, 1, bgCol);
		}

		mu.pushMatrix(gmu::MODEL);
		mu.translate(gmu::MODEL, barX + barW*0.5f, barY + barH*0.5f, 0.0f);
		mu.scale(gmu::MODEL, barW, barH, 1.0f);
		mu.computeDerivedMatrix(gmu::PROJ_VIEW_MODEL);
		mu.computeNormalMatrix3x3();

		data.meshID = 7;      // same mesh used for both rects is okay
		data.texMode = 0;
		data.vm  = mu.get(gmu::VIEW_MODEL);
		data.pvm = mu.get(gmu::PROJ_VIEW_MODEL);
		data.normal = mu.getNormalMatrix();
		renderer.setIsModel(false);
		renderer.renderMesh(data);
		mu.popMatrix(gmu::MODEL);

		// --- draw fill rect (unlit, set color BEFORE draw) ---
		float fillW = barW * batteryLevel;
		if (fillW < 4.0f) fillW = 4.0f;

		float rcol = (batteryLevel < 0.3f) ? 1.0f : 0.0f;
		float gcol = (batteryLevel >= 0.5f) ? 1.0f : (batteryLevel < 0.3f ? 0.0f : (batteryLevel * 2.0f));
		float fillCol[4] = { rcol, gcol, 0.0f, 1.0f };

		if (locHudColor != -1) {
			glProgramUniform4fv((GLuint)curProg, locHudColor, 1, fillCol);
		}

		mu.pushMatrix(gmu::MODEL);
		mu.translate(gmu::MODEL, barX + 2.0f + (fillW - 4.0f)*0.5f, barY + barH*0.5f, 0.0f);
		mu.scale(gmu::MODEL, fillW - 4.0f, barH - 4.0f, 1.0f);
		mu.computeDerivedMatrix(gmu::PROJ_VIEW_MODEL);
		mu.computeNormalMatrix3x3();

		data.vm  = mu.get(gmu::VIEW_MODEL);
		data.pvm = mu.get(gmu::PROJ_VIEW_MODEL);
		renderer.setIsModel(false);
		renderer.renderMesh(data);
		mu.popMatrix(gmu::MODEL);

		glEnable(GL_CULL_FACE);

		// disable HUD mode on shader
		if (locIsHud != -1) {
			glProgramUniform1i((GLuint)curProg, locIsHud, 0);
		}

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

	// create geometry and VAO of the HUD
	amesh = createQuad(1.0f, 1.0f); 
	float hudAmb[]   = {0.0f, 0.0f, 0.0f, 1.0f};
	float hudDiff[]  = {1.0f, 1.0f, 1.0f, 1.0f};
	float hudSpec[]  = {0.0f, 0.0f, 0.0f, 1.0f};
	float hudEmiss[] = {1.0f, 1.0f, 1.0f, 1.0f};
	memcpy(amesh.mat.ambient,  hudAmb,  4 * sizeof(float));
	memcpy(amesh.mat.diffuse,  hudDiff, 4 * sizeof(float));
	memcpy(amesh.mat.specular, hudSpec, 4 * sizeof(float));
	memcpy(amesh.mat.emissive, hudEmiss, 4 * sizeof(float));
	amesh.mat.shininess = 1.0f;
	amesh.mat.texCount = 0;
	renderer.myMeshes.push_back(amesh);

	//The truetypeInit creates a texture object in TexObjArray for storing the fontAtlasTexture
	
	fontLoaded = renderer.truetypeInit(fontPathFile);
	if (!fontLoaded)
		cerr << "Fonts not loaded\n";
	else 
		cerr << "Fonts loaded\n";

	// path to the folder containing drone.obj and its textures (relative to EXE)
    std::string droneObj = "spider/spider.obj"; 

    printf("[DRONE] Loading: %s\n", droneObj.c_str());

	if (!Import3DFromFile(droneObj.c_str(), droneImporter, droneScene, droneImportedScale)) 
		return;

	strcpy(model_dir, "spider/");
	// create meshes & textures for the drone model using the existing helper
	renderer.droneMeshes = createMeshFromAssimp(droneScene, droneTextureIds);
	if (!droneTextureIds) { printf("Drone textures not loaded\n"); }

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glEnable(GL_MULTISAMPLE);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Back to black background


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

	// Core OpenGL state setup
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);        // Ensure proper depth testing
	glDepthMask(GL_TRUE);          // Enable depth writes by default
	
	// Face culling setup
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);          // Cull back faces
	glFrontFace(GL_CCW);          // Counter-clockwise front faces
	
	// Blending setup for transparent objects
	glDisable(GL_BLEND);          // Start with blending off
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquation(GL_FUNC_ADD);
	
	// Multisampling for smoother edges
	glEnable(GL_MULTISAMPLE);
	
	// Clear color - black background
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
	resetGameState();
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
