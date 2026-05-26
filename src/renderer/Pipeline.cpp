// renderer/Pipeline.cpp
#include "renderer/Pipeline.hpp"
#include <fstream>
#include <stdexcept>
#include <array>

namespace ndde::renderer {

Pipeline::~Pipeline() { destroy(); }

void Pipeline::init(VkDevice device, VkFormat color_format,
                    const std::string& vert_spv_path,
                    const std::string& frag_spv_path,
                    VkPrimitiveTopology topology)
{
    m_device = device;

    auto vert_code = read_spv(vert_spv_path);
    auto frag_code = read_spv(frag_spv_path);
    VkShaderModule vert_mod = create_shader_module(device, vert_code);
    VkShaderModule frag_mod = create_shader_module(device, frag_code);

    std::array<VkPipelineShaderStageCreateInfo, 2> stages{{
        { .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage  = VK_SHADER_STAGE_VERTEX_BIT,   .module = vert_mod, .pName = "main" },
        { .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage  = VK_SHADER_STAGE_FRAGMENT_BIT, .module = frag_mod, .pName = "main" }
    }};

    auto binding = vertex_binding_description();
    auto attribs = vertex_attribute_descriptions();

    VkPipelineVertexInputStateCreateInfo vert_input{
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 1,
        .pVertexBindingDescriptions      = &binding,
        .vertexAttributeDescriptionCount = static_cast<u32>(attribs.size()),
        .pVertexAttributeDescriptions    = attribs.data()
    };

    VkPipelineInputAssemblyStateCreateInfo input_asm{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology               = topology,
        .primitiveRestartEnable = VK_FALSE
    };

    VkPipelineViewportStateCreateInfo viewport_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1, .scissorCount = 1
    };

    VkPipelineRasterizationStateCreateInfo raster{
        .sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode    = VK_CULL_MODE_NONE,
        .frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth   = 1.0f
    };

    VkPipelineMultisampleStateCreateInfo msaa{
        .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    VkPipelineColorBlendAttachmentState blend_att{
        .blendEnable         = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp        = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp        = VK_BLEND_OP_ADD,
        .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };

    VkPipelineColorBlendStateCreateInfo blend{
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1, .pAttachments = &blend_att
    };

    std::array<VkDynamicState, 2> dyn_states{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic{
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<u32>(dyn_states.size()),
        .pDynamicStates    = dyn_states.data()
    };

    VkPushConstantRange push_range{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0, .size = sizeof(PushConstants)
    };

    VkPipelineLayoutCreateInfo layout_info{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pushConstantRangeCount = 1, .pPushConstantRanges = &push_range
    };
    if (vkCreatePipelineLayout(device, &layout_info, nullptr, &m_layout) != VK_SUCCESS)
        throw std::runtime_error("[Pipeline] vkCreatePipelineLayout failed");

    VkPipelineRenderingCreateInfo dyn_render{
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount    = 1,
        .pColorAttachmentFormats = &color_format
    };

    VkGraphicsPipelineCreateInfo pipe_info{
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext               = &dyn_render,
        .stageCount          = static_cast<u32>(stages.size()),
        .pStages             = stages.data(),
        .pVertexInputState   = &vert_input,
        .pInputAssemblyState = &input_asm,
        .pViewportState      = &viewport_state,
        .pRasterizationState = &raster,
        .pMultisampleState   = &msaa,
        .pColorBlendState    = &blend,
        .pDynamicState       = &dynamic,
        .layout              = m_layout,
        .renderPass          = VK_NULL_HANDLE
    };

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipe_info, nullptr, &m_pipeline) != VK_SUCCESS)
        throw std::runtime_error("[Pipeline] vkCreateGraphicsPipelines failed");

    vkDestroyShaderModule(device, vert_mod, nullptr);
    vkDestroyShaderModule(device, frag_mod, nullptr);
}

void Pipeline::destroy() {
    if (m_device == VK_NULL_HANDLE) return;
    if (m_pipeline != VK_NULL_HANDLE) { vkDestroyPipeline(m_device, m_pipeline, nullptr); m_pipeline = VK_NULL_HANDLE; }
    if (m_layout   != VK_NULL_HANDLE) { vkDestroyPipelineLayout(m_device, m_layout, nullptr); m_layout = VK_NULL_HANDLE; }
    m_device = VK_NULL_HANDLE;
}

std::vector<byte> Pipeline::read_spv(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("[Pipeline] Cannot open shader: " + path);
    const auto size = static_cast<std::size_t>(file.tellg());
    std::vector<byte> buf(size);
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(size));
    return buf;
}

VkShaderModule Pipeline::create_shader_module(VkDevice device, const std::vector<byte>& code) {
    VkShaderModuleCreateInfo info{
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code.size(),
        .pCode    = reinterpret_cast<const u32*>(code.data())
    };
    VkShaderModule mod;
    if (vkCreateShaderModule(device, &info, nullptr, &mod) != VK_SUCCESS)
        throw std::runtime_error("[Pipeline] vkCreateShaderModule failed");
    return mod;
}

} // namespace ndde::renderer
