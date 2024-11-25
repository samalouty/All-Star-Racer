#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <iostream>
#include "TextureBuilder.h"
#include "Model_3DS.h"
//#include "Model_GLB.h"
#include "GLTexture.h"
#include <glut.h>
#include "tiny_gltf.h"
#include <glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <unordered_map>
#include <vector>


GLuint shaderProgram;

tinygltf::Model gltfModel;
bool modelLoaded = false;


class GLTFModel {
public:
	static bool LoadModel(const std::string& filename, tinygltf::Model& model) {
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
			std::cerr << "Failed to load GLTF file: " << filename << std::endl;
		}

		if (!GLEW_ARB_vertex_array_object && !GLEW_VERSION_3_0) {
			std::cerr << "VAOs are not supported on your platform!" << std::endl;
			return false;
		}

		return ret;
	}

	static void DrawModel(const tinygltf::Model& model, const std::string& modelPath) {
		// Pre-process and cache data
		static std::unordered_map<const tinygltf::Model*, ModelData> modelCache;
		if (modelCache.find(&model) == modelCache.end()) {
			modelCache[&model] = PreprocessModel(model, modelPath);
		}
		const ModelData& modelData = modelCache[&model];

		// Draw all meshes
		for (const auto& meshData : modelData.meshes) {
			// Bind texture
			glBindTexture(GL_TEXTURE_2D, meshData.textureID);

			// Enable client-side capabilities
			glEnableClientState(GL_VERTEX_ARRAY);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);

			// Set up vertex and texture coordinate pointers
			glVertexPointer(3, GL_FLOAT, 5 * sizeof(float), meshData.vertexData.data());
			glTexCoordPointer(2, GL_FLOAT, 5 * sizeof(float), meshData.vertexData.data() + 3);

			// Draw
			glDrawArrays(GL_TRIANGLES, 0, meshData.vertexCount);

			// Disable client-side capabilities
			glDisableClientState(GL_VERTEX_ARRAY);
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		}

		// Unbind texture
		glBindTexture(GL_TEXTURE_2D, 0);
	}

private:
	struct MeshData {
		std::vector<float> vertexData;  // Interleaved: x, y, z, u, v
		int vertexCount;
		GLuint textureID;
	};

	struct ModelData {
		std::vector<MeshData> meshes;
	};

	static ModelData PreprocessModel(const tinygltf::Model& model, const std::string& modelPath) {
		ModelData modelData;
		const tinygltf::Scene& scene = model.scenes[model.defaultScene];
		for (size_t i = 0; i < scene.nodes.size(); ++i) {
			ProcessNode(model, model.nodes[scene.nodes[i]], modelPath, modelData);
		}
		return modelData;
	}

	static void ProcessNode(const tinygltf::Model& model, const tinygltf::Node& node, const std::string& modelPath, ModelData& modelData) {
		if (node.mesh >= 0) {
			ProcessMesh(model, model.meshes[node.mesh], modelPath, modelData);
		}

		for (size_t i = 0; i < node.children.size(); ++i) {
			ProcessNode(model, model.nodes[node.children[i]], modelPath, modelData);
		}
	}

	static void ProcessMesh(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const std::string& modelPath, ModelData& modelData) {
		for (const auto& primitive : mesh.primitives) {
			if (primitive.indices < 0) continue;

			MeshData meshData;

			// Process vertex data
			const auto& positionAccessor = model.accessors[primitive.attributes.at("POSITION")];
			const auto& positionView = model.bufferViews[positionAccessor.bufferView];
			const auto& positionBuffer = model.buffers[positionView.buffer];
			const float* positions = reinterpret_cast<const float*>(&positionBuffer.data[positionView.byteOffset]);

			const auto& texcoordAccessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
			const auto& texcoordView = model.bufferViews[texcoordAccessor.bufferView];
			const auto& texcoordBuffer = model.buffers[texcoordView.buffer];
			const float* texcoords = reinterpret_cast<const float*>(&texcoordBuffer.data[texcoordView.byteOffset]);

			const auto& indexAccessor = model.accessors[primitive.indices];
			const auto& indexView = model.bufferViews[indexAccessor.bufferView];
			const auto& indexBuffer = model.buffers[indexView.buffer];
			const unsigned short* indices = reinterpret_cast<const unsigned short*>(&indexBuffer.data[indexView.byteOffset]);

			// Interleave vertex data
			for (size_t i = 0; i < indexAccessor.count; ++i) {
				unsigned int index = indices[i];
				meshData.vertexData.push_back(positions[index * 3]);
				meshData.vertexData.push_back(positions[index * 3 + 1]);
				meshData.vertexData.push_back(positions[index * 3 + 2]);
				meshData.vertexData.push_back(texcoords[index * 2]);
				meshData.vertexData.push_back(texcoords[index * 2 + 1]);
			}
			meshData.vertexCount = indexAccessor.count;

			// Load texture
			if (primitive.material >= 0) {
				const auto& material = model.materials[primitive.material];
				if (material.pbrMetallicRoughness.baseColorTexture.index >= 0) {
					int textureIndex = material.pbrMetallicRoughness.baseColorTexture.index;
					const auto& texture = model.textures[textureIndex];
					const auto& image = model.images[texture.source];
					meshData.textureID = LoadTexture(modelPath, image.uri);
				}
			}

			modelData.meshes.push_back(meshData);
		}
	}

	static GLuint LoadTexture(const std::string& modelPath, const std::string& textureName) {
		std::string directory = modelPath.substr(0, modelPath.find_last_of("/\\"));
		std::string texturePath = directory + "/" + textureName;

		GLuint textureID = 0;
		glGenTextures(1, &textureID);
		if (textureID == 0) {
			std::cerr << "Failed to generate texture ID" << std::endl;
			return 0;
		}

		glBindTexture(GL_TEXTURE_2D, textureID);

		// Set texture wrapping parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		// Set texture filtering parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		int width, height, nrChannels;
		unsigned char* data = stbi_load(texturePath.c_str(), &width, &height, &nrChannels, 0);
		if (data) {
			GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
			glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);

			// Check if glGenerateMipmap is available
			if (GLEW_VERSION_3_0 || GLEW_ARB_framebuffer_object) {
				glGenerateMipmap(GL_TEXTURE_2D);
			}
			else {
				//std::cout << "glGenerateMipmap is not available. Falling back to gluBuild2DMipmaps." << std::endl;
				gluBuild2DMipmaps(GL_TEXTURE_2D, format, width, height, format, GL_UNSIGNED_BYTE, data);
			}

			std::cout << "Loaded texture: " << texturePath << " (" << width << "x" << height << ", " << nrChannels << " channels)" << std::endl;
		}
		else {
			std::cerr << "Failed to load texture: " << texturePath << std::endl;
			glDeleteTextures(1, &textureID);
			return 0;
		}

		stbi_image_free(data);
		return textureID;
	}



	static void DrawMesh(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const std::string& modelPath) {
		for (const auto& primitive : mesh.primitives) {
			if (primitive.indices < 0) continue;

			// Load position data
			const auto& positionAccessor = model.accessors[primitive.attributes.at("POSITION")];
			const auto& positionView = model.bufferViews[positionAccessor.bufferView];
			const auto& positionBuffer = model.buffers[positionView.buffer];
			const float* positions = reinterpret_cast<const float*>(&positionBuffer.data[positionView.byteOffset]);

			// Load texture coordinates
			const auto& texcoordAccessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
			const auto& texcoordView = model.bufferViews[texcoordAccessor.bufferView];
			const auto& texcoordBuffer = model.buffers[texcoordView.buffer];
			const float* texcoords = reinterpret_cast<const float*>(&texcoordBuffer.data[texcoordView.byteOffset]);

			// Load indices
			const auto& indexAccessor = model.accessors[primitive.indices];
			const auto& indexView = model.bufferViews[indexAccessor.bufferView];
			const auto& indexBuffer = model.buffers[indexView.buffer];
			const unsigned short* indices = reinterpret_cast<const unsigned short*>(&indexBuffer.data[indexView.byteOffset]);

			// Load texture if available
			if (primitive.material >= 0) {
				const auto& material = model.materials[primitive.material];
				if (material.pbrMetallicRoughness.baseColorTexture.index >= 0) {
					int textureIndex = material.pbrMetallicRoughness.baseColorTexture.index;
					const auto& texture = model.textures[textureIndex];
					const auto& image = model.images[texture.source];

					GLuint texID = LoadTexture(modelPath, image.uri);
					glBindTexture(GL_TEXTURE_2D, texID);
					glEnable(GL_TEXTURE_2D);
				}
			}

			// Draw the mesh
			glBegin(GL_TRIANGLES);
			for (size_t i = 0; i < indexAccessor.count; ++i) {
				unsigned int index = indices[i];
				glTexCoord2fv(&texcoords[index * 2]); // Apply texture coordinates
				glVertex3fv(&positions[index * 3]);   // Apply vertex positions
			}
			glEnd();
		}
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

	glPushMatrix();
	glTranslatef(0, 0, 0);  // Position your model
	glScalef(0.09, 0.09, 0.09);  // Scale if needed
	glRotatef(0, 1, 0, 0);  // Rotate if needed
	GLTFModel::DrawModel(gltfModel, "models/test/");
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
	GLTFModel::LoadModel("Models/test/scene.gltf", gltfModel);

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
