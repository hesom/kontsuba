#include <iostream>
#include <string>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/DefaultLogger.hpp>
#include <tinyxml2.h>
#include "exporter.h"

int main(int argc, char const *argv[])
{
    using Assimp::DefaultLogger;
    DefaultLogger::create("AssimpLog.txt", Assimp::Logger::VERBOSE);

    Assimp::Importer importer;
    const std::string path = "../../test_models/shapenet/models/model_normalized.obj";
    DefaultLogger::get()->info("Loading model: " + path);
    const aiScene* scene = importer.ReadFile(path,
    aiProcess_CalcTangentSpace      |
    aiProcess_Triangulate           |
    aiProcess_JoinIdenticalVertices |
    aiProcess_FindDegenerates       |
    aiProcess_FixInfacingNormals    |
    aiProcess_SortByPType);

    if(!scene){
        std::cout << "Error: " << importer.GetErrorString() << std::endl;
        return 1;
    }

    // loop over all meshes of the scene and print info about them
    for(unsigned int i = 0; i < scene->mNumMeshes; ++i){
        aiMesh* mesh = scene->mMeshes[i];
        std::cout << "Mesh " << i << std::endl;
        std::cout << "  Number of vertices: " << mesh->mNumVertices << std::endl;
        std::cout << "  Number of faces: " << mesh->mNumFaces << std::endl;
        std::cout << "  Number of normals: " << mesh->mNumVertices << std::endl;
        std::cout << "  Number of texture coordinates: " << mesh->mNumVertices << std::endl;
        std::cout << "  Number of colors: " << mesh->mNumVertices << std::endl;

        // print material info
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        aiString name;
        material->Get(AI_MATKEY_NAME, name);
        std::cout << "  Material name: " << name.C_Str() << std::endl;
        std::cout << "    Number of textures: " << material->GetTextureCount(aiTextureType_DIFFUSE) << std::endl;
        std::cout << "    Number of properties: " << material->mNumProperties << std::endl;
        // Print lighting properties
        aiColor3D color;
        material->Get(AI_MATKEY_COLOR_DIFFUSE, color);
        std::cout << "    Diffuse color: " << color.r << ", " << color.g << ", " << color.b << std::endl;
        material->Get(AI_MATKEY_COLOR_AMBIENT, color);
        std::cout << "    Ambient color: " << color.r << ", " << color.g << ", " << color.b << std::endl;
        material->Get(AI_MATKEY_COLOR_SPECULAR, color);
        std::cout << "    Specular color: " << color.r << ", " << color.g << ", " << color.b << std::endl;
        material->Get(AI_MATKEY_COLOR_EMISSIVE, color);
        std::cout << "    Emissive color: " << color.r << ", " << color.g << ", " << color.b << std::endl;
        float shininess;
        material->Get(AI_MATKEY_SHININESS, shininess);
        std::cout << "    Shininess: " << shininess << std::endl;
        float opacity;
        material->Get(AI_MATKEY_OPACITY, opacity);
        std::cout << "    Opacity: " << opacity << std::endl;
        float reflectivity;
        if(material->Get(AI_MATKEY_REFLECTIVITY, reflectivity))
            std::cout << "    Reflectivity: " << reflectivity << std::endl;
        float refracti;
        if(material->Get(AI_MATKEY_REFRACTI, refracti))
            std::cout << "    Refracti: " << refracti << std::endl;
        float metallic;
        if(material->Get(AI_MATKEY_METALLIC_FACTOR, metallic))
            std::cout << "    Metallic: " << metallic << std::endl;
        float sheen;
        if(material->Get(AI_MATKEY_SHEEN_COLOR_FACTOR, sheen))
            std::cout << "    Sheen: " << sheen << std::endl;
        float roughness;
        if(material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness))
            std::cout << "    Roughness: " << roughness << std::endl;
        float specularFactor;
        if(material->Get(AI_MATKEY_SPECULAR_FACTOR, specularFactor))
            std::cout << "    Specular factor: " << specularFactor << std::endl;

        // print shading model as string
        int shadingModel;
        material->Get(AI_MATKEY_SHADING_MODEL, shadingModel);
    }

    exportScene(scene, "test");

    DefaultLogger::kill();

    return 0;
}
