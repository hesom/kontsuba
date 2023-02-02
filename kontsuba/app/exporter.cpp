#include "exporter.h"
#include <iostream>
#include <fstream>
#include <tinyxml2.h>
#include <assimp/Exporter.hpp>
#include <assimp/SceneCombiner.h>
#include <tinyply.h>
#include <filesystem>
#include <optional>
#include <unordered_set>
#include <random>

namespace fs = std::filesystem;

auto constructNode(tinyxml2::XMLDocument& doc, const std::string& type, const std::string& name, const std::string& value){
    tinyxml2::XMLElement* node = doc.NewElement(type.c_str());
    node->SetAttribute("name", name.c_str());
    node->SetAttribute("value", value.c_str());
    return node;
}

auto probeMaterialTexture(const aiMaterial* material, aiTextureType type){
    aiString path;
    if(material->GetTextureCount(type) != 0){
        if(material->GetTexture(type, 0, &path) == aiReturn_SUCCESS){
            return std::optional<std::string>(path.C_Str());
        }
    }
    return std::optional<std::string>();
}

template<typename T>
auto probeMaterialProperty(const aiMaterial* material, const char* pKey, unsigned int type, unsigned int idx){
    T value;
    if(material->Get(pKey, type, idx, value) == aiReturn_SUCCESS){
        return std::optional<T>(value);
    }
    return std::optional<T>();
}

template<typename T>
auto translateValue(const T value){
    return std::to_string(value);
}

template<>
auto translateValue(const aiColor3D value){
    return std::to_string(value.r) + ", " + std::to_string(value.g) + ", " + std::to_string(value.b);
}

template<typename T>
auto valueOrTexture(const T value, const std::optional<std::string>& texture){
    if(texture){
        return texture.value();
    }else{
        return translateValue(value);
    }
}

template<typename T>
auto valueOrTextureNode(tinyxml2::XMLDocument& doc,
                        const std::string& type,
                        const std::string& name,
                        T value,
                        const std::optional<std::string>& texture){
    if(texture){
        auto node = doc.NewElement("texture");
        node->SetAttribute("type", "bitmap");
        node->SetAttribute("name", name.c_str());
        auto textureFileName = fs::path(texture.value()).filename();
        auto filenameNode = constructNode(doc, "string", "filename", "textures/" + textureFileName.string());
        node->InsertEndChild(filenameNode);
        return node;
    }
    
    auto node = constructNode(doc, type, name, translateValue(value));
    return node;
}

void writeMeshPly(aiMesh* mesh, const std::string& filename){
    
    std::filebuf fb_binary;
    fb_binary.open(filename, std::ios::out | std::ios::binary);
    std::ostream outstream_binary(&fb_binary);
    if(outstream_binary.fail()){
        throw std::runtime_error("failed to open " + filename);
    }

    tinyply::PlyFile meshFile;
    std::vector<aiVector3D> vertices;
    std::vector<aiVector3D> normals;
    for(unsigned int i = 0; i < mesh->mNumVertices; i++){
        vertices.push_back({mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z});
        if(mesh->HasNormals()){
            normals.push_back({mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z});
        }
    }

    std::vector<uint32_t> indices;
    for(unsigned int i = 0; i < mesh->mNumFaces; i++){
        const aiFace& face = mesh->mFaces[i];
        auto numIndices = face.mNumIndices;
        if(numIndices != 3){
            throw std::runtime_error("only triangles are supported");
        }
        for(unsigned int j = 0; j < numIndices; j++){
            indices.push_back(face.mIndices[j]);
        }
    }

    using FaceData = std::tuple<uint32_t, uint32_t, uint32_t>; // indices

    std::vector<FaceData> faces;
    for(int i = 0; i < indices.size(); i += 3){
        faces.push_back({indices[i], indices[i + 1], indices[i + 2]});
    }

    auto equal = [&](const FaceData& f1, const FaceData& f2){
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

        auto test = [=](const aiVector3D& v, const aiVector3D& w){
            return abs(v.x - w.x) < epsilon && abs(v.y - w.y) < epsilon && abs(v.z - w.z) < epsilon;
        };

        // test distance smaller than epsilon for every permutation (3! = 6)
        if(test(v1, w1) && test(v2, w2) && test(v3, w3)){
            return true;
        }
        if(test(v1, w1) && test(v2, w3) && test(v3, w2)){
            return true;
        }
        if(test(v1, w2) && test(v2, w1) && test(v3, w3)){
            return true;
        }
        if(test(v1, w2) && test(v2, w3) && test(v3, w1)){
            return true;
        }
        if(test(v1, w3) && test(v2, w1) && test(v3, w2)){
            return true;
        }
        if(test(v1, w3) && test(v2, w2) && test(v3, w1)){
            return true;
        }

        return false;
    };

    auto hash = [&](const FaceData& f){
        auto [i1, i2, i3] = f;
        auto v1 = vertices[i1];
        auto v2 = vertices[i2];
        auto v3 = vertices[i3];

        return std::hash<float>()(v1.x) ^ std::hash<float>()(v1.y) ^ std::hash<float>()(v1.z) ^
               std::hash<float>()(v2.x) ^ std::hash<float>()(v2.y) ^ std::hash<float>()(v2.z) ^
               std::hash<float>()(v3.x) ^ std::hash<float>()(v3.y) ^ std::hash<float>()(v3.z);
    };

    using FaceSet = std::unordered_set<FaceData, decltype(hash), decltype(equal)>;
    FaceSet faceSet(faces.begin(), faces.end(), 0, hash, equal);

    std::vector<uint32_t> uniqueIndices;
    for(const auto& face : faceSet){
        auto [i1, i2, i3] = face;
        uniqueIndices.push_back(i1);
        uniqueIndices.push_back(i2);
        uniqueIndices.push_back(i3);
    }

    meshFile.add_properties_to_element("vertex", {"x", "y", "z"},
        tinyply::Type::FLOAT32, mesh->mNumVertices, reinterpret_cast<uint8_t*>(vertices.data()), tinyply::Type::INVALID, 0);

    if(mesh->HasNormals()){
        meshFile.add_properties_to_element("vertex", {"nx", "ny", "nz"},
            tinyply::Type::FLOAT32, mesh->mNumVertices, reinterpret_cast<uint8_t*>(normals.data()), tinyply::Type::INVALID, 0);
    }
    
    std::vector<aiVector2D> texCoords;   // this needs to be in the same scope as meshFile
    if(mesh->HasTextureCoords(0)){
        for(unsigned int i = 0; i < mesh->mNumVertices; i++){
            texCoords.push_back({mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y});
        }
        meshFile.add_properties_to_element("vertex", {"u", "v"},
            tinyply::Type::FLOAT32, mesh->mNumVertices, reinterpret_cast<uint8_t*>(texCoords.data()), tinyply::Type::INVALID, 0);
    }

    meshFile.add_properties_to_element("face", {"vertex_indices"},
        tinyply::Type::UINT32, faceSet.size(), reinterpret_cast<uint8_t*>(uniqueIndices.data()), tinyply::Type::UINT8, 3);

    meshFile.get_comments().push_back("generated by kontsuba");
    meshFile.write(outstream_binary, true);
}

void exportScene(const aiScene* scene, const std::string& from_path, const std::string& path)
{
    auto baseDir = fs::canonical(from_path);

    // if the path is a file, remove the file name
    if(!fs::is_directory(baseDir)){
        baseDir = baseDir.parent_path();
    }

    auto outputBasePath = fs::path(path);
    auto outputSceneDescPath = outputBasePath / "scene.xml";
    auto outputMeshPath = outputBasePath / "meshes";
    auto outputTexturePath = outputBasePath / "textures";
    fs::create_directories(path);
    fs::create_directories(outputMeshPath);
    fs::create_directories(outputTexturePath);

    tinyxml2::XMLDocument doc;

    tinyxml2::XMLElement* root = doc.NewElement("scene");
    root->SetAttribute("version", "3.0.0");
    doc.InsertFirstChild(root);

    auto integratorNode = doc.NewElement("integrator");
    integratorNode->SetAttribute("type", "path");
    auto maxDepthNode = constructNode(doc, "integer", "max_depth", "3");
    integratorNode->InsertEndChild(maxDepthNode);
    root->InsertEndChild(integratorNode);

    auto sensorNode = doc.NewElement("sensor");
    sensorNode->SetAttribute("type", "perspective");
    auto fovNode = constructNode(doc, "float", "fov", "45");
    sensorNode->InsertEndChild(fovNode);
    auto toWorldNode = doc.NewElement("transform");
    toWorldNode->SetAttribute("name", "to_world");
    auto lookAtNode = doc.NewElement("lookat");
    lookAtNode->SetAttribute("origin", "1, 1, 0");
    lookAtNode->SetAttribute("target", "0, 0, 0");
    lookAtNode->SetAttribute("up", "0, 0, 1");
    toWorldNode->InsertEndChild(lookAtNode);
    sensorNode->InsertEndChild(toWorldNode);

    auto samplerNode = doc.NewElement("sampler");
    samplerNode->SetAttribute("type", "independent");
    auto sampleCountNode = constructNode(doc, "integer", "sample_count", "32");
    samplerNode->InsertEndChild(sampleCountNode);
    sensorNode->InsertEndChild(samplerNode);

    auto filmNode = doc.NewElement("film");
    filmNode->SetAttribute("type", "hdrfilm");
    auto widthNode = constructNode(doc, "integer", "width", "512");
    filmNode->InsertEndChild(widthNode);
    auto heightNode = constructNode(doc, "integer", "height", "512");
    filmNode->InsertEndChild(heightNode);
    auto pixelFormatNode = constructNode(doc, "string", "pixel_format", "rgb");
    filmNode->InsertEndChild(pixelFormatNode);
    sensorNode->InsertEndChild(filmNode);

    root->InsertEndChild(sensorNode);

    auto emitterNode = doc.NewElement("emitter");
    emitterNode->SetAttribute("type", "point");
    auto intensityNode = constructNode(doc, "rgb", "intensity", "10");
    emitterNode->InsertEndChild(intensityNode);
    auto positionNode = constructNode(doc, "point", "position", "2, 2, 2");
    emitterNode->InsertEndChild(positionNode);
    root->InsertEndChild(emitterNode);

    // loop over all materials in scene
    for(size_t i = 0; i < scene->mNumMaterials; i++){
        aiMaterial* material = scene->mMaterials[i];

        // set name to random number if not available because it is used as id
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> dis(0, std::numeric_limits<uint32_t>::max());
        auto guid = dis(gen);

        auto name = probeMaterialProperty<aiString>(material, AI_MATKEY_NAME).value_or(aiString(std::to_string(guid)));
        auto shadingModel = probeMaterialProperty<int>(material, AI_MATKEY_SHADING_MODEL).value_or(aiShadingMode_Phong);

        // Get all possble material bsdf properties and set to default if not available
        // clang-format off
        auto ka =                   probeMaterialProperty<aiColor3D>(material, AI_MATKEY_COLOR_AMBIENT).value_or(aiColor3D(0.0f, 0.0f, 0.0f));
        auto kd =                   probeMaterialProperty<aiColor3D>(material, AI_MATKEY_COLOR_DIFFUSE).value_or(aiColor3D(0.0f, 0.0f, 0.0f));
        auto ks =                   probeMaterialProperty<aiColor3D>(material, AI_MATKEY_COLOR_SPECULAR).value_or(aiColor3D(0.0f, 0.0f, 0.0f));
        auto baseColor =            probeMaterialProperty<aiColor3D>(material, AI_MATKEY_BASE_COLOR).value_or(kd);
        auto shininess =            probeMaterialProperty<float>(material, AI_MATKEY_SHININESS).value_or(1.0f);
        auto opacity =              probeMaterialProperty<float>(material, AI_MATKEY_OPACITY).value_or(1.0f);
        auto roughness =            probeMaterialProperty<float>(material, AI_MATKEY_ROUGHNESS_FACTOR).value_or(0.5f);
        auto metallic =             probeMaterialProperty<float>(material, AI_MATKEY_METALLIC_FACTOR).value_or(0.0f);
        auto sheenFactor =          probeMaterialProperty<float>(material, AI_MATKEY_SHEEN_COLOR_FACTOR).value_or(0.0f);
        auto anisotropy =           probeMaterialProperty<float>(material, AI_MATKEY_ANISOTROPY_FACTOR).value_or(0.0f);
        auto clearCoat =            probeMaterialProperty<float>(material, AI_MATKEY_CLEARCOAT_FACTOR).value_or(0.0f);
        auto clearCoatRoughness =   probeMaterialProperty<float>(material, AI_MATKEY_CLEARCOAT_ROUGHNESS_FACTOR).value_or(0.0f);
        auto specularFactor =       probeMaterialProperty<float>(material, AI_MATKEY_SPECULAR_FACTOR).value_or(0.5f);

        // Get all possible texture paths (these are all optional)
        auto diffuseTexture =       probeMaterialTexture(material, aiTextureType_DIFFUSE);
        auto metallicTexture =      probeMaterialTexture(material, aiTextureType_METALNESS);
        auto roughnessTexture =     probeMaterialTexture(material, aiTextureType_DIFFUSE_ROUGHNESS);
        auto normalTexture =        probeMaterialTexture(material, aiTextureType_NORMALS);
        auto displacementTexture =  probeMaterialTexture(material, aiTextureType_DISPLACEMENT);
        auto occlusionTexture =     probeMaterialTexture(material, aiTextureType_AMBIENT_OCCLUSION);
        auto emissiveTexture =      probeMaterialTexture(material, aiTextureType_EMISSIVE);
        // clang-format on

        if(shadingModel != aiShadingMode_PBR_BRDF){
            // set reasonable defaults, because other values cannot be trusted
            roughness = 0.5f;
            metallic = 0.0f;
            sheenFactor = 0.0f;
            anisotropy = 0.0f;
            clearCoat = 0.0f;
            clearCoatRoughness = 0.0f;
            specularFactor = 0.5f;
        }

        auto twoSidedNode = doc.NewElement("bsdf");
        twoSidedNode->SetAttribute("type", "twosided");
        auto materialNode = doc.NewElement("bsdf");
        materialNode->SetAttribute("type", "principled");
        twoSidedNode->SetAttribute("id", name.C_Str());
        auto baseColorNode = valueOrTextureNode(doc, "rgb", "base_color", baseColor, diffuseTexture);
        materialNode->InsertEndChild(baseColorNode);
        auto roughnessNode = valueOrTextureNode(doc, "float", "roughness", roughness, roughnessTexture);
        materialNode->InsertEndChild(roughnessNode);
        auto specularFactorNode = constructNode(doc, "float", "specular", std::to_string(specularFactor));
        materialNode->InsertEndChild(specularFactorNode);
        auto metallicNode = valueOrTextureNode(doc, "float", "metallic", metallic, metallicTexture);
        materialNode->InsertEndChild(metallicNode);
        auto sheenNode = constructNode(doc, "float", "sheen", std::to_string(sheenFactor));
        materialNode->InsertEndChild(sheenNode);
        auto anisotropyNode = constructNode(doc, "float", "anisotropic", std::to_string(anisotropy));
        materialNode->InsertEndChild(anisotropyNode);
        auto clearCoatNode = constructNode(doc, "float", "clearcoat", std::to_string(clearCoat));
        materialNode->InsertEndChild(clearCoatNode);
        auto clearCoatRoughnessNode = constructNode(doc, "float", "clearcoat_gloss", std::to_string(clearCoatRoughness));
        materialNode->InsertEndChild(clearCoatRoughnessNode);
        twoSidedNode->InsertEndChild(materialNode);
        root->InsertEndChild(twoSidedNode);

        // copy textures to the correct folder if they exist
        if(diffuseTexture){
            fs::copy_file(baseDir / diffuseTexture.value(), outputTexturePath / fs::path(diffuseTexture.value()).filename(),
                fs::copy_options::overwrite_existing);
        }
    }

    // loop over all meshes in scene
    for(size_t i = 0; i < scene->mNumMeshes; i++){
        aiMesh* mesh = scene->mMeshes[i];

        aiString name;
        auto plyName = outputMeshPath / ("mesh" + std::to_string(i) + ".ply");
        scene->mMaterials[mesh->mMaterialIndex]->Get(AI_MATKEY_NAME, name);
        auto plySceneFileName = fs::relative(plyName, outputBasePath).string();

        auto meshNode = doc.NewElement("shape");
        meshNode->SetAttribute("type", "ply");
        auto filenameNode = constructNode(doc, "string", "filename", plySceneFileName.c_str());
        meshNode->InsertEndChild(filenameNode);
        auto refNode = doc.NewElement("ref");
        refNode->SetAttribute("id", name.C_Str());
        meshNode->InsertEndChild(refNode);

        root->InsertEndChild(meshNode);

        writeMeshPly(mesh, plyName);
    }

    doc.SaveFile(outputSceneDescPath.c_str());
}