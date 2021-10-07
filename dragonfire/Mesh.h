#pragma once
#include "Vertex.h"

using IndexArray = std::variant<std::vector<uint8_t>, std::vector<uint16_t>, std::vector<uint32_t>>;

struct MeshPart
{
    vk::PrimitiveTopology topology;
    std::optional<IndexArray> indices;
    std::vector<Vertex> vertices;
    bool loop;
};

class Mesh
{
public:
    Mesh(const std::filesystem::path& path);

private:
    std::vector<MeshPart> parts;
};
