#include "pch.h"

#include "OriginGizmo.h"

#include "Engine.h"

const UnlitVertex OriginGizmo::vertices[12] = {
    {glm::vec3(0, 0, 0), glm::vec4(1, 1, 0, 1)}, {glm::vec3(1, 0, 0), glm::vec4(1, 1, 0, 1)}, {glm::vec3(1, 1, 0), glm::vec4(1, 1, 0, 1)},
    {glm::vec3(0, 1, 0), glm::vec4(1, 1, 0, 1)}, {glm::vec3(0, 0, 0), glm::vec4(1, 0, 1, 1)}, {glm::vec3(1, 0, 0), glm::vec4(1, 0, 1, 1)},
    {glm::vec3(1, 0, 1), glm::vec4(1, 0, 1, 1)}, {glm::vec3(0, 0, 1), glm::vec4(1, 0, 1, 1)}, {glm::vec3(0, 0, 0), glm::vec4(0, 1, 1, 1)},
    {glm::vec3(0, 1, 0), glm::vec4(0, 1, 1, 1)}, {glm::vec3(0, 1, 1), glm::vec4(0, 1, 1, 1)}, {glm::vec3(0, 0, 1), glm::vec4(0, 1, 1, 1)}};

const uint32_t OriginGizmo::indices[18] = {0, 1, 2, 0, 2, 3, 4, 7, 6, 4, 6, 5, 8, 9, 10, 8, 10, 11}; // CCW ordering

#pragma warning(push)
#pragma warning(disable : 26455)
OriginGizmo::OriginGizmo()
#pragma warning(pop)
{
    auto allocator = Graphics().Allocator();
    auto bufferCreateInfo =
        vk::BufferCreateInfo{}.setSize(sizeof(vertices)).setUsage(vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);
    VmaAllocationCreateInfo vmaInfo{};
    vmaInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    if (const auto result = vmaCreateBuffer(allocator, reinterpret_cast<VkBufferCreateInfo*>(&bufferCreateInfo), &vmaInfo,
                                            reinterpret_cast<VkBuffer*>(&vertexBuffer), &vertexBufferAlloc, nullptr))
        throw std::runtime_error{vk::to_string(static_cast<vk::Result>(result))};

    bufferCreateInfo.setUsage(vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst);
    bufferCreateInfo.setSize(sizeof(indices));
    if (const auto result = vmaCreateBuffer(allocator, reinterpret_cast<VkBufferCreateInfo*>(&bufferCreateInfo), &vmaInfo,
                                            reinterpret_cast<VkBuffer*>(&indexBuffer), &indexBufferAlloc, nullptr))
        throw std::runtime_error{vk::to_string(static_cast<vk::Result>(result))};

    VkBuffer stagingBuffer{};
    VmaAllocation stagingBufferAlloc{};
    bufferCreateInfo.setUsage(vk::BufferUsageFlagBits::eTransferSrc);
    bufferCreateInfo.setSize(sizeof(indices) + sizeof(vertices));
    vmaInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    vmaInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VmaAllocationInfo allocInfo{};
    if (const auto result =
            vmaCreateBuffer(allocator, reinterpret_cast<VkBufferCreateInfo*>(&bufferCreateInfo), &vmaInfo, &stagingBuffer, &stagingBufferAlloc, &allocInfo))
        throw std::runtime_error{vk::to_string(static_cast<vk::Result>(result))};
    std::memcpy(allocInfo.pMappedData, vertices, sizeof(vertices));
    std::memcpy(static_cast<std::byte*>(allocInfo.pMappedData) + sizeof(vertices), indices, sizeof(indices));
    vmaFlushAllocation(allocator, stagingBufferAlloc, 0, VK_WHOLE_SIZE);

    const auto inputAssembly = vk::PipelineInputAssemblyStateCreateInfo{}.setTopology(vk::PrimitiveTopology::eTriangleList);
    std::array attribs = {vk::VertexInputAttributeDescription{}.setFormat(vk::Format::eR32G32B32Sfloat),
                          vk::VertexInputAttributeDescription{}.setFormat(vk::Format::eR32G32B32A32Sfloat).setLocation(1).setOffset(offsetof(UnlitVertex, color))};

    const auto vertexBinding = vk::VertexInputBindingDescription{}.setInputRate(vk::VertexInputRate::eVertex).setStride(sizeof(UnlitVertex));
    const auto vertexState = vk::PipelineVertexInputStateCreateInfo{}.setVertexBindingDescriptions(vertexBinding).setVertexAttributeDescriptions(attribs);

    const auto device = Graphics().Device();
    const auto layoutBinding = vk::DescriptorSetLayoutBinding{}
                                   .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                                   .setStageFlags(vk::ShaderStageFlagBits::eVertex)
                                   .setDescriptorCount(1);
    setLayout = device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo{}.setBindings(layoutBinding));

    pipelineLayout = device.createPipelineLayout(vk::PipelineLayoutCreateInfo{}.setSetLayouts(setLayout));

    std::array dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    const auto dynamicStateInfo = vk::PipelineDynamicStateCreateInfo{}.setDynamicStates(dynamicStates);

    auto vert = Resources().Get<vk::ShaderModule>("shaders/origin.vert.spv");
    auto frag = Resources().Get<vk::ShaderModule>("shaders/origin.frag.spv");
    const auto blendAttachmentState = vk::PipelineColorBlendAttachmentState{}.setColorWriteMask(
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
    const auto blendState = vk::PipelineColorBlendStateCreateInfo{}.setAttachments(blendAttachmentState);
    std::array stages = {vk::PipelineShaderStageCreateInfo{}.setModule(vert.get()).setPName("main").setStage(vk::ShaderStageFlagBits::eVertex),
                         vk::PipelineShaderStageCreateInfo{}.setModule(frag.get()).setPName("main").setStage(vk::ShaderStageFlagBits::eFragment)};

    const auto viewport = vk::Viewport{}.setMaxDepth(1).setHeight(static_cast<float>(Graphics().Height())).setWidth(static_cast<float>(Graphics().Width()));
    const auto scissor = vk::Rect2D{}.setExtent(vk::Extent2D{Graphics().Width(), Graphics().Height()});
    const auto viewportInfo = vk::PipelineViewportStateCreateInfo{}.setViewports(viewport).setScissors(scissor);
    const auto msState = vk::PipelineMultisampleStateCreateInfo{}.setRasterizationSamples(vk::SampleCountFlagBits::e1);
    const auto rasterizationState = vk::PipelineRasterizationStateCreateInfo{}.setLineWidth(1).setFrontFace(vk::FrontFace::eCounterClockwise);
    const auto pipelineInfo = vk::GraphicsPipelineCreateInfo{}
                                  .setLayout(pipelineLayout)
                                  .setStages(stages)
                                  .setRenderPass(Graphics().RenderPass())
                                  .setPInputAssemblyState(&inputAssembly)
                                  .setPVertexInputState(&vertexState)
                                  .setPDynamicState(&dynamicStateInfo)
                                  .setPRasterizationState(&rasterizationState)
                                  .setPColorBlendState(&blendState)
                                  .setPViewportState(&viewportInfo)
                                  .setPMultisampleState(&msState);

    pipeline = device.createGraphicsPipeline(nullptr, pipelineInfo).value;
    Resources().RequestFree("shaders/origin.vert.spv");
    Resources().RequestFree("shaders/origin.frag.spv");
    // one set per frame slot, each pointing at that slot's uniform buffer
    constexpr auto frameCount = static_cast<uint32_t>(gfx::Graphics::MaxFramesInFlight);
    const auto descPoolSize = vk::DescriptorPoolSize{}.setDescriptorCount(frameCount).setType(vk::DescriptorType::eUniformBuffer);
    descPool = device.createDescriptorPool(vk::DescriptorPoolCreateInfo{}.setMaxSets(frameCount).setPoolSizes(descPoolSize));
    const std::vector<vk::DescriptorSetLayout> setLayouts(frameCount, setLayout);
    const auto sets = device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo{}.setDescriptorPool(descPool).setSetLayouts(setLayouts));
    std::copy(sets.begin(), sets.end(), descriptorSets.begin());

    for (size_t i = 0; i < descriptorSets.size(); ++i)
    {
        const auto ubInfo = vk::DescriptorBufferInfo{}.setBuffer(Graphics().MatrixBuffer(i)).setRange(VK_WHOLE_SIZE);
        device.updateDescriptorSets(vk::WriteDescriptorSet{}
                                        .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                                        .setDescriptorCount(1)
                                        .setBufferInfo(ubInfo)
                                        .setDstSet(descriptorSets[i]),
                                    nullptr);
    }

    auto cmdBuffer =
        device.allocateCommandBuffers(vk::CommandBufferAllocateInfo{}.setCommandPool(Graphics().TransferCommandPool()).setCommandBufferCount(1)).front();
    cmdBuffer.begin(vk::CommandBufferBeginInfo{}.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
    cmdBuffer.copyBuffer(stagingBuffer, vertexBuffer, vk::BufferCopy{}.setSize(sizeof(vertices)));
    cmdBuffer.copyBuffer(stagingBuffer, indexBuffer, vk::BufferCopy{}.setSize(sizeof(indices)).setSrcOffset(sizeof(vertices)));
    cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eBottomOfPipe, {}, {},
                              vk::BufferMemoryBarrier{}
                                  .setBuffer(vertexBuffer)
                                  .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
                                  .setSize(sizeof(vertices))
                                  .setSrcQueueFamilyIndex(Graphics().TransferQueueFamily())
                                  .setDstQueueFamilyIndex(Graphics().GraphicsQueueFamily()),
                              {});
    cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eBottomOfPipe, {}, {},
                              vk::BufferMemoryBarrier{}
                                  .setBuffer(indexBuffer)
                                  .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
                                  .setSize(sizeof(indices))
                                  .setSrcQueueFamilyIndex(Graphics().TransferQueueFamily())
                                  .setDstQueueFamilyIndex(Graphics().GraphicsQueueFamily()),
                              {});
    cmdBuffer.end();
    Graphics().EnqueueTransfer(
        {cmdBuffer, stagingBuffer, stagingBufferAlloc, {std::make_tuple(vertexBuffer, sizeof(vertices)), std::make_tuple(indexBuffer, sizeof(indices))}});
}

OriginGizmo::~OriginGizmo()
{
    auto allocator = Graphics().Allocator();
    const auto device = Graphics().Device();
    device.destroyDescriptorPool(descPool);
    device.destroyPipeline(pipeline);
    device.destroyDescriptorSetLayout(setLayout);
    device.destroyPipelineLayout(pipelineLayout);
    vmaDestroyBuffer(allocator, vertexBuffer, vertexBufferAlloc);
    vmaDestroyBuffer(allocator, indexBuffer, indexBufferAlloc);
}
