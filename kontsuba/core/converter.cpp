#include "converter.h"
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <tinyply.h>
#include <tinyxml2.h>
#include <fmt/core.h>
#include "principled_brdf.h"

namespace Kontsuba {
using namespace tinyxml2;
namespace fs = std::filesystem;

class Converter {
public:
  Converter(const std::string &inputFile,
                 const std::string &outputDirectory)
      : m_inputFile(inputFile), m_outputDirectory(outputDirectory),
        m_importer(), m_xmlDoc() {
    m_fromDir = fs::canonical(inputFile);
    if (!fs::is_directory(m_fromDir)) {
      m_fromDir = m_fromDir.parent_path();
    }

    m_outputMeshPath = m_outputDirectory / "meshes";
    m_outputTexturePath = m_outputDirectory / "textures";
    m_outputSceneDescPath = m_outputDirectory / "scene.xml";

    m_xmlRoot = m_xmlDoc.NewElement("scene");
    m_xmlRoot->SetAttribute("version", "3.0.0");
    m_xmlDoc.InsertFirstChild(m_xmlRoot);
  }

  void convert();

private:
  XMLElement *defaultIntegrator();
  XMLElement *defaultSensor();
  XMLElement *defaultLighting();
  XMLElement *materialToBSDFNode(const aiMaterial *material);
  void writeMeshPly(const aiMesh *mesh, const std::string &path);

  auto probeMaterialTexture(const aiMaterial *material, aiTextureType type) {
    aiString path;
    if (material->GetTextureCount(type) != 0) {
      if (material->GetTexture(type, 0, &path) == aiReturn_SUCCESS) {
        return std::optional<std::string>(path.C_Str());
      }
    }
    return std::optional<std::string>();
  }

  template <typename T>
  auto probeMaterialProperty(const aiMaterial *material, const char *pKey,
                             unsigned int type, unsigned int idx) {
    T value;
    if (material->Get(pKey, type, idx, value) == aiReturn_SUCCESS) {
      return std::optional<T>(value);
    }
    return std::optional<T>();
  }

  template <typename T> auto translateValue(const T value) {
    return std::to_string(value);
  }

  auto constructNode(const std::string &type, const std::string &name,
                     const std::string &value) {
    tinyxml2::XMLElement *node = m_xmlDoc.NewElement(type.c_str());
    node->SetAttribute("name", name.c_str());
    node->SetAttribute("value", value.c_str());
    return node;
  }

  template <typename T>
  auto valueOrTextureNode(const std::string &type, const std::string &name,
                          T value, const std::optional<std::string> &texture) {
    if (texture) {
      auto node = m_xmlDoc.NewElement("texture");
      node->SetAttribute("type", "bitmap");
      node->SetAttribute("name", name.c_str());
      auto textureFileName = fs::path(texture.value()).filename();
      auto filenameNode = constructNode("string", "filename",
                                        "textures/" + textureFileName.string());
      node->InsertEndChild(filenameNode);
      return node;
    }

    auto node = constructNode(type, name, translateValue(value));
    return node;
  }

  Assimp::Importer m_importer;
  XMLDocument m_xmlDoc;
  XMLElement *m_xmlRoot;
  fs::path m_inputFile;
  fs::path m_fromDir;
  fs::path m_outputDirectory;
  fs::path m_outputMeshPath;
  fs::path m_outputTexturePath;
  fs::path m_outputSceneDescPath;
};

template <> auto Converter::translateValue(const aiColor3D value) {
  return std::to_string(value.r) + " " + std::to_string(value.g) + " " +
         std::to_string(value.b);
}

XMLElement *Converter::defaultIntegrator() {
  auto integrator = m_xmlDoc.NewElement("integrator");
  integrator->SetAttribute("type", "path");
  auto maxDepthNode = constructNode("integer", "max_depth", "3");
  integrator->InsertEndChild(maxDepthNode);
  return integrator;
}

XMLElement *Converter::defaultLighting() {
  auto emitterNode = m_xmlDoc.NewElement("emitter");
  emitterNode->SetAttribute("type", "point");
  auto intensityNode = constructNode("rgb", "intensity", "10");
  emitterNode->InsertEndChild(intensityNode);
  auto positionNode = constructNode("point", "position", "2, 2, 2");
  emitterNode->InsertEndChild(positionNode);
  return emitterNode;
}

XMLElement *Converter::defaultSensor() {
  auto sensorNode = m_xmlDoc.NewElement("sensor");
  sensorNode->SetAttribute("type", "perspective");
  auto fovNode = constructNode("float", "fov", "45");
  sensorNode->InsertEndChild(fovNode);
  auto toWorldNode = m_xmlDoc.NewElement("transform");
  toWorldNode->SetAttribute("name", "to_world");
  auto lookAtNode = m_xmlDoc.NewElement("lookat");
  lookAtNode->SetAttribute("origin", "1, 1, 0");
  lookAtNode->SetAttribute("target", "0, 0, 0");
  lookAtNode->SetAttribute("up", "0, 0, 1");
  toWorldNode->InsertEndChild(lookAtNode);
  sensorNode->InsertEndChild(toWorldNode);

  auto samplerNode = m_xmlDoc.NewElement("sampler");
  samplerNode->SetAttribute("type", "independent");
  auto sampleCountNode = constructNode("integer", "sample_count", "32");
  samplerNode->InsertEndChild(sampleCountNode);
  sensorNode->InsertEndChild(samplerNode);

  auto filmNode = m_xmlDoc.NewElement("film");
  filmNode->SetAttribute("type", "hdrfilm");
  auto widthNode = constructNode("integer", "width", "512");
  filmNode->InsertEndChild(widthNode);
  auto heightNode = constructNode("integer", "height", "512");
  filmNode->InsertEndChild(heightNode);
  auto pixelFormatNode = constructNode("string", "pixel_format", "rgb");
  filmNode->InsertEndChild(pixelFormatNode);
  sensorNode->InsertEndChild(filmNode);

  return sensorNode;
}

void Converter::writeMeshPly(const aiMesh *mesh,
                                  const std::string &filename) {

  std::filebuf fb_binary;
  fb_binary.open(filename, std::ios::out | std::ios::binary);
  std::ostream outstream_binary(&fb_binary);
  if (outstream_binary.fail()) {
    throw std::runtime_error("failed to open " + filename);
  }

  tinyply::PlyFile meshFile;
  std::vector<aiVector3D> vertices;
  std::vector<aiVector3D> normals;
  for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
    vertices.push_back(
        {mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z});
    if (mesh->HasNormals()) {
      normals.push_back(
          {mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z});
    }
  }

  std::vector<uint32_t> indices;
  for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
    const aiFace &face = mesh->mFaces[i];
    auto numIndices = face.mNumIndices;
    if (numIndices != 3) {
      throw std::runtime_error("only triangles are supported");
    }
    for (unsigned int j = 0; j < numIndices; j++) {
      indices.push_back(face.mIndices[j]);
    }
  }

  using FaceData = std::tuple<uint32_t, uint32_t, uint32_t>; // indices

  std::vector<FaceData> faces;
  for (int i = 0; i < indices.size(); i += 3) {
    faces.push_back({indices[i], indices[i + 1], indices[i + 2]});
  }

  auto equal = [&](const FaceData &f1, const FaceData &f2) {
    float epsilon = 0.0001f;
    auto [i1, i2, i3] = f1;
    auto [j1, j2, j3] = f2;

    // get vertices from indices
    auto v1 = vertices[i1];
    auto v2 = vertices[i2];
    auto v3 = vertices[i3];

    auto w1 = vertices[j1];
    auto w2 = vertices[j2];
    auto w3 = vertices[j3];

    auto test = [=](const aiVector3D &v, const aiVector3D &w) {
      return abs(v.x - w.x) < epsilon && abs(v.y - w.y) < epsilon &&
             abs(v.z - w.z) < epsilon;
    };

    // test distance smaller than epsilon for every permutation (3! = 6)
    if (test(v1, w1) && test(v2, w2) && test(v3, w3)) {
      return true;
    }
    if (test(v1, w1) && test(v2, w3) && test(v3, w2)) {
      return true;
    }
    if (test(v1, w2) && test(v2, w1) && test(v3, w3)) {
      return true;
    }
    if (test(v1, w2) && test(v2, w3) && test(v3, w1)) {
      return true;
    }
    if (test(v1, w3) && test(v2, w1) && test(v3, w2)) {
      return true;
    }
    if (test(v1, w3) && test(v2, w2) && test(v3, w1)) {
      return true;
    }

    return false;
  };

  auto hash = [&](const FaceData &f) {
    auto [i1, i2, i3] = f;
    auto v1 = vertices[i1];
    auto v2 = vertices[i2];
    auto v3 = vertices[i3];

    return std::hash<float>()(v1.x) ^ std::hash<float>()(v1.y) ^
           std::hash<float>()(v1.z) ^ std::hash<float>()(v2.x) ^
           std::hash<float>()(v2.y) ^ std::hash<float>()(v2.z) ^
           std::hash<float>()(v3.x) ^ std::hash<float>()(v3.y) ^
           std::hash<float>()(v3.z);
  };

  using FaceSet = std::unordered_set<FaceData, decltype(hash), decltype(equal)>;
  FaceSet faceSet(faces.begin(), faces.end(), 0, hash, equal);

  std::vector<uint32_t> uniqueIndices;
  for (const auto &face : faceSet) {
    auto [i1, i2, i3] = face;
    uniqueIndices.push_back(i1);
    uniqueIndices.push_back(i2);
    uniqueIndices.push_back(i3);
  }

  meshFile.add_properties_to_element(
      "vertex", {"x", "y", "z"}, tinyply::Type::FLOAT32, mesh->mNumVertices,
      reinterpret_cast<uint8_t *>(vertices.data()), tinyply::Type::INVALID, 0);

  if (mesh->HasNormals()) {
    meshFile.add_properties_to_element(
        "vertex", {"nx", "ny", "nz"}, tinyply::Type::FLOAT32,
        mesh->mNumVertices, reinterpret_cast<uint8_t *>(normals.data()),
        tinyply::Type::INVALID, 0);
  }

  std::vector<aiVector2D>
      texCoords; // this needs to be in the same scope as meshFile
  if (mesh->HasTextureCoords(0)) {
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
      texCoords.push_back(
          {mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y});
    }
    meshFile.add_properties_to_element(
        "vertex", {"u", "v"}, tinyply::Type::FLOAT32, mesh->mNumVertices,
        reinterpret_cast<uint8_t *>(texCoords.data()), tinyply::Type::INVALID,
        0);
  }

  meshFile.add_properties_to_element(
      "face", {"vertex_indices"}, tinyply::Type::UINT32, faceSet.size(),
      reinterpret_cast<uint8_t *>(uniqueIndices.data()), tinyply::Type::UINT8,
      3);

  meshFile.get_comments().push_back("generated by kontsuba");
  meshFile.write(outstream_binary, true);
}

void Converter::convert() {
  // clang-format off
  const aiScene *scene = m_importer.ReadFile(m_inputFile,
    aiProcess_Triangulate           |
    aiProcess_JoinIdenticalVertices |
    aiProcess_FindDegenerates       |
    aiProcess_FixInfacingNormals    |
    aiProcess_PreTransformVertices  |
    aiProcess_FlipUVs               |
    aiProcess_TransformUVCoords     |
    aiProcess_SortByPType
  );
  // clang-format on

  if (!scene) {
    throw std::runtime_error(m_importer.GetErrorString());
  }

  fs::create_directories(m_outputDirectory);
  fs::create_directories(m_outputMeshPath);
  fs::create_directories(m_outputTexturePath);

  auto integratorNode = defaultIntegrator();
  auto lightingNode = defaultLighting();
  auto sensorNode = defaultSensor();

  m_xmlRoot->InsertEndChild(integratorNode);
  m_xmlRoot->InsertEndChild(lightingNode);
  m_xmlRoot->InsertEndChild(sensorNode);

  // loop over all materials in scene
  for (size_t i = 0; i < scene->mNumMaterials; i++) {
    auto brdf = PrincipledBRDF::fromMaterial(scene->mMaterials[i], true);
    auto materialNode = toXML(m_xmlDoc, brdf);
    m_xmlRoot->InsertEndChild(materialNode);

    for(const auto& texture : brdf.textures) {
      // copy texture to new location
      fs::copy_file(m_fromDir / texture,
        m_outputTexturePath / fs::path(texture).filename(),
        fs::copy_options::overwrite_existing);
    }
  }

  // loop over all meshes in scene
  for (size_t i = 0; i < scene->mNumMeshes; i++) {
    aiMesh *mesh = scene->mMeshes[i];

    aiString name;
    auto plyName = m_outputMeshPath / ("mesh" + std::to_string(i) + ".ply");
    scene->mMaterials[mesh->mMaterialIndex]->Get(AI_MATKEY_NAME, name);
    auto plySceneFileName = fs::relative(plyName, m_outputDirectory).string();

    auto meshNode = m_xmlDoc.NewElement("shape");
    meshNode->SetAttribute("type", "ply");
    auto filenameNode =
        constructNode("string", "filename", plySceneFileName.c_str());
    meshNode->InsertEndChild(filenameNode);
    auto refNode = m_xmlDoc.NewElement("ref");
    refNode->SetAttribute("id", name.C_Str());
    meshNode->InsertEndChild(refNode);

    m_xmlRoot->InsertEndChild(meshNode);

    writeMeshPly(mesh, plyName);
  }

  m_xmlDoc.SaveFile(m_outputSceneDescPath.c_str());
}

void convert(const std::string &inputFile, const std::string &outputDirectory) {
  Converter converter(inputFile, outputDirectory);
  converter.convert();
}

} // namespace Kontsuba