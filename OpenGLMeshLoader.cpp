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

#define M_PI 3.14159265358979323846


GLuint shaderProgram;

tinygltf::Model gltfModel;
tinygltf::Model carModel; 
tinygltf::Model redWheelsFrontLeft;
tinygltf::Model redWheelsFrontRight;
tinygltf::Model redWheelsBackLeft;
tinygltf::Model redWheelsBackRight;




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
GLTFModel redWheelsFrontLeft1;
GLTFModel redWheelsFrontRight1;
GLTFModel redWheelsBackLeft1;
GLTFModel redWheelsBackRight1;

int WIDTH = 1280;
int HEIGHT = 720;

GLuint tex;
char title[] = "All Star Racer";

// 3D Projection Options
GLdouble fovy = 45.0;
GLdouble aspectRatio = (GLdouble)WIDTH / (GLdouble)HEIGHT;
GLdouble zNear = 0.1;
GLdouble zFar = 10000;



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
Vector cameraOffset(0.2, 0.61, -0.1);
float cameraMovementSpeed = 0.05f;


Vector carVelocity(0, 0, 0);
float carSpeed = 0.0f; // in km/h
float maxSpeed = 300.0f; // km/h

float accelerationTime = 0.5f; // seconds
float accelerationRate = maxSpeed / accelerationTime;
bool isAccelerating = false;
float wheelRotationX = 145.0f;
float wheelRotationY = -100.5f;


void updateCarPosition(float deltaTime) {
	// Convert speed from km/h to units per frame
	float speedPerFrame = (carSpeed / 3600.0f) * deltaTime;

	// Update car position based on its rotation and speed
	float radians = carRotation * M_PI / 180.0;
	carPosition.x -= sin(radians) * speedPerFrame;
	carPosition.z -= cos(radians) * speedPerFrame;
	//Vector(-0.05, 0.51, 0.15)
	// Update camera position in first-person view
	if (currentView == INSIDE_FRONT) {
		Eye = Vector(
			carPosition.x + cameraOffset.x * cos(radians) - cameraOffset.z * sin(radians),
			carPosition.y + cameraOffset.y + 10000,
			carPosition.z + cameraOffset.x * sin(radians) + cameraOffset.z * cos(radians)
		);	

		At = Vector(
			carPosition.x + (cameraOffset.x + 1) * cos(radians) - (cameraOffset.z + 1) * sin(radians),
			carPosition.y + cameraOffset.y,
			carPosition.z + (cameraOffset.x + 1) * sin(radians) + (cameraOffset.z + 1) * cos(radians)
		);
	}
}

void updateCarSpeed(float deltaTime) {
	if (isAccelerating && carSpeed < maxSpeed) {
		carSpeed += accelerationRate * deltaTime;
		if (carSpeed > maxSpeed) carSpeed = maxSpeed;
	}
	else if (!isAccelerating && carSpeed > 0) {
		carSpeed -= accelerationRate * deltaTime;
		if (carSpeed < 0) carSpeed = 0;
	}
}

void specialKeyboard(int key, int x, int y)
{
	switch (key)
	{
	case GLUT_KEY_LEFT:
		wheelRotationY += 2.0f;
		break;
	case GLUT_KEY_RIGHT:
		wheelRotationY -= 2.0f;
		break;
	case GLUT_KEY_UP:
		wheelRotationX += 2.0f;
		break;
	case GLUT_KEY_DOWN:
		wheelRotationX -= 2.0f;
		break;
	}
	glutPostRedisplay();
}

//=======================================================================
// Lighting Configuration Function
//=======================================================================
void InitLightSource()
{
	// Enable Lighting for this OpenGL Program
	glEnable(GL_LIGHTING);

	// Enable Light Source number 0
	// OpengL has 8 light sources
	glEnable(GL_LIGHT0);

	// Define Light source 0 ambient light
	GLfloat ambient[] = { 0.1f, 0.1f, 0.1, 1.0f };
	glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);

	// Define Light source 0 diffuse light
	GLfloat diffuse[] = { 0.5f, 0.5f, 0.5f, 1.0f };
	glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);

	// Define Light source 0 Specular light
	GLfloat specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glLightfv(GL_LIGHT0, GL_SPECULAR, specular);

	// Finally, define light source 0 position in World Space
	GLfloat light_position[] = { 0.0f, 10.0f, 0.0f, 1.0f };
	glLightfv(GL_LIGHT0, GL_POSITION, light_position);
}

//=======================================================================
// Camera Function
//=======================================================================
void updateCamera()
{
	if (currentView == INSIDE_FRONT)
	{
		Vector lookAtOffset(0 + cameraOffset.x, 0.05 + cameraOffset.y, 0.4 + cameraOffset.z);

		float radians = -(carRotation * M_PI / 180.0);
		Vector rotatedCameraOffset(
			cameraOffset.x * cos(radians) - cameraOffset.z * sin(radians),
			cameraOffset.y,
			cameraOffset.x * sin(radians) + cameraOffset.z * cos(radians)
		);

		Vector rotatedLookAtOffset(
			lookAtOffset.x * cos(radians) - lookAtOffset.z * sin(radians),
			lookAtOffset.y,
			lookAtOffset.x * sin(radians) + lookAtOffset.z * cos(radians)
		);

		Eye = Vector(
			carPosition.x + rotatedCameraOffset.x,
			carPosition.y + rotatedCameraOffset.y,
			carPosition.z + rotatedCameraOffset.z
		);

		At = Vector(
			carPosition.x + rotatedLookAtOffset.x,
			carPosition.y + rotatedLookAtOffset.y,
			carPosition.z + rotatedLookAtOffset.z
		);

		Up = Vector(-0.1, 1, 0);
	}
	else if (currentView == THIRD_PERSON) {
		float radians = carRotation * M_PI / 180.0;

		Eye = Vector(
			carPosition.x - thirdPersonDistance * sin(radians),
			carPosition.y + 2.0, // Slightly above the car
			carPosition.z - thirdPersonDistance * cos(radians)
		);

		// Look at point is slightly ahead of the car
		At = Vector(
			carPosition.x - sin(radians),
			carPosition.y + 1.0,
			carPosition.z - cos(radians)
		);

		Up = Vector(0, 1, 0);

	}
	else
	{
		Eye = Vector(20, 5, 20);
		At = Vector(0, 0, 0);
		Up = Vector(0, 1, 0);
	}
	cameraOffset.print();
	glLoadIdentity();
	gluLookAt(Eye.x, Eye.y, Eye.z, At.x, At.y, At.z, Up.x, Up.y, Up.z);
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
	//*******************************************************************************************//
	// fovy:			Angle between the bottom and top of the projectors, in degrees.			 //
	// aspectRatio:		Ratio of width to height of the clipping plane.							 //
	// zNear and zFar:	Specify the front and back clipping planes distances from camera.		 //
	//*******************************************************************************************//

	glMatrixMode(GL_MODELVIEW);

	glLoadIdentity();

	gluLookAt(Eye.x, Eye.y, Eye.z, At.x, At.y, At.z, Up.x, Up.y, Up.z);
	//*******************************************************************************************//
	// EYE (ex, ey, ez): defines the location of the camera.									 //
	// AT (ax, ay, az):	 denotes the direction where the camera is aiming at.					 //
	// UP (ux, uy, uz):  denotes the upward orientation of the camera.							 //
	//*******************************************************************************************//

	InitLightSource();

	InitMaterial();

	glEnable(GL_DEPTH_TEST);

	glEnable(GL_NORMALIZE);

	// Enable texturing
	glEnable(GL_TEXTURE_2D);

	// Enable lighting and material properties
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);

	// Enable depth testing
	glEnable(GL_DEPTH_TEST);

	// Enable normal normalization
	glEnable(GL_NORMALIZE);

	// Enable color material
	glEnable(GL_COLOR_MATERIAL);
	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
}

//=======================================================================
// Render Ground Function
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

//=======================================================================
// Display Function
//=======================================================================
void myDisplay(void)
{
	static int lastTime = 0;
	int currentTime = glutGet(GLUT_ELAPSED_TIME);
	float deltaTime = (currentTime - lastTime) / 1000.0f;
	lastTime = currentTime;

	updateCarSpeed(deltaTime);
	updateCarPosition(deltaTime);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	updateCamera();

	GLfloat lightIntensity[] = { 0.7, 0.7, 0.7, 1.0f };
	GLfloat lightPosition[] = { 0.0f, 100.0f, 0.0f, 0.0f };
	glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
	glLightfv(GL_LIGHT0, GL_AMBIENT, lightIntensity);

	//// Draw Ground
	//RenderGround();

	// Draw Tree Model
	glPushMatrix();
	glTranslatef(10, 0, 0);
	glScalef(0.7, 0.7, 0.7);
	model_tree.Draw();
	glPopMatrix();

	// Draw house Model
	//glPushMatrix();
	//glRotatef(90.f, 1, 0, 0);
	//model_house.Draw();
	//glPopMatrix();

	//trynna draw a bugatti
	//glPushMatrix();
	//glTranslatef(0, 0, 0);  // Position your model
	//glScalef(0.3, 0.3, 0.3);  // Scale if needed
	//glRotatef(90, 1, 0, 0);  // Rotate if needed
	//model_bugatti.Draw();
	//glPopMatrix();

	//glPushMatrix();
	//glTranslatef(0, 0, 0);  // Position your model
	//glScalef(1.0, 1.0, 1.0);  // Scale if needed
	//glRotatef(0, 1, 0, 0);  // Rotate if needed
	//model_moscow.Draw();
	//glPopMatrix();

	// use tinygltf to draw gltf model

	//glPushMatrix();
	//glTranslatef(0, 0, 0);  // Position your model
	//glScalef(0.09, 0.09, 0.09);  // Scale if needed
	//glRotatef(0, 1, 0, 0);  // Rotate if needed
	//GLTFModel::DrawModel(gltfModel, "models/test/scene.gltf");
	//glPopMatrix();

	// In your render function
	glPushMatrix();
	glTranslatef(0, 0, 0);  // Position your model
	glScalef(1, 1, 1);  // Scale if needed
	glRotatef(90, 0, 1, 0);  // Rotate if needed
	gltfModel1.DrawModel();
	glPopMatrix();

	// Update car model position and rotation
	glPushMatrix();
	glTranslatef(carPosition.x, carPosition.y, carPosition.z);
	glRotatef(carRotation, 0, 1, 0);
	glScalef(0.5, 0.5, 0.5);
	glRotatef(0, 0, 1, 0);
	carModel1.DrawModel();
	glPopMatrix();

// Offsets for the wheels relative to the car's position
	float wheelOffsetX = -0.6f; // Horizontal offset from the car's center
	float wheelOffsetY = 0.2f; // Vertical offset below the car
	float wheelOffsetZFront = -0.85f; // Forward offset for front wheels
	float wheelOffsetZBack = 0.85f; // Backward offset for back wheels

	// Draw back left wheel
	glPushMatrix();
	glTranslatef(carPosition.x - wheelOffsetX, carPosition.y + wheelOffsetY, carPosition.z + wheelOffsetZFront);
	glScalef(0.5, 0.5, 0.5); 
	glRotatef(wheelRotationX, 1, 0, 0);  // rotate on x here when clicking up or down
	glRotatef(180, 0, 1, 0);  // to face the right direction
	redWheelsBackLeft1.DrawModel();
	glPopMatrix();

	// Draw back right wheel
	glPushMatrix();
	glTranslatef(carPosition.x + wheelOffsetX, carPosition.y + wheelOffsetY, carPosition.z + wheelOffsetZFront);
	glScalef(0.5, 0.5, 0.5);
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
	glTranslatef(carPosition.x - wheelOffsetX, carPosition.y + wheelOffsetY, carPosition.z + wheelOffsetZBack);
	glScalef(0.5, 0.5, 0.5);
	glRotatef(180 + wheelRotationY, 0, 1, 0);
	glRotatef(-wheelRotationX, 1, 0, 0);  // rotate on x here when clicking up or 
	redWheelsFrontLeft1.DrawModel();
	glPopMatrix();

	// Draw front right wheel
	glPushMatrix();
	glTranslatef(carPosition.x + wheelOffsetX, carPosition.y + wheelOffsetY, carPosition.z + wheelOffsetZBack);
	glScalef(0.5, 0.5, 0.5);
	glRotatef(wheelRotationY,0, 1, 0);
	glRotatef(wheelRotationX, 1, 0, 0);  // rotate on x here when clicking up or 
	redWheelsFrontRight1.DrawModel(); 
	glPopMatrix();



	//sky box
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



	glutSwapBuffers();
}

//=======================================================================
// Keyboard Function
//=======================================================================
void myKeyboard(unsigned char button, int x, int y)
{
	switch (button)
	{
	case 'a':
		if (currentView == INSIDE_FRONT)
			cameraOffset.z -= cameraMovementSpeed;
		else
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		break;
	case 'd':
		if (currentView == INSIDE_FRONT)
			cameraOffset.z += cameraMovementSpeed;
		break;
	case 'e':
		if (currentView == INSIDE_FRONT)
			cameraOffset.x -= cameraMovementSpeed;
		break;
	case 'q':
		if (currentView == INSIDE_FRONT)
			cameraOffset.x += cameraMovementSpeed;
		break;
	case 'w':
		if (currentView == INSIDE_FRONT)
			cameraOffset.y += cameraMovementSpeed;
		break;
	case 's':
		if (currentView == INSIDE_FRONT)
			cameraOffset.y -= cameraMovementSpeed;
		break;
	case 'r':
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		break;
	case '1':
		currentView = INSIDE_FRONT;
		break;
	case '3':
		currentView = THIRD_PERSON;
		break;
	case 27:
		exit(0);
		break;
	default:
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

	if (!gltfModel1.LoadModel("models/track2/scene.gltf")) {
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

	glTranslatef(carPosition.x, carPosition.y, carPosition.z);

	// Loading texture files
	loadBMP(&tex, "Textures/blu-sky-3.bmp", true);
}

//=======================================================================
// Main Function
//=======================================================================
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


	glutMotionFunc(myMotion);

	glutMouseFunc(myMouse);

	glutReshapeFunc(myReshape);

	myInit();

	LoadAssets();
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_NORMALIZE);
	glEnable(GL_COLOR_MATERIAL);

	glShadeModel(GL_SMOOTH);

	glutMainLoop();
}
