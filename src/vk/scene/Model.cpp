#include "vk/scene/Model.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace vkfw {
namespace {

static glm::vec3 ToVec3(aiVector3D const& v) { return {v.x, v.y, v.z}; }

} // namespace

Model Model::LoadFromFile(std::string const& path)
{
  Assimp::Importer importer;

  unsigned int const flags =
      aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality | aiProcess_GenNormals |
      aiProcess_PreTransformVertices;

  aiScene const* scene = importer.ReadFile(path, flags);
  if (!scene || !scene->mRootNode)
    throw std::runtime_error("Assimp ReadFile failed: " + path + " : " + importer.GetErrorString());

  Model model{};

  glm::vec3 minp{std::numeric_limits<float>::max()};
  glm::vec3 maxp{std::numeric_limits<float>::lowest()};

  for (unsigned int mi = 0; mi < scene->mNumMeshes; ++mi) {
    aiMesh const* mesh = scene->mMeshes[mi];
    if (!mesh || mesh->mNumVertices == 0)
      continue;

    uint32_t const baseVertex = static_cast<uint32_t>(model.vertices.size());

    model.vertices.reserve(model.vertices.size() + mesh->mNumVertices);
    for (unsigned int vi = 0; vi < mesh->mNumVertices; ++vi) {
      Vertex v{};

      v.pos = ToVec3(mesh->mVertices[vi]);

      if (mesh->mNormals)
        v.normal = ToVec3(mesh->mNormals[vi]);
      else
        v.normal = glm::vec3{0.0f, 1.0f, 0.0f};

      if (mesh->mTextureCoords[0]) {
        v.uv = glm::vec2{mesh->mTextureCoords[0][vi].x, mesh->mTextureCoords[0][vi].y};
      } else {
        v.uv = glm::vec2{0.0f, 0.0f};
      }

      minp = glm::min(minp, v.pos);
      maxp = glm::max(maxp, v.pos);

      model.vertices.push_back(v);
    }

    for (unsigned int fi = 0; fi < mesh->mNumFaces; ++fi) {
      aiFace const& f = mesh->mFaces[fi];
      if (f.mNumIndices != 3) continue;

      model.indices.push_back(baseVertex + static_cast<uint32_t>(f.mIndices[0]));
      model.indices.push_back(baseVertex + static_cast<uint32_t>(f.mIndices[1]));
      model.indices.push_back(baseVertex + static_cast<uint32_t>(f.mIndices[2]));
    }
  }

  if (model.vertices.empty() || model.indices.empty())
    throw std::runtime_error("Model has no drawable geometry: " + path);

  model.center = 0.5f * (minp + maxp);
  float const eps = 1e-6f;
  model.radius = std::max(eps, glm::length(maxp - model.center));
  return model;
}

} // namespace vkfw
