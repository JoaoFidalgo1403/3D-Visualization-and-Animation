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

// Camera modes
enum CameraMode { FOLLOW, TOP_ORTHO, TOP_PERSPECTIVE};
CameraMode cameraMode = FOLLOW;

// Mouse Tracking Variables
int startX, startY, tracking = 0;

// Frame counting and FPS computation
long myTime,timebase = 0,frame = 0;
char s[32];

float lightPos[4] = {4.0f, 15.0f, 2.0f, 1.0f};
//float lightPos[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

//Spotlight
bool spotlight_mode = false;
float coneDir[4] = { 0.0f, -0.0f, -1.0f, 0.0f };

bool fontLoaded = false;

float cubeHeights[6][6]; // Store heights for each cube

struct vec3 {
	float x, y, z;
	vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
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

void refresh(int value)
{
	//PUT YOUR CODE HERE
}

// ------------------------------------------------------------
//
// Reshape Callback Function
//

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


// Initialize birds with random positions, velocities, and rotation speeds
void initBirds(int numBirds) {
    birds.clear();
    for (int i = 0; i < numBirds; ++i) {
        float x = -10.0f + 50.0f * (rand() / (float)RAND_MAX);
        float y = 5.0f + 8.0f * (rand() / (float)RAND_MAX);
        float z = -10.0f + 30.0f * (rand() / (float)RAND_MAX);
        float vx = 10.0f * ((rand() / (float)RAND_MAX) - 0.5f);
        float vy = 10.0f * ((rand() / (float)RAND_MAX) - 0.5f);
        float vz = 10.0f * ((rand() / (float)RAND_MAX) - 0.5f);
        float rotSpeed = 90.0f * ((rand() / (float)RAND_MAX) - 0.5f); // -30 to +30 deg/sec
        birds.emplace_back(x, y, z, vx, vy, vz, rotSpeed);
    }
}

// PUTO poe os passaros a darem spawn a 1a vez tambem no interior da esfera

// Update bird positions and rotations
void updateBirds(float dt) {
    const float maxDistFromDrone = 25.0f; // radius around drone

    // Speed multiplier after 30 seconds
    int elapsedMs = glutGet(GLUT_ELAPSED_TIME);
    float speedMultiplier = (elapsedMs > 30000) ? 2.0f : 1.0f;

    for (auto& b : birds) {
        // Update position and rotation
        for (int i = 0; i < 3; ++i)
            b.pos[i] += b.velocity[i] * dt * speedMultiplier;
        b.rotation += b.rotSpeed * dt;
        if (b.rotation > 360.0f) b.rotation -= 360.0f;
        if (b.rotation < 0.0f) b.rotation += 360.0f;

        // If too far from drone, respawn at random position/direction on sphere surface
        float dx = b.pos[0] - drone.pos[0];
        float dy = b.pos[1] - drone.pos[1];
        float dz = b.pos[2] - drone.pos[2];
        float dist = sqrt(dx*dx + dy*dy + dz*dz);
        if (dist > maxDistFromDrone) {
            // Random point on sphere surface
            float theta = 2.0f * 3.14159265f * (rand() / (float)RAND_MAX); // azimuthal angle
            float phi = acos(2.0f * (rand() / (float)RAND_MAX) - 1.0f);    // polar angle
            float r = maxDistFromDrone;
            b.pos[0] = drone.pos[0] + r * sin(phi) * cos(theta);
            b.pos[1] = drone.pos[1] + r * cos(phi);
            b.pos[2] = drone.pos[2] + r * sin(phi) * sin(theta);

            // Inward direction with random spread
            float dir[3] = {
                drone.pos[0] - b.pos[0],
                drone.pos[1] - b.pos[1],
                drone.pos[2] - b.pos[2]
            };
            // Normalize direction
            float len = sqrt(dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2]);
            dir[0] /= len; dir[1] /= len; dir[2] /= len;

            // Add random spread (angle up to ~30 degrees)
            float spreadAngle = 0.5236f; // ~30 degrees in radians
            float randAngle = (rand() / (float)RAND_MAX) * spreadAngle;
            float randAxisTheta = 2.0f * 3.14159265f * (rand() / (float)RAND_MAX);

            // Generate a random axis perpendicular to dir
            float axis[3] = {
                -dir[1], dir[0], 0
            };
            float axisLen = sqrt(axis[0]*axis[0] + axis[1]*axis[1] + axis[2]*axis[2]);
            if (axisLen > 0.0001f) {
                axis[0] /= axisLen; axis[1] /= axisLen; axis[2] /= axisLen;
            } else {
                axis[0] = 1; axis[1] = 0; axis[2] = 0;
            }

            // Rodrigues' rotation formula to rotate dir by randAngle around axis
            float cosA = cos(randAngle);
            float sinA = sin(randAngle);
            float dot = dir[0]*axis[0] + dir[1]*axis[1] + dir[2]*axis[2];
            float newDir[3];
            newDir[0] = cosA * dir[0] + sinA * (axis[1]*dir[2] - axis[2]*dir[1]) + (1-cosA)*dot*axis[0];
            newDir[1] = cosA * dir[1] + sinA * (axis[2]*dir[0] - axis[0]*dir[2]) + (1-cosA)*dot*axis[1];
            newDir[2] = cosA * dir[2] + sinA * (axis[0]*dir[1] - axis[1]*dir[0]) + (1-cosA)*dot*axis[2];

            // Final velocity
            float speed = 10.0f;
            b.velocity[0] = speed * newDir[0];
            b.velocity[1] = speed * newDir[1];
            b.velocity[2] = speed * newDir[2];

            b.rotation = 0.0f;
            b.rotSpeed = 90.0f * ((rand() / (float)RAND_MAX) - 0.5f);
        }
    }
}

// ------------------------------------------------------------
//
// Update Drone State
//

void updateDroneState(float dt) {
	// Parameters (tweak to taste)
    const float maxThrottle = 10.0f;   // upward accel when throttleCmd == 1
    const float maxTilt = 45.0f;            // degrees: maximum pitch/roll allowed
	
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

	drone.velocity[0] = drone.throttle * drone.dir[0] + (1 + drone.throttle) * -drone.pitch;
	drone.velocity[1] = drone.throttle * drone.dir[1];
	drone.velocity[2] = drone.throttle * drone.dir[2] + (1 + drone.throttle) * drone.roll;

	// Integrate position

	drone.pos[0] += drone.velocity[0] * dt;
    drone.pos[1] += drone.velocity[1] * dt;
    drone.pos[2] += drone.velocity[2] * dt;
}


// ------------------------------------------------------------
//
// Render stuff
//

void drawBirds(dataMesh& data) {
    data.meshID = 2; // Sphere mesh
    for (const auto& b : birds) {
        mu.pushMatrix(gmu::MODEL);
        mu.translate(gmu::MODEL, b.pos[0], b.pos[1], b.pos[2]);
        mu.rotate(gmu::MODEL, b.rotation, 0, 1, 0); // Rotate around Y axis
        mu.scale(gmu::MODEL, 0.2f, 0.2f, 0.2f); // Sphere size
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
		mu.translate(gmu::MODEL, drone_parts_position[i].x, drone_parts_position[i].y, drone_parts_position[i].z);
		if (i == 0) {
			mu.scale(gmu::MODEL, 1.25f, 0.15f, 0.75f);
		} else if (i > 0 && i < 3) {
			mu.scale(gmu::MODEL, 0.5f, 0.05f, 0.5f);
		} else {
			mu.scale(gmu::MODEL, 0.625f, 0.05f, 0.625f);
		}

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

void renderSim(void) {

	FrameCount++;
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	renderer.activateRenderMeshesShaderProg(); // use the required GLSL program to draw the meshes with illumination

	//Associar os Texture Units aos Objects Texture
	//stone.tga loaded in TU0; checker.tga loaded in TU1;  lightwood.tga loaded in TU2
	renderer.setTexUnit(0, 0);
	renderer.setTexUnit(1, 1);
	renderer.setTexUnit(2, 2);

	// load identity matrices
	mu.loadIdentity(gmu::VIEW);
	mu.loadIdentity(gmu::MODEL);
	
	switch (cameraMode) {
		case FOLLOW:
			mu.lookAt(drone.pos[0] + camX, drone.pos[1] + camY,  drone.pos[2] + camZ, 
					  drone.pos[0] + 1.0f, drone.pos[1] + 0.25f, drone.pos[2] + 1.0f, 
					  0, 1, 0);
			break;
		case TOP_ORTHO:
			mu.lookAt(0, 50, 0, 
					  0, 0,  0, 
					  0, 0, -1);
			mu.loadIdentity(gmu::PROJECTION);
			mu.ortho(-30, 30, -30, 30, -100, 100);
			break;
		
		case TOP_PERSPECTIVE:
			mu.lookAt(0, 50, 0, 
					  0, 0,  0, 
					  0, 0, -1);
			mu.loadIdentity(gmu::PROJECTION);
			mu.perspective(53.13f, (1.0f * WinX) / WinY, 0.1f, 1000.0f);
			break;
	}

	//send the light position in eye coordinates
	//renderer.setLightPos(lightPos); //efeito capacete do mineiro, ou seja lighPos foi definido em eye coord 

	float lposAux[4];
	mu.multMatrixPoint(gmu::VIEW, lightPos, lposAux);   //lightPos definido em World Coord so is converted to eye space
	renderer.setLightPos(lposAux);

	//Spotlight settings
	renderer.setSpotLightMode(spotlight_mode);
	renderer.setSpotParam(coneDir, 0.93f);

	
	// Draw the terrain - myMeshes[6] contains the Quad object
	mu.pushMatrix(gmu::MODEL);
	mu.translate(gmu::MODEL, 0.0f, -1.15f, 0.0f);
	mu.scale(gmu::MODEL, 30.0f, 0.1f, 30.0f);
	mu.rotate(gmu::MODEL, -90.0f, 1.0f, 0.0f, 0.0f); // Rotate quad from XY to XZ plane

	mu.computeDerivedMatrix(gmu::PROJ_VIEW_MODEL);
	mu.computeNormalMatrix3x3();

	dataMesh data;

	data.meshID = 6; // For the terrain (last mesh)
	data.texMode = 0; //modulate diffuse color with texel color
	data.vm = mu.get(gmu::VIEW_MODEL),
	data.pvm = mu.get(gmu::PROJ_VIEW_MODEL);
	data.normal = mu.getNormalMatrix();
	renderer.renderMesh(data);
	mu.popMatrix(gmu::MODEL);

	data.meshID = 0; // For the cube (myMeshes[0])
	for (int i=0; i<6; ++i) {  // Draw the other objects in the scene (myMeshes[0] to myMeshes[5])
		for (int j=0; j<6; ++j) {

			if (i == 3 && j == 4 || (i == 1 || i == 2) && j == 1) continue;

			data.texMode = 0; //no texturing
			mu.pushMatrix(gmu::MODEL);
			mu.translate(gmu::MODEL, -20.0f + i * 20.0f, 0.0f, -20.0f + j * 20.0f);
			mu.scale(gmu::MODEL, 10.0f, cubeHeights[i][j], 10.0f);

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
    updateDroneState(dt);

	drawDrone(data);
	

	//Render text (bitmap fonts) in screen coordinates. So use ortoghonal projection with viewport coordinates.
	//Each glyph quad texture needs just one byte color channel: 0 in background and 1 for the actual character pixels. Use it for alpha blending
	//text to be rendered in last place to be in front of everything
	
	if(fontLoaded) {
		glDisable(GL_DEPTH_TEST);
		TextCommand textCmd = { "Yaw: " + std::to_string(drone.yaw) + 
								"\n\nPitch: " + std::to_string(drone.pitch) + 
								"\n\nRoll: " + std::to_string(drone.roll) +
								"\n\n\nThrottle: " + std::to_string(drone.throttle) +
								"\n\n\nDirection: (" + std::to_string(drone.dir[0]) + ", " + std::to_string(drone.dir[1]) + ", " + std::to_string(drone.dir[2]) + ")", 
								{30, 450}, 0.3f };
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

// ------------------------------------------------------------
//
// Events from the Keyboard
//

void processSpecialKeys(int key, int xx, int yy) {
    switch (key) {
        case GLUT_KEY_UP:    drone.pitch -=  2.0f; break; // nose down => forward
        case GLUT_KEY_DOWN:  drone.pitch +=  2.0; break; // nose up => backward
        case GLUT_KEY_LEFT:  drone.roll  -=  2.0; break; // roll left
        case GLUT_KEY_RIGHT: drone.roll  +=  2.0; break; // roll right
    }
}


void processKeysUp(unsigned char key, int xx, int yy)
{
	switch(key) {
		case 'w': drone.throttle =  0.0f; break;
		case 's': drone.throttle =  0.0f; break;
	}
}


void processKeys(unsigned char key, int xx, int yy)
{
    switch(key) {
        case 27:
            glutLeaveMainLoop();
            break;

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

        case '1': cameraMode = TOP_ORTHO; printf("Camera mode: TOP ORTHO\n"); break;
        case '2': cameraMode = TOP_PERSPECTIVE; printf("Camera mode: TOP PERSPECTIVE\n"); break;
        case '3': cameraMode = FOLLOW; printf("Camera mode: FOLLOW\n"); break;

        // throttle: set command so updateDroneState integrates throttle
        case 'w': drone.throttle +=  1.0f; break;
        case 's': drone.throttle -=  1.0f; break;

        // yaw commands (continuous while key held)
        case 'a': drone.yaw +=  2.0f; break;
        case 'd': drone.yaw -=  2.0f; break;

		// Reset drone movement
		case ' ': drone.pitch = 0.0f; drone.roll = 0.0f; drone.throttle = 0.0f; drone.yaw = 0.0f; break;
    }
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

// Track mouse motion while buttons are pressed

void processMouseMotion(int xx, int yy)
{

	int deltaX, deltaY;
	float alphaAux, betaAux;
	float rAux;

	deltaX =  - xx + startX;
	deltaY =    yy - startY;

	// left mouse button: move camera
	if (tracking == 1) {


		alphaAux = alpha + deltaX;
		betaAux = beta + deltaY;

		if (betaAux > 85.0f)
			betaAux = 85.0f;
		else if (betaAux < -85.0f)
			betaAux = -85.0f;
		rAux = r;
	}
	// right mouse button: zoom
	else if (tracking == 2) {

		alphaAux = alpha;
		betaAux = beta;
		rAux = r + (deltaY * 0.01f);
		if (rAux < 0.1f)
			rAux = 0.1f;
	}

	camX = rAux * sin(alphaAux * 3.14f / 180.0f) * cos(betaAux * 3.14f / 180.0f);
	camZ = rAux * cos(alphaAux * 3.14f / 180.0f) * cos(betaAux * 3.14f / 180.0f);
	camY = rAux *   						       sin(betaAux * 3.14f / 180.0f);


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


//  uncomment this if not using an idle or refresh func
//	glutPostRedisplay();
}


//
// Scene building with basic geometry
//

void buildScene()
{
	//Texture Object definition
	renderer.TexObjArray.texture2D_Loader("assets/stone.tga");
	renderer.TexObjArray.texture2D_Loader("assets/checker.png");
	renderer.TexObjArray.texture2D_Loader("assets/lightwood.tga");

	//Scene geometry with triangle meshes

	MyMesh amesh;

	float amb[] = { 0.2f, 0.15f, 0.1f, 1.0f };
	float diff[] = { 0.8f, 0.6f, 0.4f, 1.0f };
	float spec[] = { 0.8f, 0.8f, 0.8f, 1.0f };

	float amb1[] = { 0.3f, 0.0f, 0.0f, 1.0f };
	float diff1[] = { 0.8f, 0.1f, 0.1f, 1.0f };
	float spec1[] = { 0.3f, 0.3f, 0.3f, 1.0f };

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
	amesh.mat.texCount = 1;
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
	//glutTimerFunc(0, refresh, 0);    //use it to to get 60 FPS whatever

//	Mouse and Keyboard Callbacks
	glutKeyboardFunc(processKeys);
	glutMouseFunc(processMouseButtons);
	glutMotionFunc(processMouseMotion);
	glutMouseWheelFunc ( mouseWheel ) ;

	glutSpecialFunc(processSpecialKeys);
	glutKeyboardUpFunc(processKeysUp);
	

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

	if(!renderer.setRenderMeshesShaderProg("shaders/mesh.vert", "shaders/mesh.frag") || 
		!renderer.setRenderTextShaderProg("shaders/ttf.vert", "shaders/ttf.frag"))
	return(1);

	// Initialize birds
	initBirds(50); // Create 50 birds

	//  GLUT main loop
	glutMainLoop();

	return(0);
}



