#pragma once

#include "Graphics.h"
#include "Vertex.h"

class OriginGizmo
{
public:
    OriginGizmo();
    OriginGizmo(OriginGizmo&) = delete;
    OriginGizmo(OriginGizmo&&) = delete;
    OriginGizmo& operator=(OriginGizmo&) = delete;
    OriginGizmo& operator=(OriginGizmo&&) = delete;
    ~OriginGizmo();

    vk::Buffer vertexBuffer;
    vk::Buffer indexBuffer;
    vk::Pipeline pipeline;
    vk::PipelineLayout pipelineLayout;
    std::array<vk::DescriptorSet, gfx::Graphics::MaxFramesInFlight> descriptorSets{};

private:
    static const UnlitVertex vertices[12];
    static const uint32_t indices[18];
    VmaAllocation vertexBufferAlloc{};
    VmaAllocation indexBufferAlloc{};
    vk::DescriptorSetLayout setLayout;
    vk::DescriptorPool descPool;
};
