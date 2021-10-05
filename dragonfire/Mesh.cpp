#include "pch.h"

#include "Mesh.h"

#include <GLTFSDK/Deserialize.h>
#include <GLTFSDK/GLBResourceReader.h>
#include <GLTFSDK/GLTF.h>

#include "StreamReader.h"
#include "Vertex.h"
#include "tmp.h"

namespace gltf = Microsoft::glTF;
using namespace std::string_literals;

static const std::string PositionAttribute{"POSITION"};
static const std::string NormalAttribute{"NORMAL"};
static const std::string TangentAttribute{"TANGENT"};

constexpr vk::PrimitiveTopology TopologyFromMeshMode(gltf::MeshMode mode)
{
    switch (mode)
    {
    case gltf::MESH_POINTS:
        return vk::PrimitiveTopology::ePointList;
    case gltf::MESH_LINES:
        return vk::PrimitiveTopology::eLineList;
    case gltf::MESH_LINE_LOOP:
        return vk::PrimitiveTopology::eLineStrip;
    case gltf::MESH_LINE_STRIP:
        return vk::PrimitiveTopology::eLineStrip;
    case gltf::MESH_TRIANGLES:
        return vk::PrimitiveTopology::eTriangleList;
    case gltf::MESH_TRIANGLE_STRIP:
        return vk::PrimitiveTopology::eTriangleStrip;
    case gltf::MESH_TRIANGLE_FAN:
        return vk::PrimitiveTopology::eTriangleFan;
    default:
        throw std::runtime_error("Unknown MeshMode.");
    }
}

template <gltf::MeshMode mode>
inline constexpr vk::PrimitiveTopology VulkanTopology = TopologyFromMeshMode(mode);

template <uint8_t index>
const std::string& TexcoordAttribute()
{
    static std::string attribute = "TEXCOORD_"s + std::to_string(index);
    return attribute;
}

template <uint8_t index>
const std::string& ColorAttribute()
{
    static std::string attribute = "COLOR_"s + std::to_string(index);
    return attribute;
}

template <uint8_t index>
const std::string& JointsAttribute()
{
    static std::string attribute = "JOINTS_"s + std::to_string(index);
    return attribute;
}

template <uint8_t index>
const std::string& WeightsAttribute()
{
    static std::string attribute = "WEIGHTS_"s + std::to_string(index);
    return attribute;
}

Mesh::Mesh(const std::filesystem::path& path)
{
    auto ext = path.extension().string();
    for (auto& ch : ext)
        ch = static_cast<char>(std::tolower(ch));

    auto stream = std::make_shared<StreamReader>(path.parent_path());
    std::string manifest;
    std::unique_ptr<gltf::GLTFResourceReader> reader;

    if (ext == ".gltf")
    {
        reader = std::make_unique<gltf::GLTFResourceReader>(stream);
        std::stringstream sstream;
        sstream << stream->GetInputStream(path.filename().u8string())->rdbuf();
        manifest = sstream.str();
    }
    else if (ext == ".glb")
    {
        auto glbReader = std::make_unique<gltf::GLBResourceReader>(stream, stream->GetInputStream(path.filename().u8string()));
        manifest = glbReader->GetJson();
        reader = std::move(glbReader);
    }
    else
        throw std::runtime_error("Wrong extension.");

    auto doc = gltf::Deserialize(manifest);

    const auto& mesh = doc.meshes.Get(
        std::find_if(doc.nodes.Elements().begin(), doc.nodes.Elements().end(), [](const auto& node) noexcept { return !node.meshId.empty(); })->meshId);

    for (const auto& primitive : mesh.primitives)
    {
        IndexArray indices;

        const auto& positionAccessor = doc.accessors.Get(primitive.GetAttributeAccessorId(PositionAttribute));
        if (primitive.indicesAccessorId.empty())
        {
            if (!primitive.HasAttribute(PositionAttribute))
                continue;

            if (positionAccessor.count < UINT8_MAX)
                indices = std::vector<uint8_t>(positionAccessor.count);
            else if (positionAccessor.count < UINT16_MAX)
                indices = std::vector<uint16_t>(positionAccessor.count);
            else
                indices = std::vector<uint32_t>(positionAccessor.count);

            std::visit(
                [&](auto&& arg) {
                    using T = std::decay_t<decltype(arg)>;
                    std::iota(std::get<T>(indices).begin(), std::get<T>(indices).end(), 0);
                },
                indices);
        }
        else
        {
            const auto& indicesAccessor = doc.accessors.Get(primitive.indicesAccessorId);
            switch (indicesAccessor.componentType)
            {
            case gltf::ComponentType::COMPONENT_UNSIGNED_BYTE:
                indices = reader->ReadBinaryData<uint8_t>(doc, indicesAccessor);
                break;
            case gltf::ComponentType::COMPONENT_UNSIGNED_SHORT:
                indices = reader->ReadBinaryData<uint16_t>(doc, indicesAccessor);
                break;
            case gltf::ComponentType::COMPONENT_UNSIGNED_INT:
                indices = reader->ReadBinaryData<uint32_t>(doc, indicesAccessor);
                break;
            default:
                throw std::runtime_error("Unexpected ComponentType.");
            }
        }

        std::vector<Vertex> vertices(positionAccessor.count, {{}, {}, glm::vec4(1, 1, 1, 1)});
        const auto positionComponents = reader->ReadFloatData(doc, positionAccessor);
        std::vector<float> normalComponents;
        if (primitive.HasAttribute(NormalAttribute))
        {
            normalComponents = reader->ReadFloatData(doc, doc.accessors.Get(primitive.GetAttributeAccessorId(NormalAttribute)));
        }
        std::vector<float> colorComponents;
        if (primitive.HasAttribute(ColorAttribute<0>()))
        {
            colorComponents = reader->ReadFloatData(doc, doc.accessors.Get(primitive.GetAttributeAccessorId(ColorAttribute<0>())));
        }

        for (size_t i = 0; i < positionAccessor.count; ++i)
        {
            auto& currentVertex = vertices[i];
            currentVertex.position = {positionComponents[i * 3], positionComponents[(i * 3) + 1], positionComponents[(i * 3) + 2]};
            if (normalComponents.empty())
            {
                if (primitive.mode == gltf::MeshMode::MESH_TRIANGLE_STRIP)
                {
                    // pi = {vi, vi+(1+i%2), vi+(2-i%2)}
                    if (i >= 2)
                    {
                        const auto j = i - 2;
                        const auto v1 = vertices[j].position;
                        const auto v2 = vertices[j + (1 + j % 2)].position;
                        const auto v3 = vertices[j + (2 - j % 2)].position;
                        const auto normal = glm::normalize(glm::cross(v2 - v1, v3 - v1));
                        vertices[j].normal = normal;
                        vertices[j + 1].normal = normal;
                        vertices[j + 2].normal = normal;
                    }
                }
                else if (primitive.mode == gltf::MeshMode::MESH_TRIANGLES && i > 0 && i % 3 == 0)
                {
                    // pi = {v3i, v3i+1, v3i+2}
                    const auto v1 = vertices[i - 2].position;
                    const auto v2 = vertices[i - 1].position;
                    const auto v3 = vertices[i].position;
                    const auto normal = glm::normalize(glm::cross(v2 - v1, v3 - v1));
                    vertices[i - 2].normal = normal;
                    vertices[i - 1].normal = normal;
                    vertices[i].normal = normal;
                }
                else
                    throw std::runtime_error("Unsupported MeshMode.");
            }
            else
                currentVertex.normal = {normalComponents[i * 3], normalComponents[(i * 3) + 1], normalComponents[(i * 3) + 2]};
            if (!colorComponents.empty())
                currentVertex.color = {colorComponents[i * 4], colorComponents[(i * 4) + 1], colorComponents[(i * 4) + 2], colorComponents[(i * 4) + 3]};
        }

        parts.emplace_back();
        auto& part = parts.back();
        part.indices = std::move(indices);
        part.vertices = std::move(vertices);
        part.topology = TopologyFromMeshMode(primitive.mode);
        part.loop = primitive.mode == gltf::MeshMode::MESH_LINE_LOOP;
    }
}
