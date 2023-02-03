#pragma once

#include <optional>
#include <filesystem>
#include <set>
#include <random>

#include <assimp/scene.h>
#include <fmt/format.h>
#include <tinyxml2.h>

using Spectrum = aiColor3D;
using Float = float;
using Texture = std::string;

using namespace tinyxml2;
namespace fs = std::filesystem;

template <typename T>
struct TextureOr {
  std::string type;
  T value;
  std::optional<Texture> texture;

  TextureOr(std::string type, T value) : type(type), value(value) {}

  bool isTexture() const { return texture.has_value(); }
};

auto probeMaterialTexture(const aiMaterial *material, aiTextureType type) {
  aiString path;
  if (material->GetTextureCount(type) != 0) {
    if (material->GetTexture(type, 0, &path) == aiReturn_SUCCESS) {
      return std::optional<Texture>(path.C_Str());
    }
  }
  return std::optional<Texture>();
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

template<typename T>
auto set_if(const std::optional<T> &opt, T &value){
  if(opt.has_value()){
    value = opt.value();
    return true;
  }
  return false;
}

template<typename T>
auto insert_if(const std::optional<T>& opt, std::set<T>& set){
  if(opt.has_value()){
    set.insert(opt.value());
    return true;
  }
  return false;
}

namespace Kontsuba {

#define BRDF_PARAM(name, type, ...) \
  TextureOr<type> name {#name, type(__VA_ARGS__)}

struct PrincipledBRDF {
  // Disney Principled BRDF as used by Mitsuba
  // https://www.mitsuba-renderer.org/docs/current/bsdf.html#bsdf-principled

  std::string name = "id";
  BRDF_PARAM(base_color, Spectrum, {0.5,0.5,0.5});
  BRDF_PARAM(roughness, Float, 0.5);
  BRDF_PARAM(anisotropic, Float, 0.0);
  BRDF_PARAM(metallic, Float, 0.0);
  BRDF_PARAM(spec_trans, Float, 0.0);
  BRDF_PARAM(specular, Float, 0.5);
  BRDF_PARAM(sheen, Float, 0.0);
  BRDF_PARAM(sheen_tint, Float, 0.0);
  BRDF_PARAM(flatness, Float, 0.0);
  BRDF_PARAM(clearcoat, Float, 0.0);
  BRDF_PARAM(clearcoat_gloss, Float, 0.0);

  bool twoSided;
  std::set<Texture> textures;

  static PrincipledBRDF fromMaterial(aiMaterial* material, bool makeTwoSided = false){
    PrincipledBRDF brdf;  // initializes with defaults
    // Get all possible material bsdf properties and set to default if not available

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(
        0, std::numeric_limits<uint32_t>::max());
    auto guid = dis(gen);
    auto name = probeMaterialProperty<aiString>(material, AI_MATKEY_NAME);
    brdf.name = name.has_value() ? name.value().C_Str() : fmt::format("id{}", guid);

    // clang-format off
    auto ka =                   probeMaterialProperty<aiColor3D>(material, AI_MATKEY_COLOR_AMBIENT);
    auto kd =                   probeMaterialProperty<aiColor3D>(material, AI_MATKEY_COLOR_DIFFUSE);
    auto ks =                   probeMaterialProperty<aiColor3D>(material, AI_MATKEY_COLOR_SPECULAR);
    auto baseColor =            probeMaterialProperty<aiColor3D>(material, AI_MATKEY_BASE_COLOR);
    auto shininess =            probeMaterialProperty<float>(material, AI_MATKEY_SHININESS);
    auto opacity =              probeMaterialProperty<float>(material, AI_MATKEY_OPACITY);
    auto roughness =            probeMaterialProperty<float>(material, AI_MATKEY_ROUGHNESS_FACTOR);
    auto metallic =             probeMaterialProperty<float>(material, AI_MATKEY_METALLIC_FACTOR);
    auto sheenFactor =          probeMaterialProperty<float>(material, AI_MATKEY_SHEEN_COLOR_FACTOR);
    auto anisotropic =          probeMaterialProperty<float>(material, AI_MATKEY_ANISOTROPY_FACTOR);
    auto clearCoat =            probeMaterialProperty<float>(material, AI_MATKEY_CLEARCOAT_FACTOR);
    auto clearCoatRoughness =   probeMaterialProperty<float>(material, AI_MATKEY_CLEARCOAT_ROUGHNESS_FACTOR);
    auto specularFactor =       probeMaterialProperty<float>(material, AI_MATKEY_SPECULAR_FACTOR);

    set_if(kd, brdf.base_color.value);
    set_if(baseColor, brdf.base_color.value);
    set_if(roughness, brdf.roughness.value);
    set_if(anisotropic, brdf.anisotropic.value);
    set_if(metallic, brdf.metallic.value);
    set_if(specularFactor, brdf.specular.value);
    set_if(sheenFactor, brdf.sheen.value);
    set_if(clearCoat, brdf.clearcoat.value);
    set_if(clearCoatRoughness, brdf.clearcoat_gloss.value);

    // Get all possible texture paths (these are all optional)
    auto diffuseTexture =       probeMaterialTexture(material, aiTextureType_DIFFUSE);
    auto metallicTexture =      probeMaterialTexture(material, aiTextureType_METALNESS);
    auto roughnessTexture =     probeMaterialTexture(material, aiTextureType_DIFFUSE_ROUGHNESS);
    auto normalTexture =        probeMaterialTexture(material, aiTextureType_NORMALS);
    auto displacementTexture =  probeMaterialTexture(material, aiTextureType_DISPLACEMENT);
    auto occlusionTexture =     probeMaterialTexture(material, aiTextureType_AMBIENT_OCCLUSION);
    auto emissiveTexture =      probeMaterialTexture(material, aiTextureType_EMISSIVE);
    // clang-format on

    brdf.base_color.texture = diffuseTexture;
    brdf.metallic.texture = metallicTexture;
    brdf.roughness.texture = roughnessTexture;

    insert_if(diffuseTexture, brdf.textures);
    insert_if(metallicTexture, brdf.textures);
    insert_if(roughnessTexture, brdf.textures);
    insert_if(normalTexture, brdf.textures);
    insert_if(displacementTexture, brdf.textures);
    insert_if(occlusionTexture, brdf.textures);
    insert_if(emissiveTexture, brdf.textures);

    brdf.twoSided = makeTwoSided;
    return brdf;
  }
};

template <typename T>
XMLElement* toXML(XMLDocument& doc, const TextureOr<T>& t){
  if(t.isTexture()){
    auto element = doc.NewElement("texture");
    element->SetAttribute("type", "bitmap");
    element->SetAttribute("name", t.type.c_str());
    auto filenameNode = element->InsertNewChildElement("string");
    auto filename = "textures" / fs::path(t.texture.value()).filename();
    filenameNode->SetAttribute("name", "filename");
    filenameNode->SetAttribute("value", filename.c_str());
    return element;
  }else{
    if constexpr (std::is_same_v<T, Float>){
      auto element = doc.NewElement("float");
      element->SetAttribute("name", t.type.c_str());
      element->SetAttribute("value", t.value);
      return element;
    }else{
      auto element = doc.NewElement("rgb");
      element->SetAttribute("name", t.type.c_str());
      element->SetAttribute("value", fmt::format("{},{},{}", t.value.r, t.value.g, t.value.b).c_str());
      return element;
    }
  }
}

XMLElement* toXML(XMLDocument& doc, const PrincipledBRDF& brdf){
  auto element = doc.NewElement("bsdf");
  element->SetAttribute("type", "principled");
  element->InsertEndChild(toXML(doc, brdf.base_color));
  element->InsertEndChild(toXML(doc, brdf.roughness));
  element->InsertEndChild(toXML(doc, brdf.anisotropic));
  element->InsertEndChild(toXML(doc, brdf.metallic));
  element->InsertEndChild(toXML(doc, brdf.specular));
  element->InsertEndChild(toXML(doc, brdf.sheen));
  element->InsertEndChild(toXML(doc, brdf.sheen_tint));
  element->InsertEndChild(toXML(doc, brdf.flatness));
  element->InsertEndChild(toXML(doc, brdf.clearcoat));
  element->InsertEndChild(toXML(doc, brdf.clearcoat_gloss));

  if(brdf.twoSided){
    auto twosided = doc.NewElement("bsdf");
    twosided->SetAttribute("type", "twosided");
    twosided->SetAttribute("id", brdf.name.c_str());
    twosided->InsertEndChild(element);
    return twosided;
  }
  element->SetAttribute("id", brdf.name.c_str());
  return element;
}
} // namespace Kontsuba


