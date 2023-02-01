#pragma once

#include <string>
#include <assimp/scene.h>

void exportScene(const aiScene* scene, const std::string& from_path, const std::string& path);
