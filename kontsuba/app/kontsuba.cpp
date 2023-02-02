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
        aiProcess_Triangulate           |
        aiProcess_JoinIdenticalVertices |
        aiProcess_FindDegenerates       |
        aiProcess_FixInfacingNormals    |
        aiProcess_PreTransformVertices  |
        aiProcess_FlipUVs               |
        aiProcess_TransformUVCoords     |
        aiProcess_SortByPType
    );

    if(!scene){
        std::cout << "Error: " << importer.GetErrorString() << std::endl;
        return 1;
    }

    exportScene(scene, path, "test");

    DefaultLogger::kill();

    return 0;
}
