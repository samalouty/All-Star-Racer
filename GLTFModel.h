//#pragma once
//#ifndef GLTFMODEL_H
//#define GLTFMODEL_H
//
//#include "tiny_gltf.h"
//#include <glm/glm.hpp>
//#include <unordered_map>
//#include <string>
//
//// OpenGL headers for GLuint and related types
//#ifdef _WIN32
//#include <windows.h> // For Windows systems
//#endif
//#include <GL/gl.h>  // OpenGL header for GLuint
//#include <GL/glu.h> // Optional for OpenGL Utility Library (if using glu functions)
//
//
//class GLTFModel {
//public:
//    static bool LoadModel(const std::string& filename, tinygltf::Model& model);
//    static void DrawModel(const tinygltf::Model& model, const glm::mat4& transform = glm::mat4(1.0f));
//
//private:
//    static std::unordered_map<int, GLuint> textureCache;
//
//    static void DrawNode(const tinygltf::Model& model, int nodeIndex, const glm::mat4& parentTransform);
//    static void DrawMesh(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const glm::mat4& transform);
//    static void SetMaterial(const tinygltf::Model& model, const tinygltf::Material& material);
//    static GLuint GetOrCreateTexture(const tinygltf::Model& model, int sourceIndex);
//};
//
//#endif // GLTFMODEL_H
