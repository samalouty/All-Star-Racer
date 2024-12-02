#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <iostream>
#include "TextureBuilder.h"
#include "Model_3DS.h"
#include <filesystem>
//#include "Model_GLB.h"
#include "GLTexture.h"
#include <glut.h>
#include "tiny_gltf.h"
#include <glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <unordered_map>
#include <string>

#define M_PI 3.14159265358979323846


GLuint shaderProgram;


class Vector
{
public:
	GLdouble x, y, z;
	Vector() {}
	Vector(GLdouble _x, GLdouble _y, GLdouble _z) : x(_x), y(_y), z(_z) {}
	//================================================================================================//
	// Operator Overloading; In C++ you can override the behavior of operators for you class objects. //
	// Here we are overloading the += operator to add a given value to all vector coordinates.        //
	//================================================================================================//
	void operator +=(float value)
	{
		x += value;
		y += value;
		z += value;
	}

    float distanceToNoY(const Vector& other) const {
        return std::sqrt(std::pow(x - other.x, 2) + std::pow(z - other.z, 2));
    }

	void print() const {
		std::cout << "Vector(" << x << ", " << y << ", " << z << ")" << std::endl;
	}
};




class GLTFModel {
public:
	bool LoadModel(const std::string& filename) {
		tinygltf::TinyGLTF loader;
		std::string err;
		std::string warn;

		bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename);

		if (!warn.empty()) {
			std::cout << "GLTF loading warning: " << warn << std::endl;
		}

		if (!err.empty()) {
			std::cerr << "GLTF loading error: " << err << std::endl;
		}

		if (!ret) {
			std::cerr << "Failed to load glTF: " << filename << std::endl;
			return false;
		}

		return true;
	}

	void DrawModel(const glm::mat4& transform = glm::mat4(1.0f)) const {
		const tinygltf::Scene& scene = model.scenes[model.defaultScene];
		for (size_t i = 0; i < scene.nodes.size(); ++i) {
			DrawNode(scene.nodes[i], transform);
		}
	}

private:
	tinygltf::Model model;
	mutable std::unordered_map<int, GLuint> textureCache;

	void DrawNode(int nodeIndex, const glm::mat4& parentTransform) const {
		const tinygltf::Node& node = model.nodes[nodeIndex];

		glm::mat4 localTransform = glm::mat4(1.0f);

		if (node.matrix.size() == 16) {
			localTransform = glm::make_mat4(node.matrix.data());
		}
		else {
			if (node.translation.size() == 3) {
				localTransform = glm::translate(localTransform,
					glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
			}

			if (node.rotation.size() == 4) {
				glm::quat q = glm::quat(node.rotation[3], node.rotation[0],
					node.rotation[1], node.rotation[2]);
				localTransform = localTransform * glm::mat4_cast(q);
			}

			if (node.scale.size() == 3) {
				localTransform = glm::scale(localTransform,
					glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
			}
		}

		glm::mat4 nodeTransform = parentTransform * localTransform;

		if (node.mesh >= 0) {
			DrawMesh(model.meshes[node.mesh], nodeTransform);
		}

		for (int child : node.children) {
			DrawNode(child, nodeTransform);
		}
	}

	void DrawMesh(const tinygltf::Mesh& mesh, const glm::mat4& transform) const {
		glPushMatrix();
		glMultMatrixf(glm::value_ptr(transform));

		for (const auto& primitive : mesh.primitives) {
			if (primitive.indices < 0) continue;

			// Set material properties before drawing
			if (primitive.material >= 0) {
				const auto& material = model.materials[primitive.material];
				SetMaterial(material);
			}

			// Get vertex positions
			const auto& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
			const auto& posView = model.bufferViews[posAccessor.bufferView];
			const float* positions = reinterpret_cast<const float*>(
				&model.buffers[posView.buffer].data[posView.byteOffset + posAccessor.byteOffset]);

			// Get texture coordinates if available
			const float* texcoords = nullptr;
			if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
				const auto& texAccessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
				const auto& texView = model.bufferViews[texAccessor.bufferView];
				texcoords = reinterpret_cast<const float*>(
					&model.buffers[texView.buffer].data[texView.byteOffset + texAccessor.byteOffset]);
			}

			// Get indices
			const auto& indexAccessor = model.accessors[primitive.indices];
			const auto& indexView = model.bufferViews[indexAccessor.bufferView];
			const void* indices = &model.buffers[indexView.buffer].data[indexView.byteOffset +
				indexAccessor.byteOffset];

			// Draw the primitive
			glBegin(GL_TRIANGLES);
			for (size_t i = 0; i < indexAccessor.count; i++) {
				unsigned int idx;
				switch (indexAccessor.componentType) {
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
					idx = ((unsigned short*)indices)[i];
					break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
					idx = ((unsigned int*)indices)[i];
					break;
				default:
					continue;
				}

				if (texcoords) {
					glTexCoord2f(texcoords[idx * 2], texcoords[idx * 2 + 1]);
				}
				glVertex3fv(&positions[idx * 3]);
			}
			glEnd();
		}

		glPopMatrix();
	}

	void SetMaterial(const tinygltf::Material& material) const {
		// Set default material color
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

		if (material.pbrMetallicRoughness.baseColorTexture.index >= 0) {
			const auto& texture = model.textures[material.pbrMetallicRoughness.baseColorTexture.index];
			if (texture.source >= 0) {
				GLuint textureId = GetOrCreateTexture(texture.source);
				if (textureId != 0) {
					glEnable(GL_TEXTURE_2D);
					glBindTexture(GL_TEXTURE_2D, textureId);
				}
			}
		}
		else if (!material.pbrMetallicRoughness.baseColorFactor.empty()) {
			glColor4f(
				material.pbrMetallicRoughness.baseColorFactor[0],
				material.pbrMetallicRoughness.baseColorFactor[1],
				material.pbrMetallicRoughness.baseColorFactor[2],
				material.pbrMetallicRoughness.baseColorFactor[3]
			);
		}
	}

	GLuint GetOrCreateTexture(int sourceIndex) const {
		if (textureCache.find(sourceIndex) != textureCache.end()) {
			return textureCache.at(sourceIndex);
		}

		const auto& image = model.images[sourceIndex];
		GLuint textureId;
		glGenTextures(1, &textureId);
		glBindTexture(GL_TEXTURE_2D, textureId);

		GLenum format = GL_RGBA;
		if (image.component == 3) {
			format = GL_RGB;
		}

		glTexImage2D(GL_TEXTURE_2D, 0, format, image.width, image.height, 0, format, GL_UNSIGNED_BYTE, &image.image[0]);

		GLenum type = GL_UNSIGNED_BYTE;
		if (image.bits == 16) {
			type = GL_UNSIGNED_SHORT;
		}

		GLint internalFormat = (format == GL_RGB) ? GL_RGB8 : GL_RGBA8;

		GLint buildMipmapsResult = gluBuild2DMipmaps(GL_TEXTURE_2D, internalFormat, image.width, image.height, format, type, image.image.data());

		if (buildMipmapsResult != 0) {
			std::cerr << "Failed to build mipmaps for texture. GLU error: " << gluErrorString(buildMipmapsResult) << std::endl;
			glDeleteTextures(1, &textureId);
			return 0;
		}
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		textureCache[sourceIndex] = textureId;
		return textureId;
	}
};

GLTFModel gltfModel1;
GLTFModel carModel1;
GLTFModel coneModel;
GLTFModel nitroModel;
GLTFModel redWheelsFrontLeft1;
GLTFModel redWheelsFrontRight1;
GLTFModel redWheelsBackLeft1;
GLTFModel redWheelsBackRight1;
GLTFModel finishModel; 
GLTFModel horizontalTraffic;
GLTFModel trafficObstacle;



int WIDTH = 1280;
int HEIGHT = 720;

GLuint tex;
char title[] = "All Star Racer";

// 3D Projection Options
GLdouble fovy = 45.0;
GLdouble aspectRatio = (GLdouble)WIDTH / (GLdouble)HEIGHT;
GLdouble zNear = 0.1;
GLdouble zFar = 10000;


struct Cone {
    float x;
    float y;
    float z;

    Cone(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
};

struct Nitro {
    float x;
    float y;
    float z;
    float animationPhase;

    Nitro(float _x, float _y, float _z, float _animationPhase) : x(_x), y(_y), z(_z), animationPhase(_animationPhase) {}
};

std::vector<Cone> cones = {
    Cone(-1.35632f, 1.3f, 65.1768f),
    Cone(108.473f, 1.3f, 139.522f),
    Cone(195.703f, 1.3f, 204.171f),
    Cone(278.372f, 1.3f, 257.79f),
    Cone(378.248f, 1.3f, 293.617f),
    Cone(420.81f, 1.3f, 160.304f),
    Cone(351.948f, 1.3f, 77.5647f),
    Cone(227.368f, 1.3f, -155.744f),
    Cone(230.393f, 1.3f, -241.257f),
    Cone(235.758f, 1.3f, -376.608f),
    Cone(61.4926f, 1.3f, -358.944f),
    Cone(-2.4635f, 1.3f, -263.153f),
    Cone(-7.77166f, 1.3f, -119.751f),
    Cone(-152.706f, 1.3f, -55.6917f),
    Cone(-264.162f, 1.3f, 45.1598f),
    Cone(-396.0f, 1.3f, 45.1598f) // Assuming the last value was cut off, added a placeholder value
};

std::vector<Nitro> nitros = {
    //Nitro(1,1,1),
    Nitro(32.1886, 1.5, 113.886, 0.0),
    Nitro(407.525, 1.5, 153.901, 0.0),
    Nitro(228.196, 1.5, -276.122, 0.0),
    Nitro(228.196, 1.5, -276.122, 0.0)
};

std::vector<Nitro> originalNitros = {
    //Nitro(1,1,1),
    Nitro(32.1886, 1.5, 113.886, 0.0),
    Nitro(407.525, 1.5, 153.901, 0.0),
    Nitro(228.196, 1.5, -276.122, 0.0),
    Nitro(228.196, 1.5, -276.122, 0.0)
};

std::vector<Vector> barriers = {
    Vector(-2.23767, 0, -106.795),
    Vector(5.23767, 0, -106.795),
    Vector(181.899, 0, 221.466),
    Vector(181.899, 0, 226.466),
    Vector(181.899, 0, 231.466)
};

Vector Eye(20, 5, 20);
Vector At(0, 0, 0);
Vector Up(0, 1, 0);

int cameraZoom = 0;

// Model Variables
Model_3DS model_house;
Model_3DS model_tree;
Model_3DS model_bugatti;
//Model_GLB model_moscow;


// Textures
GLTexture tex_ground;

enum CameraView { OUTSIDE, INSIDE_FRONT, THIRD_PERSON };
CameraView currentView = THIRD_PERSON;
float thirdPersonDistance = 3.0f;
Vector carPosition(0, 0, 0);
float carRotation = 0; // in degrees, 0 means facing negative z-axis
//Vector(0.2, 0.61, -0.1)
Vector cameraOffset(0.2, 1.11, -0.2);
float cameraMovementSpeed = 0.05f;
float cameraYaw = 0.0f;
float cameraPitch = 0.0f;
float cameraRotationSpeed = 2.0f;
float thirdPersonYaw = 0.0f;
float thirdPersonPitch = 0.0f;
Vector thirdPersonOffset(-0.1, 2.4, -8.7); // Initial offset behind and above the car
float thirdPersonMovementSpeed = 0.1f;

//Vector carVelocity(0, 0, 0);
//float carSpeed = 0.0f; // in km/h
//float maxSpeed = 300.0f; // km/h

//float accelerationTime = 0.5f; // seconds
//float accelerationRate = maxSpeed / accelerationTime;
//bool isAccelerating = false;
float wheelRotationX = 0.0f;
float wheelRotationY = 0.0f;

float wheelRotationSpeed = 180.0f; // Degrees per second
float steeringAngle = 0.0f;
float maxSteeringAngle = 52.50f; // Maximum steering angle in degrees
float steeringSpeed = 90.0f; // Degrees per second
float deceleration = 50.0f; // Units per second^2

float carSpeed = 0.0f;
float maxSpeed = 70.0f; // Maximum speed in units per second
float acceleration = 9.0f; // Acceleration in units per second^2
//float deceleration = 3.0f; // Deceleration in units per second^2
float turnSpeed = 90.0f; // Turn speed in degrees per second
bool isAccelerating = false;
bool isBraking = false;

float cameraDistance = 8.0f; // Distance behind the car
float cameraHeight = 3.0f; // Height above the car
float cameraLookAheadDistance = 10.0f; // How far ahead of the car to look

float sunsetProgress = 0.0f;
const float sunsetDuration = 90.0f; // 2 minutes in seconds
glm::vec3 morningSkyColor(0.678f, 0.847f, 0.902f); // Bright morning sky blue
glm::vec3 noonSkyColor(0.529f, 0.808f, 0.922f); // Noon sky blue
glm::vec3 sunsetSkyColor(0.698f, 0.502f, 0.569f); // Purplish pink sunset
glm::vec3 nightSkyColor(0.1f, 0.1f, 0.2f); // Dark blue night sky
glm::vec3 currentSkyColor = morningSkyColor;

// Sun variables
glm::vec3 sunPosition(100.0f, 10.0f, 0.0f); // Start slightly above horizon
glm::vec3 sunColor(1.0f, 1.0f, 0.9f); // Bright white-yellow for morning
float sunVisibility = 1.0f; // Full visibility at start

// Modified light variables
GLfloat lightPosition[] = { 100.0f, 10.0f, 0.0f, 1.0f };
GLfloat lightAmbient[] = { 0.3f, 0.3f, 0.3f, 1.0f };
GLfloat lightDiffuse[] = { 1.0f, 1.0f, 0.9f, 1.0f };
GLfloat lightSpecular[] = { 1.0f, 1.0f, 1.0f, 1.0f };

GLfloat headlight1_pos[] = { 30.0f, 1.5f, 100.0f, 1.0f }; // Right headlight position
GLfloat headlight2_pos[] = { -0.5f, 0.2f, 1.0f, 1.0f }; // Left headlight position
GLfloat headlight_dir[] = { 0.0f, 0.0f, -1.0f };        // Direction of headlights

// game over variables 
bool gameOver = false;
Vector lastCarPosition(0, 0, 0);
bool gameWon = false;
float gameTimer = 90.0f; // 90 seconds timer
float playerTime = 0.0f;
bool timerStarted = false;

bool isColliding = false;
bool isRespawning = false;
float respawnTimer = 0.0f;
float respawnDuration = 1.2f; // 3 seconds for the entire respawn process
float blinkInterval = 0.2f; 
bool isCarVisible = true;
float collisionRecoil = 0.0f;
float recoilDuration = 1.5f; // 0.5 seconds of collision recoil
bool isNitroActive = false;
float nitroTimer = 0.0f;
float nitroDuration = 3.0f; // 3 seconds of nitro boost
float nitroSpeedMultiplier = 20;
float lastSpeed = 0.0f;

// Function to set up the headlights
void setupLighting() {
    glEnable(GL_LIGHTING);

    // Headlight 1 (Right)
    glEnable(GL_LIGHT2);
    //glLightfv(GL_LIGHT2, GL_POSITION, headlight1_pos);     // Position
    //glLightfv(GL_LIGHT2, GL_SPOT_DIRECTION, headlight_dir); // Direction
    glLightf(GL_LIGHT2, GL_SPOT_CUTOFF, 30.0f);            // Cone angle
    glLightf(GL_LIGHT2, GL_SPOT_EXPONENT, 10.0f);          // Intensity falloff
    GLfloat lightColor[] = { 1.0f, 1.0f, 0.8f, 1.0f };     // Warm white
    glLightfv(GL_LIGHT2, GL_DIFFUSE, lightColor);
    glLightfv(GL_LIGHT2, GL_SPECULAR, lightColor);

    // Headlight 2 (Left)
    glEnable(GL_LIGHT1);
    //glLightfv(GL_LIGHT1, GL_POSITION, headlight2_pos);     // Position
    //glLightfv(GL_LIGHT1, GL_SPOT_DIRECTION, headlight_dir); // Direction
    glLightf(GL_LIGHT1, GL_SPOT_CUTOFF, 20.0f);            // Cone angle
    glLightf(GL_LIGHT1, GL_SPOT_EXPONENT, 5.0f);          // Intensity falloff
    glLightfv(GL_LIGHT1, GL_DIFFUSE, lightColor);
    glLightfv(GL_LIGHT1, GL_SPECULAR, lightColor);
}

//=======================================================================
// Collision Functions
//=======================================================================
struct Vertex {
    float x; // X coordinate
    float y; // Y coordinate
    float z; // Z coordinate

    // Constructor for initialization
    Vertex(float x, float y, float z) : x(x), y(y), z(z) {}

};

std::vector<Vertex> trackVertices = {
    {0.0f, 0.0f, -0.1458f},
    {0.0f, 0.0f, -0.57015f},
    {0.0f, 0.0f, -1.13135f},
    {0.0f, 0.0f, -1.89395f},
    {0.0f, 0.0f, -2.8303f},
    {0.0f, 0.0f, -3.9742f},
    {0.0f, 0.0f, -5.1942f},
    {0.0f, 0.0f, -6.4342f},
    {0.0f, 0.0f, -7.6742f},
    {0.0f, 0.0f, -8.9342f},
    {0.0f, 0.0f, -9.9742f},
    {0.0f, 0.0f, -11.1142f},
    {0.0, 0.0f, -22.5767f},
    {0, 0, -34.2791f},
    {0, 0, -45.5523f},
    {0, 0, -56.8534f},
    {0, 0 , -67},
    {0, 0 , -78},
    {0, 0 , -89},
    {0, 0, -100},
    {0, 0, -111},
    {0.0f, 0.0f, -116.163f},
    {0.0f, 0.0f, -117.423f},
    {0.0f, 0.0f, -118.703f},
    {0.0f, 0.0f, -119.64f},
    {0.0f, 0.0f, -120.644f},
    {0.0f, 0.0f, -121.634f},
    {0.0f, 0.0f, -122.513f},
    {0.0f, 0.0f, -123.318f},
    {0.0f, 0.0f, -124.001f},
    {0.0f, 0.0f, -124.573f},
    {0.0f, 0.0f, -125.203f},
    {0.0f, 0.0f, -126.039f},
    {0.0f, 0.0f, -127.136f},
    {0.0f, 0.0f, -128.336f},
    {0.0f, 0.0f, -129.395f},
    {0.0f, 0.0f, -130.557f},
    {0.0f, 0.0f, -131.492f},
    {0.0f, 0.0f, -132.334f},
    {0.0f, 0.0f, -133.083f},
    {0.0f, 0.0f, -133.738f},
    {0.0f, 0.0f, -134.301f},
    {0.0f, 0.0f, -134.777f},
    {0.0f, 0.0f, -135.157f},
    {0.0f, 0.0f, -135.443f},
    {0.0f, 0.0f, -135.628f},
    {0.0f, 0.0f, -135.719f},
    {0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.016641f},
    {0.0f, 0.0f, 0.059787f},
    {0.0f, 0.0f, 0.117747f},
    {0.0f, 0.0f, 0.230463f},
    {0.0f, 0.0f, 0.333135f},
    {0.0f, 0.0f, 0.45792f},
    {0.0f, 0.0f, 0.602586f},
    {0.0f, 0.0f, 0.763218f},
    {0.0f, 0.0f, 1.00514f},
    {0.0f, 0.0f, 1.20965f},
    {0.0f, 0.0f, 1.43321f},
    {0.0f, 0.0f, 1.68151f},
    {0.0f, 0.0f, 1.92532f},
    {0.0f, 0.0f, 2.23132f},
    {0.0f, 0.0f, 2.51113f},
    {0.0f, 0.0f, 2.86639f},
    {0.0f, 0.0f, 3.21367f},
    {0.0f, 0.0f, 3.57261f},
    {0.0f, 0.0f, 3.93337f},
    {0.0f, 0.0f, 4.34766f},
    {0.0f, 0.0f, 4.72603f},
    {0.0f, 0.0f, 5.19898f},
    {0.0f, 0.0f, 5.62236f},
    {0.0f, 0.0f, 6.13716f},
    {0.0f, 0.0f, 6.60761f},
    {0.0f, 0.0f, 7.16471f},
    {0.0f, 0.0f, 7.67238f},
    {0.0f, 0.0f, 8.25935f},
    {0.0f, 0.0f, 8.79109f},
    {0.0f, 0.0f, 9.44517f},
    {0.0f, 0.0f, 9.99969f},
    {0.0f, 0.0f, 10.6823f},
    {0.0f, 0.0f, 11.2861f},
    {0.0f, 0.0f, 11.9807f},
    {0.0f, 0.0f, 12.6655f},
    {0.0f, 0.0f, 13.6711f},
    {0.0f, 0.0f, 14.4181f},
    {0.0f, 0.0f, 15.1516f},
    {0.0f, 0.0f, 15.989f},
    {0.0f, 0.0f, 16.7609f},
    {0.0f, 0.0f, 17.6051f},
    {0.0f, 0.0f, 18.3779f},
    {0.0f, 0.0f, 19.2991f},
    {0.0f, 0.0f, 20.0887f},
    {0.0f, 0.0f, 21.0513f},
    {0.0f, 0.0f, 21.8957f},
    {0.0f, 0.0f, 22.8796f},
    {0.0f, 0.0f, 23.7388f},
    {0.0f, 0.0f, 24.8055f},
    {0.0f, 0.0f, 25.6782f},
    {0.0f, 0.0f, 26.7871f},
    {0.0f, 0.0f, 27.7613f},
    {0.0f, 0.0f, 28.9138f},
    {0.0f, 0.0f, 30.0428f},
    {0.0f, 0.0f, 31.2652f},
    {0.0f, 0.0f, 32.3656f},
    {0.0f, 0.0f, 33.6339f},
    {0.0f, 0.0f, 34.9518f},
    {0.0f, 0.0f, 36.3467f},
    {0.0f, 0.0f, 37.6373f},
    {0.0f, 0.0f, 38.7905f},
    {0.0f, 0.0f, 39.9432f},
    {0.0f, 0.0f, 41.0657f},
    {0.0f, 0.0f, 42.0898f},
    {0.0f, 0.0f, 43.1042f},
    {0.0f, 0.0f, 44.0633f},
    {0.0f, 0.0f, 44.9492f},
    {0.0f, 0.0f, 45.7822f},
    {0.0f, 0.0f, 46.578f},
    {0.0f, 0.0f, 47.7336f},
    {0.0f, 0.0f, 48.6183f},
    {0.0f, 0.0f, 49.4333f},
    {0.0f, 0.0f, 50.4416f},
    {0.0f, 0.0f, 51.2036f},
    {0.0f, 0.0f, 52.0016f},
    {0.0f, 0.0f, 52.8551f},
    {0.0f, 0.0f, 53.692f},
    {0.0f, 0.0f, 54.548f},
    {0.0f, 0.0f, 55.423f},
    {0.0f, 0.0f, 56.6168f},
    {0.0f, 0.0f, 57.8441f},
    {0.0f, 0.0f, 58.7259f},
    {0.0f, 0.0f, 59.6244f},
    {0.0f, 0.0f, 60.6698f},
    {0.0f, 0.0f, 61.6481f},
    {0.0265053f, 0.0f, 62.7346f},
    {0.0777236f, 0.0f, 63.7958f},
    {0.153882f, 0.0f, 64.8519f},
    {0.260266f, 0.0f, 65.9484f},
    {0.393085f, 0.0f, 67.0377f},
    {0.519257f, 0.0f, 68.0724f},
    {0.671735f, 0.0f, 69.3229f},
    {0.858164f, 0.0f, 70.8519f},
    {1.00421f, 0.0f, 72.0496f},
    {1.15588f, 0.0f, 73.2935f},
    {1.31802f, 0.0f, 74.3939f},
    {1.55449f, 0.0f, 75.7196f},
    {1.81669f, 0.0f, 76.9783f},
    {2.10562f, 0.0f, 78.1937f},
    {2.44192f, 0.0f, 79.4453f},
    {2.81133f, 0.0f, 80.6772f},
    {3.24263f, 0.0f, 81.9703f},
    {3.69843f, 0.0f, 83.2127f},
    {4.2225f, 0.0f, 84.5132f},
    {4.76937f, 0.0f, 85.759f},
    {5.3909f, 0.0f, 87.0591f},
    {5.99383f, 0.0f, 88.3202f},
    {6.63214f, 0.0f, 89.6554f},
    {7.26508f, 0.0f, 90.9793f},
    {7.92036f, 0.0f, 92.35f},
    {8.77085f, 0.0f, 94.129f},
    {9.44565f, 0.0f, 95.5404f},
    {10.0957f, 0.0f, 96.7961f},
    {10.9372f, 0.0f, 98.2798f},
    {11.7696f, 0.0f, 99.6326f},
    {12.9987f, 0.0f, 101.427f},
    {13.9925f, 0.0f, 102.768f},
    {15.0489f, 0.0f, 104.086f},
    {16.0028f, 0.0f, 105.276f},
    {17.2224f, 0.0f, 106.797f},
    {18.3164f, 0.0f, 108.162f},
    {19.3989f, 0.0f, 109.512f},
    {20.4674f, 0.0f, 110.749f},
    {21.8421f, 0.0f, 112.203f},
    {23.1224f, 0.0f, 113.453f},
    {24.4971f, 0.0f, 114.688f},
    {25.9377f, 0.0f, 115.876f},
    {27.2686f, 0.0f, 116.975f},
    {28.8334f, 0.0f, 118.266f},
    {30.3197f, 0.0f, 119.492f},
    {32.2096f, 0.0f, 121.052f},
    {33.7147f, 0.0f, 122.19f},
    {35.4274f, 0.0f, 123.364f},
    {37.1361f, 0.0f, 124.426f},
    {38.9089f, 0.0f, 125.418f},
    {40.7431f, 0.0f, 126.336f},
    {42.5948f, 0.0f, 127.161f},
    {45.2303f, 0.0f, 128.14f},
    {47.204f, 0.0f, 128.77f},
    {49.9462f, 0.0f, 129.458f},
    {52.8075f, 0.0f, 129.98f},
    {54.8691f, 0.0f, 130.356f},
    {59.2065f, 0.0f, 131.147f},
    {66.7469f, 0.0f, 132.523f},
    {88.6154f, 0.0f, 136.512f},
    {91.8839f, 0.0f, 137.369f},
    {94.6663f, 0.0f, 138.098f},
    {97.3581f, 0.0f, 138.803f},
    {99.3296f, 0.0f, 139.32f},
    {100.859f, 0.0f, 139.72f},
    {102.731f, 0.0f, 140.211f},
    {104.027f, 0.0f, 140.55f},
    {105.892f, 0.0f, 141.039f},
    {109.373f, 0.0f, 141.951f},
    {110.93f, 0.0f, 142.359f},
    {112.302f, 0.0f, 142.719f},
    {113.37f, 0.0f, 142.999f},
    {114.374f, 0.0f, 143.262f},
    {115.5f, 0.0f, 143.557f},
    {116.337f, 0.0f, 143.776f},
    {117.312f, 0.0f, 144.032f},
    {118.266f, 0.0f, 144.282f},
    {119.346f, 0.0f, 144.564f},
    {120.319f, 0.0f, 144.819f},
    {121.289f, 0.0f, 145.074f},
    {122.321f, 0.0f, 145.344f},
    {123.578f, 0.0f, 145.673f},
    {124.722f, 0.0f, 145.973f},
    {126.066f, 0.0f, 146.371f},
    {127.142f, 0.0f, 146.719f},
    {128.608f, 0.0f, 147.251f},
    {130.062f, 0.0f, 147.837f},
    {131.5f, 0.0f, 148.477f},
    {132.967f, 0.0f, 149.196f},
    {134.46f, 0.0f, 149.998f},
    {135.591f, 0.0f, 150.65f},
    {137.101f, 0.0f, 151.602f},
    {138.222f, 0.0f, 152.357f},
    {139.71f, 0.0f, 153.452f},
    {140.879f, 0.0f, 154.374f},
    {142.17f, 0.0f, 155.475f},
    {143.354f, 0.0f, 156.56f},
    {144.534f, 0.0f, 157.725f},
    {145.421f, 0.0f, 158.653f},
    {146.288f, 0.0f, 159.615f},
    {146.937f, 0.0f, 160.368f},
    {147.523f, 0.0f, 161.077f},
    {148.084f, 0.0f, 161.783f},
    {148.792f, 0.0f, 162.727f},
    {149.506f, 0.0f, 163.734f},
    {150.187f, 0.0f, 164.754f},
    {150.878f, 0.0f, 165.858f},
    {151.394f, 0.0f, 166.723f},
    {152.055f, 0.0f, 167.915f},
    {152.674f, 0.0f, 169.116f},
    {153.438f, 0.0f, 170.758f},
    {154.153f, 0.0f, 172.295f},
    {154.845f, 0.0f, 173.781f},
    {155.285f, 0.0f, 174.727f},
    {155.971f, 0.0f, 176.202f},
    {156.528f, 0.0f, 177.399f},
    {157.174f, 0.0f, 178.788f},
    {157.7f, 0.0f, 179.843f},
    {158.33f, 0.0f, 181.02f},
    {159.202f, 0.0f, 182.506f},
    {160.135f, 0.0f, 183.962f},
    {160.898f, 0.0f, 185.079f},
    {161.966f, 0.0f, 186.513f},
    {163.153f, 0.0f, 187.975f},
    {164.404f, 0.0f, 189.388f},
    {165.4f, 0.0f, 190.443f},
    {166.775f, 0.0f, 191.778f},
    {168.257f, 0.0f, 193.094f},
    {169.429f, 0.0f, 194.066f},
    {170.977f, 0.0f, 195.24f},
    {172.259f, 0.0f, 196.142f},
    {173.974f, 0.0f, 197.235f},
    {175.216f, 0.0f, 198.028f},
    {176.74f, 0.0f, 198.999f},
    {178.555f, 0.0f, 200.157f},
    {180.001f, 0.0f, 201.079f},
    {181.96f, 0.0f, 202.328f},
    {183.384f, 0.0f, 203.237f},
    {185.364f, 0.0f, 204.5f},
    {186.826f, 0.0f, 205.432f},
    {188.826f, 0.0f, 206.707f},
    {191.355f, 0.0f, 208.32f},
    {193.386f, 0.0f, 209.616f},
    {194.867f, 0.0f, 210.481f},
    {196.615f, 0.0f, 211.403f},
    {198.455f, 0.0f, 212.372f},
    {200.575f, 0.0f, 213.489f},
    {202.31f, 0.0f, 214.403f},
    {204.063f, 0.0f, 215.326f},
    {205.795f, 0.0f, 216.239f},
    {207.583f, 0.0f, 217.181f},
    {209.427f, 0.0f, 218.152f},
    {211.25f, 0.0f, 219.113f},
    {213.091f, 0.0f, 220.083f},
    {214.989f, 0.0f, 221.083f},
    {216.865f, 0.0f, 222.071f},
    {218.678f, 0.0f, 223.026f},
    {220.589f, 0.0f, 224.033f},
    {222.517f, 0.0f, 225.049f},
    {225.175f, 0.0f, 226.449f},
    {227.145f, 0.0f, 227.487f},
    {229.09f, 0.0f, 228.512f},
    {231.052f, 0.0f, 229.545f},
    {233.03f, 0.0f, 230.588f},
    {235.026f, 0.0f, 231.639f},
    {236.994f, 0.0f, 232.676f},
    {239.111f, 0.0f, 233.791f},
    {241.203f, 0.0f, 234.893f},
    {243.266f, 0.0f, 235.98f},
    {245.346f, 0.0f, 237.076f},
    {247.49f, 0.0f, 238.205f},
    {249.65f, 0.0f, 239.344f},
    {251.782f, 0.0f, 240.467f},
    {253.977f, 0.0f, 241.623f},
    {256.238f, 0.0f, 242.814f},
    {258.469f, 0.0f, 243.99f},
    {260.67f, 0.0f, 245.149f},
    {263.666f, 0.0f, 246.728f},
    {265.955f, 0.0f, 247.934f},
    {268.163f, 0.0f, 249.097f},
    {270.487f, 0.0f, 250.321f},
    {272.657f, 0.0f, 251.627f},
    {274.944f, 0.0f, 253.206f},
    {277.016f, 0.0f, 254.823f},
    {278.965f, 0.0f, 256.346f},
    {281.204f, 0.0f, 258.094f},
    {284.11f, 0.0f, 260.364f},
    {286.997f, 0.0f, 262.619f},
    {289.107f, 0.0f, 264.267f},
    {291.327f, 0.0f, 266.001f},
    {293.512f, 0.0f, 267.507f},
    {297.481f, 0.0f, 270.243f},
    {300.625f, 0.0f, 272.41f},
    {302.921f, 0.0f, 273.993f},
    {305.336f, 0.0f, 275.658f},
    {307.664f, 0.0f, 277.263f},
    {310.111f, 0.0f, 278.949f},
    {312.522f, 0.0f, 280.611f},
    {314.788f, 0.0f, 282.173f},
    {317.114f, 0.0f, 283.776f},
    {319.396f, 0.0f, 285.35f},
    {321.621f, 0.0f, 286.699f},
    {324.085f, 0.0f, 287.99f},
    {326.519f, 0.0f, 289.085f},
    {328.961f, 0.0f, 290.016f},
    {331.152f, 0.0f, 290.851f},
    {333.641f, 0.0f, 291.8f},
    {336.638f, 0.0f, 292.942f},
    {339.59f, 0.0f, 294.068f},
    {342.494f, 0.0f, 295.174f},
    {344.388f, 0.0f, 295.801f},
    {346.663f, 0.0f, 296.422f},
    {348.73f, 0.0f, 296.882f},
    {351.393f, 0.0f, 297.307f},
    {353.329f, 0.0f, 297.53f},
    {355.172f, 0.0f, 297.743f},
    {357.081f, 0.0f, 297.963f},
    {358.857f, 0.0f, 298.167f},
    {360.618f, 0.0f, 298.37f},
    {362.288f, 0.0f, 298.563f},
    {363.98f, 0.0f, 298.693f},
    {366.169f, 0.0f, 298.753f},
    {367.84f, 0.0f, 298.736f},
    {369.528f, 0.0f, 298.655f},
    {371.828f, 0.0f, 298.425f},
    {373.546f, 0.0f, 298.185f},
    {375.271f, 0.0f, 297.875f},
    {377.002f, 0.0f, 297.492f},
    {379.336f, 0.0f, 296.84f},
    {381.163f, 0.0f, 296.33f},
    {383.557f, 0.0f, 295.662f},
    {385.351f, 0.0f, 295.161f},
    {387.722f, 0.0f, 294.499f},
    {389.398f, 0.0f, 294.032f},
    {391.454f, 0.0f, 293.458f},
    {393.122f, 0.0f, 292.992f},
    {395.229f, 0.0f, 292.289f},
    {397.121f, 0.0f, 291.561f},
    {398.956f, 0.0f, 290.758f},
    {400.808f, 0.0f, 289.842f},
    {402.595f, 0.0f, 288.855f},
    {404.426f, 0.0f, 287.726f},
    {406.143f, 0.0f, 286.552f},
    {407.783f, 0.0f, 285.316f},
    {409.447f, 0.0f, 283.931f},
    {411.479f, 0.0f, 282.011f},
    {413.438f, 0.0f, 279.905f},
    {414.848f, 0.0f, 278.232f},
    {416.332f, 0.0f, 276.471f},
    {417.704f, 0.0f, 274.843f},
    {419.627f, 0.0f, 272.56f},
    {421.154f, 0.0f, 270.748f},
    {422.694f, 0.0f, 268.92f},
    {423.974f, 0.0f, 267.246f},
    {425.474f, 0.0f, 265.019f},
    {428.258f, 0.0f, 259.235f},
    {429.181f, 0.0f, 257.019f},
    {429.974f, 0.0f, 254.789f},
    {430.664f, 0.0f, 252.451f},
    {431.228f, 0.0f, 250.059f},
    {431.662f, 0.0f, 247.621f},
    {431.996f, 0.0f, 244.431f},
    {432.116f, 0.0f, 241.914f},
    {432.089f, 0.0f, 239.32f},
    {432.063f, 0.0f, 236.93f},
    {432.035f, 0.0f, 234.185f},
    {432.007f, 0.0f, 231.532f},
    {431.979f, 0.0f, 228.916f},
    {431.951f, 0.0f, 226.224f},
    {431.913f, 0.0f, 222.696f},
    {431.732f, 0.0f, 220.082f},
    {431.36f, 0.0f, 217.29f},
    {430.807f, 0.0f, 214.508f},
    {429.808f, 0.0f, 211.008f},
    {429.017f, 0.0f, 208.235f},
    {428.003f, 0.0f, 204.678f},
    {427.199f, 0.0f, 201.859f},
    {426.406f, 0.0f, 199.08f},
    {425.351f, 0.0f, 195.38f},
    {424.528f, 0.0f, 192.496f},
    {423.717f, 0.0f, 189.653f},
    {422.918f, 0.0f, 186.853f},
    {422.114f, 0.0f, 184.033f},
    {421.003f, 0.0f, 180.138f},
    {420.168f, 0.0f, 177.213f},
    {419.365f, 0.0f, 174.394f},
    {418.266f, 0.0f, 170.542f},
    {417.261f, 0.0f, 167.743f},
    {416.002f, 0.0f, 164.882f},
    {414.499f, 0.0f, 162.043f},
    {412.414f, 0.0f, 158.813f},
    {410.832f, 0.0f, 156.362f},
    {408.832f, 0.0f, 153.262f},
    {407.117f, 0.0f, 150.605f},
    {405.393f, 0.0f, 147.933f},
    {403.62f, 0.0f, 145.185f},
    {401.836f, 0.0f, 142.421f},
    {400.09f, 0.0f, 139.715f},
    {397.774f, 0.0f, 136.128f},
    {396.028f, 0.0f, 133.422f},
    {394.244f, 0.0f, 130.658f},
    {392.498f, 0.0f, 127.952f},
    {390.676f, 0.0f, 125.129f},
    {388.323f, 0.0f, 121.483f},
    {386.539f, 0.0f, 118.718f},
    {384.793f, 0.0f, 116.013f},
    {383.009f, 0.0f, 113.248f},
    {381.225f, 0.0f, 110.484f},
    {378.872f, 0.0f, 106.838f},
    {375.911f, 0.0f, 102.25f},
    {374.127f, 0.0f, 99.4855f},
    {372.267f, 0.0f, 96.6036f},
    {370.521f, 0.0f, 93.8981f},
    {368.775f, 0.0f, 91.1925f},
    {367.067f, 0.0f, 88.5458f},
    {365.321f, 0.0f, 85.8403f},
    {363.727f, 0.0f, 83.37f},
    {361.753f, 0.0f, 80.3116f},
    {359.969f, 0.0f, 77.5473f},
    {358.261f, 0.0f, 74.9006f},
    {355.908f, 0.0f, 71.254f},
    {354.086f, 0.0f, 68.4308f},
    {352.34f, 0.0f, 65.7253f},
    {350.449f, 0.0f, 63.2059f},
    {347.76f, 0.0f, 59.6226f},
    {345.786f, 0.0f, 56.9912f},
    {343.769f, 0.0f, 54.3038f},
    {341.164f, 0.0f, 50.8325f},
    {339.189f, 0.0f, 48.2011f},
    {336.584f, 0.0f, 44.7298f},
    {334.567f, 0.0f, 42.0424f},
    {332.635f, 0.0f, 39.4669f},
    {330.66f, 0.0f, 36.8355f},
    {328.727f, 0.0f, 34.26f},
    {326.752f, 0.0f, 31.6286f},
    {324.82f, 0.0f, 29.0531f},
    {322.887f, 0.0f, 26.4777f},
    {320.912f, 0.0f, 23.8462f},
    {318.307f, 0.0f, 20.375f},
    {316.375f, 0.0f, 17.7995f},
    {314.4f, 0.0f, 15.1681f},
    {312.383f, 0.0f, 12.4807f},
    {310.492f, 0.0f, 9.96121f},
    {308.56f, 0.0f, 7.38576f},
    {306.543f, 0.0f, 4.69833f},
    {304.869f, 0.0f, 2.11306f},
    {303.231f, 0.0f, -0.900817f},
    {301.46f, 0.0f, -4.16073f},
    {299.589f, 0.0f, -7.60517f},
    {298.018f, 0.0f, -10.496f},
    {296.514f, 0.0f, -13.2639f},
    {294.409f, 0.0f, -17.1389f},
    {292.796f, 0.0f, -19.6807f},
    {290.997f, 0.0f, -22.5181f},
    {289.235f, 0.0f, -25.2963f},
    {287.51f, 0.0f, -28.0155f},
    {285.785f, 0.0f, -30.7346f},
    {284.06f, 0.0f, -33.4538f},
    {282.336f, 0.0f, -36.1729f},
    {280.574f, 0.0f, -38.9512f},
    {278.811f, 0.0f, -41.7295f},
    {277.049f, 0.0f, -44.5077f},
    {275.324f, 0.0f, -47.2269f},
    {273.637f, 0.0f, -49.8869f},
    {272.05f, 0.0f, -52.3897f},
    {269.672f, 0.0f, -56.1392f},
    {268.008f, 0.0f, -58.7627f},
    {266.407f, 0.0f, -61.2857f},
    {264.835f, 0.0f, -63.764f},
    {263.226f, 0.0f, -66.3014f},
    {261.712f, 0.0f, -68.6885f},
    {259.754f, 0.0f, -71.7753f},
    {258.306f, 0.0f, -74.0584f},
    {256.856f, 0.0f, -76.3445f},
    {255.406f, 0.0f, -78.6307f},
    {254.044f, 0.0f, -80.7768f},
    {252.683f, 0.0f, -82.923f},
    {251.324f, 0.0f, -85.0663f},
    {250.022f, 0.0f, -87.1182f},
    {248.803f, 0.0f, -89.0401f},
    {247.621f, 0.0f, -91.1494f},
    {246.381f, 0.0f, -93.7388f},
    {245.532f, 0.0f, -95.7617f},
    {244.756f, 0.0f, -97.6108f},
    {243.915f, 0.0f, -99.6146f},
    {243.146f, 0.0f, -101.447f},
    {242.397f, 0.0f, -103.231f},
    {241.639f, 0.0f, -105.039f},
    {240.917f, 0.0f, -106.758f},
    {240.251f, 0.0f, -108.347f},
    {239.451f, 0.0f, -110.252f},
    {238.449f, 0.0f, -112.639f},
    {237.727f, 0.0f, -114.361f},
    {236.997f, 0.0f, -116.099f},
    {235.979f, 0.0f, -118.527f},
    {235.215f, 0.0f, -120.345f},
    {234.568f, 0.0f, -122.089f},
    {233.908f, 0.0f, -124.172f},
    {233.375f, 0.0f, -126.176f},
    {232.837f, 0.0f, -128.848f},
    {232.458f, 0.0f, -130.728f},
    {232.04f, 0.0f, -132.803f},
    {231.666f, 0.0f, -134.66f},
    {231.313f, 0.0f, -136.412f},
    {230.898f, 0.0f, -138.471f},
    {230.495f, 0.0f, -140.468f},
    {230.089f, 0.0f, -142.486f},
    {229.687f, 0.0f, -144.481f},
    {229.289f, 0.0f, -146.453f},
    {228.879f, 0.0f, -148.486f},
    {228.457f, 0.0f, -150.584f},
    {228.143f, 0.0f, -152.629f},
    {227.925f, 0.0f, -154.66f},
    {227.682f, 0.0f, -156.939f},
    {227.386f, 0.0f, -159.706f},
    {227.147f, 0.0f, -161.94f},
    {226.915f, 0.0f, -164.1f},
    {226.605f, 0.0f, -166.998f},
    {226.479f, 0.0f, -169.211f},
    {226.467f, 0.0f, -171.496f},
    {226.568f, 0.0f, -173.749f},
    {226.668f, 0.0f, -175.971f},
    {226.781f, 0.0f, -178.463f},
    {226.883f, 0.0f, -180.723f},
    {226.99f, 0.0f, -183.104f},
    {227.096f, 0.0f, -185.454f},
    {227.203f, 0.0f, -187.822f},
    {227.31f, 0.0f, -190.209f},
    {227.289f, 0.0f, -192.618f},
    {227.26f, 0.0f, -195.792f},
    {227.23f, 0.0f, -199.161f},
    {227.207f, 0.0f, -201.64f},
    {227.184f, 0.0f, -204.192f},
    {227.161f, 0.0f, -206.82f},
    {227.13f, 0.0f, -210.192f},
    {227.083f, 0.0f, -215.412f},
    {227.06f, 0.0f, -217.956f},
    {227.037f, 0.0f, -220.575f},
    {227.012f, 0.0f, -223.272f},
    {226.988f, 0.0f, -225.93f},
    {226.964f, 0.0f, -228.607f},
    {226.939f, 0.0f, -231.362f},
    {226.915f, 0.0f, -234.077f},
    {226.89f, 0.0f, -236.812f},
    {226.865f, 0.0f, -239.626f},
    {226.839f, 0.0f, -242.459f},
    {226.806f, 0.0f, -246.171f},
    {226.78f, 0.0f, -249.112f},
    {226.754f, 0.0f, -251.95f},
    {226.728f, 0.0f, -254.869f},
    {226.7f, 0.0f, -257.934f},
    {226.673f, 0.0f, -260.894f},
    {226.647f, 0.0f, -263.873f},
    {226.621f, 0.0f, -266.744f},
    {226.593f, 0.0f, -269.763f},
    {226.749f, 0.0f, -272.602f},
    {227.164f, 0.0f, -275.829f},
    {227.565f, 0.0f, -278.948f},
    {228.087f, 0.0f, -283.01f},
    {228.605f, 0.0f, -287.041f},
    {229.006f, 0.0f, -290.166f},
    {229.174f, 0.0f, -293.808f},
    {229.024f, 0.0f, -297.615f},
    {228.905f, 0.0f, -300.623f},
    {228.774f, 0.0f, -303.925f},
    {228.648f, 0.0f, -307.109f},
    {228.519f, 0.0f, -310.382f},
    {228.389f, 0.0f, -313.669f},
    {228.495f, 0.0f, -316.888f},
    {228.857f, 0.0f, -320.228f},
    {229.182f, 0.0f, -323.22f},
    {229.568f, 0.0f, -326.77f},
    {229.923f, 0.0f, -330.04f},
    {230.263f, 0.0f, -333.172f},
    {230.731f, 0.0f, -337.487f},
    {231.207f, 0.0f, -341.871f},
    {231.676f, 0.0f, -346.185f},
    {232.091f, 0.0f, -350.013f},
    {232.723f, 0.0f, -353.384f},
    {233.393f, 0.0f, -356.962f},
    {233.951f, 0.0f, -359.942f},
    {234.561f, 0.0f, -363.201f},
    {235.125f, 0.0f, -366.209f},
    {235.866f, 0.0f, -370.169f},
    {236.418f, 0.0f, -373.116f},
    {237.129f, 0.0f, -376.91f},
    {237.646f, 0.0f, -379.672f},
    {238.175f, 0.0f, -382.498f},
    {238.831f, 0.0f, -385.998f},
    {239.325f, 0.0f, -388.641f},
    {239.81f, 0.0f, -391.229f},
    {240.275f, 0.0f, -393.71f},
    {240.594f, 0.0f, -396.161f},
    {240.777f, 0.0f, -398.572f},
    {240.826f, 0.0f, -401.038f},
    {240.869f, 0.0f, -403.2f},
    {240.915f, 0.0f, -405.461f},
    {240.973f, 0.0f, -408.365f},
    {241.017f, 0.0f, -410.548f},
    {241.059f, 0.0f, -412.631f},
    {241.1f, 0.0f, -414.705f},
    {241.055f, 0.0f, -416.639f},
    {240.913f, 0.0f, -418.681f},
    {240.727f, 0.0f, -420.399f},
    {240.388f, 0.0f, -422.545f},
    {239.982f, 0.0f, -424.526f},
    {239.615f, 0.0f, -426.312f},
    {239.192f, 0.0f, -428.377f},
    {238.797f, 0.0f, -430.303f},
    {238.399f, 0.0f, -431.931f},
    {237.835f, 0.0f, -433.857f},
    {237.275f, 0.0f, -435.525f},
    {236.684f, 0.0f, -437.085f},
    {236.043f, 0.0f, -438.606f},
    {235.372f, 0.0f, -440.052f},
    {234.676f, 0.0f, -441.425f},
    {233.977f, 0.0f, -442.7f},
    {233.249f, 0.0f, -443.931f},
    {232.292f, 0.0f, -445.412f},
    {231.554f, 0.0f, -446.486f},
    {230.601f, 0.0f, -447.769f},
    {229.957f, 0.0f, -448.637f},
    {229.164f, 0.0f, -449.706f},
    {228.451f, 0.0f, -450.667f},
    {227.694f, 0.0f, -451.686f},
    {227.0f, 0.0f, -452.573f},
    {226.091f, 0.0f, -453.662f},
    {225.227f, 0.0f, -454.637f},
    {224.341f, 0.0f, -455.581f},
    {223.499f, 0.0f, -456.431f},
    {222.604f, 0.0f, -457.334f},
    {221.508f, 0.0f, -458.44f},
    {220.223f, 0.0f, -459.737f},
    {219.219f, 0.0f, -460.75f},
    {218.223f, 0.0f, -461.756f},
    {216.834f, 0.0f, -463.158f},
    {215.806f, 0.0f, -464.195f},
    {214.78f, 0.0f, -465.168f},
    {213.633f, 0.0f, -466.181f},
    {212.088f, 0.0f, -467.429f},
    {210.941f, 0.0f, -468.355f},
    {209.647f, 0.0f, -469.4f},
    {208.391f, 0.0f, -470.415f},
    {207.146f, 0.0f, -471.42f},
    {205.915f, 0.0f, -472.415f},
    {204.634f, 0.0f, -473.376f},
    {203.18f, 0.0f, -474.378f},
    {201.638f, 0.0f, -475.351f},
    {200.203f, 0.0f, -476.257f},
    {198.303f, 0.0f, -477.456f},
    {196.799f, 0.0f, -478.405f},
    {194.817f, 0.0f, -479.655f},
    {193.166f, 0.0f, -480.601f},
    {191.635f, 0.0f, -481.401f},
    {190.019f, 0.0f, -482.245f},
    {188.349f, 0.0f, -483.118f},
    {186.153f, 0.0f, -484.265f},
    {184.442f, 0.0f, -485.159f},
    {182.752f, 0.0f, -486.042f},
    {181.047f, 0.0f, -486.843f},
    {178.604f, 0.0f, -487.819f},
    {176.702f, 0.0f, -488.481f},
    {174.154f, 0.0f, -489.201f},
    {171.536f, 0.0f, -489.774f},
    {169.435f, 0.0f, -490.129f},
    {166.71f, 0.0f, -490.42f},
    {164.628f, 0.0f, -490.543f},
    {161.926f, 0.0f, -490.704f},
    {158.77f, 0.0f, -490.891f},
    {156.664f, 0.0f, -491.016f},
    {154.492f, 0.0f, -491.145f},
    {152.201f, 0.0f, -491.163f},
    {149.309f, 0.0f, -490.997f},
    {146.353f, 0.0f, -490.629f},
    {143.348f, 0.0f, -490.043f},
    {140.405f, 0.0f, -489.256f},
    {138.202f, 0.0f, -488.543f},
    {135.883f, 0.0f, -487.793f},
    {132.855f, 0.0f, -486.813f},
    {130.59f, 0.0f, -486.08f},
    {128.307f, 0.0f, -485.341f},
    {126.169f, 0.0f, -484.649f},
    {123.157f, 0.0f, -483.674f},
    {120.287f, 0.0f, -482.745f},
    {118.154f, 0.0f, -482.055f},
    {116.276f, 0.0f, -481.352f},
    {114.24f, 0.0f, -480.473f},
    {112.263f, 0.0f, -479.619f},
    {110.082f, 0.0f, -478.677f},
    {107.799f, 0.0f, -477.69f},
    {105.604f, 0.0f, -476.742f},
    {103.884f, 0.0f, -475.91f},
    {101.874f, 0.0f, -474.806f},
    {100.268f, 0.0f, -473.834f},
    {98.2283f, 0.0f, -472.44f},
    {95.2231f, 0.0f, -469.975f},
    {93.5125f, 0.0f, -468.417f},
    {91.7456f, 0.0f, -466.614f},
    {90.2381f, 0.0f, -464.91f},
    {88.9154f, 0.0f, -463.264f},
    {87.9574f, 0.0f, -461.98f},
    {86.8809f, 0.0f, -460.4f},
    {85.9193f, 0.0f, -458.988f},
    {84.8646f, 0.0f, -457.44f},
    {83.751f, 0.0f, -455.805f},
    {82.4513f, 0.0f, -453.898f},
    {81.2352f, 0.0f, -452.112f},
    {80.5466f, 0.0f, -451.102f},
    {79.7664f, 0.0f, -449.956f},
    {78.7559f, 0.0f, -448.473f},
    {77.7095f, 0.0f, -446.937f},
    {76.905f, 0.0f, -445.756f},
    {76.0717f, 0.0f, -444.533f},
    {75.2708f, 0.0f, -443.358f},
    {74.4109f, 0.0f, -442.095f},
    {73.7642f, 0.0f, -441.089f},
    {73.0234f, 0.0f, -439.85f},
    {72.4016f, 0.0f, -438.739f},
    {71.8794f, 0.0f, -437.748f},
    {71.2462f, 0.0f, -436.446f},
    {70.7657f, 0.0f, -435.389f},
    {70.2446f, 0.0f, -434.242f},
    {69.5799f, 0.0f, -432.779f},
    {69.0746f, 0.0f, -431.575f},
    {68.5063f, 0.0f, -430.07f},
    {67.9596f, 0.0f, -428.434f},
    {67.571f, 0.0f, -427.142f},
    {67.2105f, 0.0f, -425.79f},
    {66.9062f, 0.0f, -424.496f},
    {66.6273f, 0.0f, -423.116f},
    {66.3282f, 0.0f, -421.216f},
    {66.1165f, 0.0f, -419.871f},
    {65.8774f, 0.0f, -418.353f},
    {65.5696f, 0.0f, -416.398f},
    {65.2563f, 0.0f, -414.408f},
    {64.9481f, 0.0f, -412.451f},
    {64.7645f, 0.0f, -410.958f},
    {64.545f, 0.0f, -409.174f},
    {64.3523f, 0.0f, -407.607f},
    {64.0925f, 0.0f, -405.496f},
    {63.89f, 0.0f, -403.85f},
    {63.6186f, 0.0f, -401.643f},
    {63.3475f, 0.0f, -399.44f},
    {63.1316f, 0.0f, -397.685f},
    {62.9802f, 0.0f, -395.789f},
    {62.8085f, 0.0f, -393.641f},
    {62.6742f, 0.0f, -391.96f},
    {62.4982f, 0.0f, -389.757f},
    {62.3765f, 0.0f, -388.234f},
    {62.219f, 0.0f, -386.262f},
    {62.0665f, 0.0f, -384.354f},
    {61.9173f, 0.0f, -382.486f},
    {61.8121f, 0.0f, -381.169f},
    {61.6789f, 0.0f, -379.503f},
    {61.5575f, 0.0f, -377.983f},
    {61.4416f, 0.0f, -376.534f},
    {61.3582f, 0.0f, -375.49f},
    {61.2808f, 0.0f, -374.521f},
    {61.1856f, 0.0f, -373.329f},
    {61.1152f, 0.0f, -372.448f},
    {61.032f, 0.0f, -371.407f},
    {60.9733f, 0.0f, -370.672f},
    {60.8971f, 0.0f, -369.719f},
    {60.8116f, 0.0f, -368.648f},
    {60.7429f, 0.0f, -367.789f},
    {60.6656f, 0.0f, -366.999f},
    {60.5329f, 0.0f, -365.916f},
    {60.3648f, 0.0f, -364.786f},
    {60.218f, 0.0f, -363.918f},
    {59.9815f, 0.0f, -362.719f},
    {59.7884f, 0.0f, -361.835f},
    {59.562f, 0.0f, -360.897f},
    {59.2182f, 0.0f, -359.636f},
    {58.9474f, 0.0f, -358.72f},
    {58.6758f, 0.0f, -357.865f},
    {58.3353f, 0.0f, -356.875f},
    {58.058f, 0.0f, -356.114f},
    {57.7615f, 0.0f, -355.345f},
    {57.5148f, 0.0f, -354.732f},
    {57.21f, 0.0f, -354.011f},
    {56.8488f, 0.0f, -353.203f},
    {56.5637f, 0.0f, -352.59f},
    {56.1467f, 0.0f, -351.741f},
    {55.6874f, 0.0f, -350.856f},
    {55.2075f, 0.0f, -349.98f},
    {54.6906f, 0.0f, -349.087f},
    {54.1351f, 0.0f, -348.177f},
    {53.5392f, 0.0f, -347.254f},
    {53.0551f, 0.0f, -346.535f},
    {52.5341f, 0.0f, -345.794f},
    {51.9854f, 0.0f, -345.047f},
    {51.2563f, 0.0f, -344.054f},
    {50.6918f, 0.0f, -343.286f},
    {49.9519f, 0.0f, -342.279f},
    {49.3863f, 0.0f, -341.509f},
    {48.7512f, 0.0f, -340.685f},
    {47.8915f, 0.0f, -339.638f},
    {46.9775f, 0.0f, -338.593f},
    {46.2913f, 0.0f, -337.808f},
    {45.323f, 0.0f, -336.701f},
    {44.5921f, 0.0f, -335.865f},
    {43.6174f, 0.0f, -334.75f},
    {42.8576f, 0.0f, -333.881f},
    {41.8107f, 0.0f, -332.684f},
    {41.0215f, 0.0f, -331.781f},
    {40.1761f, 0.0f, -330.867f},
    {39.1127f, 0.0f, -329.793f},
    {38.167f, 0.0f, -328.838f},
    {36.946f, 0.0f, -327.605f},
    {35.7212f, 0.0f, -326.367f},
    {34.452f, 0.0f, -325.085f},
    {33.1585f, 0.0f, -323.779f},
    {31.8189f, 0.0f, -322.425f},
    {30.4763f, 0.0f, -321.069f},
    {29.1541f, 0.0f, -319.734f},
    {27.9519f, 0.0f, -318.418f},
    {26.8465f, 0.0f, -317.111f},
    {25.8534f, 0.0f, -315.936f},
    {24.869f, 0.0f, -314.772f},
    {23.4979f, 0.0f, -313.151f},
    {21.765f, 0.0f, -311.101f},
    {20.0198f, 0.0f, -309.038f},
    {18.5688f, 0.0f, -307.322f},
    {17.4971f, 0.0f, -306.054f},
    {16.4131f, 0.0f, -304.773f},
    {15.3168f, 0.0f, -303.476f},
    {14.0128f, 0.0f, -301.934f},
    {12.8458f, 0.0f, -300.554f},
    {11.741f, 0.0f, -299.247f},
    {10.7379f, 0.0f, -298.061f},
    {9.42817f, 0.0f, -296.512f},
    {8.2201f, 0.0f, -295.084f},
    {7.32495f, 0.0f, -294.025f},
    {6.5125f, 0.0f, -293.005f},
    {5.70094f, 0.0f, -291.921f},
    {5.01184f, 0.0f, -290.946f},
    {4.19288f, 0.0f, -289.702f},
    {3.62603f, 0.0f, -288.794f},
    {2.93813f, 0.0f, -287.613f},
    {2.34002f, 0.0f, -286.516f},
    {1.91308f, 0.0f, -285.693f},
    {1.54293f, 0.0f, -284.946f},
    {1.03233f, 0.0f, -283.84f},
    {0.672644f, 0.0f, -283.017f},
    {0.344515f, 0.0f, -282.226f},
    {0.0595654f, 0.0f, -281.504f},
    {-0.199f, 0.0f, -280.815f},
    {-0.525761f, 0.0f, -279.882f},
    {-0.91987f, 0.0f, -278.643f},
    {-1.29076f, 0.0f, -277.341f},
    {-1.57146f, 0.0f, -276.251f},
    {-1.82766f, 0.0f, -275.14f},
    {-2.11415f, 0.0f, -273.675f},
    {-2.36317f, 0.0f, -272.108f},
    {-2.48656f, 0.0f, -271.214f},
    {-2.60544f, 0.0f, -270.174f},
    {-2.70692f, 0.0f, -269.021f},
    {-2.78896f, 0.0f, -267.479f},
    {-2.82042f, 0.0f, -266.263f},
    {-2.8033f, 0.0f, -264.64f},
    {-2.78623f, 0.0f, -263.023f},
    {-2.77161f, 0.0f, -261.638f},
    {-2.75907f, 0.0f, -260.451f},
    {-2.74334f, 0.0f, -258.961f},
    {-2.72477f, 0.0f, -257.201f},
    {-2.70861f, 0.0f, -255.67f},
    {-2.69645f, 0.0f, -254.519f},
    {-2.67973f, 0.0f, -252.935f},
    {-2.66479f, 0.0f, -251.519f},
    {-2.6435f, 0.0f, -249.503f},
    {-2.62392f, 0.0f, -247.648f},
    {-2.60744f, 0.0f, -246.087f},
    {-2.59358f, 0.0f, -244.774f},
    {-2.57693f, 0.0f, -243.197f},
    {-2.56102f, 0.0f, -241.69f},
    {-2.54254f, 0.0f, -239.939f},
    {-2.52682f, 0.0f, -238.45f},
    {-2.50641f, 0.0f, -236.517f},
    {-2.49021f, 0.0f, -234.982f},
    {-2.46919f, 0.0f, -232.991f},
    {-2.45107f, 0.0f, -231.275f},
    {-2.43051f, 0.0f, -229.327f},
    {-2.40816f, 0.0f, -227.21f},
    {-2.37938f, 0.0f, -224.484f},
    {-2.36008f, 0.0f, -222.655f},
    {-2.34285f, 0.0f, -221.023f},
    {-2.32347f, 0.0f, -219.187f},
    {-2.30425f, 0.0f, -217.367f},
    {-2.2848f, 0.0f, -215.524f},
    {-2.2639f, 0.0f, -213.545f},
    {-2.24645f, 0.0f, -211.891f},
    {-2.21924f, 0.0f, -209.314f},
    {-2.20122f, 0.0f, -207.607f},
    {-2.17742f, 0.0f, -205.352f},
    {-2.15995f, 0.0f, -203.698f},
    {-2.13827f, 0.0f, -201.644f},
    {-2.12215f, 0.0f, -200.117f},
    {-2.10157f, 0.0f, -198.168f},
    {-2.07887f, 0.0f, -196.017f},
    {-2.05062f, 0.0f, -193.341f},
    {-2.03302f, 0.0f, -191.674f},
    {-2.01029f, 0.0f, -189.521f},
    {-1.99337f, 0.0f, -187.918f},
    {-1.97271f, 0.0f, -185.961f},
    {-1.95176f, 0.0f, -183.976f},
    {-1.93247f, 0.0f, -182.149f},
    {-1.9142f, 0.0f, -180.419f},
    {-1.89695f, 0.0f, -178.784f},
    {-1.88046f, 0.0f, -177.223f},
    {-1.86525f, 0.0f, -175.782f},
    {-1.85106f, 0.0f, -174.437f},
    {-1.83807f, 0.0f, -173.207f},
    {-1.86254f, 0.0f, -171.902f},
    {-1.95015f, 0.0f, -170.302f},
    {-2.00768f, 0.0f, -169.251f},
    {-2.07981f, 0.0f, -167.934f},
    {-2.12394f, 0.0f, -167.128f},
    {-2.16912f, 0.0f, -166.303f},
    {-2.23085f, 0.0f, -165.175f},
    {-2.3117f, 0.0f, -163.699f},
    {-2.39546f, 0.0f, -162.169f},
    {-2.44767f, 0.0f, -161.215f},
    {-2.51844f, 0.0f, -159.923f},
    {-2.59111f, 0.0f, -158.596f},
    {-2.67316f, 0.0f, -157.549f},
    {-2.81865f, 0.0f, -156.213f},
    {-3.02193f, 0.0f, -154.78f},
    {-3.20658f, 0.0f, -153.478f},
    {-3.38146f, 0.0f, -152.245f},
    {-3.58428f, 0.0f, -150.815f},
    {-3.80228f, 0.0f, -149.278f},
    {-3.96306f, 0.0f, -148.145f},
    {-4.18571f, 0.0f, -146.575f},
    {-4.41687f, 0.0f, -144.945f},
    {-4.63592f, 0.0f, -143.669f},
    {-4.97531f, 0.0f, -142.053f},
    {-5.27446f, 0.0f, -140.629f},
    {-5.60837f, 0.0f, -139.039f},
    {-5.87574f, 0.0f, -137.766f},
    {-6.15296f, 0.0f, -136.446f},
    {-6.53371f, 0.0f, -134.633f},
    {-6.82012f, 0.0f, -133.269f},
    {-7.15411f, 0.0f, -131.897f},
    {-7.68374f, 0.0f, -130.065f},
    {-8.21155f, 0.0f, -128.469f},
    {-8.73705f, 0.0f, -126.879f},
    {-9.23901f, 0.0f, -125.36f},
    {-9.88501f, 0.0f, -123.406f},
    {-10.381f, 0.0f, -121.905f},
    {-11.0242f, 0.0f, -119.959f},
    {-11.6445f, 0.0f, -118.083f},
    {-12.2199f, 0.0f, -116.537f},
    {-13.1408f, 0.0f, -114.408f},
    {-13.9308f, 0.0f, -112.582f},
    {-14.7022f, 0.0f, -110.799f},
    {-15.3792f, 0.0f, -109.234f},
    {-16.0789f, 0.0f, -107.616f},
    {-17.0156f, 0.0f, -105.451f},
    {-17.7457f, 0.0f, -103.925f},
    {-18.9902f, 0.0f, -101.669f},
    {-19.9528f, 0.0f, -100.084f},
    {-20.9686f, 0.0f, -98.5563f},
    {-22.0175f, 0.0f, -96.9791f},
    {-23.0568f, 0.0f, -95.4163f},
    {-24.326f, 0.0f, -93.5078f},
    {-25.3056f, 0.0f, -92.1484f},
    {-26.6314f, 0.0f, -90.4827f},
    {-27.9927f, 0.0f, -88.9263f},
    {-29.0043f, 0.0f, -87.8445f},
    {-30.3819f, 0.0f, -86.4936f},
    {-31.3094f, 0.0f, -85.584f},
    {-32.2801f, 0.0f, -84.632f},
    {-33.4474f, 0.0f, -83.4874f},
    {-34.3247f, 0.0f, -82.6271f},
    {-35.1791f, 0.0f, -81.7892f},
    {-36.336f, 0.0f, -80.6547f},
    {-37.2024f, 0.0f, -79.805f},
    {-38.3808f, 0.0f, -78.6494f},
    {-39.3662f, 0.0f, -77.7396f},
    {-40.688f, 0.0f, -76.6114f},
    {-41.748f, 0.0f, -75.7612f},
    {-43.1941f, 0.0f, -74.6941f},
    {-44.3486f, 0.0f, -73.8422f},
    {-45.8179f, 0.0f, -72.758f},
    {-46.9848f, 0.0f, -71.8969f},
    {-48.552f, 0.0f, -70.7405f},
    {-49.782f, 0.0f, -69.8328f},
    {-51.3455f, 0.0f, -68.6791f},
    {-52.5054f, 0.0f, -67.8786f},
    {-54.3477f, 0.0f, -66.7352f},
    {-56.1847f, 0.0f, -65.7119f},
    {-57.6858f, 0.0f, -64.8757f},
    {-59.4844f, 0.0f, -63.8737f},
    {-62.4657f, 0.0f, -62.4902f},
    {-64.5863f, 0.0f, -61.6353f},
    {-66.7823f, 0.0f, -60.8805f},
    {-68.9754f, 0.0f, -60.2501f},
    {-70.665f, 0.0f, -59.8352f},
    {-73.0029f, 0.0f, -59.3918f},
    {-75.3169f, 0.0f, -59.0774f},
    {-77.1097f, 0.0f, -58.8338f},
    {-79.5605f, 0.0f, -58.5008f},
    {-82.005f, 0.0f, -58.1687f},
    {-83.9081f, 0.0f, -57.9101f},
    {-86.4113f, 0.0f, -57.57f},
    {-88.3597f, 0.0f, -57.3053f},
    {-90.2951f, 0.0f, -57.1279f},
    {-92.2554f, 0.0f, -57.0349f},
    {-94.5393f, 0.0f, -56.9266f},
    {-96.8485f, 0.0f, -56.8171f},
    {-99.5843f, 0.0f, -56.6874f},
    {-102.264f, 0.0f, -56.5603f},
    {-105.068f, 0.0f, -56.4273f},
    {-107.953f, 0.0f, -56.2905f},
    {-110.779f, 0.0f, -56.1565f},
    {-113.687f, 0.0f, -56.0186f},
    {-115.863f, 0.0f, -55.9154f},
    {-118.058f, 0.0f, -55.8113f},
    {-120.321f, 0.0f, -55.704f},
    {-122.555f, 0.0f, -55.5981f},
    {-125.107f, 0.0f, -55.6233f},
    {-128.026f, 0.0f, -55.8443f},
    {-130.22f, 0.0f, -56.0105f},
    {-132.736f, 0.0f, -56.2009f},
    {-135.889f, 0.0f, -56.4397f},
    {-139.026f, 0.0f, -56.6772f},
    {-141.521f, 0.0f, -56.726f},
    {-144.726f, 0.0f, -56.7887f},
    {-147.108f, 0.0f, -56.8354f},
    {-149.616f, 0.0f, -56.8844f},
    {-152.09f, 0.0f, -56.9328f},
    {-155.349f, 0.0f, -56.9966f},
    {-158.807f, 0.0f, -57.0643f},
    {-161.406f, 0.0f, -57.1151f},
    {-163.97f, 0.0f, -57.1653f},
    {-167.459f, 0.0f, -57.2335f},
    {-170.122f, 0.0f, -57.1263f},
    {-173.079f, 0.0f, -56.8088f},
    {-175.532f, 0.0f, -56.5453f},
    {-178.683f, 0.0f, -56.207f},
    {-181.904f, 0.0f, -55.8612f},
    {-185.032f, 0.0f, -55.5252f},
    {-187.339f, 0.0f, -55.2775f},
    {-189.593f, 0.0f, -55.0355f},
    {-192.489f, 0.0f, -54.7245f},
    {-194.711f, 0.0f, -54.4859f},
    {-197.269f, 0.0f, -54.2112f},
    {-199.31f, 0.0f, -53.8957f},
    {-201.483f, 0.0f, -53.4482f},
    {-203.313f, 0.0f, -53.0713f},
    {-205.248f, 0.0f, -52.6729f},
    {-207.601f, 0.0f, -52.1882f},
    {-209.896f, 0.0f, -51.7157f},
    {-211.519f, 0.0f, -51.3814f},
    {-213.143f, 0.0f, -50.9829f},
    {-215.122f, 0.0f, -50.399f},
    {-216.982f, 0.0f, -49.7595f},
    {-218.534f, 0.0f, -49.2262f},
    {-220.042f, 0.0f, -48.7076f},
    {-221.652f, 0.0f, -48.1543f},
    {-222.901f, 0.0f, -47.6826f},
    {-224.355f, 0.0f, -47.0741f},
    {-225.864f, 0.0f, -46.3753f},
    {-227.4f, 0.0f, -45.5904f},
    {-228.909f, 0.0f, -44.7438f},
    {-230.242f, 0.0f, -43.9958f},
    {-231.598f, 0.0f, -43.2354f},
    {-232.972f, 0.0f, -42.3977f},
    {-234.737f, 0.0f, -41.2031f},
    {-236.28f, 0.0f, -40.058f},
    {-237.774f, 0.0f, -38.8459f},
    {-239.263f, 0.0f, -37.5243f},
    {-240.714f, 0.0f, -36.1123f},
    {-242.101f, 0.0f, -34.6352f},
    {-243.346f, 0.0f, -33.3085f},
    {-244.609f, 0.0f, -31.9626f},
    {-246.059f, 0.0f, -30.4179f},
    {-247.211f, 0.0f, -29.0888f},
    {-248.536f, 0.0f, -27.4038f},
    {-249.835f, 0.0f, -25.5675f},
    {-251.038f, 0.0f, -23.6705f},
    {-252.159f, 0.0f, -21.6853f},
    {-253.206f, 0.0f, -19.5792f},
    {-254.139f, 0.0f, -17.425f},
    {-254.944f, 0.0f, -15.5663f},
    {-255.838f, 0.0f, -13.502f},
    {-256.84f, 0.0f, -11.1877f},
    {-257.84f, 0.0f, -8.87835f},
    {-258.854f, 0.0f, -6.53726f},
    {-259.932f, 0.0f, -4.048f},
    {-260.974f, 0.0f, -1.64189f},
    {-261.742f, 0.0f, 0.398798f},
    {-262.47f, 0.0f, 2.70894f},
    {-263.119f, 0.0f, 5.30086f},
    {-263.616f, 0.0f, 7.9603f},
    {-263.956f, 0.0f, 10.7239f},
    {-264.117f, 0.0f, 12.9862f},
    {-264.155f, 0.0f, 15.4902f},
    {-264.036f, 0.0f, 18.1562f},
    {-263.944f, 0.0f, 20.2096f},
    {-263.841f, 0.0f, 22.5076f},
    {-263.734f, 0.0f, 24.9046f},
    {-263.634f, 0.0f, 27.1325f},
    {-263.515f, 0.0f, 29.7967f},
    {-263.401f, 0.0f, 32.3436f},
    {-263.312f, 0.0f, 34.3311f},
    {-263.231f, 0.0f, 36.1324f},
    {-263.154f, 0.0f, 37.8673f},
    {-263.082f, 0.0f, 39.4563f},
    {-263.066f, 0.0f, 40.969f},
    {-263.123f, 0.0f, 42.8185f},
    {-263.168f, 0.0f, 44.2782f},
    {-263.216f, 0.0f, 45.8493f},
    {-263.261f, 0.0f, 47.2932f},
    {-263.306f, 0.0f, 48.7648f},
    {-263.353f, 0.0f, 50.2921f},
    {-263.397f, 0.0f, 51.7203f},
    {-263.436f, 0.0f, 52.9905f},
    {-263.508f, 0.0f, 54.2249f},
    {-263.645f, 0.0f, 55.7124f},
    {-263.824f, 0.0f, 57.1417f},
    {-263.984f, 0.0f, 58.2104f},
    {-264.202f, 0.0f, 59.435f},
    {-264.433f, 0.0f, 60.5594f},
    {-264.668f, 0.0f, 61.5844f},
    {-264.902f, 0.0f, 62.5109f},
    {-265.125f, 0.0f, 63.3282f},
    {-265.334f, 0.0f, 64.045f},
    {-265.576f, 0.0f, 64.825f},
    {-265.836f, 0.0f, 65.6613f},
    {-266.154f, 0.0f, 66.6829f},
    {-266.453f, 0.0f, 67.645f},
    {-266.767f, 0.0f, 68.6558f},
    {-267.054f, 0.0f, 69.5786f},
    {-267.37f, 0.0f, 70.5205f},
    {-267.858f, 0.0f, 71.8382f},
    {-268.305f, 0.0f, 72.9483f},
    {-268.795f, 0.0f, 74.0775f},
    {-269.322f, 0.0f, 75.2048f},
    {-269.907f, 0.0f, 76.3655f},
    {-270.532f, 0.0f, 77.5194f},
    {-271.208f, 0.0f, 78.6828f},
    {-271.963f, 0.0f, 79.8918f},
    {-272.627f, 0.0f, 80.8928f},
    {-273.64f, 0.0f, 82.2997f},
    {-274.452f, 0.0f, 83.3579f},
    {-275.548f, 0.0f, 84.6784f},
    {-276.489f, 0.0f, 85.7423f},
    {-277.712f, 0.0f, 87.0178f},
    {-278.714f, 0.0f, 87.9992f},
    {-279.849f, 0.0f, 89.0367f},
    {-281.085f, 0.0f, 90.0873f},
    {-282.524f, 0.0f, 91.2114f},
    {-283.694f, 0.0f, 92.1256f},
    {-284.977f, 0.0f, 93.1276f},
    {-286.376f, 0.0f, 94.221f},
    {-287.802f, 0.0f, 95.3345f},
    {-289.302f, 0.0f, 96.5063f},
    {-290.854f, 0.0f, 97.719f},
    {-292.191f, 0.0f, 98.6861f},
    {-293.723f, 0.0f, 99.701f},
    {-295.498f, 0.0f, 100.761f},
    {-297.291f, 0.0f, 101.724f},
    {-298.967f, 0.0f, 102.536f},
    {-300.605f, 0.0f, 103.329f},
    {-302.562f, 0.0f, 104.165f},
    {-304.623f, 0.0f, 104.926f},
    {-306.718f, 0.0f, 105.585f},
    {-308.948f, 0.0f, 106.162f},
    {-311.202f, 0.0f, 106.622f},
    {-313.154f, 0.0f, 107.021f},
    {-315.245f, 0.0f, 107.448f},
    {-317.593f, 0.0f, 107.927f},
    {-319.975f, 0.0f, 108.414f},
    {-322.094f, 0.0f, 108.741f},
    {-324.293f, 0.0f, 108.97f},
    {-326.817f, 0.0f, 109.089f},
    {-329.166f, 0.0f, 109.199f},
    {-331.376f, 0.0f, 109.303f},
    {-333.953f, 0.0f, 109.425f},
    {-336.607f, 0.0f, 109.549f},
    {-339.204f, 0.0f, 109.672f},
    {-342.231f, 0.0f, 109.814f},
    {-344.644f, 0.0f, 109.928f},
    {-346.921f, 0.0f, 110.035f},
    {-349.032f, 0.0f, 110.134f},
    {-351.019f, 0.0f, 110.227f},
    {-353.006f, 0.0f, 110.321f},
    {-354.865f, 0.0f, 110.408f},
    {-356.629f, 0.0f, 110.491f},
    {-358.271f, 0.0f, 110.569f},
    {-359.892f, 0.0f, 110.645f},
    {-361.343f, 0.0f, 110.713f},
    {-362.681f, 0.0f, 110.776f},
    {-363.958f, 0.0f, 110.836f},
    {-365.551f, 0.0f, 110.911f},
    {-367.037f, 0.0f, 110.981f},
    {-368.256f, 0.0f, 111.072f},
    {-369.604f, 0.0f, 111.214f},
    {-370.704f, 0.0f, 111.33f},
    {-371.854f, 0.0f, 111.451f},
    {-373.008f, 0.0f, 111.572f},
    {-374.28f, 0.0f, 111.706f},
    {-375.339f, 0.0f, 111.843f},
    {-376.569f, 0.0f, 112.038f},
    {-377.344f, 0.0f, 112.174f},
    {-378.19f, 0.0f, 112.34f},
    {-379.067f, 0.0f, 112.531f},
    {-380.184f, 0.0f, 112.804f},
    {-381.097f, 0.0f, 113.048f},
    {-382.053f, 0.0f, 113.326f},
    {-383.034f, 0.0f, 113.636f},
    {-383.789f, 0.0f, 113.89f},
    {-384.534f, 0.0f, 114.155f},
    {-385.26f, 0.0f, 114.428f},
    {-385.881f, 0.0f, 114.672f},
    {-386.416f, 0.0f, 114.89f},
    {-386.847f, 0.0f, 115.071f},
    {-387.091f, 0.0f, 115.174f},
    {-387.206f, 0.0f, 115.222f},
    {-387.161f, 0.0f, 115.203f},
    {-386.938f, 0.0f, 115.11f},
    {-386.548f, 0.0f, 114.946f},
    {-385.962f, 0.0f, 114.699f},
    {-385.208f, 0.0f, 114.382f},
    {-384.456f, 0.0f, 114.05f},
    {-383.533f, 0.0f, 113.617f},
    {-382.426f, 0.0f, 113.058f},
    {-381.533f, 0.0f, 112.608f},
    {-380.497f, 0.0f, 112.086f},
    {-379.613f, 0.0f, 111.641f},
    {-378.764f, 0.0f, 111.212f},
    {-377.904f, 0.0f, 110.779f},
    {-377.117f, 0.0f, 110.382f},
    {-376.405f, 0.0f, 110.023f},
    {-375.791f, 0.0f, 109.713f},
    {-375.279f, 0.0f, 109.455f},
    {-374.799f, 0.0f, 109.214f},
    {-374.253f, 0.0f, 108.938f},
    {-373.762f, 0.0f, 108.691f},
    {-373.294f, 0.0f, 108.455f},
    {-372.863f, 0.0f, 108.238f},
    {-372.457f, 0.0f, 108.033f},
    {-372.081f, 0.0f, 107.843f},
    {-371.741f, 0.0f, 107.672f},
    {-371.426f, 0.0f, 107.513f},
    {-371.188f, 0.0f, 107.393f},
    {-370.952f, 0.0f, 107.274f},
    {-370.752f, 0.0f, 107.173f},
    {-370.571f, 0.0f, 107.082f},
    {-370.42f, 0.0f, 107.006f},
    {-370.277f, 0.0f, 106.934f},
    {-370.168f, 0.0f, 106.879f},
    {-370.081f, 0.0f, 106.835f},
    {-370.019f, 0.0f, 106.804f},
    {-369.976f, 0.0f, 106.782f},
    {-369.959f, 0.0f, 106.774f},
    {-369.975f, 0.0f, 106.782f},
    {-370.011f, 0.0f, 106.8f},
    {-370.073f, 0.0f, 106.831f},
    {-370.153f, 0.0f, 106.871f},
    {-370.259f, 0.0f, 106.925f},
    {-370.394f, 0.0f, 106.993f},
    {-370.589f, 0.0f, 107.091f},
    {-370.76f, 0.0f, 107.177f},
    {-370.999f, 0.0f, 107.298f},
    {-371.216f, 0.0f, 107.407f},
    {-371.491f, 0.0f, 107.546f},
    {-371.753f, 0.0f, 107.678f},
    {-372.072f, 0.0f, 107.839f},
    {-372.385f, 0.0f, 107.996f},
    {-372.853f, 0.0f, 108.232f},
    {-373.217f, 0.0f, 108.416f},
    {-373.896f, 0.0f, 108.758f},
    {-374.546f, 0.0f, 109.086f},
    {-375.066f, 0.0f, 109.348f},
    {-375.556f, 0.0f, 109.595f},
    {-376.119f, 0.0f, 109.879f},
    {-376.698f, 0.0f, 110.171f},
    {-377.404f, 0.0f, 110.527f},
    {-378.215f, 0.0f, 110.936f},
    {-378.994f, 0.0f, 111.328f},
    {-379.681f, 0.0f, 111.674f},
    {-380.431f, 0.0f, 112.053f},
    {-381.169f, 0.0f, 112.425f},
    {-382.06f, 0.0f, 112.874f},
    {-382.887f, 0.0f, 113.313f},
    {-383.717f, 0.0f, 113.777f},
    {-384.623f, 0.0f, 114.311f},
    {-385.529f, 0.0f, 114.875f},
    {-386.338f, 0.0f, 115.378f},
    {-387.168f, 0.0f, 115.895f},
    {-388.152f, 0.0f, 116.508f},
    {-388.967f, 0.0f, 117.04f},
    {-389.827f, 0.0f, 117.631f},
    {-390.655f, 0.0f, 118.228f},
    {-391.372f, 0.0f, 118.768f},
    {-392.026f, 0.0f, 119.28f},
    {-392.59f, 0.0f, 119.736f},
    {-393.132f, 0.0f, 120.189f},
    {-393.789f, 0.0f, 120.76f},
    {-394.403f, 0.0f, 121.313f},
    {-395.04f, 0.0f, 121.911f},
    {-395.701f, 0.0f, 122.558f},
    {-396.351f, 0.0f, 123.221f},
    {-396.999f, 0.0f, 123.91f},
    {-397.664f, 0.0f, 124.649f},
    {-398.344f, 0.0f, 125.443f},
    {-398.983f, 0.0f, 126.224f},
    {-399.578f, 0.0f, 126.984f},
    {-400.227f, 0.0f, 127.855f},
    {-400.773f, 0.0f, 128.623f},
    {-401.391f, 0.0f, 129.537f},
    {-401.938f, 0.0f, 130.388f},
    {-402.55f, 0.0f, 131.397f},
    {-403.088f, 0.0f, 132.336f},
    {-403.644f, 0.0f, 133.368f},
    {-404.213f, 0.0f, 134.5f},
    {-404.852f, 0.0f, 135.886f},
    {-405.304f, 0.0f, 136.938f},
    {-405.785f, 0.0f, 138.157f},
    {-406.248f, 0.0f, 139.439f},
    {-406.74f, 0.0f, 140.979f},
    {-407.131f, 0.0f, 142.351f},
    {-407.522f, 0.0f, 143.942f},
    {-407.819f, 0.0f, 145.346f},
    {-408.083f, 0.0f, 146.839f},
    {-408.28f, 0.0f, 148.205f},
    {-408.452f, 0.0f, 149.796f},
    {-408.606f, 0.0f, 151.218f},
    {-408.784f, 0.0f, 152.865f},
    {-408.947f, 0.0f, 154.366f},
    {-409.128f, 0.0f, 156.04f},
    {-409.296f, 0.0f, 157.592f},
    {-409.48f, 0.0f, 159.291f},
    {-409.649f, 0.0f, 160.862f},
    {-409.874f, 0.0f, 162.934f},
    {-409.993f, 0.0f, 164.661f},
    {-410.042f, 0.0f, 166.616f},
    {-409.994f, 0.0f, 168.738f},
    {-409.956f, 0.0f, 170.419f},
    {-409.912f, 0.0f, 172.33f},
    {-409.864f, 0.0f, 174.48f},
    {-409.814f, 0.0f, 176.7f},
    {-409.763f, 0.0f, 178.92f},
    {-409.652f, 0.0f, 180.693f},
    {-409.412f, 0.0f, 182.85f},
    {-409.027f, 0.0f, 185.171f},
    {-408.621f, 0.0f, 187.095f},
    {-408.226f, 0.0f, 188.967f},
    {-407.793f, 0.0f, 191.015f},
    {-407.298f, 0.0f, 193.361f},
    {-406.765f, 0.0f, 195.401f},
    {-406.086f, 0.0f, 197.552f},
    {-405.417f, 0.0f, 199.385f},
    {-404.586f, 0.0f, 201.361f},
    {-403.601f, 0.0f, 203.407f},
    {-402.582f, 0.0f, 205.286f},
    {-401.527f, 0.0f, 207.035f},
    {-400.415f, 0.0f, 208.703f},
    {-399.346f, 0.0f, 210.174f},
    {-398.277f, 0.0f, 211.532f},
    {-396.92f, 0.0f, 213.098f},
    {-395.602f, 0.0f, 214.494f},
    {-394.439f, 0.0f, 215.725f},
    {-392.873f, 0.0f, 217.383f},
    {-391.462f, 0.0f, 218.877f},
    {-390.213f, 0.0f, 220.099f},
    {-388.792f, 0.0f, 221.376f},
    {-387.306f, 0.0f, 222.599f},
    {-385.669f, 0.0f, 223.825f},
    {-384.164f, 0.0f, 224.859f},
    {-382.679f, 0.0f, 225.794f},
    {-381.269f, 0.0f, 226.613f},
    {-380.041f, 0.0f, 227.275f},
    {-378.884f, 0.0f, 227.856f},
    {-377.374f, 0.0f, 228.546f},
    {-376.232f, 0.0f, 229.029f},
    {-374.698f, 0.0f, 229.613f},
    {-373.084f, 0.0f, 230.157f},
    {-371.416f, 0.0f, 230.648f},
    {-369.697f, 0.0f, 231.081f},
    {-367.987f, 0.0f, 231.44f},
    {-366.201f, 0.0f, 231.741f},
    {-364.341f, 0.0f, 231.974f},
    {-362.782f, 0.0f, 232.114f},
    {-361.01f, 0.0f, 232.273f},
    {-359.432f, 0.0f, 232.414f},
    {-357.839f, 0.0f, 232.5f},
    {-356.169f, 0.0f, 232.527f},
    {-354.572f, 0.0f, 232.496f},
    {-353.098f, 0.0f, 232.418f},
    {-351.929f, 0.0f, 232.356f},
    {-350.748f, 0.0f, 232.294f},
    {-349.555f, 0.0f, 232.231f},
    {-348.607f, 0.0f, 232.16f},
    {-347.635f, 0.0f, 232.067f},
    {-346.707f, 0.0f, 231.958f},
    {-345.91f, 0.0f, 231.849f},
    {-344.993f, 0.0f, 231.705f},
    {-343.983f, 0.0f, 231.523f},
    {-342.945f, 0.0f, 231.309f},
    {-341.879f, 0.0f, 231.063f},
    {-340.769f, 0.0f, 230.776f},
    {-339.671f, 0.0f, 230.462f},
    {-338.533f, 0.0f, 230.103f},
    {-337.334f, 0.0f, 229.687f},
    {-336.133f, 0.0f, 229.232f},
    {-335.174f, 0.0f, 228.842f},
    {-334.023f, 0.0f, 228.336f},
    {-333.043f, 0.0f, 227.877f},
    {-331.891f, 0.0f, 227.296f},
    {-330.671f, 0.0f, 226.631f},
    {-329.465f, 0.0f, 225.925f},
    {-328.379f, 0.0f, 225.246f},
    {-326.821f, 0.0f, 224.178f},
    {-325.574f, 0.0f, 223.258f},
    {-324.335f, 0.0f, 222.274f},
    {-323.047f, 0.0f, 221.168f},
    {-321.815f, 0.0f, 220.027f},
    {-320.584f, 0.0f, 218.794f},
    {-319.417f, 0.0f, 217.531f},
    {-318.26f, 0.0f, 216.174f},
    {-317.066f, 0.0f, 214.774f},
    {-316.134f, 0.0f, 213.681f},
    {-315.164f, 0.0f, 212.544f},
    {-314.243f, 0.0f, 211.464f},
    {-313.399f, 0.0f, 210.474f},
    {-312.617f, 0.0f, 209.557f},
    {-312.013f, 0.0f, 208.877f},
    {-311.374f, 0.0f, 208.188f},
    {-310.708f, 0.0f, 207.5f},
    {-310.188f, 0.0f, 206.962f},
    {-309.606f, 0.0f, 206.36f},
    {-309.143f, 0.0f, 205.882f},
    {-308.728f, 0.0f, 205.454f},
    {-308.289f, 0.0f, 205.013f},
    {-307.77f, 0.0f, 204.508f},
    {-307.041f, 0.0f, 203.829f},
    {-306.436f, 0.0f, 203.287f},
    {-305.784f, 0.0f, 202.725f},
    {-304.533f, 0.0f, 201.722f},
    {-303.788f, 0.0f, 201.15f},
    {-303.003f, 0.0f, 200.575f},
    {-302.15f, 0.0f, 199.979f},
    {-301.387f, 0.0f, 199.469f},
    {-300.357f, 0.0f, 198.822f},
    {-299.502f, 0.0f, 198.31f},
    {-298.662f, 0.0f, 197.832f},
    {-297.791f, 0.0f, 197.361f},
    {-296.796f, 0.0f, 196.856f},
    {-295.838f, 0.0f, 196.398f},
    {-294.846f, 0.0f, 195.952f},
    {-293.86f, 0.0f, 195.537f},
    {-292.655f, 0.0f, 195.072f},
    {-291.558f, 0.0f, 194.68f},
    {-290.093f, 0.0f, 194.213f},
    {-288.917f, 0.0f, 193.874f},
    {-287.661f, 0.0f, 193.551f},
    {-286.369f, 0.0f, 193.26f},
    {-284.719f, 0.0f, 192.952f},
    {-283.427f, 0.0f, 192.75f},
    {-282.079f, 0.0f, 192.581f},
    {-280.653f, 0.0f, 192.402f},
    {-279.227f, 0.0f, 192.224f},
    {-277.556f, 0.0f, 192.015f},
    {-275.85f, 0.0f, 191.801f},
    {-274.109f, 0.0f, 191.583f},
    {-272.533f, 0.0f, 191.442f},
    {-270.929f, 0.0f, 191.299f},
    {-269.147f, 0.0f, 191.14f},
    {-267.302f, 0.0f, 190.976f},
    {-265.392f, 0.0f, 190.805f},
    {-263.448f, 0.0f, 190.631f},
    {-261.437f, 0.0f, 190.452f},
    {-259.781f, 0.0f, 190.366f},
    {-257.933f, 0.0f, 190.347f},
    {-256.124f, 0.0f, 190.328f},
    {-254.289f, 0.0f, 190.309f},
    {-252.112f, 0.0f, 190.287f},
    {-249.971f, 0.0f, 190.265f},
    {-247.688f, 0.0f, 190.241f},
    {-245.515f, 0.0f, 190.219f},
    {-243.645f, 0.0f, 190.199f},
    {-241.525f, 0.0f, 190.177f},
    {-239.224f, 0.0f, 190.153f},
    {-236.888f, 0.0f, 190.129f},
    {-234.441f, 0.0f, 190.104f},
    {-231.997f, 0.0f, 190.079f},
    {-230.016f, 0.0f, 190.058f},
    {-227.96f, 0.0f, 190.037f},
    {-225.78f, 0.0f, 190.014f},
    {-223.664f, 0.0f, 189.993f},
    {-221.709f, 0.0f, 189.972f},
    {-219.817f, 0.0f, 189.953f},
    {-218.049f, 0.0f, 189.934f},
    {-216.4f, 0.0f, 189.917f},
    {-214.768f, 0.0f, 189.901f},
    {-213.28f, 0.0f, 189.885f},
    {-211.886f, 0.0f, 189.871f},
    {-210.584f, 0.0f, 189.857f},
    {-209.376f, 0.0f, 189.845f},
    {-208.384f, 0.0f, 189.834f},
    {-207.093f, 0.0f, 189.821f},
    {-205.966f, 0.0f, 189.781f},
    {-204.748f, 0.0f, 189.704f},
    {-203.368f, 0.0f, 189.574f},
    {-201.913f, 0.0f, 189.388f},
    {-200.706f, 0.0f, 189.234f},
    {-199.403f, 0.0f, 189.067f},
    {-197.949f, 0.0f, 188.882f},
    {-196.362f, 0.0f, 188.679f},
    {-194.79f, 0.0f, 188.478f},
    {-193.21f, 0.0f, 188.276f},
    {-191.624f, 0.0f, 188.073f},
    {-189.923f, 0.0f, 187.856f},
    {-188.502f, 0.0f, 187.628f},
    {-186.925f, 0.0f, 187.316f},
    {-185.219f, 0.0f, 186.908f},
    {-183.442f, 0.0f, 186.404f},
    {-181.683f, 0.0f, 185.825f},
    {-180.184f, 0.0f, 185.332f},
    {-178.671f, 0.0f, 184.834f},
    {-177.007f, 0.0f, 184.287f},
    {-175.756f, 0.0f, 183.834f},
    {-174.429f, 0.0f, 183.304f},
    {-173.117f, 0.0f, 182.731f},
    {-171.927f, 0.0f, 182.169f},
    {-170.821f, 0.0f, 181.608f},
    {-169.784f, 0.0f, 181.047f},
    {-168.902f, 0.0f, 180.544f},
    {-168.097f, 0.0f, 180.062f},
    {-167.359f, 0.0f, 179.6f},
    {-166.73f, 0.0f, 179.192f},
    {-166.271f, 0.0f, 178.886f},
    {-165.799f, 0.0f, 178.562f},
    {-165.373f, 0.0f, 178.263f},
    {-164.878f, 0.0f, 177.905f},
    {-164.327f, 0.0f, 177.493f},
    {-163.756f, 0.0f, 177.051f},
    {-163.165f, 0.0f, 176.578f},
    {-162.546f, 0.0f, 176.064f},
    {-161.92f, 0.0f, 175.524f},
    {-161.477f, 0.0f, 175.131f},
    {-161.02f, 0.0f, 174.715f},
    {-160.607f, 0.0f, 174.329f},
    {-160.268f, 0.0f, 174.006f},
    {-160.001f, 0.0f, 173.747f},
    {-159.806f, 0.0f, 173.555f},
    {-159.677f, 0.0f, 173.427f},
    {-159.616f, 0.0f, 173.367f},
    {-159.546f, 0.0f, 173.297f},
    {-159.445f, 0.0f, 173.196f},
    {-159.318f, 0.0f, 173.066f},
    {-159.165f, 0.0f, 172.909f},
    {-158.988f, 0.0f, 172.727f},
    {-158.793f, 0.0f, 172.522f},
    {-158.568f, 0.0f, 172.283f},
    {-158.325f, 0.0f, 172.02f},
    {-158.066f, 0.0f, 171.735f},
    {-157.782f, 0.0f, 171.417f},
    {-157.541f, 0.0f, 171.142f},
    {-157.255f, 0.0f, 170.809f},
    {-156.982f, 0.0f, 170.486f},
    {-156.658f, 0.0f, 170.092f},
    {-156.373f, 0.0f, 169.739f},
    {-156.041f, 0.0f, 169.318f},
    {-155.769f, 0.0f, 168.964f},
    {-155.465f, 0.0f, 168.561f},
    {-155.306f, 0.0f, 168.349f},
    {-155.225f, 0.0f, 168.243f},
    {-155.255f, 0.0f, 168.283f},
    {-155.396f, 0.0f, 168.471f},
    {-155.645f, 0.0f, 168.811f},
    {-156.012f, 0.0f, 169.327f},
    {-156.473f, 0.0f, 170.0f},
    {-157.034f, 0.0f, 170.861f},
    {-157.674f, 0.0f, 171.906f},
    {-158.56f, 0.0f, 173.495f},
    {-159.076f, 0.0f, 174.42f},
    {-159.65f, 0.0f, 175.448f},
    {-160.078f, 0.0f, 176.182f},
    {-160.598f, 0.0f, 177.03f},
    {-161.084f, 0.0f, 177.788f},
    {-161.465f, 0.0f, 178.381f},
    {-161.874f, 0.0f, 179.019f},
    {-162.288f, 0.0f, 179.664f},
    {-162.703f, 0.0f, 180.311f},
    {-163.052f, 0.0f, 180.839f},
    {-163.394f, 0.0f, 181.34f},
    {-163.768f, 0.0f, 181.872f},
    {-164.131f, 0.0f, 182.373f},
    {-164.485f, 0.0f, 182.849f},
    {-164.83f, 0.0f, 183.3f},
    {-165.148f, 0.0f, 183.706f},
    {-165.454f, 0.0f, 184.088f},
    {-165.742f, 0.0f, 184.441f},
    {-166.008f, 0.0f, 184.759f},
    {-166.216f, 0.0f, 185.005f},
    {-166.439f, 0.0f, 185.265f},
    {-166.625f, 0.0f, 185.478f},
    {-166.803f, 0.0f, 185.68f},
    {-166.954f, 0.0f, 185.85f},
    {-167.113f, 0.0f, 186.027f},
    {-167.227f, 0.0f, 186.153f},
    {-167.344f, 0.0f, 186.281f},
    {-167.427f, 0.0f, 186.371f},
    {-167.492f, 0.0f, 186.441f},
    {-167.539f, 0.0f, 186.493f},
    {-167.571f, 0.0f, 186.526f},
    {-167.583f, 0.0f, 186.54f},
    {-167.569f, 0.0f, 186.524f},
    {-167.542f, 0.0f, 186.495f},
    {-167.496f, 0.0f, 186.446f},
    {-167.429f, 0.0f, 186.372f},
    {-167.324f, 0.0f, 186.258f},
    {-167.221f, 0.0f, 186.144f},
    {-167.056f, 0.0f, 185.96f},
    {-166.881f, 0.0f, 185.762f},
    {-166.655f, 0.0f, 185.503f},
    {-166.434f, 0.0f, 185.246f},
    {-166.208f, 0.0f, 184.978f},
    {-165.973f, 0.0f, 184.695f},
    {-165.725f, 0.0f, 184.391f},
    {-165.447f, 0.0f, 184.044f},
    {-165.174f, 0.0f, 183.695f},
    {-164.828f, 0.0f, 183.241f},
    {-164.548f, 0.0f, 182.867f},
    {-164.142f, 0.0f, 182.305f},
    {-163.846f, 0.0f, 181.884f},
    {-163.498f, 0.0f, 181.376f},
    {-163.185f, 0.0f, 180.906f},
    {-162.846f, 0.0f, 180.382f},
    {-162.521f, 0.0f, 179.863f},
    {-162.138f, 0.0f, 179.229f},
    {-161.797f, 0.0f, 178.644f},
    {-161.34f, 0.0f, 177.822f},
    {-160.994f, 0.0f, 177.174f},
    {-160.642f, 0.0f, 176.487f},
    {-160.298f, 0.0f, 175.784f},
    {-159.917f, 0.0f, 174.965f},
    {-159.573f, 0.0f, 174.187f},
    {-159.129f, 0.0f, 173.107f},
    {-158.77f, 0.0f, 172.233f},
    {-158.421f, 0.0f, 171.385f},
    {-157.992f, 0.0f, 170.341f},
    {-157.528f, 0.0f, 169.212f},
    {-157.072f, 0.0f, 168.102f},
    {-156.681f, 0.0f, 167.083f},
    {-156.321f, 0.0f, 166.069f},
    {-155.929f, 0.0f, 164.858f},
    {-155.564f, 0.0f, 163.604f},
    {-155.246f, 0.0f, 162.511f},
    {-154.888f, 0.0f, 161.286f},
    {-154.49f, 0.0f, 159.919f},
    {-154.166f, 0.0f, 158.671f},
    {-153.872f, 0.0f, 157.388f},
    {-153.587f, 0.0f, 155.921f},
    {-153.345f, 0.0f, 154.387f},
    {-153.138f, 0.0f, 153.076f},
    {-152.902f, 0.0f, 151.584f},
    {-152.59f, 0.0f, 149.605f},
    {-152.326f, 0.0f, 147.935f},
    {-152.152f, 0.0f, 146.56f},
    {-152.006f, 0.0f, 144.899f},
    {-151.919f, 0.0f, 143.027f},
    {-151.793f, 0.0f, 140.3f},
    {-151.706f, 0.0f, 138.397f},
    {-151.616f, 0.0f, 136.459f},
    {-151.607f, 0.0f, 134.642f},
    {-151.596f, 0.0f, 132.568f},
    {-151.585f, 0.0f, 130.522f},
    {-151.574f, 0.0f, 128.508f},
    {-151.563f, 0.0f, 126.426f},
    {-151.552f, 0.0f, 124.309f},
    {-151.54f, 0.0f, 122.158f},
    {-151.531f, 0.0f, 120.454f},
    {-151.521f, 0.0f, 118.569f},
    {-151.512f, 0.0f, 116.742f},
    {-151.502f, 0.0f, 114.95f},
    {-151.493f, 0.0f, 113.255f},
    {-151.484f, 0.0f, 111.655f},
    {-151.476f, 0.0f, 110.084f},
    {-151.47f, 0.0f, 108.878f},
    {-151.462f, 0.0f, 107.394f},
    {-151.453f, 0.0f, 105.801f},
    {-151.445f, 0.0f, 104.199f},
    {-151.436f, 0.0f, 102.565f},
    {-151.427f, 0.0f, 100.84f},
    {-151.418f, 0.0f, 99.109f},
    {-151.409f, 0.0f, 97.372f},
    {-151.399f, 0.0f, 95.5424f},
    {-151.389f, 0.0f, 93.7675f},
    {-151.38f, 0.0f, 91.9909f},
    {-151.37f, 0.0f, 90.0581f},
    {-151.361f, 0.0f, 88.4384f},
    {-151.349f, 0.0f, 86.2464f},
    {-151.339f, 0.0f, 84.3078f},
    {-151.328f, 0.0f, 82.2367f},
    {-151.317f, 0.0f, 80.0958f},
    {-151.306f, 0.0f, 78.0908f},
    {-151.297f, 0.0f, 76.2994f},
    {-151.285f, 0.0f, 74.1657f},
    {-151.275f, 0.0f, 72.2266f},
    {-151.265f, 0.0f, 70.3274f},
    {-151.256f, 0.0f, 68.5836f},
    {-151.247f, 0.0f, 66.961f},
    {-151.19f, 0.0f, 65.4797f},
    {-151.081f, 0.0f, 63.9779f},
    {-150.946f, 0.0f, 62.6632f},
    {-150.827f, 0.0f, 61.4951f},
    {-150.724f, 0.0f, 60.4967f},
    {-150.612f, 0.0f, 59.4018f},
    {-150.51f, 0.0f, 58.4026f},
    {-150.393f, 0.0f, 57.4571f},
    {-150.22f, 0.0f, 56.3098f},
    {-150.031f, 0.0f, 55.2364f},
    {-149.801f, 0.0f, 54.0983f},
    {-149.537f, 0.0f, 52.9509f},
    {-149.227f, 0.0f, 51.7583f},
    {-148.88f, 0.0f, 50.5603f},
    {-148.495f, 0.0f, 49.36f},
    {-148.064f, 0.0f, 48.1397f},
    {-147.55f, 0.0f, 46.8193f},
    {-147.112f, 0.0f, 45.7722f},
    {-146.576f, 0.0f, 44.5901f},
    {-146.101f, 0.0f, 43.6086f},
    {-145.453f, 0.0f, 42.3681f},
    {-144.737f, 0.0f, 41.1005f},
    {-143.955f, 0.0f, 39.7163f},
    {-143.345f, 0.0f, 38.6377f},
    {-142.609f, 0.0f, 37.3347f},
    {-141.857f, 0.0f, 36.0991f},
    {-141.066f, 0.0f, 34.8886f},
    {-140.264f, 0.0f, 33.7435f},
    {-139.335f, 0.0f, 32.5083f},
    {-138.458f, 0.0f, 31.4174f},
    {-137.641f, 0.0f, 30.4564f},
    {-136.86f, 0.0f, 29.5864f},
    {-136.123f, 0.0f, 28.8032f},
    {-135.437f, 0.0f, 28.1057f},
    {-134.518f, 0.0f, 27.2235f},
    {-133.648f, 0.0f, 26.4317f},
    {-132.246f, 0.0f, 25.2568f},
    {-130.965f, 0.0f, 24.2606f},
    {-129.896f, 0.0f, 23.4789f},
    {-128.794f, 0.0f, 22.7227f},
    {-127.601f, 0.0f, 21.9591f},
    {-126.395f, 0.0f, 21.2392f},
    {-125.244f, 0.0f, 20.5977f},
    {-124.094f, 0.0f, 19.9995f},
    {-122.948f, 0.0f, 19.4448f},
    {-121.622f, 0.0f, 18.8557f},
    {-120.421f, 0.0f, 18.3629f},
    {-119.058f, 0.0f, 17.8554f},
    {-117.652f, 0.0f, 17.3845f},
    {-115.937f, 0.0f, 16.8854f},
    {-114.605f, 0.0f, 16.5419f},
    {-113.03f, 0.0f, 16.136f},
    {-111.651f, 0.0f, 15.7804f},
    {-109.561f, 0.0f, 15.2419f},
    {-108.236f, 0.0f, 14.9003f},
    {-106.687f, 0.0f, 14.5593f},
    {-105.17f, 0.0f, 14.28f},
    {-103.74f, 0.0f, 14.0644f},
    {-102.399f, 0.0f, 13.9037f},
    {-101.15f, 0.0f, 13.7895f},
    {-99.9945f, 0.0f, 13.714f},
    {-98.9801f, 0.0f, 13.671f},
    {-97.9397f, 0.0f, 13.6512f},
    {-96.7086f, 0.0f, 13.6617f},
    {-95.4236f, 0.0f, 13.7098f},
    {-94.1278f, 0.0f, 13.7962f},
    {-92.735f, 0.0f, 13.9331f},
    {-91.3563f, 0.0f, 14.1122f},
    {-89.9499f, 0.0f, 14.3409f},
    {-88.5413f, 0.0f, 14.6167f},
    {-87.0622f, 0.0f, 14.959f},
    {-85.7783f, 0.0f, 15.2967f},
    {-84.1393f, 0.0f, 15.7959f},
    {-82.9484f, 0.0f, 16.1957f},
    {-81.5027f, 0.0f, 16.7374f},
    {-79.9489f, 0.0f, 17.3875f},
    {-78.4635f, 0.0f, 18.0742f},
    {-77.2552f, 0.0f, 18.6779f},
    {-76.0236f, 0.0f, 19.3423f},
    {-74.7909f, 0.0f, 20.0592f},
    {-73.6451f, 0.0f, 20.7727f},
    {-72.6026f, 0.0f, 21.4634f},
    {-71.673f, 0.0f, 22.1138f},
    {-70.8242f, 0.0f, 22.7382f},
    {-70.258f, 0.0f, 23.1687f},
    {-69.7477f, 0.0f, 23.5686f},
    {-69.3557f, 0.0f, 23.883f},
    {-69.1021f, 0.0f, 24.0895f},
    {-68.9985f, 0.0f, 24.1743f},
    {-69.0453f, 0.0f, 24.1361f},
    {-69.2419f, 0.0f, 23.9774f},
    {-69.5908f, 0.0f, 23.7014f},
    {-70.0853f, 0.0f, 23.3213f},
    {-70.7495f, 0.0f, 22.83f},
    {-71.394f, 0.0f, 22.3709f},
    {-72.2037f, 0.0f, 21.8204f},
    {-73.1114f, 0.0f, 21.2351f},
    {-74.0862f, 0.0f, 20.6064f},
    {-74.918f, 0.0f, 20.0699f},
    {-75.7176f, 0.0f, 19.5542f},
    {-76.4323f, 0.0f, 19.0735f},
    {-77.0678f, 0.0f, 18.6297f},
    {-77.7239f, 0.0f, 18.1536f},
    {-78.1939f, 0.0f, 17.8126f},
    {-78.6893f, 0.0f, 17.4531f},
    {-79.1322f, 0.0f, 17.1318f},
    {-79.4972f, 0.0f, 16.8669f},
    {-79.7423f, 0.0f, 16.6916f},
    {-79.9538f, 0.0f, 16.5422f},
    {-80.1248f, 0.0f, 16.4225f},
    {-80.299f, 0.0f, 16.3019f},
    {-80.4551f, 0.0f, 16.1947f},
    {-80.5839f, 0.0f, 16.107f},
    {-80.6853f, 0.0f, 16.0384f},
    {-80.7597f, 0.0f, 15.9882f},
    {-80.8055f, 0.0f, 15.9574f},
    {-80.8224f, 0.0f, 15.946f},
    {-80.818f, 0.0f, 15.949f},
    {-80.7847f, 0.0f, 15.9714f},
    {-80.7294f, 0.0f, 16.0087f},
    {-80.6311f, 0.0f, 16.0755f},
    {-80.5415f, 0.0f, 16.1367f},
    {-80.408f, 0.0f, 16.2286f},
    {-80.2646f, 0.0f, 16.3281f},
    {-80.0435f, 0.0f, 16.4836f},
    {-79.856f, 0.0f, 16.6168f},
    {-79.5696f, 0.0f, 16.8238f},
    {-79.3411f, 0.0f, 16.9912f},
    {-79.0789f, 0.0f, 17.1862f},
    {-78.8293f, 0.0f, 17.3746f},
    {-78.4943f, 0.0f, 17.6325f},
    {-78.1976f, 0.0f, 17.8649f},
    {-77.8782f, 0.0f, 18.1198f},
    {-77.5425f, 0.0f, 18.3932f},
    {-77.1053f, 0.0f, 18.7585f},
    {-76.7497f, 0.0f, 19.0621f},
    {-76.3344f, 0.0f, 19.4256f},
    {-75.9188f, 0.0f, 19.7987f},
    {-75.4642f, 0.0f, 20.2184f},
    {-75.0453f, 0.0f, 20.6153f},
    {-74.5543f, 0.0f, 21.095f},
    {-74.0943f, 0.0f, 21.5579f},
    {-73.4596f, 0.0f, 22.2233f},
    {-72.9952f, 0.0f, 22.7255f},
    {-72.5044f, 0.0f, 23.274f},
    {-71.959f, 0.0f, 23.9073f},
    {-71.368f, 0.0f, 24.6228f},
    {-70.8682f, 0.0f, 25.2509f},
    {-70.2291f, 0.0f, 26.0941f},
    {-69.7145f, 0.0f, 26.8015f},
    {-69.161f, 0.0f, 27.5983f},
    {-68.6672f, 0.0f, 28.3405f},
    {-68.1285f, 0.0f, 29.1913f},
    {-67.6327f, 0.0f, 30.0127f},
    {-67.0968f, 0.0f, 30.9515f},
    {-66.5706f, 0.0f, 31.929f},
    {-65.9776f, 0.0f, 33.1136f},
    {-65.5311f, 0.0f, 34.0607f},
    {-65.0898f, 0.0f, 35.0594f},
    {-64.6722f, 0.0f, 36.0709f},
    {-64.1975f, 0.0f, 37.3265f},
    {-63.8131f, 0.0f, 38.4301f},
    {-63.4251f, 0.0f, 39.657f},
    {-63.0716f, 0.0f, 40.8987f},
    {-62.6764f, 0.0f, 42.5135f},
    {-62.4084f, 0.0f, 43.7623f},
    {-62.1611f, 0.0f, 45.1181f},
    {-61.9556f, 0.0f, 46.482f},
    {-61.7574f, 0.0f, 48.3034f},
    {-61.6046f, 0.0f, 49.7075f},
    {-61.4372f, 0.0f, 51.2467f},
    {-61.2635f, 0.0f, 52.843f},
    {-61.0584f, 0.0f, 54.7272f},
    {-60.8713f, 0.0f, 56.4467f},
    {-60.6645f, 0.0f, 58.3474f},
    {-60.5471f, 0.0f, 59.9881f},
    {-60.4902f, 0.0f, 61.5975f},
    {-60.3921f, 0.0f, 64.3713f},
    {-60.2973f, 0.0f, 67.0528f},
    {-60.2238f, 0.0f, 69.1326f},
    {-60.1525f, 0.0f, 71.1485f},
    {-60.0765f, 0.0f, 73.3003f},
    {-60.0028f, 0.0f, 75.3847f},
    {-59.9279f, 0.0f, 77.5026f},
    {-59.8519f, 0.0f, 79.6539f},
    {-59.7733f, 0.0f, 81.8751f},
    {-59.6923f, 0.0f, 84.1678f},
    {-59.6113f, 0.0f, 86.4587f},
    {-59.5433f, 0.0f, 88.3811f},
    {-59.478f, 0.0f, 90.2307f},
    {-59.4074f, 0.0f, 92.2271f},
    {-59.3401f, 0.0f, 94.1305f},
    {-59.2761f, 0.0f, 95.941f},
    {-59.2163f, 0.0f, 97.6318f},
    {-59.1657f, 0.0f, 99.0633f},
    {-59.1049f, 0.0f, 100.784f},
    {-59.0408f, 0.0f, 102.596f},
    {-58.9767f, 0.0f, 104.411f},
    {-58.9264f, 0.0f, 105.833f},
    {-58.868f, 0.0f, 107.485f},
    {-58.8128f, 0.0f, 109.046f},
    {-58.7575f, 0.0f, 110.61f},
    {-58.7081f, 0.0f, 112.007f},
    {-58.662f, 0.0f, 113.311f},
    {-58.6179f, 0.0f, 114.559f},
    {-58.5618f, 0.0f, 115.537f},
    {-58.488f, 0.0f, 116.476f},
    {-58.4019f, 0.0f, 117.35f},
    {-58.2991f, 0.0f, 118.394f},
    {-58.1839f, 0.0f, 119.564f},
    {-58.069f, 0.0f, 120.73f},
    {-57.9454f, 0.0f, 121.745f},
    {-57.7769f, 0.0f, 122.884f},
    {-57.5563f, 0.0f, 124.129f},
    {-57.2825f, 0.0f, 125.443f},
    {-56.9539f, 0.0f, 126.804f},
    {-56.5922f, 0.0f, 128.124f},
    {-56.1717f, 0.0f, 129.485f},
    {-55.7906f, 0.0f, 130.719f},
    {-55.4087f, 0.0f, 131.956f},
    {-54.96f, 0.0f, 133.409f},
    {-54.5086f, 0.0f, 134.87f},
    {-54.0397f, 0.0f, 136.389f},
    {-53.543f, 0.0f, 137.822f},
    {-52.9212f, 0.0f, 139.416f},
    {-52.2589f, 0.0f, 140.942f},
    {-51.651f, 0.0f, 142.344f},
    {-50.8712f, 0.0f, 144.141f},
    {-49.968f, 0.0f, 146.223f},
    {-49.2381f, 0.0f, 147.745f},
    {-48.2808f, 0.0f, 149.528f},
    {-47.5115f, 0.0f, 150.962f},
    {-46.6854f, 0.0f, 152.501f},
    {-45.7391f, 0.0f, 154.264f},
    {-44.807f, 0.0f, 156.0f},
    {-43.8355f, 0.0f, 157.638f},
    {-42.6579f, 0.0f, 159.423f},
    {-41.5879f, 0.0f, 160.908f},
    {-40.4821f, 0.0f, 162.442f},
    {-39.2362f, 0.0f, 164.171f},
    {-37.9918f, 0.0f, 165.898f},
    {-36.9913f, 0.0f, 167.287f},
    {-35.8799f, 0.0f, 168.829f},
    {-34.7653f, 0.0f, 170.376f},
    {-33.7052f, 0.0f, 171.847f},
    {-32.7307f, 0.0f, 173.2f},
    {-31.8597f, 0.0f, 174.33f},
    {-30.8719f, 0.0f, 175.524f},
    {-29.8613f, 0.0f, 176.664f},
    {-28.9291f, 0.0f, 177.653f},
    {-28.035f, 0.0f, 178.549f},
    {-27.2511f, 0.0f, 179.297f},
    {-26.5495f, 0.0f, 179.967f},
    {-25.3624f, 0.0f, 181.1f},
    {-23.9422f, 0.0f, 182.456f},
    {-22.715f, 0.0f, 183.628f},
    {-21.547f, 0.0f, 184.667f},
    {-20.0597f, 0.0f, 185.99f},
    {-18.3555f, 0.0f, 187.506f},
    {-16.6501f, 0.0f, 189.024f},
    {-15.4253f, 0.0f, 190.038f},
    {-13.8282f, 0.0f, 191.243f},
    {-12.1344f, 0.0f, 192.403f},
    {-10.0211f, 0.0f, 193.85f},
    {-8.11186f, 0.0f, 195.158f},
    {-6.62188f, 0.0f, 196.178f},
    {-5.07841f, 0.0f, 197.235f},
    {-3.50545f, 0.0f, 198.312f},
    {-1.90303f, 0.0f, 199.41f},
    {-0.271133f, 0.0f, 200.527f},
    {1.33659f, 0.0f, 201.628f},
    {3.19389f, 0.0f, 202.771f},
    {5.32437f, 0.0f, 203.927f},
    {7.1944f, 0.0f, 204.941f},
    {9.0345f, 0.0f, 205.94f},
    {10.9663f, 0.0f, 206.988f},
    {12.9295f, 0.0f, 208.053f},
    {14.8599f, 0.0f, 209.101f},
    {16.787f, 0.0f, 210.146f},
    {18.8089f, 0.0f, 211.243f},
    {20.8275f, 0.0f, 212.339f},
    {23.1829f, 0.0f, 213.444f},
    {25.7511f, 0.0f, 214.46f},
    {28.6953f, 0.0f, 215.395f},
    {30.7199f, 0.0f, 216.038f},
    {33.5542f, 0.0f, 216.938f},
    {36.5948f, 0.0f, 217.903f},
    {39.1165f, 0.0f, 218.704f},
    {42.2495f, 0.0f, 219.699f},
    {45.4333f, 0.0f, 220.71f},
    {48.207f, 0.0f, 221.399f},
    {50.788f, 0.0f, 221.88f},
    {53.3549f, 0.0f, 222.359f},
    {56.3599f, 0.0f, 222.919f},
    {59.213f, 0.0f, 223.452f},
    {62.8267f, 0.0f, 224.126f},
    {65.2783f, 0.0f, 224.583f},
    {67.8036f, 0.0f, 224.906f},
    {70.5152f, 0.0f, 225.086f},
    {73.2276f, 0.0f, 225.101f},
    {75.159f, 0.0f, 225.028f},
    {76.8843f, 0.0f, 224.962f},
    {79.0836f, 0.0f, 224.879f},
    {81.2553f, 0.0f, 224.796f},
    {83.2789f, 0.0f, 224.719f},
    {84.6732f, 0.0f, 224.71f},
    {86.3149f, 0.0f, 224.699f},
    {87.9665f, 0.0f, 224.688f},
    {89.4486f, 0.0f, 224.678f},
    {90.5621f, 0.0f, 224.671f},
    {91.7885f, 0.0f, 224.663f},
    {93.0352f, 0.0f, 224.69f},
    {94.423f, 0.0f, 224.719f},
    {95.7771f, 0.0f, 224.748f},
    {97.2024f, 0.0f, 224.778f},
    {98.4042f, 0.0f, 224.804f},
    {100.277f, 0.0f, 224.844f},
    {101.843f, 0.0f, 224.877f},
    {103.177f, 0.0f, 224.906f},
    {104.192f, 0.0f, 224.928f},
    {105.228f, 0.0f, 224.95f},
    {106.263f, 0.0f, 224.972f},
    {106.991f, 0.0f, 224.987f},
    {107.863f, 0.0f, 225.006f},
    {108.827f, 0.0f, 225.027f},
    {109.496f, 0.0f, 225.041f},
    {110.07f, 0.0f, 225.053f},
    {110.442f, 0.0f, 225.061f},
    {110.849f, 0.0f, 225.07f},
    {111.097f, 0.0f, 225.075f},
    {111.296f, 0.0f, 225.079f},
    {111.462f, 0.0f, 225.083f},
    {111.532f, 0.0f, 225.084f},
    {111.545f, 0.0f, 225.085f},
    {111.545f, 0.0f, 225.085f},
    {123.386, 0, 225.572 }, 
    {133.439, 0, 224.257 }, 
    {143, 0, 225}, 
    { 153, 0, 225 },
    { 163, 0, 225 },
    { 173, 0, 225 },
    { 183, 0, 225 },
    {198.964, 0, 225.877}

};

bool isPointInTrack(const std::vector<Vertex>& trackVertices, const Vector& carPosition, float threshold = 12.0f) {
    // Loop through each vertex in the track
    for (const auto& vertex : trackVertices) {
        // Create a Vector for the track vertex
        Vector trackVertex(vertex.x, vertex.y, vertex.z);

        // Check the distance between the car and the vertex
        float distance = carPosition.distanceToNoY(trackVertex);

        // If the distance is smaller than the threshold, the car is close enough to this vertex
        if (distance <= threshold) {
            //std::cout << "Car is near vertex: ";
            //trackVertex.print();
            return true; // Car is close to this vertex
        }
    }

    return false; // Car is not close to any vertex
}

bool checkCollisionWithObstacles(const Vector& carPosition, float collisionThreshold = 2.0f) {
    for (const auto& cone : cones) {
        Vector conePosition(cone.x, cone.y, cone.z);
        if (carPosition.distanceToNoY(conePosition) <= collisionThreshold) {
            return true; // Collision detected
        }
    }
    for (const auto& barrier : barriers) {
        if (carPosition.distanceToNoY(barrier) <= 4) {
            return true; 
        }
    }
    return false; // No collision
}

void activateNitro() {
    if (!isNitroActive) {
        isNitroActive = true;
        lastSpeed = carSpeed;
        nitroTimer = 0.0f;
        carSpeed = carSpeed + nitroSpeedMultiplier;
    }
}

bool checkCollisionWithNitros(Vector& carPosition, std::vector<Nitro>& nitros, float collisionThreshold = 2.0f) {
    for (auto it = nitros.begin(); it != nitros.end(); ++it) {
        Vector nitroPosition(it->x, it->y, it->z);
        if (carPosition.distanceToNoY(nitroPosition) <= collisionThreshold) {
            nitros.erase(it); // Remove the Nitro from the array
            activateNitro(); // Activate nitro boost
            return true; // Collision detected
        }
    }
    return false; // No collision
}

void startRespawn() {
    isRespawning = true;
    respawnTimer = 0.0f;
}

void updateCollisionRecoil(float deltaTime) {
    float radians = carRotation * M_PI / 180.0f;
    while (!(collisionRecoil <= 0)) {
        carPosition.x -= sin(radians) * 4.0f * deltaTime;
        carPosition.z -= cos(radians) * 4.0f * deltaTime;
        collisionRecoil -= deltaTime / 2;
    }
    if (collisionRecoil <= 0) {
        collisionRecoil = 0.0f;
         isColliding = false;
         startRespawn();
    }
}

void applyCollisionRecoil(float deltaTime) {
    isColliding = true;
    carSpeed = 0.0f;
    collisionRecoil = recoilDuration;
    updateCollisionRecoil(deltaTime);
}

//=======================================================================
// Car Motion Functions
//=======================================================================
bool gravityEnabled = false;

bool hasPassedFinishLine() {
    float finishX = 111.845f;
    float finishZ = 225.249f;
    float threshold = 10.0f;

    return (abs(carPosition.x - finishX) < threshold &&
        abs(carPosition.z - finishZ) < threshold);
}

void updateCarPosition(float deltaTime) {
    if (isNitroActive) {
        nitroTimer += deltaTime;
        if (nitroTimer >= nitroDuration) {
            isNitroActive = false;
            nitroTimer = 0.0f;
            carSpeed = lastSpeed;
        }
    }

    if (isRespawning) {
        respawnTimer += deltaTime;

        // Blink the car
        isCarVisible = (static_cast<int>(respawnTimer / blinkInterval) % 2 == 0);

        // End respawn after 3 seconds
        if (respawnTimer >= respawnDuration) {
            isRespawning = false;
            respawnTimer = 0.0f;
            isCarVisible = true;
        }
        return;
    }

    float radians = carRotation * M_PI / 180.0;
    if (gravityEnabled) {
        carPosition.y -= 9.8065 * deltaTime;
        //std::cout << "Car position when gravity enabled: ";
        //carPosition.print();

        if (carPosition.y < -1.0f && !gameWon) {
            gameOver = true;
            lastCarPosition = carPosition;
        }
    }

    carPosition.x += sin(radians) * carSpeed * deltaTime;
    carPosition.z += cos(radians) * carSpeed * deltaTime;

    if (isPointInTrack(trackVertices, carPosition)) {
        std::cout << "Car Pos: ";
        carPosition.print();
    }
    else {
        gravityEnabled = true;
    }

    wheelRotationX += carSpeed * 360.0f * deltaTime;

    if (!gameWon && hasPassedFinishLine()) {
        gameWon = true;
        playerTime = 90.0f - gameTimer; // Calculate player's time
    }

    // Update game timer
    if (!gameWon && !gameOver && timerStarted) {
        gameTimer -= deltaTime;
        if (gameTimer <= 0) {
            gameOver = true;
            lastCarPosition = carPosition;
        }
    }
}

void handleCarControls(float deltaTime) {
    // Accelerate
    if (isAccelerating) {
        carSpeed += acceleration * deltaTime;
        if (carSpeed > maxSpeed && !isNitroActive) carSpeed = maxSpeed;
    }
    // Brake/Reverse
    else if (isBraking) {
        carSpeed -= deceleration * deltaTime;
        if (carSpeed < -20) carSpeed = -20; // Allow negative speed for reverse
    }
    // Coast (slow down gradually)
    else {
        if (carSpeed > 0) {
            carSpeed -= deceleration * 0.5f * deltaTime; // Adjust this factor for desired coasting behavior
            if (carSpeed < 0) carSpeed = 0;
        }
        else if (carSpeed < 0) {
            carSpeed += deceleration * 0.5f * deltaTime; // Adjust this factor for desired coasting behavior
            if (carSpeed > 0) carSpeed = 0;
        }
    }

    // Turn left
    if (wheelRotationY > 0) {
        carRotation += turnSpeed * deltaTime * (carSpeed / maxSpeed);
    }
    // Turn right
    else if (wheelRotationY < 0) {
        carRotation -= turnSpeed * deltaTime * (carSpeed / maxSpeed);
    }

    // Normalize rotation to 0-360 degrees
    while (carRotation >= 360.0f) carRotation -= 360.0f;
    while (carRotation < 0.0f) carRotation += 360.0f;
}

//=======================================================================
// Lighting Configuration Function
//=======================================================================
void InitLightSource() {
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
}

void updateNitroAnimation() {
    for (auto& nitro : nitros) {
        nitro.animationPhase += 0.5f;
        if (nitro.animationPhase > 360.0f) {
            nitro.animationPhase -= 360.0f;
        }
        nitro.y = nitro.y + sin(nitro.animationPhase) * 0.08f;
    }
}

void updateSunPosition(float deltaTime) {
    if (sunsetProgress < 1.0f) {
        sunsetProgress += deltaTime / sunsetDuration;
        if (sunsetProgress > 1.0f) {
            sunsetProgress = 1.0f; // Clamp to 1.0 to stop the sunset
        }

        float angle = sunsetProgress * M_PI;

        // Update sun position (arc from east to west)
        sunPosition.x = 100.0f * cosf(angle);
        sunPosition.y = 100.0f * sinf(angle) + 10.0f; // Add 10 to keep sun above horizon initially

        // Update light position to match sun
        lightPosition[0] = sunPosition.x;
        lightPosition[1] = sunPosition.y;
        lightPosition[2] = sunPosition.z;
        lightPosition[3] = 1.0f; // Ensure it's a positional light
        glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);

        // Update light color and intensity to simulate sunset
        float intensity = 1.0f - 0.7f * sunsetProgress; // Gradually reduce intensity
        float r = 1.0f;
        float g = 1.0f - 0.4f * sunsetProgress;
        float b = 0.9f - 0.6f * sunsetProgress;

        lightDiffuse[0] = r * intensity;
        lightDiffuse[1] = g * intensity;
        lightDiffuse[2] = b * intensity;
        glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);

        lightAmbient[0] = r * 0.3f * intensity;
        lightAmbient[1] = g * 0.3f * intensity;
        lightAmbient[2] = b * 0.3f * intensity;
        glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);

        // Update sky color
        glm::vec3 midSkyColor;
        if (sunsetProgress < 0.5f) {
            // First half: transition from morning to noon
            midSkyColor = glm::mix(morningSkyColor, noonSkyColor, sunsetProgress * 2);
        }
        else {
            // Second half: transition from noon to sunset, then to night
            float t = (sunsetProgress - 0.5f) * 2;
            midSkyColor = glm::mix(noonSkyColor, sunsetSkyColor, t);
            midSkyColor = glm::mix(midSkyColor, nightSkyColor, std::max(0.0f, t - 0.5f) * 2);
        }
        currentSkyColor = midSkyColor;

        // Update sun color and visibility
        sunColor = glm::vec3(r, g, b);
        sunVisibility = std::max(0.0f, 1.0f - (sunsetProgress - 0.8f) * 5.0f); // Sun starts to disappear at 80% of sunset
    }
}

//=======================================================================
// Camera Function
//=======================================================================
void updateCamera()
{
    if (gameOver) {
        Eye = Vector(lastCarPosition.x, lastCarPosition.y + 5.0f, lastCarPosition.z + 10.0f);
        At = Vector(carPosition.x, 0, carPosition.z);
	}
    else if (gameWon) {
        // Position the camera in front of the car
        float radians = carRotation * M_PI / 180.0;
        Eye = Vector(
            carPosition.x + sin(radians) * 10.0f,
            carPosition.y + 3.0f,
            carPosition.z + cos(radians) * 10.0f
        );
        At = Vector(carPosition.x, carPosition.y, carPosition.z);
    }
    else
    if (currentView == INSIDE_FRONT)
    {
        float carRadians = -(carRotation * M_PI / 180.0);
        float yawRadians = cameraYaw * M_PI / 180.0;
        float pitchRadians = cameraPitch * M_PI / 180.0;

        // Calculate the rotated camera offset using car's rotation
        Vector rotatedCameraOffset(
            cameraOffset.x * cos(carRadians) - cameraOffset.z * sin(carRadians),
            cameraOffset.y,
            cameraOffset.x * sin(carRadians) + cameraOffset.z * cos(carRadians)
        );

        // Apply yaw and pitch rotations to the look-at vector
        Vector lookAt(0.0348995, 0, 0.999391);

        // Apply yaw rotation
        Vector yawLookAt(
            lookAt.x * cos(yawRadians) - lookAt.z * sin(yawRadians),
            lookAt.y,
            lookAt.x * sin(yawRadians) + lookAt.z * cos(yawRadians)
        );

        // Apply pitch rotation
        Vector pitchLookAt(
            yawLookAt.x,
            yawLookAt.y * cos(pitchRadians) - yawLookAt.z * sin(pitchRadians),
            yawLookAt.y * sin(pitchRadians) + yawLookAt.z * cos(pitchRadians)
        );

        Eye = Vector(
            carPosition.x + rotatedCameraOffset.x,
            carPosition.y + rotatedCameraOffset.y,
            carPosition.z + rotatedCameraOffset.z
        );

        At = Vector(
            Eye.x + pitchLookAt.x + sin(-carRadians) * cameraLookAheadDistance,
            Eye.y + pitchLookAt.y,
            Eye.z + pitchLookAt.z + cos(-carRadians) * cameraLookAheadDistance
        );

        Up = Vector(0, 1, 0);
    }

    else if (currentView == THIRD_PERSON)
	{
		float radians = carRotation * M_PI / 180.0;
		
		// Calculate camera position
		Eye.x = carPosition.x - sin(radians) * cameraDistance;
		Eye.y = carPosition.y + cameraHeight;
		Eye.z = carPosition.z - cos(radians) * cameraDistance;

		// Calculate look-at point
		At.x = carPosition.x + sin(radians) * cameraLookAheadDistance;
		At.y = carPosition.y + 1.0f; // Look slightly above the car
		At.z = carPosition.z + cos(radians) * cameraLookAheadDistance;

		Up = Vector(0, 1, 0); // Keep the up vector vertical


	}
	else
	{
		Eye = Vector(20, 5, 20);
		At = Vector(0, 0, 0);
		Up = Vector(0, 1, 0);
	}
	/*thirdPersonOffset.print();*/
	glLoadIdentity();
	gluLookAt(Eye.x, Eye.y, Eye.z, At.x, At.y, At.z, Up.x, Up.y, Up.z);
}

void drawGameOverText() {

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, WIDTH, 0, HEIGHT);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glColor3f(1.0f, 0.0f, 0.0f);  // Red color for text
    glRasterPos2i(WIDTH / 2 - 50, HEIGHT / 2);

    const char* text = "Game Over";
    for (const char* c = text; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }

    glRasterPos2i(WIDTH / 2 - 100, HEIGHT / 2 - 30);
    const char* restartText = "Press 'R' to restart";
    for (const char* c = restartText; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    }

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glEnable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
}

void renderSpeedOMeter(float speed) {
    const float centerX = 1100.0f;  // Center of the speedometer (X position)
    const float centerY = 100.0f;  // Center of the speedometer (Y position)
    const float radius = 80.0f;    // Radius of the speedometer
    const float maxSpeed = 200.0f; // Maximum speed
    const int numMarkers = 10;     // Number of markers

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, WIDTH, 0, HEIGHT);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Disable lighting for 2D rendering
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);

    // **Draw Metallic Circular Border**
    glColor3f(0.6f, 0.6f, 0.6f);  // Silver color
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(centerX, centerY);
    for (int i = 0; i <= 360; ++i) {
        float angle = i * M_PI / 180.0f;
        glVertex2f(centerX + (radius + 5) * cos(angle), centerY + (radius + 5) * sin(angle));
    }
    glEnd();

    // **Draw Gradient Background**
    for (int i = 0; i < 100; ++i) {
        glColor3f(0.0f, 0.0f, 0.0f + (i * 0.01f));  // Dark-to-light gradient
        glBegin(GL_TRIANGLE_FAN);
        for (int j = 0; j <= 360; ++j) {
            float angle = j * M_PI / 180.0f;
            glVertex2f(centerX + radius * (1 - i * 0.01f) * cos(angle),
                centerY + radius * (1 - i * 0.01f) * sin(angle));
        }
        glEnd();
    }

    // **Draw Markers and Labels**
    glColor3f(1.0f, 1.0f, 1.0f); // White color for markers and labels
    for (int i = 0; i <= numMarkers; ++i) {
        float angle = (135.0f - (270.0f * i / numMarkers)) * M_PI / 180.0f;
        float innerX = centerX + (radius - 10) * cos(angle);
        float innerY = centerY + (radius - 10) * sin(angle);
        float outerX = centerX + radius * cos(angle);
        float outerY = centerY + radius * sin(angle);
        glBegin(GL_LINES);
        glVertex2f(innerX, innerY);
        glVertex2f(outerX, outerY);
        glEnd();

        // Draw speed labels
        std::ostringstream oss;
        oss << (i * maxSpeed / numMarkers);
        std::string speedLabel = oss.str();
        float textX = centerX + (radius - 25) * cos(angle) - 5;
        float textY = centerY + (radius - 25) * sin(angle) - 5;
        glRasterPos2f(textX, textY);
        for (char c : speedLabel) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, c);
        }
    }

    // **Draw Needle**
    float needleAngle = (135.0f - (270.0f * speed / maxSpeed)) * M_PI / 180.0f;

    // Needle Shadow
    glColor3f(0.3f, 0.0f, 0.0f);  // Dark red shadow
    glBegin(GL_QUADS);
    glVertex2f(centerX + 2 * cos(needleAngle - M_PI / 2), centerY + 2 * sin(needleAngle - M_PI / 2)); // Left base
    glVertex2f(centerX + 2 * cos(needleAngle + M_PI / 2), centerY + 2 * sin(needleAngle + M_PI / 2)); // Right base
    glVertex2f(centerX + (radius - 20) * cos(needleAngle + 0.02f),
        centerY + (radius - 20) * sin(needleAngle + 0.02f));  // Right tip
    glVertex2f(centerX + (radius - 20) * cos(needleAngle - 0.02f),
        centerY + (radius - 20) * sin(needleAngle - 0.02f));  // Left tip
    glEnd();

    // Needle Body
    glColor3f(1.0f, 0.0f, 0.0f);  // Bright red
    glBegin(GL_QUADS);
    glVertex2f(centerX + 2 * cos(needleAngle - M_PI / 2), centerY + 2 * sin(needleAngle - M_PI / 2)); // Left base
    glVertex2f(centerX + 2 * cos(needleAngle + M_PI / 2), centerY + 2 * sin(needleAngle + M_PI / 2)); // Right base
    glVertex2f(centerX + (radius - 20) * cos(needleAngle + 0.02f),
        centerY + (radius - 20) * sin(needleAngle + 0.02f));  // Right tip
    glVertex2f(centerX + (radius - 20) * cos(needleAngle - 0.02f),
        centerY + (radius - 20) * sin(needleAngle - 0.02f));  // Left tip
    glEnd();

    // **Draw Center Cap**
    glColor3f(0.8f, 0.8f, 0.8f);  // Metallic silver
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(centerX, centerY);
    for (int i = 0; i <= 360; ++i) {
        float angle = i * M_PI / 180.0f;
        glVertex2f(centerX + 5 * cos(angle), centerY + 5 * sin(angle));
    }
    glEnd();

    // **Glass Overlay**
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 1.0f, 1.0f, 0.2f);  // Transparent white
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(centerX, centerY);
    for (int i = 0; i <= 360; ++i) {
        float angle = i * M_PI / 180.0f;
        glVertex2f(centerX + radius * cos(angle), centerY + radius * sin(angle));
    }
    glEnd();
    glDisable(GL_BLEND);

    // Re-enable lighting and depth testing
    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void drawRoundedRect(float x, float y, float width, float height, float radius) {
    int segments = 20;
    glBegin(GL_POLYGON);
    for (int i = 0; i <= segments; i++) {
        float theta = i * 2.0f * M_PI / segments;
        float cx = x + radius - cosf(theta) * radius;
        float cy = y + radius - sinf(theta) * radius;
        glVertex2f(cx, cy);
    }
    for (int i = 0; i <= segments; i++) {
        float theta = i * 2.0f * M_PI / segments;
        float cx = x + width - radius + cosf(theta) * radius;
        float cy = y + radius - sinf(theta) * radius;
        glVertex2f(cx, cy);
    }
    for (int i = 0; i <= segments; i++) {
        float theta = i * 2.0f * M_PI / segments;
        float cx = x + width - radius + cosf(theta) * radius;
        float cy = y + height - radius + sinf(theta) * radius;
        glVertex2f(cx, cy);
    }
    for (int i = 0; i <= segments; i++) {
        float theta = i * 2.0f * M_PI / segments;
        float cx = x + radius - cosf(theta) * radius;
        float cy = y + height - radius + sinf(theta) * radius;
        glVertex2f(cx, cy);
    }
    glEnd();
}

void drawRoundedRectOutline(float x, float y, float width, float height, float radius) {
    int segments = 20;
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i <= segments; i++) {
        float theta = i * 2.0f * M_PI / segments;
        float cx = x + radius - cosf(theta) * radius;
        float cy = y + radius - sinf(theta) * radius;
        glVertex2f(cx, cy);
    }
    for (int i = 0; i <= segments; i++) {
        float theta = i * 2.0f * M_PI / segments;
        float cx = x + width - radius + cosf(theta) * radius;
        float cy = y + radius - sinf(theta) * radius;
        glVertex2f(cx, cy);
    }
    for (int i = 0; i <= segments; i++) {
        float theta = i * 2.0f * M_PI / segments;
        float cx = x + width - radius + cosf(theta) * radius;
        float cy = y + height - radius + sinf(theta) * radius;
        glVertex2f(cx, cy);
    }
    for (int i = 0; i <= segments; i++) {
        float theta = i * 2.0f * M_PI / segments;
        float cx = x + radius - cosf(theta) * radius;
        float cy = y + height - radius + sinf(theta) * radius;
        glVertex2f(cx, cy);
    }
    glEnd();
}

void drawHUD() {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, WIDTH, 0, HEIGHT);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Disable lighting for 2D rendering
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);

    // Stopwatch dimensions and position
    float stopwatchX = 20;
    float stopwatchY = HEIGHT - 100;
    float stopwatchWidth = 180;
    float stopwatchHeight = 60;

    // Draw stopwatch body (rounded rectangle)
    glColor3f(0.2f, 0.2f, 0.2f); // Dark gray
    drawRoundedRect(stopwatchX, stopwatchY, stopwatchWidth, stopwatchHeight, 10);

    // Draw stopwatch screen
    glColor3f(0.0f, 0.0f, 0.0f); // Black screen
    drawRoundedRect(stopwatchX + 5, stopwatchY + 5, stopwatchWidth - 10, stopwatchHeight - 10, 5);

    // Draw screen border
    glLineWidth(2.0f);
    glColor3f(0.5f, 0.5f, 0.5f); // Gray border
    glBegin(GL_LINE_LOOP);
    drawRoundedRectOutline(stopwatchX + 5, stopwatchY + 5, stopwatchWidth - 10, stopwatchHeight - 10, 5);
    glEnd();

    // Format time as HH:MM:SS
    int totalSeconds = static_cast<int>(gameTimer);
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;
    int milliseconds = static_cast<int>((gameTimer - totalSeconds) * 100);

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << hours << ":"
        << std::setfill('0') << std::setw(2) << minutes << ":"
        << std::setfill('0') << std::setw(2) << seconds << "."
        << std::setfill('0') << std::setw(2) << milliseconds;

    std::string timerText = oss.str();

    // Draw digital time text
    glColor3f(0.0f, 1.0f, 0.0f); // Bright green for digital display
    float textX = stopwatchX + 15;
    float textY = stopwatchY + stopwatchHeight / 2 + 5;
    glRasterPos2f(textX, textY);
    for (char c : timerText) {
        glutBitmapCharacter(GLUT_BITMAP_9_BY_15, c);
    }

    // Render speedometer if game is active
    if (!gameWon && !gameOver) {
        renderSpeedOMeter(abs(carSpeed * 2.1));
    }

    // Draw win text
    if (gameWon) {
        glColor3f(0.0f, 1.0f, 0.0f); // Green color for "You Win"
        glRasterPos2i(WIDTH / 2 - 100, HEIGHT / 2);
        std::string winText = "You Win! Time: " + std::to_string(playerTime) + " seconds";
        for (char c : winText) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
        }
    }

    // Draw game-over text
    if (gameOver) {
        drawGameOverText();
    }

    // Re-enable lighting and depth testing
    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

//=======================================================================
// Game Over Screen
//======================================================================
void resetGame() {
    gravityEnabled = false;
    gameOver = false;
    carPosition = Vector(0, 0, 0);  // Reset car position
    carRotation = 0;  // Reset car rotation
    carSpeed = 0;  // Reset car speed
    wheelRotationX = 0;  // Reset wheel rotation
    wheelRotationY = 0;  // Reset wheel rotation
    sunsetProgress = 0.0f;  // Reset sunset progress
    gameWon = false;
    gameTimer = 90.0f;
    playerTime = 0.0f;

}

//=======================================================================
// Material Configuration Function
//======================================================================
void InitMaterial()
{
	// Enable Material Tracking
	glEnable(GL_COLOR_MATERIAL);

	// Sich will be assigneet Material Properties whd by glColor
	glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);

	// Set Material's Specular Color
	// Will be applied to all objects
	GLfloat specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glMaterialfv(GL_FRONT, GL_SPECULAR, specular);

	// Set Material's Shine value (0->128)
	GLfloat shininess[] = { 96.0f };
	glMaterialfv(GL_FRONT, GL_SHININESS, shininess);
}

//=======================================================================
// OpengGL Configuration Function
//=======================================================================
void myInit(void)
{
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(fovy, aspectRatio, zNear, zFar);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(Eye.x, Eye.y, Eye.z, At.x, At.y, At.z, Up.x, Up.y, Up.z);
    InitLightSource();
    InitMaterial();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
}

//=======================================================================
// Render Functions
//=======================================================================
void RenderGround()
{
	glDisable(GL_LIGHTING);	// Disable lighting 

	glColor3f(0.6, 0.6, 0.6);	// Dim the ground texture a bit

	glEnable(GL_TEXTURE_2D);	// Enable 2D texturing

	glBindTexture(GL_TEXTURE_2D, tex_ground.texture[0]);	// Bind the ground texture

	glPushMatrix();
	glBegin(GL_QUADS);
	glNormal3f(0, 1, 0);	// Set quad normal direction.
	glTexCoord2f(0, 0);		// Set tex coordinates ( Using (0,0) -> (5,5) with texture wrapping set to GL_REPEAT to simulate the ground repeated grass texture).
	glVertex3f(-20, 0, -20);
	glTexCoord2f(5, 0);
	glVertex3f(20, 0, -20);
	glTexCoord2f(5, 5);
	glVertex3f(20, 0, 20);
	glTexCoord2f(0, 5);
	glVertex3f(-20, 0, 20);
	glEnd();
	glPopMatrix();

	glEnable(GL_LIGHTING);	// Enable lighting again for other entites coming throung the pipeline.

	glColor3f(1, 1, 1);	// Set material back to white instead of grey used for the ground texture.
}

void renderCones() {
    for (const auto& cone : cones) {
        glPushMatrix();
        glTranslatef(cone.x, cone.y, cone.z);
        glScalef(1.0f, 1.0f, 1.0f);  // Adjust scale if needed
        glRotatef(90.0f, 0, 1, 0);   // Adjust rotation if needed
        coneModel.DrawModel();
        glPopMatrix();
    }
}

void renderText(float x, float y, const std::string& text) {
    glRasterPos2f(x, y);
    for (char c : text) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
    }
}

std::string formatSpeed(float speed) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << speed;
    return oss.str();
}

void renderNitros() {
    for (const auto& nitro : nitros) {
        float rotation = -nitro.animationPhase * 45;

        glPushMatrix();
        glTranslatef(nitro.x, nitro.y, nitro.z);
        glScalef(0.2f, 0.2f, 0.2f);  // Adjust scale if needed
        glRotatef(20, 1, 0, 0);
        glRotatef(rotation, 0, 1, 0);   // Adjust rotation if needed
        nitroModel.DrawModel();
        glPopMatrix();
    }
}

void renderCar() {
    // Update car model position and rotation
    glPushMatrix();
    glTranslatef(carPosition.x, carPosition.y, carPosition.z);
    glRotatef(carRotation, 0, 1, 0);
    //glScalef(1, 1, 1);
    glRotatef(0, 0, 1, 0);
    // Update headlight position and direction
    headlight1_pos[0] = 2.0f;
    headlight1_pos[1] = 0.5f;
    headlight1_pos[2] = 1.0f;

	headlight2_pos[0] = -2.0f;
	headlight2_pos[1] = 0.5f;
	headlight2_pos[2] = 1.0f;

    headlight_dir[0] = 0.0f;
    headlight_dir[1] = -0.1f;
    headlight_dir[2] = 1.0f;

    glLightfv(GL_LIGHT1, GL_POSITION, headlight1_pos);
    glLightfv(GL_LIGHT1, GL_SPOT_DIRECTION, headlight_dir);
    glLightfv(GL_LIGHT2, GL_POSITION, headlight2_pos);
    glLightfv(GL_LIGHT2, GL_SPOT_DIRECTION, headlight_dir);
    carModel1.DrawModel();
    glPopMatrix();

        // Offsets for the wheels relative to the car's position
        float wheelOffsetX = -1.15f; // Horizontal offset from the car's center
        float wheelOffsetY = 0.5f; // Vertical offset below the car
        float wheelOffsetZFront = -1.7f; // Forward offset for front wheels
        float wheelOffsetZBack = 1.7f; // Backward offset for back wheels

        // Draw back left wheel
        glPushMatrix();
        //glScalef(1, 1, 1); 
        glTranslatef(carPosition.x, carPosition.y, carPosition.z);
        glRotatef(carRotation, 0, 1, 0);  // to face the right direction
        glTranslatef(-wheelOffsetX, wheelOffsetY, wheelOffsetZFront);

        glRotatef(wheelRotationX, 1, 0, 0);  // rotate on x here when clicking up or down
        glRotatef(180, 0, 1, 0);  // to face the right direction
        redWheelsBackLeft1.DrawModel();
        glPopMatrix();

        // Draw back right wheel
        glPushMatrix();
        glTranslatef(carPosition.x, carPosition.y, carPosition.z);
        glRotatef(carRotation, 0, 1, 0);  // to face the right direction
        glTranslatef(wheelOffsetX, wheelOffsetY, wheelOffsetZFront);
        //glScalef(0.5, 0.5, 0.5);
        glRotatef(wheelRotationX, 1, 0, 0);  // rotate on x here when clicking up or down
        glRotatef(0, 0, 1, 0);
        redWheelsBackRight1.DrawModel();
        glPopMatrix();

        if (wheelRotationY > 52.5) {
            wheelRotationY = 52.5;
        }

        if (wheelRotationY < -52.5) {
            wheelRotationY = -52.5;
        }


        // Draw front left wheel
        glPushMatrix();
        glTranslatef(carPosition.x, carPosition.y, carPosition.z);
        glRotatef(carRotation, 0, 1, 0);  // to face the right direction
        glTranslatef(-wheelOffsetX, wheelOffsetY, wheelOffsetZBack);
        //glScalef(0.5, 0.5, 0.5);
        glRotatef(180 + wheelRotationY, 0, 1, 0);
        glRotatef(-wheelRotationX, 1, 0, 0);  // rotate on x here when clicking up or 
        redWheelsFrontLeft1.DrawModel();
        glPopMatrix();

        // Draw front right wheel
        glPushMatrix();
        glTranslatef(carPosition.x, carPosition.y, carPosition.z);
        glRotatef(carRotation, 0, 1, 0);  // to face the right direction
        glTranslatef(wheelOffsetX, wheelOffsetY, wheelOffsetZBack);	//glScalef(0.5, 0.5, 0.5);
        glRotatef(wheelRotationY, 0, 1, 0);
        glRotatef(wheelRotationX, 1, 0, 0);  // rotate on x here when clicking up or 
        redWheelsFrontRight1.DrawModel();
        glPopMatrix();

    if (wheelRotationY < -52.5) {
        wheelRotationY = -52.5;
    }


    // Draw front left wheel
    glPushMatrix();
    glTranslatef(carPosition.x, carPosition.y, carPosition.z);
    glRotatef(carRotation, 0, 1, 0);  // to face the right direction
    glTranslatef(-wheelOffsetX, wheelOffsetY, wheelOffsetZBack);
    //glScalef(0.5, 0.5, 0.5);
    glRotatef(180 + wheelRotationY, 0, 1, 0);
    glRotatef(-wheelRotationX, 1, 0, 0);  // rotate on x here when clicking up or 
    redWheelsFrontLeft1.DrawModel();
    glPopMatrix();

    // Draw front right wheel
    glPushMatrix();
    glTranslatef(carPosition.x, carPosition.y, carPosition.z);
    glRotatef(carRotation, 0, 1, 0);  // to face the right direction
    glTranslatef(wheelOffsetX, wheelOffsetY, wheelOffsetZBack);	//glScalef(0.5, 0.5, 0.5);
    glRotatef(wheelRotationY, 0, 1, 0);
    glRotatef(wheelRotationX, 1, 0, 0);  // rotate on x here when clicking up or 
    redWheelsFrontRight1.DrawModel();
    glPopMatrix();

 //   // Update headlight positions
 //   glm::mat4 carTransform = glm::mat4(1.0f);
 //   carTransform = glm::translate(carTransform, glm::vec3(carPosition.x, carPosition.y, carPosition.z));
 //   carTransform = glm::rotate(carTransform, glm::radians(carRotation), glm::vec3(0, 1, 0));

 //   glm::vec4 rightHeadlightPos = carTransform * glm::vec4(0.5f, 0.5f, -1.0f, 1.0f);
 //   glm::vec4 leftHeadlightPos = carTransform * glm::vec4(-0.5f, 0.5f, -1.0f, 1.0f);

 //   headlight1_pos[0] = rightHeadlightPos.x;
 //   headlight1_pos[1] = rightHeadlightPos.y;
 //   headlight1_pos[2] = rightHeadlightPos.z;
	//printf("headlight1_pos: %f %f %f\n", headlight1_pos[0], headlight1_pos[1], headlight1_pos[2]);

 //   headlight2_pos[0] = leftHeadlightPos.x;
 //   headlight2_pos[1] = leftHeadlightPos.y;
 //   headlight2_pos[2] = leftHeadlightPos.z;
 //   printf("headlight2_pos: %f %f %f\n", headlight2_pos[0], headlight2_pos[1], headlight2_pos[2]);

 //   // Update headlight direction
 //   glm::vec4 headlightDir = carTransform * glm::vec4(0.0f, -0.1f, 1.0f, 0.0f);
 //   headlight_dir[0] = headlightDir.x;
 //   headlight_dir[1] = headlightDir.y;
 //   headlight_dir[2] = headlightDir.z;
}

void renderGoRight() {
    // traffic obstacles
    glPushMatrix();
    glTranslatef(5.23767, 0, -106.795);
    glRotatef(180, 0, 1, 0);
    glScalef(300, 300, 300);
    trafficObstacle.DrawModel();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-2.23767, 0, -106.795);
    glRotatef(180, 0, 1, 0);
    glScalef(300, 300, 300);
    trafficObstacle.DrawModel();
    glPopMatrix();
}

void renderHorizontalBarrier() {
    // horizontal obstacle
    glPushMatrix();
    glTranslatef(181.899, 0, 221.466);
    glRotatef(270, 0, 1, 0);
    glScalef(400, 400, 400);
    horizontalTraffic.DrawModel();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(181.899, 0, 226.466);
    glRotatef(270, 0, 1, 0);
    glScalef(400, 400, 400);
    horizontalTraffic.DrawModel();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(181.899, 0, 231.466);
    glRotatef(270, 0, 1, 0);
    glScalef(400, 400, 400);
    horizontalTraffic.DrawModel();
    glPopMatrix();
}

//=======================================================================
// Display Function
//=======================================================================
void myDisplay(void)
{
    static int lastTime = 0;
    int currentTime = glutGet(GLUT_ELAPSED_TIME);
    float deltaTime = (currentTime - lastTime) / 1000.0f;
    lastTime = currentTime;
    handleCarControls(deltaTime);
    updateCarPosition(deltaTime);
    if (checkCollisionWithObstacles(carPosition)) {
        carSpeed = 0;
        isColliding = true;
        applyCollisionRecoil(deltaTime);
    }
    else {
		isColliding = false;
    }
	if (checkCollisionWithNitros(carPosition, nitros))
	{
        isNitroActive = true;
	}
    updateSunPosition(deltaTime);
    updateNitroAnimation();
    //carPosition.print();
    glClearColor(currentSkyColor.r, currentSkyColor.g, currentSkyColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    updateCamera();
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);

    // Draw skybox
    glPushMatrix();
    GLUquadricObj* qobj;
    qobj = gluNewQuadric();
    glTranslated(50, 0, 0);
    glRotated(90, 1, 0, 1);
    glBindTexture(GL_TEXTURE_2D, tex);
    gluQuadricTexture(qobj, true);
    gluQuadricNormals(qobj, GL_SMOOTH);
    gluSphere(qobj, 1000, 100, 100);
    gluDeleteQuadric(qobj);
    glPopMatrix();
   
    setupLighting();
    glPushMatrix();
    glTranslatef(10, 0, 0);
    glScalef(0.7, 0.7, 0.7);
    model_tree.Draw();
    glPopMatrix();
   
    
    glPushMatrix();
    glTranslatef(0, 0, 0);  // Position your model
    glScalef(1, 1, 1);  // Scale if needed
    glRotatef(90, 0, 1, 0);  // Rotate if needed
    gltfModel1.DrawModel();
    glPopMatrix();
#
    
    renderCones();
	renderNitros();

    renderCar();

    if (sunVisibility > 0.0f) {
        glPushMatrix();
        glTranslatef(sunPosition.x, sunPosition.y, sunPosition.z);
        glDisable(GL_LIGHTING);
        glColor4f(sunColor.r, sunColor.g, sunColor.b, sunVisibility);
        glEnable(GL_LIGHTING);
        glPopMatrix();
    }
    // Update car model position and rotation
    glPushMatrix();
    glTranslatef(112.226, 0, 226.23);
    glRotatef(270, 0, 1, 0);
    glScalef(1.2, 1.2, 1.2);
    finishModel.DrawModel();
    glPopMatrix();

    renderHorizontalBarrier(); 

	renderGoRight();

    
    drawHUD();


    glutSwapBuffers();
}

//=======================================================================
// Keyboard Function
//=======================================================================
void myKeyboard(unsigned char button, int x, int y)
{
    if (isRespawning || collisionRecoil > 0) {
        return;
    }
    if(gameOver || gameWon)
        {
		if (button == 'r' || button == 'R')
		{
            nitros = originalNitros;
			resetGame();
		}
		return;
	}
	switch (button)
	{
	case 'w':
		if (currentView == THIRD_PERSON)
			thirdPersonOffset.y += thirdPersonMovementSpeed;
		break;
	case 's':
		if (currentView == THIRD_PERSON)
			thirdPersonOffset.y -= thirdPersonMovementSpeed;
		break;
	case 'a':
		if (currentView == THIRD_PERSON)
			thirdPersonOffset.x -= thirdPersonMovementSpeed;
		break;
	case 'd':
		if (currentView == THIRD_PERSON)
			thirdPersonOffset.x += thirdPersonMovementSpeed;
		break;
	case 'q':
		if (currentView == THIRD_PERSON)
			thirdPersonOffset.z += thirdPersonMovementSpeed;
		break;
	case 'e':
		if (currentView == THIRD_PERSON)
			thirdPersonOffset.z -= thirdPersonMovementSpeed;
		break;
    case 'r':
    case 'R':
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        break;
	case '1':
		currentView = INSIDE_FRONT;
		break;
	case '3':
		currentView = THIRD_PERSON;
		break;
	//case 'j': // Rotate camera left
	//	if (currentView == INSIDE_FRONT)
	//		cameraYaw += cameraRotationSpeed;
	//	else if (currentView == THIRD_PERSON)
	//		thirdPersonYaw += cameraRotationSpeed;
	//	break;
	//case 'l': // Rotate camera right
	//	if (currentView == INSIDE_FRONT)
	//		cameraYaw -= cameraRotationSpeed;
	//	else if (currentView == THIRD_PERSON)
	//		thirdPersonYaw -= cameraRotationSpeed;
	//	break;
	//case 'i': // Rotate camera up
	//	if (currentView == INSIDE_FRONT)
	//		cameraPitch += cameraRotationSpeed;
	//	else if (currentView == THIRD_PERSON)
	//		thirdPersonPitch += cameraRotationSpeed;
	//	break;
	//case 'k': // Rotate camera down
	//	if (currentView == INSIDE_FRONT)
	//		cameraPitch -= cameraRotationSpeed;
	//	else if (currentView == THIRD_PERSON)
	//		thirdPersonPitch -= cameraRotationSpeed;
	//	break;
	case 27:
		exit(0);
		break;
	default:
		break;
	}
	glutPostRedisplay();
}

void specialKeyboard(int key, int x, int y)
{
    if (gameOver || gameWon || isRespawning || collisionRecoil > 0) {
        return;
    }

    switch (key)
    {
    case GLUT_KEY_LEFT:
        if(!isColliding) wheelRotationY += 15.0f;
        break;
    case GLUT_KEY_RIGHT:
        if (!isColliding) wheelRotationY -= 15.0f;
        break;
    case GLUT_KEY_UP:
        if (!isColliding) {
            wheelRotationX += 6.0f;
            isAccelerating = true;
            isBraking = false;
            if (!timerStarted) {
                timerStarted = true;
            }
        }
        break;
    case GLUT_KEY_DOWN:
        wheelRotationX -= 6.0f;
        isAccelerating = false;
        isBraking = true;
        break;
    }
    glutPostRedisplay();
}

void specialKeyboardUp(int key, int x, int y)
{
	switch (key)
	{
	case GLUT_KEY_LEFT:
	case GLUT_KEY_RIGHT:
		wheelRotationY = 0.0f; // Reset wheel rotation when key is released
		break;
	case GLUT_KEY_UP:
		isAccelerating = false;
		break;
	case GLUT_KEY_DOWN:
		isBraking = false;
		break;
	}
	glutPostRedisplay();
}

//=======================================================================
// Motion Function
//=======================================================================
void myMotion(int x, int y)
{
	y = HEIGHT - y;

	if (cameraZoom - y > 0)
	{
		Eye.x += -0.1;
		Eye.z += -0.1;
	}
	else
	{
		Eye.x += 0.1;
		Eye.z += 0.1;
	}

	cameraZoom = y;

	glLoadIdentity();	//Clear Model_View Matrix

	gluLookAt(Eye.x, Eye.y, Eye.z, At.x, At.y, At.z, Up.x, Up.y, Up.z);	//Setup Camera with modified paramters

	GLfloat light_position[] = { 0.0f, 10.0f, 0.0f, 1.0f };
	glLightfv(GL_LIGHT0, GL_POSITION, light_position);

	glutPostRedisplay();	//Re-draw scene 
}

//=======================================================================
// Mouse Function
//=======================================================================
void myMouse(int button, int state, int x, int y)
{
	y = HEIGHT - y;

	if (state == GLUT_DOWN)
	{
		cameraZoom = y;
	}
}

//=======================================================================
// Reshape Function
//=======================================================================
void myReshape(int w, int h)
{
	if (h == 0) {
		h = 1;
	}

	WIDTH = w;
	HEIGHT = h;

	// set the drawable region of the window
	glViewport(0, 0, w, h);

	// set up the projection matrix 
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(fovy, (GLdouble)WIDTH / (GLdouble)HEIGHT, zNear, zFar);

	// go back to modelview matrix so we can move the objects about
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(Eye.x, Eye.y, Eye.z, At.x, At.y, At.z, Up.x, Up.y, Up.z);
}

//=======================================================================
// Assets Loading Function
//=======================================================================
void LoadAssets()
{
	// Loading Model files
	model_house.Load("Models/house/house.3DS");
	model_tree.Load("Models/tree/Tree1.3ds");
	model_bugatti.Load("Models/bugatti/Bugatti_Bolide_2024_Modified_CSB.3ds");

	if (!gltfModel1.LoadModel("models/track5/scene.gltf")) {
		std::cerr << "Failed to load GLTF model" << std::endl;
		// Handle error
	}

	if (!carModel1.LoadModel("models/red-car-no-wheels/scene.gltf")) {
		std::cerr << "Failed to load GLTF model" << std::endl;
		// Handle error
	}

	if (!redWheelsFrontLeft1.LoadModel("models/wheel/scene.gltf")) {
		std::cerr << "Failed to load GLTF model" << std::endl;
		// Handle error
	}

	if (!redWheelsFrontRight1.LoadModel("models/wheel/scene.gltf")) {
		std::cerr << "Failed to load GLTF model" << std::endl;
		// Handle error
	}

	if (!redWheelsBackLeft1.LoadModel("models/wheel/scene.gltf")) {
		std::cerr << "Failed to load GLTF model" << std::endl;
		// Handle error
	}

	if (!redWheelsBackRight1.LoadModel("models/wheel/scene.gltf")) {
		std::cerr << "Failed to load GLTF model" << std::endl;
		// Handle error
	}

    //if (!coneModel.LoadModel("models/nitro2/scene.gltf")) {
    //    std::cerr << "Failed to load GLTF model" << std::endl;
    //    // Handle error
    //}

    if (!nitroModel.LoadModel("models/nitro2/scene.gltf")) {
        std::cerr << "Failed to load GLTF model" << std::endl;
        // Handle error
    }

    if (!coneModel.LoadModel("models/cone/scene.gltf")) {
        std::cerr << "Failed to load GLTF model" << std::endl;
        // Handle error
    }

    if (!finishModel.LoadModel("models/finish/scene.gltf")) {
        std::cerr << "Failed to load GLTF model" << std::endl;
        // Handle error
    }

    if (!horizontalTraffic.LoadModel("models/horizontal-obstacle/scene.gltf")) {
        std::cerr << "Failed to load GLTF model" << std::endl;
        // Handle error
    }

    if (!trafficObstacle.LoadModel("models/traffic-obstacles/scene.gltf")) {
        std::cerr << "Failed to load GLTF model" << std::endl;
        // Handle error
    }

	glTranslatef(carPosition.x, carPosition.y, carPosition.z);

	// Loading texture files
	loadBMP(&tex, "Textures/blu-sky-3.bmp", true);
}

//=======================================================================
// Main Function
//=======================================================================
void timer(int value) {
	glutPostRedisplay();
	glutTimerFunc(16, timer, 0);
}

void main(int argc, char** argv)
{

	glutInit(&argc, argv);

	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);

	glutInitWindowSize(WIDTH, HEIGHT);

	glutInitWindowPosition(100, 150);

	glutCreateWindow(title);

	glutDisplayFunc(myDisplay);

	glutKeyboardFunc(myKeyboard);
	glutSpecialFunc(specialKeyboard);
	glutSpecialUpFunc(specialKeyboardUp);



	glutMotionFunc(myMotion);

	glutMouseFunc(myMouse);

	glutReshapeFunc(myReshape);

	myInit();

	LoadAssets();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);  // Enable headlight 1
    glEnable(GL_LIGHT2);
    glEnable(GL_NORMALIZE);
    glEnable(GL_COLOR_MATERIAL);

	glShadeModel(GL_SMOOTH);

	//glut timer 

	glutTimerFunc(0, timer, 0);  // Start the timer


	glutMainLoop();
}
