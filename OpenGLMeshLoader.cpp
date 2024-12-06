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
void goToNextLevel(); 


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

    void UnloadModel() {
        for (const auto& entry : textureCache) {
            glDeleteTextures(1, &entry.second);
        }
        textureCache.clear();

        // Clear model data
        model = tinygltf::Model();
        std::cout << "Model and textures unloaded successfully." << std::endl;
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

            if (primitive.material >= 0) {
                const auto& material = model.materials[primitive.material];
                SetMaterial(material);
            }

            const auto& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
            const auto& posView = model.bufferViews[posAccessor.bufferView];
            const float* positions = reinterpret_cast<const float*>(
                &model.buffers[posView.buffer].data[posView.byteOffset + posAccessor.byteOffset]);

            const float* texcoords = nullptr;
            if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
                const auto& texAccessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
                const auto& texView = model.bufferViews[texAccessor.bufferView];
                texcoords = reinterpret_cast<const float*>(
                    &model.buffers[texView.buffer].data[texView.byteOffset + texAccessor.byteOffset]);
            }

            const auto& indexAccessor = model.accessors[primitive.indices];
            const auto& indexView = model.bufferViews[indexAccessor.bufferView];
            const void* indices = &model.buffers[indexView.buffer].data[indexView.byteOffset +
                indexAccessor.byteOffset];

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



struct Triangle {
    glm::vec3 v1, v2, v3; // Triangle vertices
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
GLTFModel moscowModel; 
GLTFModel bugattiModel;
GLTFModel blueWheelModel;
GLTFModel egpModel;
GLTFModel rockModel;
GLTFModel logModel;
GLTFModel roadBlockModel;



int WIDTH = 1280;
int HEIGHT = 720;

GLuint tex;
char title[] = "All Star Racer";

// 3D Projection Options
GLdouble fovy = 45.0;
GLdouble aspectRatio = (GLdouble)WIDTH / (GLdouble)HEIGHT;
GLdouble zNear = 0.1;
GLdouble zFar = 10000;

class SunriseEffect {
private:
    float time;         // Time elapsed since start of effect
    float duration;     // Total duration of effect in seconds
    float sunPosition[3]; // Sun position in the sky
    float sunBrightness;  // Sun brightness
    bool started;

    float lerp(float a, float b, float t) const {
        return a + t * (b - a);
    }

    float smoothStep(float t) const {
        return t * t * (3 - 2 * t);
    }

public:
    SunriseEffect(float duration = 300.0f)
        : time(0.0f), duration(duration), sunBrightness(0.0f) {
        sunPosition[0] = -1.0f; // Start from the left
        sunPosition[1] = -0.5f; // Start below the horizon
        sunPosition[2] = 1.0f;  // Sun is initially at a distant point on the z-axis
    }

    // Reset function to restart the effect
    void reset() {
        time = 0.0f;
        sunPosition[0] = -1.0f; // Reset to initial position
        sunPosition[1] = -0.5f; // Reset to initial position
        sunPosition[2] = 1.0f;  // Reset to initial position
        sunBrightness = 0.0f;   // Reset brightness
        started = true;
    }

	void start() {
		started = true;
	}

    void update(float deltaTime) {
        time += deltaTime;
        if (time > duration) {
            time = duration; // Cap at maximum duration
        }

        float t = smoothStep(time / duration);

        // Move the sun from left to right and up (sunrise motion)
        sunPosition[0] = lerp(-1.0f, 1.0f, t);
        sunPosition[1] = lerp(-0.5f, 0.5f, t);

        // Increase sun brightness
        sunBrightness = lerp(0.0f, 1.0f, t);
    }

    void apply() {
        float t = smoothStep(time / duration);
        t = smoothStep(t);

        // Sky color transition (sunrise colors)
        float startR = 0.0f, startG = 0.0f, startB = 0.2f; // Dark blue
        float endR = 0.5f, endG = 0.7f, endB = 1.0f; // Light blue

        float r = lerp(startR, endR, t);
        float g = lerp(startG, endG, t);
        float b = lerp(startB, endB, t);

        glClearColor(r, g, b, 1.0f);

        // Set up directional light
        float lightAmbient[] = { 0.1f, 0.1f, 0.1f, 1.0f };
        float lightDiffuse[] = { 1.0f, 0.9f, 0.7f, 1.0f };
        float lightSpecular[] = { 1.0f, 1.0f, 1.0f, 1.0f };

        // Apply sun brightness to light intensity
        for (int i = 0; i < 3; ++i) {
            lightAmbient[i] *= sunBrightness;
            lightDiffuse[i] *= sunBrightness;
            lightSpecular[i] *= sunBrightness;
        }

        glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
        glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);
        glLightfv(GL_LIGHT0, GL_POSITION, sunPosition);

        // Enable lighting
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
    }
};

class MovingSunEffect {
private:
    float time;         // Time elapsed since start of effect
    float duration;     // Total duration of effect in seconds
    float sunPosition[3]; // Sun position in the sky
    float sunBrightness;  // Sun brightness
    bool started;

    float lerp(float a, float b, float t) const {
        return a + t * (b - a);
    }

    float smoothStep(float t) const {
        return t * t * (3 - 2 * t);
    }

public:
    MovingSunEffect(float duration = 300.0f)
        : time(0.0f), duration(duration), sunBrightness(0.0f), started(false) {
        sunPosition[0] = -1.0f; // Start from the left
        sunPosition[1] = -0.5f; // Start below the horizon
        sunPosition[2] = 1.0f;  // Sun is initially at a distant point on the z-axis
    }

    // Reset function to restart the effect
    void reset() {
        time = 0.0f;
        sunPosition[0] = -1.0f; // Reset to initial position
        sunPosition[1] = -0.5f; // Reset to initial position
        sunPosition[2] = 1.0f;  // Reset to initial position
        sunBrightness = 0.0f;   // Reset brightness
        started = true;
    }
    void start() {
        started = true;
    }

    void update(float deltaTime) {
        time += deltaTime;
        if (time > duration) {
            time = duration; // Cap at maximum duration
        }

        float t = smoothStep(time / duration);

        // Move the sun from left to right and up (sunrise motion)
        sunPosition[0] = lerp(-1.0f, 1.0f, t);
        sunPosition[1] = lerp(-0.5f, 0.5f, t);

        // Increase sun brightness
        sunBrightness = lerp(0.0f, 1.0f, t);
    }

    void apply() {
        float t = smoothStep(time / duration);

        // Sky color transition (sunrise colors)
        float startR = 0.1f, startG = 0.1f, startB = 0.2f;
        float midR = 0.7f, midG = 0.4f, midB = 0.3f;
        float endR = 0.5f, endG = 0.7f, endB = 1.0f;

        float r, g, b;
        if (t < 0.5f) {
            float t2 = t * 2.0f;
            r = lerp(startR, midR, t2);
            g = lerp(startG, midG, t2);
            b = lerp(startB, midB, t2);
        }
        else {
            float t2 = (t - 0.5f) * 2.0f;
            r = lerp(midR, endR, t2);
            g = lerp(midG, endG, t2);
            b = lerp(midB, endB, t2);
        }

        glClearColor(r, g, b, 1.0f);

        // Set up directional light
        float lightAmbient[] = { 0.1f, 0.1f, 0.1f, 1.0f };
        float lightDiffuse[] = { 1.0f, 0.9f, 0.7f, 1.0f };
        float lightSpecular[] = { 1.0f, 1.0f, 1.0f, 1.0f };

        // Apply sun brightness to light intensity
        for (int i = 0; i < 3; ++i) {
            lightAmbient[i] *= sunBrightness;
            lightDiffuse[i] *= sunBrightness;
            lightSpecular[i] *= sunBrightness;
        }

        glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
        glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);
        glLightfv(GL_LIGHT0, GL_POSITION, sunPosition);

        // Enable lighting
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
    }
};

struct Cone {
    float x;
    float y;
    float z;

    Cone(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
};

struct Stone {
    float x;
    float y;
    float z;

    Stone(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
};

struct Log {
    float x;
    float y;
    float z;

    Log(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
};

struct Nitro {
    float x;
    float y;
    float z;
    float animationPhase;

    Nitro(float _x, float _y, float _z, float _animationPhase) : x(_x), y(_y), z(_z), animationPhase(_animationPhase) {}
};

struct Coin {
    float x;
    float y;
    float z;
    float animationPhase;

    Coin(float _x, float _y, float _z, float _animationPhase) : x(_x), y(_y), z(_z), animationPhase(_animationPhase) {}
};

//struct Coin {
//    float x;
//    float y;
//    float z;
//
//    Coin(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
//};

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
    //Cone(-396.0f, 1.3f, 45.1598f) // Assuming the last value was cut off, added a placeholder value
};

std::vector<Stone> stones = {
    Stone(-0.949136, 1, 62.6161),
    Stone(-55.3627, 1, 99.5125),
    Stone(-34.5801, 1, 215.34),
    Stone(-88.8641, 1, 243.28),
    Stone(-192.955, 1, 231.441),
    Stone(-200.625, 1, 381.979),
    Stone(-247.875, 1, 330.386),
    Stone(-237.087, 1, 301.843)
};

std::vector<Log> logs = {
    Log(-160.909, 0, 386.673),
    Log(-160.909, 0, 378.713), 
    Log(-161.442, 0, 372.175)
};

std::vector<Vector> barriers2 = {
    Vector(-47.0496, 0, -26.7259),
    Vector(-160.909, 0, 386.673),
    Vector(-160.909, 0, 378.713),
    Vector(-161.442, 0, 372.175)

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

std::vector<Coin> coins = {
    Coin(7.92534, 1, 27.4013, 0),
    Coin(10.554f, 1, 52.6967f, 0),
    Coin(-10.9683f, 1, 87.0919f, 0),
    Coin(-22.6874f, 1, 92.2444f, 0),
    Coin(-34.7078f, 1, 76.8697f, 0),
    Coin(-57.7044f, 1, 56.5818f, 0),
    Coin(-68.7227f, 1, 75.522f, 0),
    Coin(-50.6736f, 1, 96.5191f, 0),
    Coin(-28.4619f, 1, 146.144f, 0),
    Coin(-10.6378f, 1, 170.759f, 0),
    Coin(-17.725f, 1, 204.849f, 0),
    Coin(-40.5779f, 1, 230.781f, 0),
    Coin(-82.1536f, 1, 249.69f, 0),
    Coin(-110.309f, 1, 213.295f, 0),
    Coin(-161.206f, 1, 173.604f, 0),
    Coin(-196.742f, 1, 195.568f, 0),
    Coin(-185.973f, 1, 227.619f, 0),
    Coin(-167.631f, 1, 260.432f, 0),
    Coin(-157.681f, 1, 289.419f, 0),
    Coin(-171.671f, 1, 310.134f, 0),
    Coin(-175.742f, 1, 327.887f, 0),
    Coin(-162.257f, 1, 363.324f, 0),
    Coin(-215.037f, 1, 231.271f, 0),
    Coin(-227.006f, 1, 264.967f, 0),
    Coin(-237.439f, 1, 294.338f, 0),
    Coin(-165.869f, 1, 379.538f, 0),
    Coin(-181.904f, 1, 398.383f, 0),
    Coin(-209.562f, 1, 379.815f, 0),
    Coin(-234.212f, 1, 335.71f, 0),
    Coin(-216.211f, 1, 326.787f, 0),
    Coin(-183.778f, 1, 355.048f, 0)
};    

std::vector<Coin> originalCoins = {
    Coin(7.92534, 1, 27.4013, 0),
    Coin(10.554f, 1, 52.6967f, 0),
    Coin(-10.9683f, 1, 87.0919f, 0),
    Coin(-22.6874f, 1, 92.2444f, 0),
    Coin(-34.7078f, 1, 76.8697f, 0),
    Coin(-57.7044f, 1, 56.5818f, 0),
    Coin(-68.7227f, 1, 75.522f, 0),
    Coin(-50.6736f, 1, 96.5191f, 0),
    Coin(-28.4619f, 1, 146.144f, 0),
    Coin(-10.6378f, 1, 170.759f, 0),
    Coin(-17.725f, 1, 204.849f, 0),
    Coin(-40.5779f, 1, 230.781f, 0),
    Coin(-82.1536f, 1, 249.69f, 0),
    Coin(-110.309f, 1, 213.295f, 0),
    Coin(-161.206f, 1, 173.604f, 0),
    Coin(-196.742f, 1, 195.568f, 0),
    Coin(-185.973f, 1, 227.619f, 0),
    Coin(-167.631f, 1, 260.432f, 0),
    Coin(-157.681f, 1, 289.419f, 0),
    Coin(-171.671f, 1, 310.134f, 0),
    Coin(-175.742f, 1, 327.887f, 0),
    Coin(-162.257f, 1, 363.324f, 0),
    Coin(-215.037f, 1, 231.271f, 0),
    Coin(-227.006f, 1, 264.967f, 0),
    Coin(-237.439f, 1, 294.338f, 0),
    Coin(-165.869f, 1, 379.538f, 0),
    Coin(-181.904f, 1, 398.383f, 0),
    Coin(-209.562f, 1, 379.815f, 0),
    Coin(-234.212f, 1, 335.71f, 0),
    Coin(-216.211f, 1, 326.787f, 0),
    Coin(-183.778f, 1, 355.048f, 0)
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

int level = 1; 
boolean selectingCar = true;
int selectedCar = 0; 

enum CameraView { OUTSIDE, INSIDE_FRONT, THIRD_PERSON, CINEMATIC};
CameraView currentView = CINEMATIC;
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

// second car controls 
float acceleration2 = 9.0f; // Acceleration in units per second^2
float deceleration2 = 50.0f; // Deceleration in units per second^2
float turnSpeed2 = 120.0f; // Turn speed in degrees per second
float maxSpeed2 = 30.0f; // Maximum speed in units per second

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

int score = 0;
SunriseEffect sunrise(120.0f);
MovingSunEffect sunEffect(120.0f);


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


// Array to hold the coordinates of the streetlights
std::vector<std::pair<float, float>> streetlightCoords = {
     {14.0092f, 12.6268f},
    {2.79439f, 37.8671f},
    {1.01146, 77.4396},
    { -47.6336, 97.3934},
    {-38.3406, 234.351},
    {-134.88,198.58},
    {-164.885, 249.684},
	{-225.918, 375.122},
};

// Function to generate random brightness for flickering effect
float getRandomBrightness() {
    return 0.5f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 0.5f)); // Range [0.5, 1.0]
}


void renderStreetlights() {
    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);

    for (size_t i = 0; i < streetlightCoords.size(); ++i) {
        float brightness = getRandomBrightness(); // Generate random brightness for flicker

        // Set light properties
        GLfloat lightColor[] = { brightness, brightness, 0.0f, 1.0f }; // Yellow light
        GLfloat lightPos[] = { streetlightCoords[i].first, 1.0f, streetlightCoords[i].second, 1.0f }; // Position
		GLfloat lightDir[] = { 0.0f, -1.0f, 0.0f }; // Direction

        // Set attenuation (to limit light spread)
        glEnable(GL_LIGHT0 + i);
        glLightfv(GL_LIGHT0 + i, GL_DIFFUSE, lightColor);
        glLightfv(GL_LIGHT0 + i, GL_POSITION, lightPos);
        glLightf(GL_LIGHT0 + i, GL_CONSTANT_ATTENUATION, 0.8f);
        glLightf(GL_LIGHT0 + i, GL_LINEAR_ATTENUATION, 0.2f);
        glLightf(GL_LIGHT0 + i, GL_QUADRATIC_ATTENUATION, 0.1f);
		//glLightf(GL_LIGHT0 + i, GL_SPOT_CUTOFF, 30.0f);            // Cone angle
		//glLightf(GL_LIGHT0 + i, GL_SPOT_EXPONENT, 10.0f);          // Intensity falloff
		glLightfv(GL_LIGHT0 + i, GL_SPOT_DIRECTION, lightDir); // Direction
    }
}

//=======================================================================
// Car Select Screen Functions
//=======================================================================
struct Car {
    std::string name;
    std::string drivetrain;
    int weight;
    int horsepower;
    int performancePoints;
    GLuint textureID; // Texture ID for car image
};

std::vector<Car> cars;
int selectedCarIndex = -1;
int hoverCarIndex = -1;

GLuint loadTexture(const char* path) {
    int width, height, nrChannels;
    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);
    if (!data) {
        std::cerr << "Failed to load texture: " << path << std::endl;
        return 0;
    }

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    GLenum format = GL_RGB;
    if (nrChannels == 4) {
        format = GL_RGBA;
    }

    // Old version for generating mipmaps
    gluBuild2DMipmaps(GL_TEXTURE_2D, format, width, height, format, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return textureID;
}

GLuint backgroundTexture; 


void loadCars() {
    backgroundTexture = loadTexture("textures/background3.jpg");
    cars.push_back({ "Koenigsegg Agera", "SW", 1395, 1160, 1176, loadTexture("textures/koenigsegg2.png") });
    cars.push_back({ "Bugatti Bolide", "DE", 1450, 1578, 1600, loadTexture("textures/bugatti2.png") });

}

void renderCarSelectScreen() {

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, WIDTH, HEIGHT, 0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Draw background
    glBindTexture(GL_TEXTURE_2D, backgroundTexture);
    glEnable(GL_TEXTURE_2D);
    glColor4f(1.0f, 1.0f, 1.0f, 0.5f);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(0, 0);
    glTexCoord2f(1, 0); glVertex2f(WIDTH, 0);
    glTexCoord2f(1, 1); glVertex2f(WIDTH, HEIGHT);
    glTexCoord2f(0, 1); glVertex2f(0, HEIGHT);
    glEnd();
    glDisable(GL_TEXTURE_2D);

    // Draw title
    glColor3f(1.0f, 1.0f, 1.0f);
    std::string title = "Select Your Car";
    int titleWidth = glutBitmapLength(GLUT_BITMAP_HELVETICA_18, (const unsigned char*)title.c_str());
    int titleX = (WIDTH - titleWidth) / 2;
    glRasterPos2i(titleX, 40);
    for (char c : title) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
    }

    // Draw car selection
    int carWidth = 300;
    int carHeight = 150;
    int carSpacing = 50;

    int totalWidth = cars.size() * (carWidth + carSpacing) - carSpacing;
    int x = (WIDTH - totalWidth) / 2;
    int y = HEIGHT / 2 - carHeight / 2;

    for (size_t i = 0; i < cars.size(); ++i) {
        // Draw car image
        glBindTexture(GL_TEXTURE_2D, cars[i].textureID);
        glEnable(GL_TEXTURE_2D);
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2i(x, y);
        glTexCoord2f(1, 0); glVertex2i(x + carWidth, y);
        glTexCoord2f(1, 1); glVertex2i(x + carWidth, y + carHeight);
        glTexCoord2f(0, 1); glVertex2i(x, y + carHeight);
        glEnd();
        glDisable(GL_TEXTURE_2D);

        if (i == selectedCar - 1) {  // Highlight the selected car
            glColor3f(0.0f, 1.0f, 0.0f);  // Green for selected
        }
        else if (i == hoverCarIndex) {
            glColor3f(1.0f, 1.0f, 0.0f);  // Yellow for hover
        }
        else {
            glColor3f(1.0f, 1.0f, 1.0f);  // White for normal
        }
        glLineWidth(2.0f);
        glBegin(GL_LINE_LOOP);
        glVertex2i(x - 5, y - 5);
        glVertex2i(x + carWidth + 5, y - 5);
        glVertex2i(x + carWidth + 5, y + carHeight + 5);
        glVertex2i(x - 5, y + carHeight + 5);
        glEnd();

        // Draw car name
        glColor3f(1.0f, 1.0f, 1.0f);
        int nameWidth = glutBitmapLength(GLUT_BITMAP_HELVETICA_12, (const unsigned char*)cars[i].name.c_str());
        glRasterPos2i(x + (carWidth - nameWidth) / 2, y + carHeight + 20);
        for (char c : cars[i].name) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, c);
        }

        // Draw car stats
        std::string stats = cars[i].drivetrain + " | " + std::to_string(cars[i].weight) + "kg | " + std::to_string(cars[i].horsepower) + "hp | " + std::to_string(cars[i].performancePoints) + "PP";
        int statsWidth = glutBitmapLength(GLUT_BITMAP_HELVETICA_12, (const unsigned char*)stats.c_str());
        glRasterPos2i(x + (carWidth - statsWidth) / 2, y + carHeight + 40);
        for (char c : stats) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, c);
        }

        // Draw selection box
        if (i == selectedCarIndex || i == hoverCarIndex) {
            glColor3f(1.0f, 1.0f, 0.0f);
            glLineWidth(2.0f);
            glBegin(GL_LINE_LOOP);
            glVertex2i(x - 5, y - 5);
            glVertex2i(x + carWidth + 5, y - 5);
            glVertex2i(x + carWidth + 5, y + carHeight + 5);
            glVertex2i(x - 5, y + carHeight + 5);
            glEnd();
        }

        x += carWidth + carSpacing;
    }

    glColor3f(1.0f, 1.0f, 1.0f);
    std::string instructions = "Click on a car to select it. Press Enter to start the game.";
    int instructionsWidth = glutBitmapLength(GLUT_BITMAP_HELVETICA_18, (const unsigned char*)instructions.c_str());
    glRasterPos2i((WIDTH - instructionsWidth) / 2, HEIGHT - 50);
    for (char c : instructions) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
    }

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

}




//=======================================================================
// Collision Functions
//=======================================================================

// Helper function to check if a point is in a triangle (Barycentric method)
bool isPointInTriangle(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b, const glm::vec2& c) {
    glm::vec2 v0 = c - a;
    glm::vec2 v1 = b - a;
    glm::vec2 v2 = p - a;

    float dot00 = glm::dot(v0, v0);
    float dot01 = glm::dot(v0, v1);
    float dot02 = glm::dot(v0, v2);
    float dot11 = glm::dot(v1, v1);
    float dot12 = glm::dot(v1, v2);

    float invDenom = 1.0f / (dot00 * dot11 - dot01 * dot01);
    float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
    float v = (dot00 * dot12 - dot01 * dot02) * invDenom;

    return (u >= 0) && (v >= 0) && (u + v <= 1);
}


struct Vertex {
    float x; // X coordinate
    float y; // Y coordinate
    float z; // Z coordinate

    // Constructor for initialization
    Vertex(float x, float y, float z) : x(x), y(y), z(z) {}

};

std::vector<Vertex> trackVertices = {
    {76.6958, 0, 127.19},
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
    {38.5491, 0, 123.105},
    {79.3444, 0, 143.464}, 
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


std::vector<Vertex> trackVertices2 = {
    {-98.9287, 0, 212.075},
    {-102.207, 0, 213.294},
    {-231.01, 0, 288.671}, 
    {-227.259, 0, 313.454},
    {-200.92, 0, 233.601},
    {-97.4346, 0, 220.877}, 
    {-65.4639, 0, 234.629},
    {-88.3762, 0, 221.856},
    {-220.059, 0, 308.132}, 
    {-84.9432, 0, 230.151}, 
    {-53.0841, 0, -26.9856},
    {-44.3211, 0, -24.9541}, 
    {-35.3845, 0, -24.2557},
    {-26.7367, 0, -21.7646},
    {-18.2824, 0, -18.8081}, 
    {-12.4218, 0, -12.078},
    {-4.62463, 0, -7.61925},
    {0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.046656f},
    {0.00503387f, 0.0f, 0.166655f},
    {0.0223223f, 0.0f, 0.336417f},
    {0.0625273f, 0.0f, 0.557467f},
    {0.139587f, 0.0f, 0.827533f},
    {0.262191f, 0.0f, 1.12546f},
    {0.402447f, 0.0f, 1.46627f},
    {0.576397f, 0.0f, 1.88896f},
    {0.762063f, 0.0f, 2.34013f},
    {0.968037f, 0.0f, 2.84063f},
    {1.19112f, 0.0f, 3.38271f},
    {1.44091f, 0.0f, 3.98969f},
    {1.71528f, 0.0f, 4.6564f},
    {2.06379f, 0.0f, 5.50326f},
    {2.49477f, 0.0f, 6.08002f},
    {2.99758f, 0.0f, 6.75291f},
    {3.4525f, 0.0f, 7.36172f},
    {3.9194f, 0.0f, 7.98655f},
    {4.38031f, 0.0f, 8.60336f},
    {4.65714f, 0.0f, 9.28963f},
    {4.74203f, 0.0f, 10.0751f},
    {4.82047f, 0.0f, 10.8008f},
    {4.90858f, 0.0f, 11.6161f},
    {4.99132f, 0.0f, 12.3816f},
    {5.07406f, 0.0f, 13.1472f},
    {5.16003f, 0.0f, 13.9425f},
    {5.24277f, 0.0f, 14.7081f},
    {5.32551f, 0.0f, 15.4736f},
    {5.40717f, 0.0f, 16.2292f},
    {5.48884f, 0.0f, 16.9848f},
    {5.57265f, 0.0f, 17.7603f},
    {5.67044f, 0.0f, 18.665f},
    {5.75425f, 0.0f, 19.4405f},
    {5.83592f, 0.0f, 20.1961f},
    {5.92081f, 0.0f, 20.9815f},
    {6.00355f, 0.0f, 21.7471f},
    {6.08629f, 0.0f, 22.5126f},
    {6.16903f, 0.0f, 23.2782f},
    {6.25177f, 0.0f, 24.0437f},
    {6.33451f, 0.0f, 24.8092f},
    {6.41832f, 0.0f, 25.5847f},
    {6.50106f, 0.0f, 26.3503f},
    {6.5838f, 0.0f, 27.1158f},
    {6.68266f, 0.0f, 28.0305f},
    {6.78152f, 0.0f, 28.9452f},
    {6.8793f, 0.0f, 29.8499f},
    {6.96204f, 0.0f, 30.6154f},
    {7.04586f, 0.0f, 31.3909f},
    {7.14686f, 0.0f, 32.3255f},
    {7.2296f, 0.0f, 33.091f},
    {7.31557f, 0.0f, 33.8864f},
    {7.39723f, 0.0f, 34.642f},
    {7.47997f, 0.0f, 35.4075f},
    {7.56271f, 0.0f, 36.1731f},
    {7.66049f, 0.0f, 37.0778f},
    {7.74323f, 0.0f, 37.8433f},
    {7.82597f, 0.0f, 38.6089f},
    {7.90657f, 0.0f, 39.3545f},
    {7.98931f, 0.0f, 40.1201f},
    {7.87f, 0.0f, 40.8707f},
    {7.74756f, 0.0f, 41.641f},
    {7.62669f, 0.0f, 42.4014f},
    {7.48227f, 0.0f, 43.31f},
    {7.35826f, 0.0f, 44.0902f},
    {7.23268f, 0.0f, 44.8803f},
    {7.11024f, 0.0f, 45.6507f},
    {6.9878f, 0.0f, 46.421f},
    {6.86379f, 0.0f, 47.2012f},
    {6.74449f, 0.0f, 47.9518f},
    {6.41273f, 0.0f, 48.6687f},
    {6.06837f, 0.0f, 49.4129f},
    {5.67782f, 0.0f, 50.2569f},
    {5.35025f, 0.0f, 50.9648f},
    {5.02689f, 0.0f, 51.6636f},
    {4.69513f, 0.0f, 52.3806f},
    {4.36757f, 0.0f, 53.0885f},
    {4.04001f, 0.0f, 53.7964f},
    {3.71665f, 0.0f, 54.4952f},
    {3.38489f, 0.0f, 55.2122f},
    {2.99854f, 0.0f, 56.0471f},
    {2.67518f, 0.0f, 56.7459f},
    {2.34762f, 0.0f, 57.4538f},
    {2.02006f, 0.0f, 58.1617f},
    {1.6925f, 0.0f, 58.8696f},
    {1.36494f, 0.0f, 59.5775f},
    {1.04157f, 0.0f, 60.2763f},
    {0.66782f, 0.0f, 61.084f},
    {0.273067f, 0.0f, 61.9371f},
    {-0.117486f, 0.0f, 62.7811f},
    {-0.503839f, 0.0f, 63.616f},
    {-0.8272f, 0.0f, 64.3149f},
    {-1.15476f, 0.0f, 65.0227f},
    {-1.60831f, 0.0f, 66.0029f},
    {-1.99046f, 0.0f, 66.8288f},
    {-2.32222f, 0.0f, 67.5457f},
    {-2.64978f, 0.0f, 68.2536f},
    {-3.04033f, 0.0f, 69.0976f},
    {-3.36789f, 0.0f, 69.8055f},
    {-3.90963f, 0.0f, 70.9763f},
    {-4.55635f, 0.0f, 72.3739f},
    {-5.14008f, 0.0f, 73.6354f},
    {-5.58523f, 0.0f, 74.5974f},
    {-6.01357f, 0.0f, 75.5231f},
    {-6.34533f, 0.0f, 76.24f},
    {-6.72329f, 0.0f, 77.0568f},
    {-7.10964f, 0.0f, 77.8918f},
    {-7.50439f, 0.0f, 78.7449f},
    {-7.82776f, 0.0f, 79.4437f},
    {-8.23931f, 0.0f, 80.3331f},
    {-8.74516f, 0.0f, 80.9268f},
    {-9.50183f, 0.0f, 81.3954f},
    {-10.148f, 0.0f, 81.7955f},
    {-10.8196f, 0.0f, 82.2114f},
    {-11.6188f, 0.0f, 82.7063f},
    {-12.2904f, 0.0f, 83.1223f},
    {-12.9451f, 0.0f, 83.5277f},
    {-13.6167f, 0.0f, 83.9436f},
    {-14.2628f, 0.0f, 84.3437f},
    {-15.062f, 0.0f, 84.8386f},
    {-15.7167f, 0.0f, 85.244f},
    {-16.3883f, 0.0f, 85.6599f},
    {-17.0514f, 0.0f, 86.0706f},
    {-17.7231f, 0.0f, 86.4865f},
    {-18.3862f, 0.0f, 86.8972f},
    {-19.0579f, 0.0f, 87.3131f},
    {-19.704f, 0.0f, 87.7132f},
    {-20.3586f, 0.0f, 88.1186f},
    {-21.0871f, 0.0f, 88.3354f},
    {-21.8971f, 0.0f, 88.3406f},
    {-22.6471f, 0.0f, 88.3454f},
    {-23.447f, 0.0f, 88.3506f},
    {-24.217f, 0.0f, 88.3555f},
    {-25.007f, 0.0f, 88.3606f},
    {-25.7055f, 0.0f, 88.186f},
    {-26.4268f, 0.0f, 87.7555f},
    {-26.9693f, 0.0f, 87.1812f},
    {-27.4912f, 0.0f, 86.6287f},
    {-28.0405f, 0.0f, 86.0472f},
    {-28.5762f, 0.0f, 85.4802f},
    {-29.1049f, 0.0f, 84.9204f},
    {-29.4588f, 0.0f, 84.2705f},
    {-29.8413f, 0.0f, 83.5679f},
    {-30.2096f, 0.0f, 82.8917f},
    {-30.6686f, 0.0f, 82.0485f},
    {-31.0464f, 0.0f, 81.3547f},
    {-31.4147f, 0.0f, 80.6785f},
    {-31.8594f, 0.0f, 79.8617f},
    {-32.2324f, 0.0f, 79.1767f},
    {-32.6676f, 0.0f, 78.3775f},
    {-33.1027f, 0.0f, 77.5783f},
    {-33.471f, 0.0f, 76.902f},
    {-33.8296f, 0.0f, 76.2433f},
    {-34.2648f, 0.0f, 75.4441f},
    {-34.7095f, 0.0f, 74.6274f},
    {-35.1495f, 0.0f, 73.8194f},
    {-35.5225f, 0.0f, 73.1343f},
    {-35.8859f, 0.0f, 72.4669f},
    {-36.2637f, 0.0f, 71.7731f},
    {-36.6367f, 0.0f, 71.088f},
    {-37.0814f, 0.0f, 70.2713f},
    {-37.4497f, 0.0f, 69.595f},
    {-37.9404f, 0.0f, 69.0682f},
    {-38.4993f, 0.0f, 68.4682f},
    {-39.0173f, 0.0f, 67.912f},
    {-39.549f, 0.0f, 67.3413f},
    {-40.1624f, 0.0f, 66.6827f},
    {-40.6873f, 0.0f, 66.1193f},
    {-41.2121f, 0.0f, 65.5559f},
    {-41.7437f, 0.0f, 64.9851f},
    {-42.3776f, 0.0f, 64.3046f},
    {-42.9911f, 0.0f, 63.6461f},
    {-43.5295f, 0.0f, 63.068f},
    {-44.0612f, 0.0f, 62.4973f},
    {-44.5792f, 0.0f, 61.9411f},
    {-45.2062f, 0.0f, 61.268f},
    {-45.8401f, 0.0f, 60.5875f},
    {-46.474f, 0.0f, 59.9069f},
    {-46.9988f, 0.0f, 59.3435f},
    {-47.5237f, 0.0f, 58.7801f},
    {-48.1439f, 0.0f, 58.1142f},
    {-48.6824f, 0.0f, 57.5361f},
    {-49.3095f, 0.0f, 56.863f},
    {-49.9502f, 0.0f, 56.1751f},
    {-50.4886f, 0.0f, 55.5971f},
    {-51.1343f, 0.0f, 55.1962f},
    {-51.814f, 0.0f, 54.7743f},
    {-52.6126f, 0.0f, 54.2785f},
    {-53.4028f, 0.0f, 53.788f},
    {-54.0655f, 0.0f, 53.3766f},
    {-54.7627f, 0.0f, 53.1604f},
    {-55.732f, 0.0f, 53.1971f},
    {-56.4748f, 0.0f, 53.4351f},
    {-57.17f, 0.0f, 53.6579f},
    {-58.0937f, 0.0f, 53.9539f},
    {-58.9889f, 0.0f, 54.2408f},
    {-59.8745f, 0.0f, 54.5246f},
    {-60.5982f, 0.0f, 54.7565f},
    {-61.3506f, 0.0f, 54.9976f},
    {-62.0838f, 0.0f, 55.2326f},
    {-62.7214f, 0.0f, 55.6462f},
    {-63.2995f, 0.0f, 56.3747f},
    {-63.7781f, 0.0f, 56.9779f},
    {-64.4494f, 0.0f, 57.8239f},
    {-65.0275f, 0.0f, 58.5524f},
    {-65.5123f, 0.0f, 59.1634f},
    {-65.9909f, 0.0f, 59.7666f},
    {-66.569f, 0.0f, 60.4951f},
    {-67.0538f, 0.0f, 61.1061f},
    {-67.5387f, 0.0f, 61.7171f},
    {-67.8384f, 0.0f, 62.4155f},
    {-67.9406f, 0.0f, 63.1989f},
    {-68.0363f, 0.0f, 63.9327f},
    {-68.1385f, 0.0f, 64.716f},
    {-68.2381f, 0.0f, 65.4796f},
    {-68.348f, 0.0f, 66.3224f},
    {-68.467f, 0.0f, 67.2347f},
    {-68.3562f, 0.0f, 68.0068f},
    {-67.9593f, 0.0f, 68.8145f},
    {-67.6197f, 0.0f, 69.5056f},
    {-67.2184f, 0.0f, 70.3223f},
    {-66.7465f, 0.0f, 71.2827f},
    {-66.3408f, 0.0f, 72.1084f},
    {-65.9262f, 0.0f, 72.952f},
    {-65.5161f, 0.0f, 73.7867f},
    {-65.1147f, 0.0f, 74.6034f},
    {-64.5679f, 0.0f, 75.7163f},
    {-64.0916f, 0.0f, 76.6856f},
    {-63.6814f, 0.0f, 77.5203f},
    {-63.2757f, 0.0f, 78.346f},
    {-62.87f, 0.0f, 79.1717f},
    {-62.451f, 0.0f, 80.0243f},
    {-62.0365f, 0.0f, 80.868f},
    {-61.6263f, 0.0f, 81.7026f},
    {-61.2823f, 0.0f, 82.4027f},
    {-60.881f, 0.0f, 83.2194f},
    {-60.5414f, 0.0f, 83.9105f},
    {-60.193f, 0.0f, 84.6195f},
    {-59.7873f, 0.0f, 85.4452f},
    {-59.4345f, 0.0f, 86.1632f},
    {-59.0287f, 0.0f, 86.9889f},
    {-58.6803f, 0.0f, 87.6979f},
    {-58.3452f, 0.0f, 88.38f},
    {-58.0012f, 0.0f, 89.0801f},
    {-57.666f, 0.0f, 89.7622f},
    {-57.3176f, 0.0f, 90.4712f},
    {-56.9075f, 0.0f, 91.3059f},
    {-56.5679f, 0.0f, 91.997f},
    {-56.2195f, 0.0f, 92.706f},
    {-55.8755f, 0.0f, 93.406f},
    {-55.4698f, 0.0f, 94.2317f},
    {-55.0552f, 0.0f, 95.0754f},
    {-54.6495f, 0.0f, 95.9011f},
    {-54.2437f, 0.0f, 96.7268f},
    {-53.838f, 0.0f, 97.5525f},
    {-53.4455f, 0.0f, 98.3513f},
    {-53.0883f, 0.0f, 99.0782f},
    {-52.6781f, 0.0f, 99.9129f},
    {-52.268f, 0.0f, 100.748f},
    {-51.924f, 0.0f, 101.448f},
    {-51.5139f, 0.0f, 102.282f},
    {-51.1743f, 0.0f, 102.973f},
    {-50.7597f, 0.0f, 103.817f},
    {-50.354f, 0.0f, 104.643f},
    {-49.9483f, 0.0f, 105.468f},
    {-49.6087f, 0.0f, 106.16f},
    {-49.2691f, 0.0f, 106.851f},
    {-48.9251f, 0.0f, 107.551f},
    {-48.5106f, 0.0f, 108.394f},
    {-48.1136f, 0.0f, 109.202f},
    {-47.7167f, 0.0f, 110.01f},
    {-47.1709f, 0.0f, 110.608f},
    {-46.5509f, 0.0f, 111.288f},
    {-45.9309f, 0.0f, 111.968f},
    {-45.412f, 0.0f, 112.537f},
    {-44.8796f, 0.0f, 113.12f},
    {-44.5348f, 0.0f, 113.786f},
    {-44.1671f, 0.0f, 114.497f},
    {-43.8131f, 0.0f, 115.181f},
    {-43.4591f, 0.0f, 115.864f},
    {-43.1097f, 0.0f, 116.539f},
    {-42.6914f, 0.0f, 117.347f},
    {-42.3328f, 0.0f, 118.04f},
    {-41.9788f, 0.0f, 118.724f},
    {-41.6249f, 0.0f, 119.408f},
    {-41.2663f, 0.0f, 120.1f},
    {-40.9123f, 0.0f, 120.784f},
    {-40.5583f, 0.0f, 121.468f},
    {-40.2043f, 0.0f, 122.152f},
    {-39.7814f, 0.0f, 122.969f},
    {-39.3539f, 0.0f, 123.795f},
    {-38.9999f, 0.0f, 124.479f},
    {-38.6413f, 0.0f, 125.171f},
    {-38.2873f, 0.0f, 125.855f},
    {-37.9288f, 0.0f, 126.548f},
    {-37.5702f, 0.0f, 127.241f},
    {-37.2116f, 0.0f, 127.933f},
    {-36.8484f, 0.0f, 128.635f},
    {-36.4255f, 0.0f, 129.452f},
    {-35.9114f, 0.0f, 130.012f},
    {-35.3567f, 0.0f, 130.615f},
    {-34.8358f, 0.0f, 131.183f},
    {-34.2946f, 0.0f, 131.772f},
    {-33.7602f, 0.0f, 132.353f},
    {-33.4154f, 0.0f, 133.008f},
    {-33.0287f, 0.0f, 133.743f},
    {-32.6048f, 0.0f, 134.548f},
    {-32.2507f, 0.0f, 135.22f},
    {-31.8873f, 0.0f, 135.911f},
    {-31.5192f, 0.0f, 136.61f},
    {-31.1605f, 0.0f, 137.291f},
    {-30.7272f, 0.0f, 138.114f},
    {-30.2985f, 0.0f, 138.928f},
    {-29.9398f, 0.0f, 139.609f},
    {-29.5811f, 0.0f, 140.29f},
    {-29.2176f, 0.0f, 140.981f},
    {-28.8589f, 0.0f, 141.662f},
    {-28.4955f, 0.0f, 142.352f},
    {-28.1368f, 0.0f, 143.033f},
    {-27.778f, 0.0f, 143.715f},
    {-27.4193f, 0.0f, 144.396f},
    {-27.0652f, 0.0f, 145.069f},
    {-26.6319f, 0.0f, 145.891f},
    {-26.2033f, 0.0f, 146.706f},
    {-25.7746f, 0.0f, 147.52f},
    {-25.3367f, 0.0f, 148.351f},
    {-24.9034f, 0.0f, 149.174f},
    {-24.5447f, 0.0f, 149.856f},
    {-24.1812f, 0.0f, 150.546f},
    {-23.8178f, 0.0f, 151.236f},
    {-23.4544f, 0.0f, 151.926f},
    {-23.0258f, 0.0f, 152.74f},
    {-22.6671f, 0.0f, 153.421f},
    {-22.3037f, 0.0f, 154.112f},
    {-21.9542f, 0.0f, 154.775f},
    {-21.5908f, 0.0f, 155.465f},
    {-21.1622f, 0.0f, 156.279f},
    {-20.7289f, 0.0f, 157.102f},
    {-20.291f, 0.0f, 157.934f},
    {-19.867f, 0.0f, 158.739f},
    {-19.5036f, 0.0f, 159.429f},
    {-19.0703f, 0.0f, 160.252f},
    {-18.637f, 0.0f, 161.075f},
    {-18.269f, 0.0f, 161.774f},
    {-17.9102f, 0.0f, 162.456f},
    {-17.5468f, 0.0f, 163.146f},
    {-17.1881f, 0.0f, 163.827f},
    {-16.8153f, 0.0f, 164.535f},
    {-16.4473f, 0.0f, 165.234f},
    {-16.0839f, 0.0f, 165.924f},
    {-15.7205f, 0.0f, 166.614f},
    {-15.3617f, 0.0f, 167.296f},
    {-15.003f, 0.0f, 167.977f},
    {-14.5697f, 0.0f, 168.8f},
    {-14.2156f, 0.0f, 169.472f},
    {-13.8569f, 0.0f, 170.154f},
    {-13.4981f, 0.0f, 170.835f},
    {-13.1394f, 0.0f, 171.516f},
    {-12.9736f, 0.0f, 172.248f},
    {-13.0187f, 0.0f, 173.047f},
    {-13.0621f, 0.0f, 173.815f},
    {-13.1055f, 0.0f, 174.584f},
    {-13.15f, 0.0f, 175.373f},
    {-13.1951f, 0.0f, 176.172f},
    {-13.238f, 0.0f, 176.93f},
    {-13.2898f, 0.0f, 177.849f},
    {-13.3327f, 0.0f, 178.608f},
    {-13.375f, 0.0f, 179.356f},
    {-13.4189f, 0.0f, 180.135f},
    {-13.4629f, 0.0f, 180.914f},
    {-13.5069f, 0.0f, 181.693f},
    {-13.5514f, 0.0f, 182.481f},
    {-13.5948f, 0.0f, 183.25f},
    {-13.6376f, 0.0f, 184.009f},
    {-13.6816f, 0.0f, 184.788f},
    {-13.7256f, 0.0f, 185.567f},
    {-13.7678f, 0.0f, 186.315f},
    {-13.8118f, 0.0f, 187.094f},
    {-13.8552f, 0.0f, 187.863f},
    {-13.8986f, 0.0f, 188.632f},
    {-13.942f, 0.0f, 189.4f},
    {-13.9866f, 0.0f, 190.189f},
    {-14.0294f, 0.0f, 190.948f},
    {-14.0728f, 0.0f, 191.717f},
    {-14.3246f, 0.0f, 192.455f},
    {-14.5699f, 0.0f, 193.174f},
    {-14.8152f, 0.0f, 193.894f},
    {-15.0637f, 0.0f, 194.622f},
    {-15.3155f, 0.0f, 195.361f},
    {-15.5641f, 0.0f, 196.089f},
    {-15.9816f, 0.0f, 196.713f},
    {-16.5648f, 0.0f, 197.215f},
    {-17.1479f, 0.0f, 197.718f},
    {-17.7765f, 0.0f, 198.26f},
    {-18.3673f, 0.0f, 198.769f},
    {-18.9656f, 0.0f, 199.285f},
    {-19.3909f, 0.0f, 199.915f},
    {-19.917f, 0.0f, 200.694f},
    {-20.4319f, 0.0f, 201.457f},
    {-20.9579f, 0.0f, 202.236f},
    {-21.4784f, 0.0f, 203.006f},
    {-21.9093f, 0.0f, 203.644f},
    {-22.3459f, 0.0f, 204.291f},
    {-22.7824f, 0.0f, 204.937f},
    {-23.2133f, 0.0f, 205.575f},
    {-23.6386f, 0.0f, 206.205f},
    {-24.0752f, 0.0f, 206.852f},
    {-24.5173f, 0.0f, 207.506f},
    {-24.9538f, 0.0f, 208.153f},
    {-25.3903f, 0.0f, 208.799f},
    {-25.8213f, 0.0f, 209.437f},
    {-26.3361f, 0.0f, 210.2f},
    {-26.7727f, 0.0f, 210.846f},
    {-27.2092f, 0.0f, 211.493f},
    {-27.7941f, 0.0f, 211.993f},
    {-28.3866f, 0.0f, 212.501f},
    {-29.0931f, 0.0f, 213.105f},
    {-29.678f, 0.0f, 213.606f},
    {-30.2629f, 0.0f, 214.107f},
    {-30.8478f, 0.0f, 214.608f},
    {-31.4403f, 0.0f, 215.115f},
    {-32.0328f, 0.0f, 215.622f},
    {-32.7165f, 0.0f, 216.208f},
    {-33.4002f, 0.0f, 216.793f},
    {-33.8277f, 0.0f, 217.421f},
    {-34.2721f, 0.0f, 218.074f},
    {-34.6996f, 0.0f, 218.703f},
    {-35.1384f, 0.0f, 219.348f},
    {-35.5884f, 0.0f, 220.009f},
    {-36.1059f, 0.0f, 220.77f},
    {-36.5391f, 0.0f, 221.406f},
    {-37.0979f, 0.0f, 221.892f},
    {-37.717f, 0.0f, 222.429f},
    {-38.4117f, 0.0f, 223.032f},
    {-39.0988f, 0.0f, 223.629f},
    {-39.8085f, 0.0f, 224.245f},
    {-40.3824f, 0.0f, 224.744f},
    {-40.9638f, 0.0f, 225.248f},
    {-41.5528f, 0.0f, 225.76f},
    {-42.1417f, 0.0f, 226.271f},
    {-42.8213f, 0.0f, 226.861f},
    {-43.4027f, 0.0f, 227.366f},
    {-44.0973f, 0.0f, 227.969f},
    {-44.6787f, 0.0f, 228.474f},
    {-45.2677f, 0.0f, 228.986f},
    {-45.9699f, 0.0f, 229.595f},
    {-46.5438f, 0.0f, 230.094f},
    {-47.1252f, 0.0f, 230.599f},
    {-47.7066f, 0.0f, 231.103f},
    {-48.288f, 0.0f, 231.608f},
    {-48.8618f, 0.0f, 232.107f},
    {-49.4508f, 0.0f, 232.618f},
    {-50.0322f, 0.0f, 233.123f},
    {-50.606f, 0.0f, 233.621f},
    {-51.1874f, 0.0f, 234.126f},
    {-51.7688f, 0.0f, 234.631f},
    {-52.4635f, 0.0f, 235.234f},
    {-53.0524f, 0.0f, 235.745f},
    {-53.6414f, 0.0f, 236.257f},
    {-54.3512f, 0.0f, 236.873f},
    {-54.9326f, 0.0f, 237.378f},
    {-55.6348f, 0.0f, 237.988f},
    {-56.3219f, 0.0f, 238.584f},
    {-56.9108f, 0.0f, 239.096f},
    {-57.6433f, 0.0f, 239.732f},
    {-58.3304f, 0.0f, 240.328f},
    {-59.0326f, 0.0f, 240.938f},
    {-59.6215f, 0.0f, 241.449f},
    {-60.1878f, 0.0f, 241.941f},
    {-60.627f, 0.0f, 242.622f},
    {-61.1095f, 0.0f, 243.37f},
    {-61.527f, 0.0f, 244.017f},
    {-61.9445f, 0.0f, 244.664f},
    {-62.3674f, 0.0f, 245.319f},
    {-62.7794f, 0.0f, 245.958f},
    {-63.1969f, 0.0f, 246.605f},
    {-63.6957f, 0.0f, 247.378f},
    {-64.1131f, 0.0f, 248.025f},
    {-64.6174f, 0.0f, 248.806f},
    {-65.1916f, 0.0f, 249.319f},
    {-65.8815f, 0.0f, 249.661f},
    {-66.5714f, 0.0f, 250.003f},
    {-67.3867f, 0.0f, 250.407f},
    {-68.0766f, 0.0f, 250.749f},
    {-68.883f, 0.0f, 251.149f},
    {-69.5997f, 0.0f, 251.504f},
    {-70.2807f, 0.0f, 251.842f},
    {-70.9795f, 0.0f, 252.188f},
    {-71.6604f, 0.0f, 252.526f},
    {-72.4668f, 0.0f, 252.926f},
    {-73.1656f, 0.0f, 253.272f},
    {-73.8824f, 0.0f, 253.627f},
    {-74.5633f, 0.0f, 253.965f},
    {-75.278f, 0.0f, 254.114f},
    {-76.1438f, 0.0f, 254.028f},
    {-76.9001f, 0.0f, 253.954f},
    {-77.7261f, 0.0f, 253.872f},
    {-78.4924f, 0.0f, 253.797f},
    {-79.2587f, 0.0f, 253.721f},
    {-80.025f, 0.0f, 253.646f},
    {-80.7913f, 0.0f, 253.57f},
    {-81.5183f, 0.0f, 253.288f},
    {-82.236f, 0.0f, 253.009f},
    {-82.9631f, 0.0f, 252.726f},
    {-83.6902f, 0.0f, 252.444f},
    {-84.4172f, 0.0f, 252.161f},
    {-85.0287f, 0.0f, 251.71f},
    {-85.5165f, 0.0f, 251.076f},
    {-85.9738f, 0.0f, 250.482f},
    {-86.4554f, 0.0f, 249.855f},
    {-86.9249f, 0.0f, 249.245f},
    {-87.4919f, 0.0f, 248.508f},
    {-87.9675f, 0.0f, 247.89f},
    {-88.437f, 0.0f, 247.279f},
    {-88.9126f, 0.0f, 246.661f},
    {-89.382f, 0.0f, 246.051f},
    {-89.943f, 0.0f, 245.322f},
    {-90.4125f, 0.0f, 244.711f},
    {-90.8819f, 0.0f, 244.101f},
    {-91.3575f, 0.0f, 243.483f},
    {-91.827f, 0.0f, 242.872f},
    {-92.2965f, 0.0f, 242.262f},
    {-92.772f, 0.0f, 241.644f},
    {-93.2415f, 0.0f, 241.033f},
    {-93.711f, 0.0f, 240.423f},
    {-94.1744f, 0.0f, 239.821f},
    {-94.6439f, 0.0f, 239.21f},
    {-95.1194f, 0.0f, 238.592f},
    {-95.595f, 0.0f, 237.974f},
    {-96.0645f, 0.0f, 237.364f},
    {-96.6254f, 0.0f, 236.634f},
    {-97.0949f, 0.0f, 236.024f},
    {-97.5583f, 0.0f, 235.422f},
    {-98.0339f, 0.0f, 234.803f},
    {-98.5033f, 0.0f, 234.193f},
    {-99.0582f, 0.0f, 233.472f},
    {-99.5337f, 0.0f, 232.854f},
    {-100.003f, 0.0f, 232.243f},
    {-100.473f, 0.0f, 231.633f},
    {-100.936f, 0.0f, 231.031f},
    {-101.412f, 0.0f, 230.412f},
    {-101.887f, 0.0f, 229.794f},
    {-102.357f, 0.0f, 229.184f},
    {-102.838f, 0.0f, 228.558f},
    {-103.314f, 0.0f, 227.939f},
    {-103.777f, 0.0f, 227.337f},
    {-104.338f, 0.0f, 226.608f},
    {-104.82f, 0.0f, 225.982f},
    {-105.277f, 0.0f, 225.387f},
    {-105.759f, 0.0f, 224.761f},
    {-106.314f, 0.0f, 224.04f},
    {-106.789f, 0.0f, 223.421f},
    {-107.271f, 0.0f, 222.795f},
    {-107.753f, 0.0f, 222.169f},
    {-108.326f, 0.0f, 221.424f},
    {-108.807f, 0.0f, 220.798f},
    {-109.289f, 0.0f, 220.172f},
    {-109.777f, 0.0f, 219.538f},
    {-110.252f, 0.0f, 218.919f},
    {-110.716f, 0.0f, 218.317f},
    {-111.179f, 0.0f, 217.715f},
    {-111.649f, 0.0f, 217.104f},
    {-112.21f, 0.0f, 216.375f},
    {-112.758f, 0.0f, 215.662f},
    {-113.319f, 0.0f, 214.932f},
    {-113.899f, 0.0f, 214.179f},
    {-114.466f, 0.0f, 213.442f},
    {-115.027f, 0.0f, 212.713f},
    {-115.502f, 0.0f, 212.095f},
    {-115.965f, 0.0f, 211.492f},
    {-116.429f, 0.0f, 210.89f},
    {-116.904f, 0.0f, 210.272f},
    {-117.374f, 0.0f, 209.661f},
    {-117.849f, 0.0f, 209.043f},
    {-118.319f, 0.0f, 208.433f},
    {-118.795f, 0.0f, 207.815f},
    {-119.258f, 0.0f, 207.212f},
    {-119.734f, 0.0f, 206.594f},
    {-120.209f, 0.0f, 205.976f},
    {-120.672f, 0.0f, 205.373f},
    {-121.233f, 0.0f, 204.644f},
    {-121.703f, 0.0f, 204.034f},
    {-122.27f, 0.0f, 203.297f},
    {-122.739f, 0.0f, 202.686f},
    {-123.215f, 0.0f, 202.068f},
    {-123.684f, 0.0f, 201.458f},
    {-124.154f, 0.0f, 200.848f},
    {-124.721f, 0.0f, 200.11f},
    {-125.197f, 0.0f, 199.492f},
    {-125.666f, 0.0f, 198.882f},
    {-126.221f, 0.0f, 198.161f},
    {-126.703f, 0.0f, 197.534f},
    {-127.172f, 0.0f, 196.924f},
    {-127.733f, 0.0f, 196.195f},
    {-128.215f, 0.0f, 195.569f},
    {-128.696f, 0.0f, 194.942f},
    {-129.172f, 0.0f, 194.324f},
    {-129.641f, 0.0f, 193.714f},
    {-130.208f, 0.0f, 192.977f},
    {-130.769f, 0.0f, 192.248f},
    {-131.342f, 0.0f, 191.502f},
    {-131.818f, 0.0f, 190.884f},
    {-132.391f, 0.0f, 190.139f},
    {-132.855f, 0.0f, 189.537f},
    {-133.415f, 0.0f, 188.808f},
    {-133.891f, 0.0f, 188.189f},
    {-134.36f, 0.0f, 187.579f},
    {-134.934f, 0.0f, 186.834f},
    {-135.519f, 0.0f, 186.073f},
    {-136.098f, 0.0f, 185.32f},
    {-136.586f, 0.0f, 184.686f},
    {-137.171f, 0.0f, 183.925f},
    {-137.744f, 0.0f, 183.18f},
    {-138.318f, 0.0f, 182.435f},
    {-138.805f, 0.0f, 181.801f},
    {-139.378f, 0.0f, 181.056f},
    {-139.848f, 0.0f, 180.445f},
    {-140.348f, 0.0f, 179.795f},
    {-140.811f, 0.0f, 179.193f},
    {-141.372f, 0.0f, 178.464f},
    {-142.065f, 0.0f, 177.971f},
    {-142.749f, 0.0f, 177.484f},
    {-143.515f, 0.0f, 176.939f},
    {-144.281f, 0.0f, 176.394f},
    {-144.917f, 0.0f, 175.942f},
    {-145.536f, 0.0f, 175.501f},
    {-146.163f, 0.0f, 175.055f},
    {-146.799f, 0.0f, 174.603f},
    {-147.426f, 0.0f, 174.156f},
    {-148.054f, 0.0f, 173.71f},
    {-148.917f, 0.0f, 173.096f},
    {-149.713f, 0.0f, 172.827f},
    {-150.528f, 0.0f, 172.552f},
    {-151.41f, 0.0f, 172.255f},
    {-152.149f, 0.0f, 172.006f},
    {-152.869f, 0.0f, 171.763f},
    {-153.608f, 0.0f, 171.514f},
    {-154.347f, 0.0f, 171.265f},
    {-155.077f, 0.0f, 171.019f},
    {-155.797f, 0.0f, 170.776f},
    {-156.527f, 0.0f, 170.53f},
    {-157.417f, 0.0f, 170.23f},
    {-158.166f, 0.0f, 169.977f},
    {-159.047f, 0.0f, 169.68f},
    {-159.938f, 0.0f, 169.38f},
    {-160.819f, 0.0f, 169.083f},
    {-161.7f, 0.0f, 168.786f},
    {-162.582f, 0.0f, 168.489f},
    {-163.472f, 0.0f, 168.188f},
    {-164.322f, 0.0f, 168.164f},
    {-165.142f, 0.0f, 168.389f},
    {-165.903f, 0.0f, 168.599f},
    {-166.781f, 0.0f, 168.84f},
    {-167.543f, 0.0f, 169.049f},
    {-168.295f, 0.0f, 169.256f},
    {-169.182f, 0.0f, 169.5f},
    {-169.924f, 0.0f, 169.704f},
    {-170.811f, 0.0f, 169.948f},
    {-171.718f, 0.0f, 170.197f},
    {-172.615f, 0.0f, 170.443f},
    {-173.492f, 0.0f, 170.685f},
    {-174.234f, 0.0f, 170.889f},
    {-174.987f, 0.0f, 171.095f},
    {-175.719f, 0.0f, 171.297f},
    {-176.462f, 0.0f, 171.501f},
    {-177.339f, 0.0f, 171.742f},
    {-178.072f, 0.0f, 171.944f},
    {-178.824f, 0.0f, 172.15f},
    {-179.557f, 0.0f, 172.352f},
    {-180.309f, 0.0f, 172.558f},
    {-181.071f, 0.0f, 172.768f},
    {-181.823f, 0.0f, 172.974f},
    {-182.575f, 0.0f, 173.181f},
    {-183.337f, 0.0f, 173.391f},
    {-184.108f, 0.0f, 173.603f},
    {-184.86f, 0.0f, 173.809f},
    {-185.612f, 0.0f, 174.016f},
    {-186.374f, 0.0f, 174.225f},
    {-187.126f, 0.0f, 174.432f},
    {-187.809f, 0.0f, 174.849f},
    {-188.395f, 0.0f, 175.532f},
    {-188.884f, 0.0f, 176.101f},
    {-189.405f, 0.0f, 176.708f},
    {-189.906f, 0.0f, 177.292f},
    {-190.421f, 0.0f, 177.892f},
    {-191.026f, 0.0f, 178.598f},
    {-191.534f, 0.0f, 179.19f},
    {-192.036f, 0.0f, 179.774f},
    {-192.537f, 0.0f, 180.358f},
    {-193.038f, 0.0f, 180.943f},
    {-193.553f, 0.0f, 181.542f},
    {-194.061f, 0.0f, 182.134f},
    {-194.569f, 0.0f, 182.726f},
    {-195.07f, 0.0f, 183.31f},
    {-195.676f, 0.0f, 184.016f},
    {-196.177f, 0.0f, 184.601f},
    {-196.679f, 0.0f, 185.185f},
    {-197.18f, 0.0f, 185.769f},
    {-197.681f, 0.0f, 186.354f},
    {-198.002f, 0.0f, 187.01f},
    {-198.139f, 0.0f, 187.818f},
    {-198.269f, 0.0f, 188.587f},
    {-198.423f, 0.0f, 189.494f},
    {-198.552f, 0.0f, 190.253f},
    {-198.683f, 0.0f, 191.022f},
    {-198.813f, 0.0f, 191.791f},
    {-198.944f, 0.0f, 192.56f},
    {-199.073f, 0.0f, 193.319f},
    {-199.203f, 0.0f, 194.088f},
    {-199.334f, 0.0f, 194.857f},
    {-199.479f, 0.0f, 195.715f},
    {-199.606f, 0.0f, 196.465f},
    {-199.735f, 0.0f, 197.224f},
    {-199.864f, 0.0f, 197.983f},
    {-199.993f, 0.0f, 198.742f},
    {-200.147f, 0.0f, 199.649f},
    {-200.275f, 0.0f, 200.408f},
    {-200.404f, 0.0f, 201.167f},
    {-200.533f, 0.0f, 201.926f},
    {-200.662f, 0.0f, 202.686f},
    {-200.792f, 0.0f, 203.455f},
    {-200.921f, 0.0f, 204.214f},
    {-201.05f, 0.0f, 204.973f},
    {-201.176f, 0.0f, 205.712f},
    {-201.331f, 0.0f, 206.629f},
    {-201.265f, 0.0f, 207.366f},
    {-200.963f, 0.0f, 208.129f},
    {-200.684f, 0.0f, 208.836f},
    {-200.39f, 0.0f, 209.58f},
    {-200.103f, 0.0f, 210.305f},
    {-199.754f, 0.0f, 211.188f},
    {-199.474f, 0.0f, 211.895f},
    {-199.184f, 0.0f, 212.63f},
    {-198.893f, 0.0f, 213.365f},
    {-198.61f, 0.0f, 214.081f},
    {-198.324f, 0.0f, 214.806f},
    {-198.048f, 0.0f, 215.503f},
    {-197.754f, 0.0f, 216.247f},
    {-197.463f, 0.0f, 216.982f},
    {-197.177f, 0.0f, 217.708f},
    {-196.838f, 0.0f, 218.563f},
    {-196.555f, 0.0f, 219.279f},
    {-196.269f, 0.0f, 220.005f},
    {-195.806f, 0.0f, 220.62f},
    {-195.343f, 0.0f, 221.235f},
    {-194.879f, 0.0f, 221.85f},
    {-194.422f, 0.0f, 222.457f},
    {-193.965f, 0.0f, 223.065f},
    {-193.406f, 0.0f, 223.808f},
    {-192.943f, 0.0f, 224.423f},
    {-192.474f, 0.0f, 225.046f},
    {-192.011f, 0.0f, 225.661f},
    {-191.542f, 0.0f, 226.284f},
    {-191.073f, 0.0f, 226.908f},
    {-190.61f, 0.0f, 227.523f},
    {-190.141f, 0.0f, 228.146f},
    {-189.678f, 0.0f, 228.761f},
    {-189.208f, 0.0f, 229.384f},
    {-188.745f, 0.0f, 230.0f},
    {-188.282f, 0.0f, 230.615f},
    {-187.813f, 0.0f, 231.238f},
    {-187.35f, 0.0f, 231.853f},
    {-186.833f, 0.0f, 232.54f},
    {-186.37f, 0.0f, 233.156f},
    {-185.907f, 0.0f, 233.771f},
    {-185.45f, 0.0f, 234.378f},
    {-184.981f, 0.0f, 235.001f},
    {-184.428f, 0.0f, 235.736f},
    {-183.97f, 0.0f, 236.343f},
    {-183.501f, 0.0f, 236.967f},
    {-183.032f, 0.0f, 237.59f},
    {-182.575f, 0.0f, 238.197f},
    {-182.112f, 0.0f, 238.812f},
    {-181.649f, 0.0f, 239.427f},
    {-181.18f, 0.0f, 240.051f},
    {-180.717f, 0.0f, 240.666f},
    {-180.26f, 0.0f, 241.273f},
    {-179.701f, 0.0f, 242.016f},
    {-179.244f, 0.0f, 242.623f},
    {-178.775f, 0.0f, 243.246f},
    {-178.311f, 0.0f, 243.862f},
    {-177.758f, 0.0f, 244.597f},
    {-177.295f, 0.0f, 245.212f},
    {-176.742f, 0.0f, 245.947f},
    {-176.273f, 0.0f, 246.57f},
    {-175.81f, 0.0f, 247.185f},
    {-175.347f, 0.0f, 247.801f},
    {-174.884f, 0.0f, 248.416f},
    {-174.421f, 0.0f, 249.031f},
    {-173.861f, 0.0f, 249.774f},
    {-173.398f, 0.0f, 250.389f},
    {-172.941f, 0.0f, 250.996f},
    {-172.376f, 0.0f, 251.747f},
    {-171.907f, 0.0f, 252.371f},
    {-171.432f, 0.0f, 253.002f},
    {-170.951f, 0.0f, 253.641f},
    {-170.482f, 0.0f, 254.264f},
    {-169.928f, 0.0f, 254.999f},
    {-169.465f, 0.0f, 255.614f},
    {-168.99f, 0.0f, 256.246f},
    {-168.509f, 0.0f, 256.885f},
    {-167.962f, 0.0f, 257.612f},
    {-167.396f, 0.0f, 258.363f},
    {-166.927f, 0.0f, 258.986f},
    {-166.458f, 0.0f, 259.609f},
    {-165.995f, 0.0f, 260.224f},
    {-165.526f, 0.0f, 260.848f},
    {-165.051f, 0.0f, 261.479f},
    {-164.582f, 0.0f, 262.102f},
    {-164.119f, 0.0f, 262.717f},
    {-163.65f, 0.0f, 263.34f},
    {-163.187f, 0.0f, 263.956f},
    {-162.718f, 0.0f, 264.579f},
    {-162.255f, 0.0f, 265.194f},
    {-161.786f, 0.0f, 265.817f},
    {-161.323f, 0.0f, 266.432f},
    {-160.859f, 0.0f, 267.048f},
    {-160.396f, 0.0f, 267.663f},
    {-159.933f, 0.0f, 268.278f},
    {-159.47f, 0.0f, 268.893f},
    {-159.001f, 0.0f, 269.516f},
    {-158.448f, 0.0f, 270.251f},
    {-157.985f, 0.0f, 270.867f},
    {-157.426f, 0.0f, 271.61f},
    {-156.969f, 0.0f, 272.217f},
    {-156.499f, 0.0f, 272.84f},
    {-155.952f, 0.0f, 273.567f},
    {-155.393f, 0.0f, 274.31f},
    {-154.93f, 0.0f, 274.925f},
    {-154.467f, 0.0f, 275.541f},
    {-154.186f, 0.0f, 276.236f},
    {-154.129f, 0.0f, 277.154f},
    {-154.082f, 0.0f, 277.903f},
    {-154.033f, 0.0f, 278.701f},
    {-153.985f, 0.0f, 279.47f},
    {-153.927f, 0.0f, 280.398f},
    {-154.071f, 0.0f, 281.124f},
    {-154.229f, 0.0f, 281.918f},
    {-154.388f, 0.0f, 282.723f},
    {-154.569f, 0.0f, 283.635f},
    {-154.747f, 0.0f, 284.527f},
    {-155.092f, 0.0f, 285.216f},
    {-155.441f, 0.0f, 285.913f},
    {-155.791f, 0.0f, 286.61f},
    {-156.199f, 0.0f, 287.424f},
    {-156.549f, 0.0f, 288.121f},
    {-156.898f, 0.0f, 288.818f},
    {-157.397f, 0.0f, 289.379f},
    {-157.928f, 0.0f, 289.977f},
    {-158.44f, 0.0f, 290.552f},
    {-158.951f, 0.0f, 291.128f},
    {-159.463f, 0.0f, 291.703f},
    {-160.08f, 0.0f, 292.398f},
    {-160.585f, 0.0f, 292.966f},
    {-161.19f, 0.0f, 293.647f},
    {-161.715f, 0.0f, 294.237f},
    {-162.24f, 0.0f, 294.828f},
    {-162.771f, 0.0f, 295.426f},
    {-163.302f, 0.0f, 296.023f},
    {-163.634f, 0.0f, 296.674f},
    {-164.025f, 0.0f, 297.44f},
    {-164.384f, 0.0f, 298.143f},
    {-164.739f, 0.0f, 298.838f},
    {-165.161f, 0.0f, 299.667f},
    {-165.529f, 0.0f, 300.388f},
    {-165.888f, 0.0f, 301.092f},
    {-166.238f, 0.0f, 301.778f},
    {-166.584f, 0.0f, 302.455f},
    {-166.997f, 0.0f, 303.265f},
    {-167.415f, 0.0f, 304.085f},
    {-167.765f, 0.0f, 304.771f},
    {-168.115f, 0.0f, 305.457f},
    {-168.47f, 0.0f, 306.151f},
    {-168.82f, 0.0f, 306.837f},
    {-169.238f, 0.0f, 307.657f},
    {-169.592f, 0.0f, 308.352f},
    {-169.956f, 0.0f, 309.064f},
    {-170.311f, 0.0f, 309.759f},
    {-170.665f, 0.0f, 310.454f},
    {-171.019f, 0.0f, 311.149f},
    {-171.374f, 0.0f, 311.843f},
    {-171.733f, 0.0f, 312.547f},
    {-172.088f, 0.0f, 313.242f},
    {-172.437f, 0.0f, 313.928f},
    {-172.792f, 0.0f, 314.622f},
    {-173.215f, 0.0f, 315.451f},
    {-173.56f, 0.0f, 316.128f},
    {-173.915f, 0.0f, 316.823f},
    {-174.269f, 0.0f, 317.517f},
    {-174.421f, 0.0f, 318.313f},
    {-174.588f, 0.0f, 319.187f},
    {-174.735f, 0.0f, 319.953f},
    {-174.882f, 0.0f, 320.719f},
    {-175.054f, 0.0f, 321.623f},
    {-175.199f, 0.0f, 322.379f},
    {-175.141f, 0.0f, 323.137f},
    {-175.081f, 0.0f, 323.925f},
    {-175.022f, 0.0f, 324.693f},
    {-174.965f, 0.0f, 325.45f},
    {-174.905f, 0.0f, 326.228f},
    {-174.846f, 0.0f, 327.006f},
    {-174.786f, 0.0f, 327.784f},
    {-174.716f, 0.0f, 328.701f},
    {-174.658f, 0.0f, 329.459f},
    {-174.6f, 0.0f, 330.226f},
    {-174.541f, 0.0f, 330.994f},
    {-174.482f, 0.0f, 331.772f},
    {-174.239f, 0.0f, 332.471f},
    {-173.969f, 0.0f, 333.245f},
    {-173.713f, 0.0f, 333.982f},
    {-173.411f, 0.0f, 334.851f},
    {-173.155f, 0.0f, 335.588f},
    {-172.906f, 0.0f, 336.306f},
    {-172.656f, 0.0f, 337.024f},
    {-172.403f, 0.0f, 337.751f},
    {-172.157f, 0.0f, 338.459f},
    {-171.904f, 0.0f, 339.187f},
    {-171.651f, 0.0f, 339.914f},
    {-171.389f, 0.0f, 340.67f},
    {-171.132f, 0.0f, 341.406f},
    {-170.886f, 0.0f, 342.115f},
    {-170.633f, 0.0f, 342.842f},
    {-170.371f, 0.0f, 343.598f},
    {-170.118f, 0.0f, 344.325f},
    {-169.858f, 0.0f, 345.071f},
    {-169.606f, 0.0f, 345.799f},
    {-169.346f, 0.0f, 346.545f},
    {-169.034f, 0.0f, 347.442f},
    {-168.775f, 0.0f, 348.188f},
    {-168.525f, 0.0f, 348.906f},
    {-168.269f, 0.0f, 349.643f},
    {-168.016f, 0.0f, 350.37f},
    {-167.76f, 0.0f, 351.107f},
    {-167.507f, 0.0f, 351.834f},
    {-167.205f, 0.0f, 352.703f},
    {-166.953f, 0.0f, 353.431f},
    {-166.7f, 0.0f, 354.158f},
    {-166.447f, 0.0f, 354.885f},
    {-166.191f, 0.0f, 355.622f},
    {-165.938f, 0.0f, 356.349f},
    {-165.633f, 0.0f, 357.228f},
    {-165.376f, 0.0f, 357.964f},
    {-165.124f, 0.0f, 358.692f},
    {-164.822f, 0.0f, 359.561f},
    {-164.565f, 0.0f, 360.297f},
    {-164.309f, 0.0f, 361.034f},
    {-164.056f, 0.0f, 361.762f},
    {-163.804f, 0.0f, 362.489f},
    {-163.548f, 0.0f, 363.226f},
    {-163.295f, 0.0f, 363.953f},
    {-163.042f, 0.0f, 364.68f},
    {-162.733f, 0.0f, 365.568f},
    {-162.48f, 0.0f, 366.295f},
    {-162.231f, 0.0f, 367.013f},
    {-161.942f, 0.0f, 367.844f},
    {-161.886f, 0.0f, 368.582f},
    {-161.825f, 0.0f, 369.37f},
    {-161.766f, 0.0f, 370.148f},
    {-161.706f, 0.0f, 370.935f},
    {-161.646f, 0.0f, 371.713f},
    {-161.794f, 0.0f, 372.469f},
    {-162.15f, 0.0f, 373.174f},
    {-162.493f, 0.0f, 373.852f},
    {-162.85f, 0.0f, 374.557f},
    {-163.197f, 0.0f, 375.244f},
    {-163.54f, 0.0f, 375.922f},
    {-163.893f, 0.0f, 376.618f},
    {-164.308f, 0.0f, 377.439f},
    {-164.66f, 0.0f, 378.135f},
    {-165.075f, 0.0f, 378.956f},
    {-165.418f, 0.0f, 379.634f},
    {-165.766f, 0.0f, 380.322f},
    {-166.185f, 0.0f, 381.151f},
    {-166.538f, 0.0f, 381.847f},
    {-166.881f, 0.0f, 382.526f},
    {-167.228f, 0.0f, 383.213f},
    {-167.639f, 0.0f, 384.025f},
    {-167.991f, 0.0f, 384.721f},
    {-168.338f, 0.0f, 385.408f},
    {-168.749f, 0.0f, 386.22f},
    {-169.169f, 0.0f, 387.05f},
    {-169.575f, 0.0f, 387.853f},
    {-169.999f, 0.0f, 388.692f},
    {-170.347f, 0.0f, 389.379f},
    {-170.762f, 0.0f, 390.2f},
    {-171.25f, 0.0f, 391.164f},
    {-171.67f, 0.0f, 391.993f},
    {-172.08f, 0.0f, 392.805f},
    {-172.5f, 0.0f, 393.635f},
    {-172.979f, 0.0f, 394.581f},
    {-173.585f, 0.0f, 395.205f},
    {-174.413f, 0.0f, 395.629f},
    {-175.133f, 0.0f, 395.999f},
    {-175.925f, 0.0f, 396.405f},
    {-176.761f, 0.0f, 396.835f},
    {-177.713f, 0.0f, 397.323f},
    {-178.665f, 0.0f, 397.812f},
    {-179.484f, 0.0f, 398.232f},
    {-180.302f, 0.0f, 398.652f},
    {-181.108f, 0.0f, 398.805f},
    {-181.972f, 0.0f, 398.97f},
    {-182.886f, 0.0f, 399.143f},
    {-183.652f, 0.0f, 399.289f},
    {-184.556f, 0.0f, 399.461f},
    {-185.506f, 0.0f, 399.32f},
    {-186.312f, 0.0f, 398.92f},
    {-187.037f, 0.0f, 398.559f},
    {-187.834f, 0.0f, 398.163f},
    {-188.658f, 0.0f, 397.753f},
    {-189.356f, 0.0f, 397.406f},
    {-190.055f, 0.0f, 397.059f},
    {-190.879f, 0.0f, 396.65f},
    {-191.568f, 0.0f, 396.307f},
    {-192.401f, 0.0f, 395.893f},
    {-193.359f, 0.0f, 395.417f},
    {-194.174f, 0.0f, 395.011f},
    {-195.025f, 0.0f, 394.589f},
    {-195.667f, 0.0f, 393.959f},
    {-196.453f, 0.0f, 393.188f},
    {-197.11f, 0.0f, 392.544f},
    {-197.774f, 0.0f, 391.893f},
    {-198.431f, 0.0f, 391.249f},
    {-199.088f, 0.0f, 390.605f},
    {-199.745f, 0.0f, 389.961f},
    {-200.516f, 0.0f, 389.205f},
    {-201.173f, 0.0f, 388.561f},
    {-201.837f, 0.0f, 387.91f},
    {-202.487f, 0.0f, 387.273f},
    {-203.044f, 0.0f, 386.727f},
    {-203.594f, 0.0f, 386.188f},
    {-204.143f, 0.0f, 385.649f},
    {-204.808f, 0.0f, 384.998f},
    {-205.572f, 0.0f, 384.249f},
    {-206.229f, 0.0f, 383.605f},
    {-206.907f, 0.0f, 382.94f},
    {-207.578f, 0.0f, 382.282f},
    {-208.15f, 0.0f, 381.722f},
    {-208.714f, 0.0f, 381.169f},
    {-209.285f, 0.0f, 380.609f},
    {-209.963f, 0.0f, 379.943f},
    {-210.527f, 0.0f, 379.39f},
    {-211.092f, 0.0f, 378.837f},
    {-211.656f, 0.0f, 378.284f},
    {-212.198f, 0.0f, 377.752f},
    {-212.855f, 0.0f, 377.108f},
    {-213.412f, 0.0f, 376.562f},
    {-213.969f, 0.0f, 376.016f},
    {-214.526f, 0.0f, 375.47f},
    {-215.069f, 0.0f, 374.938f},
    {-215.619f, 0.0f, 374.399f},
    {-216.276f, 0.0f, 373.755f},
    {-216.826f, 0.0f, 373.216f},
    {-217.376f, 0.0f, 372.677f},
    {-217.99f, 0.0f, 372.075f},
    {-218.547f, 0.0f, 371.529f},
    {-219.111f, 0.0f, 370.976f},
    {-219.668f, 0.0f, 370.43f},
    {-220.211f, 0.0f, 369.898f},
    {-220.76f, 0.0f, 369.359f},
    {-221.31f, 0.0f, 368.82f},
    {-221.867f, 0.0f, 368.274f},
    {-222.51f, 0.0f, 367.644f},
    {-222.891f, 0.0f, 366.998f},
    {-223.303f, 0.0f, 366.3f},
    {-223.695f, 0.0f, 365.637f},
    {-224.081f, 0.0f, 364.983f},
    {-224.478f, 0.0f, 364.311f},
    {-224.87f, 0.0f, 363.648f},
    {-225.261f, 0.0f, 362.985f},
    {-225.653f, 0.0f, 362.322f},
    {-226.039f, 0.0f, 361.668f},
    {-226.512f, 0.0f, 360.867f},
    {-226.899f, 0.0f, 360.213f},
    {-227.285f, 0.0f, 359.559f},
    {-227.677f, 0.0f, 358.896f},
    {-228.069f, 0.0f, 358.233f},
    {-228.531f, 0.0f, 357.449f},
    {-228.928f, 0.0f, 356.777f},
    {-229.315f, 0.0f, 356.123f},
    {-229.711f, 0.0f, 355.451f},
    {-230.098f, 0.0f, 354.797f},
    {-230.489f, 0.0f, 354.134f},
    {-230.886f, 0.0f, 353.463f},
    {-231.278f, 0.0f, 352.8f},
    {-231.674f, 0.0f, 352.128f},
    {-232.112f, 0.0f, 351.387f},
    {-232.503f, 0.0f, 350.724f},
    {-232.976f, 0.0f, 349.924f},
    {-233.378f, 0.0f, 349.244f},
    {-233.851f, 0.0f, 348.443f},
    {-234.238f, 0.0f, 347.788f},
    {-234.629f, 0.0f, 347.125f},
    {-234.831f, 0.0f, 346.382f},
    {-234.828f, 0.0f, 345.612f},
    {-234.824f, 0.0f, 344.852f},
    {-234.821f, 0.0f, 344.072f},
    {-234.818f, 0.0f, 343.282f},
    {-234.815f, 0.0f, 342.482f},
    {-234.812f, 0.0f, 341.682f},
    {-234.808f, 0.0f, 340.892f},
    {-234.805f, 0.0f, 340.092f},
    {-234.802f, 0.0f, 339.332f},
    {-234.799f, 0.0f, 338.572f},
    {-234.796f, 0.0f, 337.772f},
    {-234.793f, 0.0f, 336.982f},
    {-234.789f, 0.0f, 336.182f},
    {-234.786f, 0.0f, 335.412f},
    {-234.783f, 0.0f, 334.642f},
    {-234.565f, 0.0f, 333.883f},
    {-234.171f, 0.0f, 333.233f},
    {-233.777f, 0.0f, 332.583f},
    {-233.341f, 0.0f, 331.865f},
    {-232.719f, 0.0f, 330.839f},
    {-232.076f, 0.0f, 329.779f},
    {-231.599f, 0.0f, 328.992f},
    {-231.117f, 0.0f, 328.197f},
    {-230.539f, 0.0f, 327.658f},
    {-229.702f, 0.0f, 327.276f},
    {-229.002f, 0.0f, 326.956f},
    {-228.302f, 0.0f, 326.635f},
    {-227.601f, 0.0f, 326.315f},
    {-226.765f, 0.0f, 325.933f},
    {-226.064f, 0.0f, 325.613f},
    {-225.355f, 0.0f, 325.288f},
    {-224.616f, 0.0f, 325.164f},
    {-223.821f, 0.0f, 325.253f},
    {-223.065f, 0.0f, 325.338f},
    {-222.28f, 0.0f, 325.427f},
    {-221.515f, 0.0f, 325.513f},
    {-220.73f, 0.0f, 325.602f},
    {-219.955f, 0.0f, 325.689f},
    {-219.2f, 0.0f, 325.774f},
    {-218.286f, 0.0f, 325.877f},
    {-217.53f, 0.0f, 325.962f},
    {-216.765f, 0.0f, 326.049f},
    {-216.0f, 0.0f, 326.135f},
    {-215.235f, 0.0f, 326.221f},
    {-214.52f, 0.0f, 326.507f},
    {-213.796f, 0.0f, 326.797f},
    {-213.081f, 0.0f, 327.084f},
    {-212.367f, 0.0f, 327.37f},
    {-211.633f, 0.0f, 327.664f},
    {-210.918f, 0.0f, 327.95f},
    {-210.204f, 0.0f, 328.237f},
    {-209.597f, 0.0f, 328.694f},
    {-208.863f, 0.0f, 329.248f},
    {-208.232f, 0.0f, 329.724f},
    {-207.617f, 0.0f, 330.188f},
    {-207.002f, 0.0f, 330.652f},
    {-206.38f, 0.0f, 331.121f},
    {-205.757f, 0.0f, 331.591f},
    {-205.28f, 0.0f, 332.221f},
    {-204.724f, 0.0f, 332.954f},
    {-204.162f, 0.0f, 333.695f},
    {-203.606f, 0.0f, 334.428f},
    {-203.056f, 0.0f, 335.153f},
    {-202.585f, 0.0f, 335.775f},
    {-202.108f, 0.0f, 336.404f},
    {-201.655f, 0.0f, 337.002f},
    {-201.184f, 0.0f, 337.623f},
    {-200.712f, 0.0f, 338.245f},
    {-200.229f, 0.0f, 338.882f},
    {-199.679f, 0.0f, 339.608f},
    {-199.214f, 0.0f, 340.221f},
    {-198.725f, 0.0f, 340.867f},
    {-198.265f, 0.0f, 341.472f},
    {-197.788f, 0.0f, 342.102f},
    {-197.329f, 0.0f, 342.707f},
    {-196.87f, 0.0f, 343.313f},
    {-196.314f, 0.0f, 344.046f},
    {-195.758f, 0.0f, 344.779f},
    {-195.196f, 0.0f, 345.52f},
    {-194.647f, 0.0f, 346.245f},
    {-194.085f, 0.0f, 346.986f},
    {-193.523f, 0.0f, 347.728f},
    {-193.064f, 0.0f, 348.333f},
    {-192.598f, 0.0f, 348.947f},
    {-192.127f, 0.0f, 349.568f},
    {-191.656f, 0.0f, 350.19f},
    {-191.191f, 0.0f, 350.803f},
    {-190.719f, 0.0f, 351.425f},
    {-190.152f, 0.0f, 352.174f},
    {-189.596f, 0.0f, 352.907f},
    {-189.13f, 0.0f, 353.521f},
    {-188.563f, 0.0f, 354.27f},
    {-188.085f, 0.0f, 354.899f},
    {-187.529f, 0.0f, 355.632f},
    {-187.058f, 0.0f, 356.254f},
    {-186.593f, 0.0f, 356.868f},
    {-186.128f, 0.0f, 357.481f},
    {-185.663f, 0.0f, 358.095f},
    {-185.197f, 0.0f, 358.708f},
    {-184.642f, 0.0f, 359.441f},
    {-184.17f, 0.0f, 360.063f},
    {-183.705f, 0.0f, 360.677f},
    {-183.24f, 0.0f, 361.29f},
    {-182.775f, 0.0f, 361.904f},
    {-182.219f, 0.0f, 362.637f},
    {-181.754f, 0.0f, 363.25f},
    {-181.204f, 0.0f, 363.976f},
    {-180.636f, 0.0f, 364.725f},
    {-180.08f, 0.0f, 365.458f},
    {-179.524f, 0.0f, 366.191f},
    {-178.962f, 0.0f, 366.932f},
    {-178.394f, 0.0f, 367.681f},
    {-177.827f, 0.0f, 368.43f},
    {-177.271f, 0.0f, 369.163f},
    {-176.806f, 0.0f, 369.777f},
    {-176.334f, 0.0f, 370.398f},
    {-175.772f, 0.0f, 371.139f},
    {-175.301f, 0.0f, 371.761f},
    {-174.842f, 0.0f, 372.366f},
    {-174.28f, 0.0f, 373.108f},
    {-173.815f, 0.0f, 373.721f},
    {-173.35f, 0.0f, 374.335f},
    {-172.782f, 0.0f, 375.084f},
    {-172.317f, 0.0f, 375.697f},
    {-171.857f, 0.0f, 376.303f},
    {-171.374f, 0.0f, 376.94f},
    {-170.897f, 0.0f, 377.57f},
    {-170.42f, 0.0f, 378.199f},
    {-169.96f, 0.0f, 378.805f},
    {-169.471f, 0.0f, 379.451f},
    {-169.012f, 0.0f, 380.056f},
    {-168.535f, 0.0f, 380.686f},
    {-168.057f, 0.0f, 381.315f},
    {-167.689f, 0.0f, 381.801f},
    {-167.396f, 0.0f, 382.188f},
    {-167.181f, 0.0f, 382.471f},
    {-167.096f, 0.0f, 382.583f},
    {-240.234f, 0.0f, 341.562f},
    {-240.27f, 0.0f, 341.524f},
    {-240.344f, 0.0f, 341.448f},
    {-240.485f, 0.0f, 341.302f},
    {-240.64f, 0.0f, 341.141f},
    {-240.831f, 0.0f, 340.942f},
    {-241.071f, 0.0f, 340.694f},
    {-241.342f, 0.0f, 340.412f},
    {-241.634f, 0.0f, 340.11f},
    {-242.004f, 0.0f, 339.726f},
    {-242.358f, 0.0f, 339.359f},
    {-242.814f, 0.0f, 338.886f},
    {-243.257f, 0.0f, 338.426f},
    {-243.748f, 0.0f, 337.916f},
    {-244.277f, 0.0f, 337.368f},
    {-244.731f, 0.0f, 336.897f},
    {-245.05f, 0.0f, 336.566f},
    {-245.261f, 0.0f, 336.348f},
    {-245.358f, 0.0f, 336.247f},
    {-245.358f, 0.0f, 336.247f},
    {-245.358f, 0.0f, 336.247f},
    {-245.396f, 0.0f, 336.207f},
    {-245.469f, 0.0f, 336.132f},
    {-245.58f, 0.0f, 336.016f},
    {-245.728f, 0.0f, 335.863f},
    {-245.919f, 0.0f, 335.665f},
    {-246.139f, 0.0f, 335.437f},
    {-246.403f, 0.0f, 335.163f},
    {-246.717f, 0.0f, 334.837f},
    {-247.039f, 0.0f, 334.503f},
    {-247.416f, 0.0f, 334.112f},
    {-247.831f, 0.0f, 333.681f},
    {-248.348f, 0.0f, 333.145f},
    {-248.817f, 0.0f, 332.659f},
    {-249.467f, 0.0f, 331.984f},
    {-250.002f, 0.0f, 331.43f},
    {-250.522f, 0.0f, 330.89f},
    {-251.057f, 0.0f, 330.336f},
    {-251.667f, 0.0f, 329.702f},
    {-252.09f, 0.0f, 329.263f},
    {-252.43f, 0.0f, 328.911f},
    {-252.655f, 0.0f, 328.678f},
    {-252.777f, 0.0f, 328.551f},
    {-252.916f, 0.0f, 328.382f},
    {-253.076f, 0.0f, 328.144f},
    {-253.228f, 0.0f, 327.847f},
    {-253.402f, 0.0f, 327.508f},
    {-253.653f, 0.0f, 327.018f},
    {-253.887f, 0.0f, 326.563f},
    {-254.201f, 0.0f, 325.95f},
    {-254.546f, 0.0f, 325.277f},
    {-254.855f, 0.0f, 324.674f},
    {-255.198f, 0.0f, 324.007f},
    {-255.549f, 0.0f, 323.322f},
    {-255.905f, 0.0f, 322.628f},
    {-256.252f, 0.0f, 321.952f},
    {-256.672f, 0.0f, 321.133f},
    {-257.023f, 0.0f, 320.448f},
    {-257.379f, 0.0f, 319.754f},
    {-257.73f, 0.0f, 319.069f},
    {-258.086f, 0.0f, 318.375f},
    {-258.438f, 0.0f, 317.69f},
    {-258.853f, 0.0f, 316.88f},
    {-259.2f, 0.0f, 316.204f},
    {-259.551f, 0.0f, 315.519f},
    {-259.71f, 0.0f, 314.806f},
    {-259.886f, 0.0f, 314.015f},
    {-260.088f, 0.0f, 313.108f},
    {-260.262f, 0.0f, 312.327f},
    {-260.457f, 0.0f, 311.448f},
    {-260.416f, 0.0f, 310.669f},
    {-260.155f, 0.0f, 309.913f},
    {-259.913f, 0.0f, 309.214f},
    {-259.655f, 0.0f, 308.467f},
    {-259.398f, 0.0f, 307.72f},
    {-259.094f, 0.0f, 306.841f},
    {-258.833f, 0.0f, 306.085f},
    {-258.575f, 0.0f, 305.338f},
    {-258.314f, 0.0f, 304.582f},
    {-258.06f, 0.0f, 303.845f},
    {-257.808f, 0.0f, 303.117f},
    {-257.557f, 0.0f, 302.389f},
    {-257.306f, 0.0f, 301.661f},
    {-257.051f, 0.0f, 300.924f},
    {-256.8f, 0.0f, 300.196f},
    {-256.545f, 0.0f, 299.459f},
    {-256.294f, 0.0f, 298.731f},
    {-256.043f, 0.0f, 298.003f},
    {-255.791f, 0.0f, 297.275f},
    {-255.537f, 0.0f, 296.538f},
    {-255.282f, 0.0f, 295.801f},
    {-255.031f, 0.0f, 295.073f},
    {-254.783f, 0.0f, 294.355f},
    {-254.532f, 0.0f, 293.627f},
    {-254.274f, 0.0f, 292.88f},
    {-254.026f, 0.0f, 292.162f},
    {-253.778f, 0.0f, 291.443f},
    {-253.474f, 0.0f, 290.564f},
    {-253.22f, 0.0f, 289.827f},
    {-252.965f, 0.0f, 289.089f},
    {-252.665f, 0.0f, 288.22f},
    {-252.414f, 0.0f, 287.492f},
    {-252.113f, 0.0f, 286.622f},
    {-251.865f, 0.0f, 285.904f},
    {-251.611f, 0.0f, 285.167f},
    {-251.363f, 0.0f, 284.448f},
    {-251.111f, 0.0f, 283.721f},
    {-250.857f, 0.0f, 282.983f},
    {-250.557f, 0.0f, 282.114f},
    {-250.302f, 0.0f, 281.376f},
    {-250.051f, 0.0f, 280.648f},
    {-249.767f, 0.0f, 279.826f},
    {-249.512f, 0.0f, 279.089f},
    {-249.258f, 0.0f, 278.352f},
    {-249.003f, 0.0f, 277.614f},
    {-248.7f, 0.0f, 276.735f},
    {-248.452f, 0.0f, 276.017f},
    {-248.2f, 0.0f, 275.289f},
    {-247.946f, 0.0f, 274.552f},
    {-247.691f, 0.0f, 273.814f},
    {-247.394f, 0.0f, 272.954f},
    {-247.143f, 0.0f, 272.226f},
    {-246.892f, 0.0f, 271.498f},
    {-246.624f, 0.0f, 270.723f},
    {-246.369f, 0.0f, 269.986f},
    {-246.118f, 0.0f, 269.258f},
    {-245.867f, 0.0f, 268.53f},
    {-245.619f, 0.0f, 267.812f},
    {-245.367f, 0.0f, 267.084f},
    {-245.113f, 0.0f, 266.347f},
    {-244.806f, 0.0f, 265.458f},
    {-244.548f, 0.0f, 264.712f},
    {-244.297f, 0.0f, 263.984f},
    {-243.993f, 0.0f, 263.105f},
    {-243.739f, 0.0f, 262.367f},
    {-243.491f, 0.0f, 261.649f},
    {-243.233f, 0.0f, 260.902f},
    {-242.933f, 0.0f, 260.033f},
    {-242.619f, 0.0f, 259.125f},
    {-242.316f, 0.0f, 258.246f},
    {-242.061f, 0.0f, 257.509f},
    {-241.813f, 0.0f, 256.79f},
    {-241.559f, 0.0f, 256.053f},
    {-241.308f, 0.0f, 255.325f},
    {-241.05f, 0.0f, 254.579f},
    {-240.795f, 0.0f, 253.841f},
    {-240.541f, 0.0f, 253.104f},
    {-240.286f, 0.0f, 252.367f},
    {-240.035f, 0.0f, 251.639f},
    {-239.783f, 0.0f, 250.911f},
    {-239.526f, 0.0f, 250.164f},
    {-239.271f, 0.0f, 249.427f},
    {-239.02f, 0.0f, 248.699f},
    {-238.768f, 0.0f, 247.971f},
    {-238.511f, 0.0f, 247.225f},
    {-238.259f, 0.0f, 246.497f},
    {-237.992f, 0.0f, 245.722f},
    {-237.737f, 0.0f, 244.984f},
    {-237.486f, 0.0f, 244.257f},
    {-237.235f, 0.0f, 243.529f},
    {-236.934f, 0.0f, 242.659f},
    {-236.676f, 0.0f, 241.912f},
    {-236.379f, 0.0f, 241.052f},
    {-236.125f, 0.0f, 240.315f},
    {-235.87f, 0.0f, 239.578f},
    {-235.619f, 0.0f, 238.85f},
    {-235.371f, 0.0f, 238.131f},
    {-235.071f, 0.0f, 237.262f},
    {-234.819f, 0.0f, 236.534f},
    {-234.565f, 0.0f, 235.797f},
    {-234.314f, 0.0f, 235.069f},
    {-234.013f, 0.0f, 234.199f},
    {-233.762f, 0.0f, 233.471f},
    {-233.514f, 0.0f, 232.753f},
    {-233.259f, 0.0f, 232.016f},
    {-233.011f, 0.0f, 231.297f},
    {-232.757f, 0.0f, 230.56f},
    {-232.506f, 0.0f, 229.832f},
    {-232.248f, 0.0f, 229.085f},
    {-231.996f, 0.0f, 228.357f},
    {-231.742f, 0.0f, 227.62f},
    {-231.487f, 0.0f, 226.883f},
    {-231.236f, 0.0f, 226.155f},
    {-230.936f, 0.0f, 225.285f},
    {-230.681f, 0.0f, 224.548f},
    {-230.261f, 0.0f, 223.927f},
    {-229.502f, 0.0f, 223.356f},
    {-228.776f, 0.0f, 223.072f},
    {-228.01f, 0.0f, 222.992f},
    {-227.274f, 0.0f, 222.915f},
    {-226.478f, 0.0f, 222.832f},
    {-225.553f, 0.0f, 222.736f},
    {-224.618f, 0.0f, 222.638f},
    {-223.849f, 0.0f, 222.769f},
    {-223.041f, 0.0f, 222.906f},
    {-222.272f, 0.0f, 223.037f},
    {-221.493f, 0.0f, 223.169f},
    {-220.586f, 0.0f, 223.324f},
    {-219.92f, 0.0f, 223.623f},
    {-219.304f, 0.0f, 224.149f},
    {-218.868f, 0.0f, 224.796f},
    {-218.621f, 0.0f, 225.672f},
    {-218.423f, 0.0f, 226.374f},
    {-218.206f, 0.0f, 227.144f},
    {-217.997f, 0.0f, 227.885f},
    {-217.788f, 0.0f, 228.626f},
    {-217.579f, 0.0f, 229.368f},
    {-217.329f, 0.0f, 230.253f},
    {-217.12f, 0.0f, 230.994f},
    {-216.911f, 0.0f, 231.735f},
    {-216.705f, 0.0f, 232.467f},
    {-216.496f, 0.0f, 233.208f},
    {-216.246f, 0.0f, 234.093f},
    {-216.236f, 0.0f, 234.843f},
    {-216.224f, 0.0f, 235.733f},
    {-216.214f, 0.0f, 236.513f},
    {-216.204f, 0.0f, 237.303f},
    {-216.194f, 0.0f, 238.073f},
    {-216.184f, 0.0f, 238.833f},
    {-216.174f, 0.0f, 239.603f},
    {-216.164f, 0.0f, 240.363f},
    {-216.151f, 0.0f, 241.293f},
    {-216.141f, 0.0f, 242.062f},
    {-216.131f, 0.0f, 242.832f},
    {-216.121f, 0.0f, 243.592f},
    {-216.111f, 0.0f, 244.362f},
    {-216.101f, 0.0f, 245.132f},
    {-216.091f, 0.0f, 245.902f},
    {-216.081f, 0.0f, 246.682f},
    {-216.068f, 0.0f, 247.622f},
    {-216.058f, 0.0f, 248.392f},
    {-216.048f, 0.0f, 249.162f},
    {-216.036f, 0.0f, 250.112f},
    {-216.025f, 0.0f, 250.892f},
    {-216.015f, 0.0f, 251.662f},
    {-216.005f, 0.0f, 252.442f},
    {-215.995f, 0.0f, 253.212f},
    {-215.985f, 0.0f, 253.971f},
    {-215.975f, 0.0f, 254.751f},
    {-215.965f, 0.0f, 255.531f},
    {-215.953f, 0.0f, 256.451f},
    {-215.942f, 0.0f, 257.231f},
    {-215.932f, 0.0f, 258.011f},
    {-215.922f, 0.0f, 258.781f},
    {-215.912f, 0.0f, 259.551f},
    {-215.9f, 0.0f, 260.471f},
    {-215.89f, 0.0f, 261.241f},
    {-215.879f, 0.0f, 262.021f},
    {-215.869f, 0.0f, 262.781f},
    {-215.859f, 0.0f, 263.551f},
    {-216.054f, 0.0f, 264.296f},
    {-216.289f, 0.0f, 265.195f},
    {-216.484f, 0.0f, 265.94f},
    {-216.681f, 0.0f, 266.695f},
    {-216.876f, 0.0f, 267.44f},
    {-217.071f, 0.0f, 268.185f},
    {-217.268f, 0.0f, 268.94f},
    {-217.465f, 0.0f, 269.694f},
    {-217.662f, 0.0f, 270.449f},
    {-217.855f, 0.0f, 271.184f},
    {-218.052f, 0.0f, 271.939f},
    {-218.249f, 0.0f, 272.693f},
    {-218.446f, 0.0f, 273.448f},
    {-218.644f, 0.0f, 274.203f},
    {-218.838f, 0.0f, 274.948f},
    {-218.834f, 0.0f, 275.698f},
    {-218.829f, 0.0f, 276.488f},
    {-218.824f, 0.0f, 277.268f},
    {-218.819f, 0.0f, 278.028f},
    {-218.815f, 0.0f, 278.798f},
    {-218.81f, 0.0f, 279.578f},
    {-218.805f, 0.0f, 280.348f},
    {-218.99f, 0.0f, 281.064f},
    {-219.222f, 0.0f, 281.965f},
    {-219.414f, 0.0f, 282.71f},
    {-219.606f, 0.0f, 283.456f},
    {-219.798f, 0.0f, 284.202f},
    {-219.99f, 0.0f, 284.947f},
    {-220.18f, 0.0f, 285.683f},
    {-220.374f, 0.0f, 286.439f},
    {-220.564f, 0.0f, 287.175f},
    {-220.756f, 0.0f, 287.92f},
    {-220.948f, 0.0f, 288.666f},
    {-221.143f, 0.0f, 289.421f},
    {-221.333f, 0.0f, 290.157f},
    {-221.527f, 0.0f, 290.913f},
    {-221.717f, 0.0f, 291.649f},
    {-221.911f, 0.0f, 292.404f},
    {-222.106f, 0.0f, 293.159f},
    {-222.336f, 0.0f, 294.05f},
    {-222.726f, 0.0f, 294.725f},
    {-223.266f, 0.0f, 295.26f},
    {-223.82f, 0.0f, 295.81f},
    {-224.466f, 0.0f, 296.45f},
    {-225.013f, 0.0f, 296.992f},
    {-225.553f, 0.0f, 297.527f},
    {-226.114f, 0.0f, 298.084f},
    {-226.745f, 0.0f, 298.45f},
    {-227.531f, 0.0f, 298.646f},
    {-228.249f, 0.0f, 298.824f},
    {-229.025f, 0.0f, 299.017f},
    {-229.792f, 0.0f, 299.207f},
    {-230.549f, 0.0f, 299.395f},
    {-231.319f, 0.0f, 299.376f},
    {-232.065f, 0.0f, 299.147f},
    {-232.801f, 0.0f, 298.922f},
    {-233.547f, 0.0f, 298.693f},
    {-234.283f, 0.0f, 298.467f},
    {-235.038f, 0.0f, 298.236f},
    {-235.674f, 0.0f, 297.838f},
    {-236.204f, 0.0f, 297.252f},
    {-236.687f, 0.0f, 296.718f},
    {-237.237f, 0.0f, 296.11f},
    {-237.761f, 0.0f, 295.532f},
    {-238.284f, 0.0f, 294.954f},
    {-238.794f, 0.0f, 294.39f},
    {-239.132f, 0.0f, 293.743f},
    {-239.291f, 0.0f, 292.938f},
    {-239.439f, 0.0f, 292.183f},
    {-239.598f, 0.0f, 291.378f},
    {-239.749f, 0.0f, 290.613f},
    {-239.902f, 0.0f, 289.838f},
    {-240.078f, 0.0f, 288.945f},
    {-240.231f, 0.0f, 288.17f},
    {-240.18f, 0.0f, 287.422f},
    {-239.884f, 0.0f, 286.636f},
    {-239.623f, 0.0f, 285.943f},
    {-239.288f, 0.0f, 285.054f},
    {-239.013f, 0.0f, 284.325f},
    {-238.731f, 0.0f, 283.576f},
    {-238.452f, 0.0f, 282.837f},
    {-238.177f, 0.0f, 282.107f},
    {-237.909f, 0.0f, 281.396f},
    {-237.585f, 0.0f, 280.535f},
    {-237.313f, 0.0f, 279.814f},
    {-237.035f, 0.0f, 279.075f},
    {-236.71f, 0.0f, 278.214f},
    {-236.432f, 0.0f, 277.475f},
    {-236.157f, 0.0f, 276.745f},
    {-235.882f, 0.0f, 276.015f},
    {-235.561f, 0.0f, 275.163f},
    {-235.282f, 0.0f, 274.424f},
    {-235.007f, 0.0f, 273.694f},
    {-234.683f, 0.0f, 272.833f},
    {-234.415f, 0.0f, 272.122f},
    {-234.136f, 0.0f, 271.383f},
    {-233.861f, 0.0f, 270.653f},
    {-233.59f, 0.0f, 269.932f},
    {-233.269f, 0.0f, 269.081f},
    {-232.997f, 0.0f, 268.36f},
    {-232.62f, 0.0f, 267.359f},
    {-232.348f, 0.0f, 266.639f},
    {-232.073f, 0.0f, 265.909f},
    {-231.802f, 0.0f, 265.188f},
    {-231.527f, 0.0f, 264.458f},
    {-231.195f, 0.0f, 263.579f},
    {-230.871f, 0.0f, 262.718f},
    {-230.599f, 0.0f, 261.997f},
    {-230.275f, 0.0f, 261.136f},
    {-229.947f, 0.0f, 260.266f},
    {-229.675f, 0.0f, 259.545f},
    {-229.404f, 0.0f, 258.825f},
    {-229.129f, 0.0f, 258.095f},
    {-228.854f, 0.0f, 257.365f},
    {-228.522f, 0.0f, 256.486f},
    {-228.244f, 0.0f, 255.746f},
    {-227.976f, 0.0f, 255.035f},
    {-227.704f, 0.0f, 254.315f},
    {-227.433f, 0.0f, 253.594f},
    {-226.955f, 0.0f, 252.952f},
    {-226.508f, 0.0f, 252.35f},
    {-226.048f, 0.0f, 251.733f},
    {-225.582f, 0.0f, 251.107f},
    {-225.117f, 0.0f, 250.481f},
    {-224.568f, 0.0f, 249.743f},
    {-224.102f, 0.0f, 249.117f},
    {-223.547f, 0.0f, 248.371f},
    {-223.081f, 0.0f, 247.745f},
    {-222.628f, 0.0f, 247.135f},
    {-222.162f, 0.0f, 246.51f},
    {-221.697f, 0.0f, 245.884f},
    {-221.231f, 0.0f, 245.258f},
    {-220.956f, 0.0f, 244.571f},
    {-220.886f, 0.0f, 243.714f},
    {-220.826f, 0.0f, 242.976f},
    {-220.761f, 0.0f, 242.179f},
    {-220.687f, 0.0f, 241.272f},
    {-220.612f, 0.0f, 240.345f},
    {-220.548f, 0.0f, 239.558f},
    {-220.486f, 0.0f, 238.79f},
    {-220.422f, 0.0f, 238.003f},
    {-220.359f, 0.0f, 237.235f},
    {-220.298f, 0.0f, 236.478f},
    {-220.235f, 0.0f, 235.71f},
    {-220.16f, 0.0f, 234.783f},
    {-220.096f, 0.0f, 233.996f},
    {-220.033f, 0.0f, 233.219f},
    {-219.981f, 0.0f, 232.587f},
    {-219.939f, 0.0f, 232.062f},
    {-219.913f, 0.0f, 231.748f},
    {-219.905f, 0.0f, 231.646f}, 
    {-208.743, 0, 212.768}, 
    {-234.449, 0, 307.815 }, 
    { -236.938, 0, 316.463 }, 
    { -244.977, 0, 300.961 }, 
    { -207.633, 0, 221.686}, 
    { -207.185, 0, 239.984 }, 
    { -218.263, 0, 316.84 }, 
    { -221.494, 0, 214.053 }, 
    { -245.818, 0, 315.46 }, 
    { -245.749, 0, 318.063 }, 
    { -73.5182, 0, 243.416 }, 
    { -216.19, 0, 206.906 }, 
    { -80.5821, 0, 237.944 }
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

bool checkCollisionWithObstacles2(const Vector& carPosition, float collisionThreshold = 2.0f) {
    for (const auto& stone : stones) {
        Vector stonePosition(stone.x, stone.y, stone.z);
        if (carPosition.distanceToNoY(stonePosition) <= collisionThreshold) {
            return true; // Collision detected
        }
    }
    return false; // No collision
}

bool checkCollisionWithBarriers2(const Vector& carPosition, float collisionThreshold = 2.0f) {
    for (const auto& barrier : barriers2) {
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

bool checkCollisionWithNitros(Vector& carPosition, std::vector<Nitro>& nitros, float collisionThreshold = 3.0f) {
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

bool checkCollisionWithCoins(Vector& carPosition, std::vector<Coin>& coins, float collisionThreshold = 2.0f) {
    for (auto it = coins.begin(); it != coins.end(); ++it) {
        Vector coinPosition(it->x, it->y, it->z);
        if (carPosition.distanceToNoY(coinPosition) <= collisionThreshold) {
            coins.erase(it); 
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
    }
    isColliding = false;
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
        //std::cout << "Car Pos: ";
        //carPosition.print();
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


void handleCarControls2(float deltaTime) {
    // Accelerate

    if (isAccelerating) {
        carSpeed += acceleration2 * deltaTime;
        if (carSpeed > maxSpeed2 && !isNitroActive) carSpeed = maxSpeed2;
    }
    // Brake/Reverse
    else if (isBraking) {
        carSpeed -= deceleration2 * deltaTime;
        if (carSpeed < -20) carSpeed = -20; // Allow negative speed for reverse
    }
    // Coast (slow down gradually)
    else {
        if (carSpeed > 0) {
            carSpeed -= deceleration2 * 0.5f * deltaTime; // Adjust this factor for desired coasting behavior
            if (carSpeed < 0) carSpeed = 0;
        }
        else if (carSpeed < 0) {
            carSpeed += deceleration2 * 0.5f * deltaTime; // Adjust this factor for desired coasting behavior
            if (carSpeed > 0) carSpeed = 0;
        }
    }

    // Turn left
    if (wheelRotationY > 0) {
        carRotation += turnSpeed2 * deltaTime * (carSpeed / maxSpeed2);
    }
    // Turn right
    else if (wheelRotationY < 0) {
        carRotation -= turnSpeed2 * deltaTime * (carSpeed / maxSpeed2);
    }

    // Normalize rotation to 0-360 degrees
    while (carRotation >= 360.0f) carRotation -= 360.0f;
    while (carRotation < 0.0f) carRotation += 360.0f;
}

bool collisionDetected = false; // Add a flag for collision

void updateCarPosition2(float deltaTime) {
    float radians = carRotation * M_PI / 180.0;

    // Update car position based on car speed and direction
    if (!collisionDetected) { // Allow movement only if no collision or moving backward
        carPosition.x += sin(radians) * carSpeed * deltaTime;
        carPosition.z += cos(radians) * carSpeed * deltaTime;
        std::cout << "Car Pos: ";
        carPosition.print();
    }
    else {
        if (carSpeed < 0) {
            carSpeed = 0;
        }
    }

    glm::vec3 carPositionG = glm::vec3(carPosition.x, carPosition.y, carPosition.z);


    if (isPointInTrack(trackVertices2, carPosition, 9.0f)) {
        /*std::cout << "Car Pos: ";
        carPosition.print();*/
        collisionDetected = false;
    }
    else {
        collisionDetected = true; // Set collision flag to true
        
        std::cout << "Collision" << std::endl;

        // Push the car back slightly
        carPosition.x -= sin(radians) * carSpeed * deltaTime;
        carPosition.z -= cos(radians) * carSpeed * deltaTime;

        // Ensure the car stops moving forward
        carSpeed = std::min(carSpeed, 0.0f); // Prevent forward movement by setting carSpeed to zero
    }
    if (checkCollisionWithBarriers2(carPosition)) {
        collisionDetected = true; // Set collision flag to true

        std::cout << "Collision" << std::endl;

        // Push the car back slightly
        carPosition.x -= sin(radians) * carSpeed * deltaTime;
        carPosition.z -= cos(radians) * carSpeed * deltaTime;

        // Ensure the car stops moving forward
        carSpeed = std::min(carSpeed, 0.0f);

        Vector startBarrier = Vector(-47.0496, 0, -26.7259);
        if(!(startBarrier.distanceToNoY(carPosition) <= 4.0f)){
            applyCollisionRecoil(deltaTime);
        }
        if(score != 0)
        score = score - 1;
    }

    wheelRotationX += carSpeed * 360.0f * deltaTime;

    // Update game timer
    if (!gameWon && !gameOver && timerStarted) {
        gameTimer -= deltaTime;
        if (gameTimer <= 0) {
            gameOver = true;
            lastCarPosition = carPosition;
        }
    }
    if (!gameWon && score == 27) {
        /*printf("fffffffffffffffff");*/
        gameWon = true;
        playerTime = 90.0f - gameTimer; // Calculate player's time
    }
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

void updateCoinAnimation() {
    for (auto& coin : coins) {
        coin.animationPhase += 0.5f;
        if (coin.animationPhase > 360.0f) {
            coin.animationPhase -= 360.0f;
        }
        coin.y = coin.y + sin(coin.animationPhase) * 0.08f;
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

void endCinematicMode()
{
    if (currentView == CINEMATIC)
    {
        currentView = THIRD_PERSON;
        // Reset car position and other game start parameters if needed
        carPosition = Vector(0, 0, 0);
        carRotation = 0;
        carSpeed = 0;
        // Add any other necessary game start initializations here
    }
}

// cinematic points 

std::vector<Vector> cinematicPoints = {
    Vector(106.284, 20, 500.741),
    Vector(0.261323, 20, -158.15),
    Vector(243.843, 20, 600.5731),
    Vector(2, 0.4, -3),
};

std::vector<Vector> cinematicPoints2 = {
  Vector(-150.249, 90, 629.321),
    Vector(-187.911, 80, 447.393),
    Vector(-41.9579, 30, 78.913),
    Vector(2, 0.4, -3),
};

const float POINT_DISPLAY_DURATION = 2.0f;
const float FINAL_POINT_DURATION = 2.0f; // Duration for final point
const float FINAL_POINT_RADIUS = 10.0f; // Radius for the final point
int currentCinematicPoint = 0;
float cinematicTimer = 0;
const float CINEMATIC_DURATION = 7.0f;

void updateCamera()
{

    if (currentView == CINEMATIC & !selectingCar) {
        // Check if we're at the final point
        if (level == 1) {
            if (currentCinematicPoint == cinematicPoints.size() - 1) {
                float t = cinematicTimer / FINAL_POINT_DURATION;
                Vector current = cinematicPoints[currentCinematicPoint];

                // Move the camera backwards continuously from the current point
                Eye.x = current.x;
                Eye.y = current.y;
                Eye.z = current.z - t * FINAL_POINT_RADIUS; // Continuously move away from the point
                At = current;

                // Update timer
                cinematicTimer += 0.016; // Assuming 60 FPS

                // Move to the next point after the display duration
                if (cinematicTimer >= FINAL_POINT_DURATION) {
                    cinematicTimer = 0;
                    currentCinematicPoint = 0; // Reset to the first point
                    currentView = THIRD_PERSON;
                }
            }
            else {
                // Cinematic camera movement for other points
                float t = cinematicTimer / CINEMATIC_DURATION;
                int nextPoint = (currentCinematicPoint + 1) % cinematicPoints.size();
                Vector current = cinematicPoints[currentCinematicPoint];
                Vector next = cinematicPoints[nextPoint];

                // Interpolate between current and next point
                Eye.x = current.x + (next.x - current.x) * t;
                Eye.y = current.y + (next.y - current.y) * t;
                Eye.z = current.z + (next.z - current.z) * t;

                // Look at the next point
                At = next;

                // Update timer and current point
                cinematicTimer += 0.016; // Assuming 60 FPS
                if (cinematicTimer >= CINEMATIC_DURATION / cinematicPoints.size()) {
                    cinematicTimer = 0;
                    currentCinematicPoint = nextPoint;
                }

                // If cinematic is complete, switch to third person view
                if (currentCinematicPoint == 0 && cinematicTimer == 0) {
                    currentView = THIRD_PERSON;
                }
            }
        }
        else {
            if (currentCinematicPoint == cinematicPoints2.size() - 1) {
                float t = cinematicTimer / FINAL_POINT_DURATION;
                Vector current = cinematicPoints2[currentCinematicPoint];

                // Move the camera backwards continuously from the current point
                Eye.x = current.x;
                Eye.y = current.y;
                Eye.z = current.z - t * FINAL_POINT_RADIUS; // Continuously move away from the point
                At = current;

                // Update timer
                cinematicTimer += 0.016; // Assuming 60 FPS

                // Move to the next point after the display duration
                if (cinematicTimer >= FINAL_POINT_DURATION) {
                    cinematicTimer = 0;
                    currentCinematicPoint = 0; // Reset to the first point
                    currentView = THIRD_PERSON;
                }
            }
            else {
                // Cinematic camera movement for other points
                float t = cinematicTimer / CINEMATIC_DURATION;
                int nextPoint = (currentCinematicPoint + 1) % cinematicPoints2.size();
                Vector current = cinematicPoints2[currentCinematicPoint];
                Vector next = cinematicPoints2[nextPoint];

                // Interpolate between current and next point
                Eye.x = current.x + (next.x - current.x) * t;
                Eye.y = current.y + (next.y - current.y) * t;
                Eye.z = current.z + (next.z - current.z) * t;

                // Look at the next point
                At = next;

                // Update timer and current point
                cinematicTimer += 0.016; // Assuming 60 FPS
                if (cinematicTimer >= CINEMATIC_DURATION / cinematicPoints2.size()) {
                    cinematicTimer = 0;
                    currentCinematicPoint = nextPoint;
                }

                // If cinematic is complete, switch to third person view
                if (currentCinematicPoint == 0 && cinematicTimer == 0) {
                    currentView = THIRD_PERSON;
                }
            }
        }
    }
    else if (gameOver) {
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

            if (selectedCar == 1) {
                Eye = Vector(
                    carPosition.x + rotatedCameraOffset.x,
                    carPosition.y + rotatedCameraOffset.y,
                    carPosition.z + rotatedCameraOffset.z
                );
            }
            else {
                Eye = Vector(
                    carPosition.x + rotatedCameraOffset.x,
                    carPosition.y + rotatedCameraOffset.y + 0.05,
                    carPosition.z + rotatedCameraOffset.z + 0.05
                );

            }

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

    //glEnable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
}

void renderSpeedOMeter(float speed) {

    if (currentView == CINEMATIC) {
       return;
    }
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
    //glEnable(GL_LIGHTING);
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

void renderCenteredText(const char* text, float screenWidth, float posY) {
    // Calculate the width of the text
    int textWidth = 0;
    for (const char* c = text; *c != '\0'; c++) {
        textWidth += glutBitmapWidth(GLUT_BITMAP_HELVETICA_18, *c);
    }

    // Calculate the X position to center the text
    float posX = (screenWidth - textWidth) / 2.0f;

    // Set the raster position for the text
    glRasterPos2f(posX, posY);

    // Render the text
    for (const char* c = text; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }
}

void drawHUD() {

    if (currentView == CINEMATIC)
    {
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        gluOrtho2D(0, WIDTH, 0, HEIGHT);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        glColor3f(0.0f, 0.0f, 0.0f);  // White color for text
        glRasterPos2i(WIDTH / 2 - 70, HEIGHT - 50);

        if (level == 1) {
            const char* text = "Press Enter to Start";
            for (const char* c = text; *c != '\0'; c++) {
                glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
            }
        }
		else {
            const char* text1 = "You need funds for a water bottle and you don't have money, collect 0.5 euros quickly!";
            const char* text2 = "Press enter to start";

            // Render first line (at the top of the screen)
            renderCenteredText(text1, WIDTH, HEIGHT - 20);

            // Render second line (below the first line)
            renderCenteredText(text2, WIDTH, HEIGHT - 60);
		}
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        return; 
    }

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
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
    }
    if (level == 2) {
        // Draw score of the player
        glColor3f(0.0f, 1.0f, 0.0f); // Bright green for digital display
        float scoreX = stopwatchX + 15;
        float scoreY = stopwatchY + stopwatchHeight / 2 - 20;

        std::string scoreText = "Wallet: " + std::to_string(score) + " EGP";
        glRasterPos2f(scoreX, scoreY);

        for (char c : scoreText) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
        }
    }

    // Render speedometer if game is active
    if (!gameWon && !gameOver) {
        renderSpeedOMeter(abs(carSpeed * 2.1));
    }

    if (gameWon) {
        int scoreTime = 9000 - static_cast<int>(gameTimer * 100);

        std::ostringstream timeStream;
        timeStream << std::fixed << std::setprecision(2) << playerTime;
        std::string formattedTime = timeStream.str();

        glColor3f(0.0f, 1.0f, 0.0f); 

        


        if (level == 1) {

            glRasterPos2i(WIDTH / 2 - 100, HEIGHT / 2);
            std::string winText = "You Win! Time: " + formattedTime + " seconds";
            for (char c : winText) {
                glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
            }
            // Second line: "Score: XXXX"
            glRasterPos2i(WIDTH / 2 - 100, HEIGHT / 2 - 30);
            std::string scoreText = "Score: " + std::to_string(scoreTime);
            for (char c : scoreText) {
                glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
            }
        
            glRasterPos2i(WIDTH / 2 - 150, HEIGHT / 2 - 60); // Adjust Y-position for the new line
            std::string instructionText = "Press R to restart or N to go to the next level";
            for (char c : instructionText) {
                glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
            }
        }
        if (level == 2) {

            glRasterPos2i(WIDTH / 2 - 100, HEIGHT / 2);
            std::string winText = "You Have Completed the Game! Time: " + formattedTime + " seconds";
            for (char c : winText) {
                glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
            }

            glRasterPos2i(WIDTH / 2 - 100, HEIGHT / 2 - 30);
            std::string scoreText = "Wallet: " + std::to_string(score) + " EGP";
            for (char c : scoreText) {
                glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
            }
            

            glRasterPos2i(WIDTH / 2 - 150, HEIGHT / 2 - 60); // Adjust Y-position for the new line
            std::string instructionText = "Press R to restart";
            for (char c : instructionText) {
                glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
            }
        }
    }

    // Draw game-over text
    if (gameOver) {
        drawGameOverText();
    }

    // Re-enable lighting and depth testing
    //glEnable(GL_LIGHTING);
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
    isNitroActive = false;
    carPosition = Vector(0, 0, 0);  // Reset car position
    carRotation = 0;  // Reset car rotation
    carSpeed = 0;  // Reset car speed
    wheelRotationX = 0;  // Reset wheel rotation
    wheelRotationY = 0;  // Reset wheel rotation
    sunsetProgress = 0.0f;  // Reset sunset progress
    gameWon = false;
    gameTimer = 90.0f;
    playerTime = 0.0f;
    nitros = originalNitros;
	coins = originalCoins;
    timerStarted = false;
    score = 0; 
    sunEffect.reset();
    sunrise.reset();
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
}

//=======================================================================
// Render Functions
//=======================================================================

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

void renderStones() {
    for (const auto& stone : stones) {
        glPushMatrix();
        glTranslatef(stone.x, stone.y, stone.z);
        glScalef(1.0f, 1.0f, 1.0f);  // Adjust scale if needed
        glRotatef(180.0f, 1, 0, 0);   // Adjust rotation if needed
        rockModel.DrawModel();
        glPopMatrix();
    }
}

void renderLogs() {
    for (const auto& log : logs) {
        glPushMatrix();
        glTranslatef(log.x, log.y, log.z);
        glScalef(3.0f, 3.0f, 3.0f);  // Adjust scale if needed
        glRotatef(90.0f, 0, 1, 0);   // Adjust rotation if needed
        logModel.DrawModel();
        glPopMatrix();
    }
}

void renderCoins() {
    for (const auto& coin : coins) {
        float rotation = -coin.animationPhase * 45;

        glPushMatrix();
        glTranslatef(coin.x, coin.y, coin.z);
        glRotatef(90, 1, 0, 0);
        glRotatef(rotation, 0, 0, 1);
        glScalef(0.5, 0.5, 0.5);
        egpModel.DrawModel();
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
        glScalef(0.4f, 0.4f, 0.4f);  // Adjust scale if needed
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

    //if (wheelRotationY < -52.5) {
    //    wheelRotationY = -52.5;
    //}


    //// Draw front left wheel
    //glPushMatrix();
    //glTranslatef(carPosition.x, carPosition.y, carPosition.z);
    //glRotatef(carRotation, 0, 1, 0);  // to face the right direction
    //glTranslatef(-wheelOffsetX, wheelOffsetY, wheelOffsetZBack);
    ////glScalef(0.5, 0.5, 0.5);
    //glRotatef(180 + wheelRotationY, 0, 1, 0);
    //glRotatef(-wheelRotationX, 1, 0, 0);  // rotate on x here when clicking up or 
    //redWheelsFrontLeft1.DrawModel();
    //glPopMatrix();

    //// Draw front right wheel
    //glPushMatrix();
    //glTranslatef(carPosition.x, carPosition.y, carPosition.z);
    //glRotatef(carRotation, 0, 1, 0);  // to face the right direction
    //glTranslatef(wheelOffsetX, wheelOffsetY, wheelOffsetZBack);	//glScalef(0.5, 0.5, 0.5);
    //glRotatef(wheelRotationY, 0, 1, 0);
    //glRotatef(wheelRotationX, 1, 0, 0);  // rotate on x here when clicking up or 
    //redWheelsFrontRight1.DrawModel();
    //glPopMatrix();

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

void renderCar2() {
    // Update car model position and rotation
    glPushMatrix();
    glTranslatef(carPosition.x, carPosition.y, carPosition.z);
    glRotatef(carRotation, 0, 1, 0);
    //glScalef(1, 1, 1);
    glRotatef(0, 0, 1, 0);
    bugattiModel.DrawModel();
    glPopMatrix();

    // Offsets for the wheels relative to the car's position
    float wheelOffsetX = -1.4f; // Horizontal offset from the car's center
    float wheelOffsetY = 0.5f; // Vertical offset below the car
    float wheelOffsetZFront = -2.0f; // Forward offset for front wheels
    float wheelOffsetZBack = 1.8f; // Backward offset for back wheels
    float scaleBlueWheel = 0.14f;

    // Draw back left wheel
    glPushMatrix();
    //glScalef(1, 1, 1); 
    glTranslatef(carPosition.x, carPosition.y, carPosition.z);
    glRotatef(carRotation, 0, 1, 0);  // to face the right direction
    glTranslatef(-wheelOffsetX, wheelOffsetY, wheelOffsetZFront);

    glRotatef(wheelRotationX, 1, 0, 0);  // rotate on x here when clicking up or down
    glRotatef(180, 0, 1, 0);  // to face the right direction
    glScalef(scaleBlueWheel, scaleBlueWheel, scaleBlueWheel);
    blueWheelModel.DrawModel();
    glPopMatrix();

    // Draw back right wheel
    glPushMatrix();
    glTranslatef(carPosition.x, carPosition.y, carPosition.z);
    glRotatef(carRotation, 0, 1, 0);  // to face the right direction
    glTranslatef(wheelOffsetX, wheelOffsetY, wheelOffsetZFront);
    //glScalef(0.5, 0.5, 0.5);
    glRotatef(wheelRotationX, 1, 0, 0);  // rotate on x here when clicking up or down
    glRotatef(0, 0, 1, 0);
    glScalef(scaleBlueWheel, scaleBlueWheel, scaleBlueWheel);

    blueWheelModel.DrawModel();
    glPopMatrix();

    if (wheelRotationY > 70) {
        wheelRotationY = 70;
    }

    if (wheelRotationY < -70) {
        wheelRotationY = -70;
    }


    // Draw front left wheel
    glPushMatrix();
    glTranslatef(carPosition.x, carPosition.y, carPosition.z);
    glRotatef(carRotation, 0, 1, 0);  // to face the right direction
    glTranslatef(-wheelOffsetX, wheelOffsetY, wheelOffsetZBack);
    //glScalef(0.5, 0.5, 0.5);
    glRotatef(180 + wheelRotationY, 0, 1, 0);
    glRotatef(-wheelRotationX, 1, 0, 0);  // rotate on x here when clicking up or 
    glScalef(scaleBlueWheel, scaleBlueWheel, scaleBlueWheel);

    blueWheelModel.DrawModel();
    glPopMatrix();

    // Draw front right wheel
    glPushMatrix();
    glTranslatef(carPosition.x, carPosition.y, carPosition.z);
    glRotatef(carRotation, 0, 1, 0);  // to face the right direction
    glTranslatef(wheelOffsetX, wheelOffsetY, wheelOffsetZBack);	//glScalef(0.5, 0.5, 0.5);
    glRotatef(wheelRotationY, 0, 1, 0);
    glRotatef(wheelRotationX, 1, 0, 0);  // rotate on x here when clicking up or 
    glScalef(scaleBlueWheel, scaleBlueWheel, scaleBlueWheel);
    blueWheelModel.DrawModel();
    glPopMatrix();
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
    if (!selectingCar) {
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


        if (sunVisibility > 0.0f) {
            glPushMatrix();
            glTranslatef(sunPosition.x, sunPosition.y, sunPosition.z);
            glDisable(GL_LIGHTING);
            glColor4f(sunColor.r, sunColor.g, sunColor.b, sunVisibility);
            glEnable(GL_LIGHTING);
            glPopMatrix();
        }

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

        if(selectedCar == 1)
        renderCar();
        else if(selectedCar == 2)
        renderCar2();

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

    }
    else {

        renderCarSelectScreen();

    }

    glutSwapBuffers();
}

void myDisplay2(void) {

    if (!selectingCar) {

        static int lastTime = 0;
        int currentTime = glutGet(GLUT_ELAPSED_TIME);
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;
        handleCarControls2(deltaTime);
        updateCarPosition2(deltaTime);

        //glClearColor(currentSkyColor.r, currentSkyColor.g, currentSkyColor.b, 1.0f);
        sunrise.update(deltaTime);
        sunEffect.update(deltaTime);

        // Clear the screen and apply the sunrise effect
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        sunrise.apply();
        sunEffect.apply();
        updateCamera();

        // draw moscow 

        glPushMatrix();
        //glTranslatef(181.899, 0, 231.466);
        //glRotatef(270, 0, 1, 0);
        glScalef(1, 1, 1);
        moscowModel.DrawModel();
        glPopMatrix();

         glPushMatrix();
         glTranslatef(-47.0496, 0, -26.7259);
         glRotatef(90, 0, 1, 0);
         glScalef(7, 7, 7);
         roadBlockModel.DrawModel();
         glPopMatrix();

         /*glPushMatrix();
         glTranslatef(5, 1, 1);
         glRotatef(70, 1, 0, 0);
         glScalef(1, 1, 1);
         rock2Model.DrawModel();
         glPopMatrix();*/

        setupLighting();

        if (selectedCar == 1)
            renderCar();
        else if (selectedCar == 2)
            renderCar2();

        renderCoins();
        renderStreetlights();
        renderLogs();
        renderStones();
        updateCoinAnimation();

        if (checkCollisionWithObstacles2(carPosition)) {
            carSpeed = 0;
            isColliding = true;
            applyCollisionRecoil(deltaTime);
            if (score != 0)
                score -= 1;
        }

        if (checkCollisionWithCoins(carPosition, coins))
        {
            score = score + 1;
        }

        drawHUD();
    }
    else {
		renderCarSelectScreen();
	}

    glutSwapBuffers();


}

//=======================================================================
// Keyboard Function
//=======================================================================



boolean secondLevelLoading; 

void myKeyboard(unsigned char button, int x, int y)
{
    if (selectingCar && button == 's' && selectedCar != 0) {  // 13 is the ASCII code for Enter
        selectingCar = false;
        std::cout << "Selected car: " << selectedCar << std::endl;
        // Initialize game with selected car
        // You might want to call a function here to set up the game based on the selected car
        //glutPostRedisplay();
        // wait for assets to be loaded 
        myInit();
    }

    if (isRespawning || collisionRecoil > 0) {
        return;
    }
    if(gameOver || gameWon)
        {
		if ((button == 'r' || button == 'R') && !secondLevelLoading)
		{
			resetGame();
		}
        if (button == 'n' || button == 'N') {
            secondLevelLoading = true; 
            goToNextLevel(); 
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
    case 13:
    // Enter key
		endCinematicMode();
		break;
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
    if (gameOver || gameWon || isRespawning || collisionRecoil > 0 || currentView == CINEMATIC) {
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
            if(carSpeed <= 20) wheelRotationX += 2.0f;
            wheelRotationX += 6.0f;
            isAccelerating = true;
            isBraking = false;
            if (!timerStarted) {
                timerStarted = true;
            }
        }
        break;
    case GLUT_KEY_DOWN:
        if (carSpeed <= 20) wheelRotationX -= 2.0f;
        wheelRotationX -= 6.0f;
        isAccelerating = false;
        isBraking = true;
        if (!timerStarted) {
            timerStarted = true;
        }
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

    if (selectingCar && button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        y = HEIGHT - y;  // Invert y coordinate

        int carWidth = 300;
        int carHeight = 150;
        int carSpacing = 50;
        int totalWidth = cars.size() * (carWidth + carSpacing) - carSpacing;
        int startX = (WIDTH - totalWidth) / 2;
        int startY = HEIGHT / 2 - carHeight / 2;

        for (size_t i = 0; i < cars.size(); ++i) {
            int carX = startX + i * (carWidth + carSpacing);
            if (x >= carX && x <= carX + carWidth && y >= startY && y <= startY + carHeight) {
                selectedCar = i + 1;  // Set the selected car
                glutPostRedisplay();
                break;
            }
        }
    }

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

// Load Level 1 

void LoadAssets()
{
	// Loading Model files
	model_house.Load("Models/house/house.3DS");
	model_tree.Load("Models/tree/Tree1.3ds");

    
	if (!gltfModel1.LoadModel("models/track5/scene.gltf")) {
		std::cerr << "Failed to load GLTF model" << std::endl;
		// Handle error
	}

	if (!carModel1.LoadModel("models/red-car-no-wheels/scene.gltf")) {
		std::cerr << "Failed to load GLTF model" << std::endl;
		// Handle error
	}
    if (!bugattiModel.LoadModel("models/bugatti-no-wheels/scene.gltf")) {
        std::cerr << "Failed to load GLTF model" << std::endl;
    }

    if (!blueWheelModel.LoadModel("models/blue-wheel/scene.gltf")) {
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

void UnloadAssets() {
    gltfModel1.UnloadModel();
    carModel1.UnloadModel();
    redWheelsFrontLeft1.UnloadModel();
    redWheelsFrontRight1.UnloadModel();
    redWheelsBackLeft1.UnloadModel();
    redWheelsBackRight1.UnloadModel();
    coneModel.UnloadModel();
    nitroModel.UnloadModel();
    finishModel.UnloadModel();
    horizontalTraffic.UnloadModel();
    trafficObstacle.UnloadModel();
    bugattiModel.UnloadModel();
    blueWheelModel.UnloadModel();
}

// Load Level 2

void LoadAssets2() {

    if (!moscowModel.LoadModel("models/moscow-test/scene.gltf")) {
	std::cerr << "Failed to load GLTF model" << std::endl;
    }
    

    if (!bugattiModel.LoadModel("models/bugatti-no-wheels/scene.gltf")) {
        std::cerr << "Failed to load GLTF model" << std::endl;
    }

    if (!egpModel.LoadModel("models/pound_egypt/scene.gltf")) {
        std::cerr << "Failed to load GLTF model" << std::endl;
    }


    if (!logModel.LoadModel("models/log2/scene.gltf")) {
        std::cerr << "Failed to load GLTF model" << std::endl;
    }

    if (!rockModel.LoadModel("models/rock/scene.gltf")) {
        std::cerr << "Failed to load GLTF model" << std::endl;
    }
    if (!roadBlockModel.LoadModel("models/roadsign/scene.gltf")) {
        std::cerr << "Failed to load GLTF model" << std::endl;
    }

 //   if (!blueWheelsFrontLeft.LoadModel("models/wheel/scene.gltf")) {
 //       std::cerr << "Failed to load GLTF model" << std::endl;
 //       // Handle error
 //   }

 //   if (!blueWheelsFrontRight.LoadModel("models/wheel/scene.gltf")) {
	//	std::cerr << "Failed to load GLTF model" << std::endl;
	//	// Handle error
	//}

 //   if (!blueWheelsBackLeft.LoadModel("models/wheel/scene.gltf")) {
	//	std::cerr << "Failed to load GLTF model" << std::endl;
	//	// Handle error
	//}

 //   if (!blueWheelsBackRight.LoadModel("models/wheel/scene.gltf")) {
 //       std::cerr << "Failed to load GLTF model" << std::endl;
 //       // Handle error
 //   }

    if (!carModel1.LoadModel("models/red-car-no-wheels/scene.gltf")) {
        std::cerr << "Failed to load GLTF model" << std::endl;
        // Handle error
    }

    if (!blueWheelModel.LoadModel("models/blue-wheel/scene.gltf")) {
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



    

}

void goToNextLevel() {
    UnloadAssets();
    LoadAssets2();
    selectedCar = 0; 
    selectingCar = true;
    timerStarted = false;
    gameTimer = 90.0f;
    playerTime = 0.0f;
    gameOver = false;
    gameWon = false;
    isNitroActive = false;
    carPosition = Vector(0, 0, 0);  // Reset car position
    carRotation = 0;  // Reset car rotation
    carSpeed = 0;  // Reset car speed
    wheelRotationX = 0;  // Reset wheel rotation
    wheelRotationY = 0;  // Reset wheel rotation
    sunsetProgress = 0.0f;  // Reset sunset progress
    gravityEnabled = false;
    nitros = originalNitros;
    coins = originalCoins;
    score = 0;
    sunEffect.reset();
    sunrise.reset();
    level = 2;
    currentCinematicPoint = 0;
    cinematicTimer = 0;
    currentView = CINEMATIC;
    secondLevelLoading = false;
    // make my display 2 the current display
    glutDisplayFunc(myDisplay2);
    sunEffect.start();
    sunrise.start();
    glutPostRedisplay();

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

    

    if(level == 1)
	{
		glutDisplayFunc(myDisplay);
	}
	else
	{
		glutDisplayFunc(myDisplay2);

	}

	glutKeyboardFunc(myKeyboard);
	glutSpecialFunc(specialKeyboard);
	glutSpecialUpFunc(specialKeyboardUp);



	glutMotionFunc(myMotion);

	glutMouseFunc(myMouse);

	glutReshapeFunc(myReshape);

	myInit();

    if (level == 1) {
        loadCars();
        LoadAssets();
    }
    else {
        loadCars();
        LoadAssets2();
	
    }
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
