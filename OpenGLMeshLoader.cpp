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

GLuint shaderProgram;

tinygltf::Model gltfModel;
bool modelLoaded = false;

class GLTFModel {
public:
	static bool LoadModel(const std::string& filename, tinygltf::Model& model) {
		tinygltf::TinyGLTF loader;
		std::string err;
		std::string warn;
		bool ret = false;

		std::string ext = GetFileExtension(filename);
		if (ext == "gltf") {
			ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename);
		}
		else if (ext == "glb") {
			ret = loader.LoadBinaryFromFile(&model, &err, &warn, filename);
		}

		if (!warn.empty()) {
			std::cout << "GLTF loading warning: " << warn << std::endl;
		}
		if (!err.empty()) {
			std::cerr << "GLTF loading error: " << err << std::endl;
		}

		return ret;
	}

	static void DrawModel(const tinygltf::Model& model, const glm::mat4& transform = glm::mat4(1.0f)) {
		glPushMatrix();
		glMultMatrixf(glm::value_ptr(transform));

		const tinygltf::Scene& scene = model.scenes[model.defaultScene];
		for (size_t i = 0; i < scene.nodes.size(); ++i) {
			DrawNode(model, scene.nodes[i], glm::mat4(1.0f));
		}

		glPopMatrix();
	}

private:
	static void DrawNode(const tinygltf::Model& model, int nodeIndex, const glm::mat4& parentTransform) {
		const tinygltf::Node& node = model.nodes[nodeIndex];

		// Calculate node's transformation matrix
		glm::mat4 localTransform = glm::mat4(1.0f);

		if (node.matrix.size() == 16) {
			localTransform = glm::make_mat4(node.matrix.data());
		}
		else {
			// Handle TRS (Translation, Rotation, Scale) properties
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
			DrawMesh(model, model.meshes[node.mesh], nodeTransform);
		}

		for (int child : node.children) {
			DrawNode(model, child, nodeTransform);
		}
	}

	static void DrawMesh(const tinygltf::Model& model, const tinygltf::Mesh& mesh,
		const glm::mat4& transform) {
		glPushMatrix();
		glMultMatrixf(glm::value_ptr(transform));

		for (const auto& primitive : mesh.primitives) {
			if (primitive.indices < 0) continue;

			// Set material properties
			if (primitive.material >= 0) {
				const auto& material = model.materials[primitive.material];
				SetMaterial(material);
			}

			// Get vertex positions
			const auto& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
			const auto& posView = model.bufferViews[posAccessor.bufferView];
			const float* positions = reinterpret_cast<const float*>(
				&model.buffers[posView.buffer].data[posView.byteOffset + posAccessor.byteOffset]);

			// Get vertex normals if available
			const float* normals = nullptr;
			if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
				const auto& normalAccessor = model.accessors[primitive.attributes.at("NORMAL")];
				const auto& normalView = model.bufferViews[normalAccessor.bufferView];
				normals = reinterpret_cast<const float*>(
					&model.buffers[normalView.buffer].data[normalView.byteOffset + normalAccessor.byteOffset]);
			}

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

				if (normals) {
					glNormal3fv(&normals[idx * 3]);
				}
				if (texcoords) {
					glTexCoord2fv(&texcoords[idx * 2]);
				}
				glVertex3fv(&positions[idx * 3]);
			}
			glEnd();
		}

		glPopMatrix();
	}

	static void SetMaterial(const tinygltf::Material& material) {
		if (material.pbrMetallicRoughness.baseColorFactor.size() == 4) {
			GLfloat color[] = {
				(GLfloat)material.pbrMetallicRoughness.baseColorFactor[0],
				(GLfloat)material.pbrMetallicRoughness.baseColorFactor[1],
				(GLfloat)material.pbrMetallicRoughness.baseColorFactor[2],
				(GLfloat)material.pbrMetallicRoughness.baseColorFactor[3]
			};
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, color);
		}

		// Handle textures if present
		if (material.pbrMetallicRoughness.baseColorTexture.index >= 0) {
			// Enable texturing
			glEnable(GL_TEXTURE_2D);
			// Bind texture (assuming you've loaded textures separately)
			// glBindTexture(GL_TEXTURE_2D, textureIds[material.pbrMetallicRoughness.baseColorTexture.index]);
		}
		else {
			glDisable(GL_TEXTURE_2D);
		}
	}

	static std::string GetFileExtension(const std::string& filename) {
		size_t pos = filename.find_last_of(".");
		if (pos != std::string::npos) {
			return filename.substr(pos + 1);
		}
		return "";
	}
};





int WIDTH = 1280;
int HEIGHT = 720;

GLuint tex;
char title[] = "3D Model Loader Sample";

// 3D Projection Options
GLdouble fovy = 45.0;
GLdouble aspectRatio = (GLdouble)WIDTH / (GLdouble)HEIGHT;
GLdouble zNear = 0.1;
GLdouble zFar = 100;

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
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);



	GLfloat lightIntensity[] = { 0.7, 0.7, 0.7, 1.0f };
	GLfloat lightPosition[] = { 0.0f, 100.0f, 0.0f, 0.0f };
	glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
	glLightfv(GL_LIGHT0, GL_AMBIENT, lightIntensity);

	// Draw Ground
	RenderGround();

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
	glm::mat4 modelTransform = glm::mat4(1.0f);
	// Apply any additional transformations you need
	modelTransform = glm::scale(modelTransform, glm::vec3(1.0f)); // Scale the model if needed
	GLTFModel::DrawModel(gltfModel, modelTransform);




	//sky box
	glPushMatrix();

	GLUquadricObj* qobj;
	qobj = gluNewQuadric();
	glTranslated(50, 0, 0);
	glRotated(90, 1, 0, 1);
	glBindTexture(GL_TEXTURE_2D, tex);
	gluQuadricTexture(qobj, true);
	gluQuadricNormals(qobj, GL_SMOOTH);
	gluSphere(qobj, 100, 100, 100);
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
	case 'w':
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		break;
	case 'r':
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
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

	// using tinygltf load gltf model
	if (!GLTFModel::LoadModel("models/cone/traffic_cone.glb", gltfModel)) {
		std::cerr << "Failed to load GLTF model" << std::endl;
		// Handle error
	}
	// Loading texture files
	tex_ground.Load("Textures/ground.bmp");
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
