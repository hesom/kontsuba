#pragma once

#include <string>
#include <assimp/scene.h>

void exportScene(const aiScene* scene, const std::string& path);
