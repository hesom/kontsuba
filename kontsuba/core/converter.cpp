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
#include <zlib.h>
#include "principled_brdf.h"
#include "utils.h"

namespace Kontsuba {
using namespace tinyxml2;
namespace fs = std::filesystem;

class Converter {
public:
  Converter(const std::string &inputFile,
                 const std::string &outputDirectory, const unsigned int &flags)
      : m_inputFile(inputFile), m_outputDirectory(outputDirectory),
        m_importer(), m_xmlDoc() {
    importingFlags = flags;
    m_fromDir = fs::canonical(expand(inputFile));
    if (!fs::is_directory(m_fromDir)) {
      m_fromDir = m_fromDir.parent_path();
    }
    m_inputFile = fs::canonical(expand(inputFile));
    m_outputDirectory = fs::canonical(expand(outputDirectory));

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
  void duplicateFaceRemover(std::vector<uint32_t>& indices, std::vector<aiVector3D>& vertices);
  void writeMeshPly(const aiMesh *mesh, const std::string &path, bool removeDuplicateFaces = false);
  void writeMeshSerialized(const aiMesh *mesh, const std::string &path, bool removeDuplicateFaces = false);
  

  auto constructNode(const std::string &type, const std::string &name,
                     const std::string &value) {
    tinyxml2::XMLElement *node = m_xmlDoc.NewElement(type.c_str());
    node->SetAttribute("name", name.c_str());
    node->SetAttribute("value", value.c_str());
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
  unsigned int importingFlags;
};

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

void Converter::duplicateFaceRemover(std::vector<uint32_t> &indices, std::vector<aiVector3D> &vertices) {
    using FaceData = std::tuple<uint32_t, uint32_t, uint32_t>; // indices

    std::vector<FaceData> faces;
    for (int i = 0; i < indices.size(); i += 3) {
        faces.push_back({ indices[i], indices[i + 1], indices[i + 2] });
    }

    auto equal = [&](const FaceData& f1, const FaceData& f2) {
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

        auto test = [=](const aiVector3D& v, const aiVector3D& w) {
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

    auto hash = [&](const FaceData& f) {
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
    for (const auto& face : faceSet) {
        auto [i1, i2, i3] = face;
        uniqueIndices.push_back(i1);
        uniqueIndices.push_back(i2);
        uniqueIndices.push_back(i3);
    }
    indices = uniqueIndices;
}

void Converter::writeMeshPly(const aiMesh *mesh,
                             const std::string &filename,
                             bool removeDuplicateFaces) {
                                    
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
      throw std::runtime_error("only triangles are supported. Number of Vertices: " +
        std::to_string(numIndices) + " in Mesh: " + mesh->mName.C_Str());
    }
    for (unsigned int j = 0; j < numIndices; j++) {
      indices.push_back(face.mIndices[j]);
    }
  }

  if (removeDuplicateFaces) {
      duplicateFaceRemover(indices, vertices);
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
      "face", {"vertex_indices"}, tinyply::Type::UINT32, indices.size() / 3,
      reinterpret_cast<uint8_t *>(indices.data()), tinyply::Type::UINT8,
      3);

  meshFile.get_comments().push_back("generated by kontsuba");
  meshFile.write(outstream_binary, true);
}

void Converter::writeMeshSerialized(const aiMesh *mesh,
                             const std::string &filename,
                             bool removeDuplicateFaces) {

  std::filebuf fb_binary;
  fb_binary.open(filename, std::ios::out | std::ios::binary);
  std::ostream outstream_binary(&fb_binary);
  if (outstream_binary.fail()) {
    throw std::runtime_error("failed to open " + filename);
  }

  uint16_t fileformatHeader = 0x041C;
  char* fileformatHeaderChar = static_cast<char*>(static_cast<void*>(&fileformatHeader));

  uint16_t fileformatVersionV4 = 0x0004;
  char* fileformatVersionV4Char = static_cast<char*>(static_cast<void*>(&fileformatVersionV4));

  outstream_binary.write(fileformatHeaderChar, 2);
  outstream_binary.write(fileformatVersionV4Char, 2);

  //get all information ready for the compressed stream
  uint32_t meshFlags = 0x0000;

  if (mesh->HasNormals()){
    meshFlags |= 0x0001;
  }
  if (mesh->HasTextureCoords(0) && mesh->mNumUVComponents[0] == 2) { //Mitsuba can only do 2D tex coords therefore we will only load any if they are 2D
    meshFlags |= 0x0002; //additionally only one set of UV coordinates is possible for a shape in Mitsuba, we'll take the first
  }
  if (mesh->HasVertexColors(0)){
    meshFlags |= 0x0008;
  }
  meshFlags |= 0x1000; //this makes everything single precision (Mitsuba could work with dp, but assimp not)

  std::vector<char> enflatedData; //this has to be filled with little endian data; guaranteed by my char* cast
  
  //estimate size enflatedData will need
  uint64_t size = 20 + mesh->mName.length + 1; //flags, numVert, numTri and name
  size += sizeof(float) * mesh->mNumVertices * 3;
  if (mesh->HasNormals()) {
      size += sizeof(float) * mesh->mNumVertices * 3;
  }
  if (mesh->HasTextureCoords(0) && mesh->mNumUVComponents[0] == 2) {
      size += sizeof(float) * mesh->mNumVertices * 2;
  }
  if (mesh->HasVertexColors(0)) {
      size += sizeof(float) * mesh->mNumVertices * 3;
  }
  size += sizeof(int32_t) * mesh->mNumFaces * 3;

  enflatedData.reserve(size); //this should hopefully prevent enflatedData ever needing a reallocation


  char* meshFlagsChar = static_cast<char*>(static_cast<void*>(&meshFlags));
  enflatedData.insert(enflatedData.end(), meshFlagsChar, meshFlagsChar + 4);

  //add the name of the mesh to the data
  char* meshName = new char [mesh->mName.length + 1]; //an aiString length excludes the terminal 0, but Mitsuba wants it
  //TODO strcpy is unsafe, replace with strcpy_s or smth like that
  strcpy(meshName, mesh->mName.C_Str()); //this is based on an assimp aiString (which is UTF-8) and therefore no more conversion needed
  enflatedData.insert(enflatedData.end(), meshName, meshName + mesh->mName.length + 1);

  //add the number of vertices of the mesh to the data
  uint64_t numVert = mesh->mNumVertices; 
  char* numVertChar = static_cast<char*>(static_cast<void*>(&numVert));
  enflatedData.insert(enflatedData.end(), numVertChar, numVertChar + 8);

  //add the number of faces of the mesh to the data
  uint64_t numTri = mesh->mNumFaces;
  char* numTriChar = static_cast<char*>(static_cast<void*>(&numTri));
  enflatedData.insert(enflatedData.end(), numTriChar, numTriChar + 8);

  //add all vertex positions to the data (for now only single precision)
  char* verticesChar = static_cast<char*>(static_cast<void*>(mesh->mVertices));
  enflatedData.insert(enflatedData.end(), verticesChar, verticesChar + sizeof(float) * numVert * 3);

  //if needed, add all vertex normals
  if (mesh->HasNormals()){
    char* normalChar = static_cast<char*>(static_cast<void*>(mesh->mNormals));
    enflatedData.insert(enflatedData.end(), normalChar, normalChar + sizeof(float) * numVert * 3);
  }

  //if needed, add all texture coordinates
  if (mesh->HasTextureCoords(0) && mesh->mNumUVComponents[0] == 2){
    //Mitsuba expects 2D coordinates, no 1D or 3D textures possible.
    std::vector<float> textureCoords;
    textureCoords.reserve(numVert * 2);
    for (int i = 0; i < numVert; i++){
      textureCoords.push_back(mesh->mTextureCoords[0][i].x);
      textureCoords.push_back(mesh->mTextureCoords[0][i].y);
    }
    char* textureCoordsChar = static_cast<char*>(static_cast<void*>(textureCoords.data()));
    enflatedData.insert(enflatedData.end(), textureCoordsChar, textureCoordsChar + sizeof(float) * numVert * 2);
  }

  //if needed, add all vertex colors
  if (mesh->HasVertexColors(0)) {
      //note: assimp can do rgba, Mitsuba only rgb
      std::vector<float> vertexColors;
      vertexColors.reserve(numVert * 3);
      for (int i = 0; i < numVert; i++) {
          vertexColors.push_back(mesh->mColors[0][i].r);
          vertexColors.push_back(mesh->mColors[0][i].g);
          vertexColors.push_back(mesh->mColors[0][i].b);
      }
      char* vertexColorsChar = static_cast<char*>(static_cast<void*>(vertexColors.data()));
      enflatedData.insert(enflatedData.end(), vertexColorsChar, vertexColorsChar + sizeof(float) * numVert * 3);
  }

  //finally: add all index data
  //for over uint32_max vertices this would need to use uint64 BUT assimp's maximum vertex count is 2^31-1
  std::vector<uint32_t> indices;
  indices.reserve(mesh->mNumFaces * 3);
  for (int i = 0; i < mesh->mNumFaces; i++) {
      indices.push_back(mesh->mFaces[i].mIndices[0]);
      indices.push_back(mesh->mFaces[i].mIndices[1]);
      indices.push_back(mesh->mFaces[i].mIndices[2]);
  }

  if (removeDuplicateFaces) {
      std::vector<aiVector3D> vertices;
      for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
          vertices.push_back({ mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z });
      }
      duplicateFaceRemover(indices, vertices);
  }

  char* indicesChar = static_cast<char*>(static_cast<void*>(&indices[0]));
  enflatedData.insert(enflatedData.end(), indicesChar, indicesChar + sizeof(uint32_t) * numTri * 3);



  std::vector<char> deflatedData;
  deflatedData.resize(enflatedData.size() * 1.1); //the compressed data will of course be smaller, but zlib needs space to work
  //also this gets resized and not reserved because if we resize later on after zlib put compressed data into in,
  //the vector does not know this, assumes size = 0 and overwrites all data with default insertible

  //time to prepare deflating the data
  z_stream deflateStream;
  deflateStream.zalloc = Z_NULL;
  deflateStream.zfree = Z_NULL;
  deflateStream.opaque = Z_NULL;

  deflateStream.avail_in = (uint32_t)enflatedData.size();
  deflateStream.next_in = (Bytef*)enflatedData.data();
  deflateStream.avail_out = deflatedData.capacity();
  deflateStream.next_out = (Bytef*)deflatedData.data();

  //the actual deflation
  deflateInit(&deflateStream, Z_BEST_COMPRESSION);
  deflate(&deflateStream, Z_FINISH);
  deflateEnd(&deflateStream);
  
  deflatedData.resize(deflateStream.total_out);

  outstream_binary.write(deflatedData.data(), deflatedData.size());

  //and finally the End-of-file dictionary, uncompressed (given I understand the mitsuba docs correctly)
  //this is very simple for now since we're only saving *one* mesh
  uint64_t firstMeshOffset = 0;
  char* firstMeshOffsetChar = static_cast<char*>(static_cast<void*>(&firstMeshOffset));
  outstream_binary.write(firstMeshOffsetChar, 8);

  uint32_t meshCount = 1;
  char* meshCountChar = static_cast<char*>(static_cast<void*>(&meshCount));
  outstream_binary.write(meshCountChar, 4);

  fb_binary.close();

  return;

}

void Converter::convert() {

  unsigned int aiFlags = 0;

  if ((importingFlags & 0x1) > 0)
      aiFlags |= aiProcess_MakeLeftHanded;
  if ((importingFlags & 0x2) > 0)
      aiFlags |= aiProcess_FlipUVs;

  std::cout << aiFlags << std::endl;

  // clang-format off
  const aiScene *scene = m_importer.ReadFile(m_inputFile.string(),
    aiProcess_Triangulate           |
    aiProcess_JoinIdenticalVertices |
    aiProcess_FindDegenerates       |
    aiProcess_FixInfacingNormals    |
    aiProcess_PreTransformVertices  |
    //aiProcess_FlipUVs             |
    aiProcess_TransformUVCoords     |
    aiProcess_SortByPType           |
    aiFlags
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

  // TODO remove this when we have proper emitter loading
  auto backgroundNode = m_xmlDoc.NewElement("emitter");
  backgroundNode->SetAttribute("type", "constant");
  auto backgroundIntensityNode = constructNode("rgb", "radiance", "1.0");
  backgroundNode->InsertEndChild(backgroundIntensityNode);
  m_xmlRoot->InsertEndChild(backgroundNode);

  // loop over all materials in scene
  for (size_t i = 0; i < scene->mNumMaterials; i++) {
    int materialType = 0;
    scene->mMaterials[i]->Get(AI_MATKEY_SHADING_MODEL, materialType);
    //std::cout << "brdf is: " << materialType << std::endl;
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
    std::string plySceneFileName = "meshes/" + plyName.filename().string();

    try {
      writeMeshPly(mesh, plyName.string());
      auto meshNode = m_xmlDoc.NewElement("shape");
      meshNode->SetAttribute("type", "ply");
      auto filenameNode =
          constructNode("string", "filename", plySceneFileName.c_str());
      meshNode->InsertEndChild(filenameNode);
      auto refNode = m_xmlDoc.NewElement("ref");
      refNode->SetAttribute("id", name.C_Str());
      meshNode->InsertEndChild(refNode);

      m_xmlRoot->InsertEndChild(meshNode);
    } catch(std::exception& e) {
      std::cout << "Warning: " << e.what() << std::endl;
    }

  }

  m_xmlDoc.SaveFile(m_outputSceneDescPath.string().c_str());
}

void convert(const std::string &inputFile, const std::string &outputDirectory, const unsigned int flags) {
  Converter converter(inputFile, outputDirectory, flags);
  converter.convert();
}

} // namespace Kontsuba