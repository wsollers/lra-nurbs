#pragma once
// renderer/Pipeline.hpp
// Owns one Vulkan graphics pipeline. One instance per topology.
// The math layer never sees this — it uses the Topology enum from Scalars.hpp.

#include <volk.h>
#include "renderer/GpuTypes.hpp"
#include <string>
#include <vector>

namespace ndde::renderer {

class Pipeline {
public:
    Pipeline()  = default;
    ~Pipeline();

    Pipeline(const Pipeline&)            = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&&)                 = delete;
    Pipeline& operator=(Pipeline&&)      = delete;

    void init(VkDevice            device,
              VkFormat            color_format,
              const std::string&  vert_spv_path,
              const std::string&  frag_spv_path,
              VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP);

    void destroy();

    [[nodiscard]] VkPipeline       pipeline() const noexcept { return m_pipeline; }
    [[nodiscard]] VkPipelineLayout layout()   const noexcept { return m_layout;   }
    [[nodiscard]] bool             valid()    const noexcept { return m_pipeline != VK_NULL_HANDLE; }

private:
    VkDevice         m_device   = VK_NULL_HANDLE;
    VkPipeline       m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_layout   = VK_NULL_HANDLE;

    [[nodiscard]] static std::vector<byte> read_spv(const std::string& path);
    [[nodiscard]] static VkShaderModule    create_shader_module(VkDevice device,
                                                                 const std::vector<byte>& code);
};

} // namespace ndde::renderer
