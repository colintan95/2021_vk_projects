#ifndef POINT_LIGHT_APP_H_
#define POINT_LIGHT_APP_H_

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <vector>

#include "utils/camera.h"
#include "utils/model.h"

class App {
public:
  bool Init();
  void Destroy();

  void MainLoop();

private:
  static void GlfwFramebufferResized(GLFWwindow* window, int width, int height);
  static void GlfwKeyCallback(GLFWwindow* window, int key, int scancode,
                              int action, int mods);

  bool InitInstanceAndSurface();
  bool ChoosePhysicalDevice();
  bool CreateDevice();
  bool CreateSwapChain();

  bool CreateScenePassResources();

  bool CreateRenderPass();
  bool CreatePipeline();
  bool CreateFramebuffers();

  bool CreateShadowPassResources();

  bool CreateShadowRenderPass();
  bool CreateShadowPipeline();
  bool CreateShadowFramebuffers();

  bool CreateCommandPool();
  bool CreateCommandBuffers();

  bool CreateDescriptorSets();
  void UpdateScenePassMatrices(int frame_index);

  bool CreateVertexBuffers();

  bool RecordCommandBuffer(int frame_index);
  void TransitionShadowTextureForShadowPass(VkCommandBuffer command_buffer,
                                            int frame_index);
  void RecordShadowPassCommands(VkCommandBuffer command_buffer,
                                int frame_index);
  void TransitionShadowTextureForScenePass(VkCommandBuffer command_buffer,
                                           int frame_index);
  void RecordScenePassCommands(VkCommandBuffer command_buffer, int frame_index);

  bool CreateSyncObjects();

  void UploadDataToBuffer(void* data, VkDeviceSize size, VkBuffer buffer);

  void DestroyVertexBuffers();
  void DestroyDescriptorSets();
  void DestroyCommandBuffers();
  void DestroyCommandPool();
  void DestroyShadowPassResources();
  void DestroyScenePassResources();
  void DestroySwapChain();

  bool DrawFrame();

  bool RecreateSwapChain();

  struct VertexShaderUbo {
    glm::mat4 model_mat;
    glm::mat4 mvp_mat;
  };

  int current_frame_ = 0;
  double current_frame_time_ = 0.0;

  utils::Camera camera_;
  utils::Model model_;

  glm::mat4 model_mat_;
  std::vector<glm::mat4> shadow_mats_;

  uint32_t graphics_queue_index_;
  uint32_t present_queue_index_;

  VkSampleCountFlagBits msaa_sample_count_ = VK_SAMPLE_COUNT_1_BIT;

  bool framebuffer_resized_ = false;

  GLFWwindow* window_;

  VkInstance instance_;
  VkDebugUtilsMessengerEXT debug_messenger_;
  VkSurfaceKHR surface_;
  VkPhysicalDevice physical_device_;
  VkDevice device_;
  VkQueue graphics_queue_;
  VkQueue present_queue_;

  VkSwapchainKHR swap_chain_;
  std::vector<VkImage> swap_chain_images_;
  VkFormat swap_chain_image_format_;
  VkExtent2D swap_chain_extent_;
  std::vector<VkImageView> swap_chain_image_views_;

  VkRenderPass render_pass_;
  VkDescriptorSetLayout descriptor_set_layout_;
  VkPipelineLayout pipeline_layout_;
  VkPipeline pipeline_;
  VkImage color_image_;
  VkDeviceMemory color_image_memory_;
  VkImageView color_image_view_;
  VkImage depth_image_;
  VkDeviceMemory depth_image_memory_;
  VkImageView depth_image_view_;
  std::vector<VkFramebuffer> swap_chain_framebuffers_;

  VkRenderPass shadow_render_pass_;
  VkDescriptorSetLayout shadow_descriptor_layout_;
  VkPipelineLayout shadow_pipeline_layout_;
  VkPipeline shadow_pipeline_;

  struct ShadowPassFrameResource {
    VkImage shadow_texture;
    VkDeviceMemory shadow_texture_memory;
    std::vector<VkImageView> depth_framebuffer_views;
    std::vector<VkFramebuffer> depth_framebuffers;
    VkImageView shadow_texture_view;
  };

  std::vector<ShadowPassFrameResource> shadow_frame_resources_;

  VkCommandPool command_pool_;
  std::vector<VkCommandBuffer> command_buffers_;
  VkDescriptorPool descriptor_pool_;
  std::vector<VkDescriptorSet> descriptor_sets_;

  std::vector<VkBuffer> vert_ubo_buffers_;
  std::vector<VkDeviceMemory> vert_ubo_buffers_memory_;
  std::vector<VkBuffer> frag_ubo_buffers_;
  std::vector<VkDeviceMemory> frag_ubo_buffers_memory_;

  VkSampler shadow_texture_sampler_;

  VkBuffer position_buffer_;
  VkDeviceMemory position_buffer_memory_;
  VkBuffer normal_buffer_;
  VkDeviceMemory normal_buffer_memory_;
  VkBuffer material_idx_buffer_;
  VkDeviceMemory material_idx_buffer_memory_;
  VkBuffer index_buffer_;
  VkDeviceMemory index_buffer_memory_;

  std::vector<VkSemaphore> image_ready_semaphores_;
  std::vector<VkSemaphore> render_complete_semaphores_;
  std::vector<VkFence> frame_ready_fences_;
  std::vector<VkFence> image_rendered_fences_;
};

#endif // POINT_LIGHT_APP_H_