//#include "GLTFModel.h"
//#include <iostream>
//#include <glm/gtc/type_ptr.hpp>
//#include <glut.h>
//
//std::unordered_map<int, GLuint> GLTFModel::textureCache;
//
//bool GLTFModel::LoadModel(const std::string& filename, tinygltf::Model& model) {
//    tinygltf::TinyGLTF loader;
//    std::string err, warn;
//
//    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename);
//
//    if (!warn.empty()) {
//        std::cerr << "GLTF loading warning: " << warn << std::endl;
//    }
//
//    if (!err.empty()) {
//        std::cerr << "GLTF loading error: " << err << std::endl;
//    }
//
//    if (!ret) {
//        std::cerr << "Failed to load glTF: " << filename << std::endl;
//        return false;
//    }
//
//    return true;
//}
//
//void GLTFModel::DrawModel(const tinygltf::Model& model, const glm::mat4& transform) {
//    const tinygltf::Scene& scene = model.scenes[model.defaultScene];
//    for (size_t i = 0; i < scene.nodes.size(); ++i) {
//        DrawNode(model, scene.nodes[i], transform);
//    }
//}
//
//void GLTFModel::DrawNode(const tinygltf::Model& model, int nodeIndex, const glm::mat4& parentTransform) {
//    const tinygltf::Node& node = model.nodes[nodeIndex];
//    glm::mat4 localTransform = glm::mat4(1.0f);
//
//    if (node.matrix.size() == 16) {
//        localTransform = glm::make_mat4(node.matrix.data());
//    }
//    else {
//        if (node.translation.size() == 3) {
//            localTransform = glm::translate(localTransform,
//                glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
//        }
//
//        if (node.rotation.size() == 4) {
//            glm::quat q = glm::quat(node.rotation[3], node.rotation[0],
//                node.rotation[1], node.rotation[2]);
//            localTransform = localTransform * glm::mat4_cast(q);
//        }
//
//        if (node.scale.size() == 3) {
//            localTransform = glm::scale(localTransform,
//                glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
//        }
//    }
//
//    glm::mat4 nodeTransform = parentTransform * localTransform;
//
//    if (node.mesh >= 0) {
//        DrawMesh(model, model.meshes[node.mesh], nodeTransform);
//    }
//
//    for (int child : node.children) {
//        DrawNode(model, child, nodeTransform);
//    }
//}
//
//void GLTFModel::DrawMesh(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const glm::mat4& transform) {
//    glPushMatrix();
//    glMultMatrixf(glm::value_ptr(transform));
//
//    for (const auto& primitive : mesh.primitives) {
//        if (primitive.indices < 0) continue;
//
//        if (primitive.material >= 0) {
//            const auto& material = model.materials[primitive.material];
//            SetMaterial(model, material);
//        }
//
//        const auto& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
//        const auto& posView = model.bufferViews[posAccessor.bufferView];
//        const float* positions = reinterpret_cast<const float*>(
//            &model.buffers[posView.buffer].data[posView.byteOffset + posAccessor.byteOffset]);
//
//        const float* texcoords = nullptr;
//        if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
//            const auto& texAccessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
//            const auto& texView = model.bufferViews[texAccessor.bufferView];
//            texcoords = reinterpret_cast<const float*>(
//                &model.buffers[texView.buffer].data[texView.byteOffset + texAccessor.byteOffset]);
//        }
//
//        const auto& indexAccessor = model.accessors[primitive.indices];
//        const auto& indexView = model.bufferViews[indexAccessor.bufferView];
//        const void* indices = &model.buffers[indexView.buffer].data[indexView.byteOffset +
//            indexAccessor.byteOffset];
//
//        glBegin(GL_TRIANGLES);
//        for (size_t i = 0; i < indexAccessor.count; i++) {
//            unsigned int idx;
//            switch (indexAccessor.componentType) {
//            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
//                idx = ((unsigned short*)indices)[i];
//                break;
//            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
//                idx = ((unsigned int*)indices)[i];
//                break;
//            default:
//                continue;
//            }
//
//            if (texcoords) {
//                glTexCoord2f(texcoords[idx * 2], texcoords[idx * 2 + 1]);
//            }
//            glVertex3fv(&positions[idx * 3]);
//        }
//        glEnd();
//    }
//
//    glPopMatrix();
//}
//
//void GLTFModel::SetMaterial(const tinygltf::Model& model, const tinygltf::Material& material) {
//    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
//
//    if (material.pbrMetallicRoughness.baseColorTexture.index >= 0) {
//        const auto& texture = model.textures[material.pbrMetallicRoughness.baseColorTexture.index];
//        if (texture.source >= 0) {
//            GLuint textureId = GetOrCreateTexture(model, texture.source);
//            if (textureId != 0) {
//                glEnable(GL_TEXTURE_2D);
//                glBindTexture(GL_TEXTURE_2D, textureId);
//            }
//        }
//    }
//    else if (!material.pbrMetallicRoughness.baseColorFactor.empty()) {
//        glColor4f(
//            material.pbrMetallicRoughness.baseColorFactor[0],
//            material.pbrMetallicRoughness.baseColorFactor[1],
//            material.pbrMetallicRoughness.baseColorFactor[2],
//            material.pbrMetallicRoughness.baseColorFactor[3]
//        );
//    }
//}
//
//GLuint GLTFModel::GetOrCreateTexture(const tinygltf::Model& model, int sourceIndex) {
//    if (textureCache.find(sourceIndex) != textureCache.end()) {
//        return textureCache[sourceIndex];
//    }
//
//    const auto& image = model.images[sourceIndex];
//    GLuint textureId;
//    glGenTextures(1, &textureId);
//    glBindTexture(GL_TEXTURE_2D, textureId);
//
//    GLenum format = (image.component == 3) ? GL_RGB : GL_RGBA;
//
//    glTexImage2D(GL_TEXTURE_2D, 0, format, image.width, image.height, 0, format,
//        GL_UNSIGNED_BYTE, &image.image[0]);
//
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//
//    gluBuild2DMipmaps(GL_TEXTURE_2D, format, image.width, image.height, format, GL_UNSIGNED_BYTE, &image.image[0]);
//
//    textureCache[sourceIndex] = textureId;
//    return textureId;
//}
