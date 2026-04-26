#include "pch.h"

#include <shroudtopia.h>
#include <memory_utils.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
    constexpr uintptr_t RVA_LOCAL_PLAYER_UI_RENDER_SETUP = 0x300390;
    constexpr uintptr_t RVA_PLAYER_WAYPOINTS_UI = 0x29CFF0;
    constexpr uintptr_t RVA_KNOWLEDGE_QUERY_MAPMARKER_VISIBILITY = 0x381350;
    constexpr uintptr_t RVA_KNOWLEDGE_QUERY_MAPMARKER_VISIBILITY_LOOP = RVA_KNOWLEDGE_QUERY_MAPMARKER_VISIBILITY + 0x37;
    constexpr uintptr_t RVA_RENDER_PRESENT_FRAME = 0xDCFAE0;
    constexpr uintptr_t RVA_VULKAN_DEVICE_TABLE_INIT = 0xDDBDE0;
    constexpr uintptr_t RVA_ITER_INIT = 0x8CD000;
    constexpr uintptr_t RVA_ITER_NEXT = 0x8C84E0;

    constexpr std::size_t LOCAL_PLAYER_UI_RENDER_SETUP_STOLEN_SIZE = 13;
    constexpr std::size_t PLAYER_WAYPOINTS_UI_STOLEN_SIZE = 15;
    constexpr std::size_t KNOWLEDGE_QUERY_MAPMARKER_VISIBILITY_STOLEN_SIZE = 11;
    constexpr std::size_t KNOWLEDGE_QUERY_MAPMARKER_VISIBILITY_LOOP_STOLEN_SIZE = 5;
    constexpr std::size_t RENDER_PRESENT_FRAME_STOLEN_SIZE = 16;
    constexpr std::size_t VULKAN_DEVICE_TABLE_INIT_STOLEN_SIZE = 15;
    constexpr std::size_t UI_RENDER_SETUP_SOURCE_OFFSET = 0x10;
    constexpr std::size_t UI_RENDER_SETUP_STATE_OFFSET = 0x20;
    constexpr std::size_t WAYPOINT_STATE_PTR_OFFSET = 0x10;
    constexpr std::size_t WAYPOINT_ARRAY_OFFSET = 0x2FD390;
    constexpr std::size_t WAYPOINT_COUNT_OFFSET = 0x2FD398;
    constexpr std::size_t WAYPOINT_ENTRY_STRIDE = 0xF0;
    constexpr std::size_t WAYPOINT_ENTRY_POSITION_OFFSET = 0x30;
    constexpr std::size_t WAYPOINT_ENTRY_ACTIVE_OFFSET = 0x48;
    constexpr std::size_t NEARBY_MARKER_ARRAY_OFFSET = 0x3698;
    constexpr std::size_t NEARBY_MARKER_COUNT_OFFSET = 0x36A0;
    constexpr std::size_t INPUT_MARKER_ARRAY_OFFSET = 0x18;
    constexpr std::size_t INPUT_MARKER_COUNT_OFFSET = 0x28;
    constexpr std::size_t NEARBY_MARKER_ENTRY_STRIDE = 0x28;
    constexpr std::size_t DYNAMIC_MARKER_SCAN_BYTES = 0x90;
    constexpr std::size_t WAYPOINT_MAX_ENTRIES = 1024;
    constexpr std::size_t NEARBY_MARKER_MAX_ENTRIES = 2048;
    constexpr std::size_t VISIBLE_MAP_MARKER_MAX_ENTRIES = 2048;
    constexpr DWORD VISIBLE_MAP_MARKER_STALE_MS = 180000;
    constexpr DWORD VISIBLE_MAP_MARKER_PRUNE_MS = 2000;
    constexpr std::size_t VULKAN_TABLE_QUEUE_SUBMIT_OFFSET = 0xB8;
    constexpr std::size_t VULKAN_TABLE_GET_DEVICE_PROC_ADDR_OFFSET = 0x78;
    constexpr std::size_t VULKAN_TABLE_CREATE_SWAPCHAIN_OFFSET = 0x570;
    constexpr std::size_t VULKAN_TABLE_GET_SWAPCHAIN_IMAGES_OFFSET = 0x580;
    constexpr std::size_t VULKAN_TABLE_ACQUIRE_NEXT_IMAGE_OFFSET = 0x588;
    constexpr std::size_t VULKAN_TABLE_QUEUE_PRESENT_OFFSET = 0x590;
    constexpr std::size_t VULKAN_DEVICE_TABLE_SCAN_LIMIT = 0x900;
    constexpr DWORD WORLD_DATA_STALE_MS = 15000;
    constexpr DWORD SESSION_LOG_POLL_MS = 1000;
    constexpr DWORD SESSION_LOG_TAIL_BYTES = 128 * 1024;
    constexpr DWORD PLAYER_CAMERA_SCAN_INTERVAL_MS = 350;
    constexpr std::size_t CLIENT_CAMERA_SIZE = 0x40;
    constexpr std::size_t PLAYER_CAMERA_ROOT_SCAN_BYTES = 0x2600;
    constexpr std::size_t PLAYER_CAMERA_CHILD_SCAN_BYTES = 0x1200;
    constexpr std::size_t PLAYER_CAMERA_POINTER_SCAN_BYTES = 0x180;
    constexpr int REAL_MAP_MIN_TEXTURE_SIZE = 512;
    constexpr int REAL_MAP_MAX_TEXTURE_SIZE = 2048;
    constexpr float REAL_MAP_WORLD_SIZE = 10240.0f;
    constexpr std::size_t STATIC_POI_ENTRY_SIZE = 16;
    constexpr std::size_t STATIC_POI_MAX_ENTRIES = 4096;
    constexpr std::size_t STATIC_POI_MAX_DRAWN = 96;
    constexpr float MINIMAP_BASE_UNITS_PER_PIXEL = 3.25f;
    constexpr float STATIC_POI_DRAW_RADIUS_FACTOR = 0.78f;
    constexpr int MINIMAP_ICON_MIN_SIZE = 8;
    constexpr int MINIMAP_ICON_MAX_SIZE = 64;
    constexpr std::size_t MINIMAP_ICON_MAX_ENTRIES = 256;
    constexpr int MINIMAP_FRAME_MIN_SIZE = 128;
    constexpr int MINIMAP_FRAME_MAX_SIZE = 1024;
    constexpr std::uint32_t MAP_MARKER_QUEST_IMPORTANT_KIND = 0xA7CADC08u;
    constexpr std::uint32_t MAP_MARKER_QUEST_KIND = 0xC52E92E5u;

    constexpr double FIXED_32_32_TO_WORLD = 1.0 / 4294967296.0;

    constexpr std::int32_t VK_SUCCESS = 0;
    constexpr std::uint32_t VK_STRUCTURE_TYPE_SUBMIT_INFO = 4;
    constexpr std::uint32_t VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO = 9;
    constexpr std::uint32_t VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO = 15;
    constexpr std::uint32_t VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO = 37;
    constexpr std::uint32_t VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO = 38;
    constexpr std::uint32_t VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO = 39;
    constexpr std::uint32_t VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO = 40;
    constexpr std::uint32_t VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO = 42;
    constexpr std::uint32_t VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO = 43;
    constexpr std::uint32_t VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER = 46;
    constexpr std::uint32_t VK_IMAGE_VIEW_TYPE_2D = 1;
    constexpr std::uint32_t VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO = 5;
    constexpr std::uint32_t VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO = 12;
    constexpr std::uint32_t VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO = 14;
    constexpr std::uint32_t VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO = 16;
    constexpr std::uint32_t VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO = 18;
    constexpr std::uint32_t VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO = 19;
    constexpr std::uint32_t VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO = 20;
    constexpr std::uint32_t VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO = 22;
    constexpr std::uint32_t VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO = 23;
    constexpr std::uint32_t VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO = 24;
    constexpr std::uint32_t VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO = 26;
    constexpr std::uint32_t VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO = 28;
    constexpr std::uint32_t VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO = 30;
    constexpr std::uint32_t VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO = 31;
    constexpr std::uint32_t VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO = 32;
    constexpr std::uint32_t VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO = 33;
    constexpr std::uint32_t VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO = 34;
    constexpr std::uint32_t VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET = 35;
    constexpr std::uint32_t VK_COMPONENT_SWIZZLE_IDENTITY = 0;
    constexpr std::uint32_t VK_FORMAT_R8G8B8A8_UNORM = 37;
    constexpr std::uint32_t VK_IMAGE_ASPECT_COLOR_BIT = 0x1;
    constexpr std::uint32_t VK_IMAGE_TYPE_2D = 0;
    constexpr std::uint32_t VK_IMAGE_TILING_OPTIMAL = 0;
    constexpr std::uint32_t VK_IMAGE_USAGE_TRANSFER_DST_BIT = 0x2;
    constexpr std::uint32_t VK_IMAGE_USAGE_SAMPLED_BIT = 0x4;
    constexpr std::uint32_t VK_BUFFER_USAGE_TRANSFER_SRC_BIT = 0x1;
    constexpr std::uint32_t VK_SAMPLE_COUNT_1_BIT = 0x1;
    constexpr std::uint32_t VK_ATTACHMENT_LOAD_OP_LOAD = 0;
    constexpr std::uint32_t VK_ATTACHMENT_STORE_OP_STORE = 0;
    constexpr std::uint32_t VK_ATTACHMENT_LOAD_OP_DONT_CARE = 2;
    constexpr std::uint32_t VK_ATTACHMENT_STORE_OP_DONT_CARE = 1;
    constexpr std::uint32_t VK_IMAGE_LAYOUT_UNDEFINED = 0;
    constexpr std::uint32_t VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL = 2;
    constexpr std::uint32_t VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL = 5;
    constexpr std::uint32_t VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL = 6;
    constexpr std::uint32_t VK_IMAGE_LAYOUT_PRESENT_SRC_KHR = 1000001002;
    constexpr std::uint32_t VK_PIPELINE_BIND_POINT_GRAPHICS = 0;
    constexpr std::uint32_t VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT = 0x2;
    constexpr std::uint32_t VK_COMMAND_BUFFER_LEVEL_PRIMARY = 0;
    constexpr std::uint32_t VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT = 0x1;
    constexpr std::uint32_t VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT = 0x1;
    constexpr std::uint32_t VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT = 0x80;
    constexpr std::uint32_t VK_PIPELINE_STAGE_TRANSFER_BIT = 0x1000;
    constexpr std::uint32_t VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT = 0x400;
    constexpr std::uint32_t VK_ACCESS_TRANSFER_WRITE_BIT = 0x1000;
    constexpr std::uint32_t VK_ACCESS_SHADER_READ_BIT = 0x20;
    constexpr std::uint32_t VK_SHARING_MODE_EXCLUSIVE = 0;
    constexpr std::uint32_t VK_FILTER_LINEAR = 1;
    constexpr std::uint32_t VK_SAMPLER_MIPMAP_MODE_LINEAR = 1;
    constexpr std::uint32_t VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE = 2;
    constexpr std::uint32_t VK_BORDER_COLOR_INT_OPAQUE_BLACK = 0;
    constexpr std::uint32_t VK_COMPARE_OP_ALWAYS = 7;
    constexpr std::uint32_t VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER = 1;
    constexpr std::uint32_t VK_SHADER_STAGE_VERTEX_BIT = 0x1;
    constexpr std::uint32_t VK_SHADER_STAGE_FRAGMENT_BIT = 0x10;
    constexpr std::uint32_t VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST = 3;
    constexpr std::uint32_t VK_POLYGON_MODE_FILL = 0;
    constexpr std::uint32_t VK_CULL_MODE_NONE = 0;
    constexpr std::uint32_t VK_FRONT_FACE_COUNTER_CLOCKWISE = 1;
    constexpr std::uint32_t VK_COLOR_COMPONENT_R_BIT = 0x1;
    constexpr std::uint32_t VK_COLOR_COMPONENT_G_BIT = 0x2;
    constexpr std::uint32_t VK_COLOR_COMPONENT_B_BIT = 0x4;
    constexpr std::uint32_t VK_COLOR_COMPONENT_A_BIT = 0x8;
    constexpr std::uint32_t VK_BLEND_FACTOR_ZERO = 0;
    constexpr std::uint32_t VK_BLEND_FACTOR_ONE = 1;
    constexpr std::uint32_t VK_BLEND_FACTOR_SRC_ALPHA = 6;
    constexpr std::uint32_t VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA = 7;
    constexpr std::uint32_t VK_BLEND_OP_ADD = 0;
    constexpr std::uint32_t VK_LOGIC_OP_COPY = 3;

    using IterInitFn = void(__fastcall*)(void* ctx, void* buffer, std::uint32_t size);
    using IterNextFn = bool(__fastcall*)(void* ctx, void* buffer, std::uint32_t size);

    struct UiRenderSetupIterationRecord
    {
        void* unknown0 = nullptr;
        void* local30 = nullptr;
        std::uint8_t* source = nullptr;
        void* entity = nullptr;
        std::uint8_t* state = nullptr;
    };
    static_assert(sizeof(UiRenderSetupIterationRecord) == 0x28, "Unexpected local_player_ui_render_setup iteration record size");
    static_assert(offsetof(UiRenderSetupIterationRecord, source) == UI_RENDER_SETUP_SOURCE_OFFSET, "Unexpected UI render source offset");
    static_assert(offsetof(UiRenderSetupIterationRecord, state) == UI_RENDER_SETUP_STATE_OFFSET, "Unexpected UI render state offset");

    struct WaypointsUiIterationRecord
    {
        void* unknown0 = nullptr;
        void* unknown8 = nullptr;
        std::uint8_t* state = nullptr;
        void* lookupContext = nullptr;
        void* waypointList = nullptr;
        void* playerList = nullptr;
    };
    static_assert(sizeof(WaypointsUiIterationRecord) == 0x30, "Unexpected player_waypoints_ui iteration record size");
    static_assert(offsetof(WaypointsUiIterationRecord, state) == WAYPOINT_STATE_PTR_OFFSET, "Unexpected state offset");

    struct MapMarkerVisibilityIterationRecord
    {
        void* unknown0 = nullptr;
        std::uint8_t* marker = nullptr;
        std::uint8_t* visibilityState = nullptr;
        void* refCounter = nullptr;
        void* knowledge = nullptr;
        void* entity = nullptr;
    };
    static_assert(sizeof(MapMarkerVisibilityIterationRecord) == 0x30, "Unexpected map marker visibility iteration record size");

    struct CapturedWaypoint
    {
        std::uint64_t raw[8] = {};
        std::uint32_t id = 0;
        std::int64_t x = 0;
        std::int64_t y = 0;
        std::int64_t z = 0;
    };

    struct CapturedNearbyMarker
    {
        std::uint64_t raw[5] = {};
        std::int64_t x = 0;
        std::int64_t y = 0;
        std::int64_t z = 0;
        std::uint32_t kind = 0;
        bool hasWorldPosition = false;
    };

    struct CapturedMapMarkerVisibility
    {
        std::uint64_t raw[5] = {};
        uintptr_t markerAddress = 0;
        uintptr_t stateAddress = 0;
        uintptr_t entityAddress = 0;
        std::int64_t x = 0;
        std::int64_t y = 0;
        std::int64_t z = 0;
        std::uint32_t markerId = 0;
        std::uint32_t markerType = 0;
        std::uint32_t kind = 0;
        std::uint8_t visibility = 0;
        DWORD lastUpdateTick = 0;
        bool hasWorldPosition = false;
    };

    struct CapturedPlayerPosition
    {
        std::int64_t x = 0;
        std::int64_t y = 0;
        std::int64_t z = 0;
        uintptr_t source = 0;
        std::uint32_t offset = 0;
        std::uint32_t channel = 0;
        float headingRadians = 0.0f;
        DWORD lastUpdateTick = 0;
        bool hasHeading = false;
        bool valid = false;
    };

    struct VkExtent2D
    {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
    };

    struct VkExtent3D
    {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint32_t depth = 1;
    };

    struct VkOffset2D
    {
        std::int32_t x = 0;
        std::int32_t y = 0;
    };

    struct VkOffset3D
    {
        std::int32_t x = 0;
        std::int32_t y = 0;
        std::int32_t z = 0;
    };

    struct VkRect2D
    {
        VkOffset2D offset;
        VkExtent2D extent;
    };

    struct VkSwapchainCreateInfoKHR
    {
        std::uint32_t sType;
        const void* pNext;
        std::uint32_t flags;
        void* surface;
        std::uint32_t minImageCount;
        std::uint32_t imageFormat;
        std::uint32_t imageColorSpace;
        VkExtent2D imageExtent;
        std::uint32_t imageArrayLayers;
        std::uint32_t imageUsage;
        std::uint32_t imageSharingMode;
        std::uint32_t queueFamilyIndexCount;
        const std::uint32_t* pQueueFamilyIndices;
        std::uint32_t preTransform;
        std::uint32_t compositeAlpha;
        std::uint32_t presentMode;
        std::uint32_t clipped;
        void* oldSwapchain;
    };

    struct VkPresentInfoKHR
    {
        std::uint32_t sType;
        const void* pNext;
        std::uint32_t waitSemaphoreCount;
        const void* const* pWaitSemaphores;
        std::uint32_t swapchainCount;
        const void* const* pSwapchains;
        const std::uint32_t* pImageIndices;
        std::int32_t* pResults;
    };
    static_assert(offsetof(VkPresentInfoKHR, pSwapchains) == 0x28, "Unexpected VkPresentInfoKHR pSwapchains offset");
    static_assert(offsetof(VkPresentInfoKHR, pImageIndices) == 0x30, "Unexpected VkPresentInfoKHR pImageIndices offset");

    struct VkComponentMapping
    {
        std::uint32_t r = VK_COMPONENT_SWIZZLE_IDENTITY;
        std::uint32_t g = VK_COMPONENT_SWIZZLE_IDENTITY;
        std::uint32_t b = VK_COMPONENT_SWIZZLE_IDENTITY;
        std::uint32_t a = VK_COMPONENT_SWIZZLE_IDENTITY;
    };

    struct VkImageSubresourceRange
    {
        std::uint32_t aspectMask = 0;
        std::uint32_t baseMipLevel = 0;
        std::uint32_t levelCount = 0;
        std::uint32_t baseArrayLayer = 0;
        std::uint32_t layerCount = 0;
    };

    struct VkImageSubresourceLayers
    {
        std::uint32_t aspectMask = 0;
        std::uint32_t mipLevel = 0;
        std::uint32_t baseArrayLayer = 0;
        std::uint32_t layerCount = 1;
    };

    struct VkImageViewCreateInfo
    {
        std::uint32_t sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        const void* pNext = nullptr;
        std::uint32_t flags = 0;
        void* image = nullptr;
        std::uint32_t viewType = VK_IMAGE_VIEW_TYPE_2D;
        std::uint32_t format = 0;
        VkComponentMapping components;
        VkImageSubresourceRange subresourceRange;
    };

    struct VkAttachmentDescription
    {
        std::uint32_t flags = 0;
        std::uint32_t format = 0;
        std::uint32_t samples = VK_SAMPLE_COUNT_1_BIT;
        std::uint32_t loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        std::uint32_t storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        std::uint32_t stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        std::uint32_t stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        std::uint32_t initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        std::uint32_t finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    };

    struct VkAttachmentReference
    {
        std::uint32_t attachment = 0;
        std::uint32_t layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    };

    struct VkSubpassDescription
    {
        std::uint32_t flags = 0;
        std::uint32_t pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        std::uint32_t inputAttachmentCount = 0;
        const void* pInputAttachments = nullptr;
        std::uint32_t colorAttachmentCount = 0;
        const VkAttachmentReference* pColorAttachments = nullptr;
        const void* pResolveAttachments = nullptr;
        const void* pDepthStencilAttachment = nullptr;
        std::uint32_t preserveAttachmentCount = 0;
        const std::uint32_t* pPreserveAttachments = nullptr;
    };

    struct VkRenderPassCreateInfo
    {
        std::uint32_t sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        const void* pNext = nullptr;
        std::uint32_t flags = 0;
        std::uint32_t attachmentCount = 0;
        const VkAttachmentDescription* pAttachments = nullptr;
        std::uint32_t subpassCount = 0;
        const VkSubpassDescription* pSubpasses = nullptr;
        std::uint32_t dependencyCount = 0;
        const void* pDependencies = nullptr;
    };

    struct VkFramebufferCreateInfo
    {
        std::uint32_t sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        const void* pNext = nullptr;
        std::uint32_t flags = 0;
        void* renderPass = nullptr;
        std::uint32_t attachmentCount = 0;
        void* const* pAttachments = nullptr;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint32_t layers = 1;
    };

    struct VkCommandPoolCreateInfo
    {
        std::uint32_t sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        const void* pNext = nullptr;
        std::uint32_t flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        std::uint32_t queueFamilyIndex = 0;
    };

    struct VkCommandBufferAllocateInfo
    {
        std::uint32_t sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        const void* pNext = nullptr;
        void* commandPool = nullptr;
        std::uint32_t level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        std::uint32_t commandBufferCount = 0;
    };

    struct VkCommandBufferBeginInfo
    {
        std::uint32_t sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        const void* pNext = nullptr;
        std::uint32_t flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        const void* pInheritanceInfo = nullptr;
    };

    union VkClearColorValue
    {
        float float32[4];
        std::int32_t int32[4];
        std::uint32_t uint32[4];
    };

    union VkClearValue
    {
        VkClearColorValue color;
    };

    struct VkRenderPassBeginInfo
    {
        std::uint32_t sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        const void* pNext = nullptr;
        void* renderPass = nullptr;
        void* framebuffer = nullptr;
        VkRect2D renderArea;
        std::uint32_t clearValueCount = 0;
        const VkClearValue* pClearValues = nullptr;
    };

    struct VkClearAttachment
    {
        std::uint32_t aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        std::uint32_t colorAttachment = 0;
        VkClearValue clearValue;
    };

    struct VkClearRect
    {
        VkRect2D rect;
        std::uint32_t baseArrayLayer = 0;
        std::uint32_t layerCount = 1;
    };

    struct VkSemaphoreCreateInfo
    {
        std::uint32_t sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        const void* pNext = nullptr;
        std::uint32_t flags = 0;
    };

    struct VkMemoryRequirements
    {
        std::uint64_t size = 0;
        std::uint64_t alignment = 0;
        std::uint32_t memoryTypeBits = 0;
    };

    struct VkMemoryAllocateInfo
    {
        std::uint32_t sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        const void* pNext = nullptr;
        std::uint64_t allocationSize = 0;
        std::uint32_t memoryTypeIndex = 0;
    };

    struct VkBufferCreateInfo
    {
        std::uint32_t sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        const void* pNext = nullptr;
        std::uint32_t flags = 0;
        std::uint64_t size = 0;
        std::uint32_t usage = 0;
        std::uint32_t sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        std::uint32_t queueFamilyIndexCount = 0;
        const std::uint32_t* pQueueFamilyIndices = nullptr;
    };

    struct VkImageCreateInfo
    {
        std::uint32_t sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        const void* pNext = nullptr;
        std::uint32_t flags = 0;
        std::uint32_t imageType = VK_IMAGE_TYPE_2D;
        std::uint32_t format = VK_FORMAT_R8G8B8A8_UNORM;
        VkExtent3D extent;
        std::uint32_t mipLevels = 1;
        std::uint32_t arrayLayers = 1;
        std::uint32_t samples = VK_SAMPLE_COUNT_1_BIT;
        std::uint32_t tiling = VK_IMAGE_TILING_OPTIMAL;
        std::uint32_t usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        std::uint32_t sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        std::uint32_t queueFamilyIndexCount = 0;
        const std::uint32_t* pQueueFamilyIndices = nullptr;
        std::uint32_t initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    };

    struct VkBufferImageCopy
    {
        std::uint64_t bufferOffset = 0;
        std::uint32_t bufferRowLength = 0;
        std::uint32_t bufferImageHeight = 0;
        VkImageSubresourceLayers imageSubresource;
        VkOffset3D imageOffset;
        VkExtent3D imageExtent;
    };

    struct VkImageMemoryBarrier
    {
        std::uint32_t sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        const void* pNext = nullptr;
        std::uint32_t srcAccessMask = 0;
        std::uint32_t dstAccessMask = 0;
        std::uint32_t oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        std::uint32_t newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        std::uint32_t srcQueueFamilyIndex = 0xFFFFFFFFu;
        std::uint32_t dstQueueFamilyIndex = 0xFFFFFFFFu;
        void* image = nullptr;
        VkImageSubresourceRange subresourceRange;
    };

    struct VkSamplerCreateInfo
    {
        std::uint32_t sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        const void* pNext = nullptr;
        std::uint32_t flags = 0;
        std::uint32_t magFilter = VK_FILTER_LINEAR;
        std::uint32_t minFilter = VK_FILTER_LINEAR;
        std::uint32_t mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        std::uint32_t addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        std::uint32_t addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        std::uint32_t addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        float mipLodBias = 0.0f;
        std::uint32_t anisotropyEnable = 0;
        float maxAnisotropy = 1.0f;
        std::uint32_t compareEnable = 0;
        std::uint32_t compareOp = VK_COMPARE_OP_ALWAYS;
        float minLod = 0.0f;
        float maxLod = 0.0f;
        std::uint32_t borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        std::uint32_t unnormalizedCoordinates = 0;
    };

    struct VkShaderModuleCreateInfo
    {
        std::uint32_t sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        const void* pNext = nullptr;
        std::uint32_t flags = 0;
        std::size_t codeSize = 0;
        const std::uint32_t* pCode = nullptr;
    };

    struct VkDescriptorSetLayoutBinding
    {
        std::uint32_t binding = 0;
        std::uint32_t descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        std::uint32_t descriptorCount = 1;
        std::uint32_t stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        const void* pImmutableSamplers = nullptr;
    };

    struct VkDescriptorSetLayoutCreateInfo
    {
        std::uint32_t sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        const void* pNext = nullptr;
        std::uint32_t flags = 0;
        std::uint32_t bindingCount = 0;
        const VkDescriptorSetLayoutBinding* pBindings = nullptr;
    };

    struct VkDescriptorPoolSize
    {
        std::uint32_t type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        std::uint32_t descriptorCount = 1;
    };

    struct VkDescriptorPoolCreateInfo
    {
        std::uint32_t sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        const void* pNext = nullptr;
        std::uint32_t flags = 0;
        std::uint32_t maxSets = 1;
        std::uint32_t poolSizeCount = 0;
        const VkDescriptorPoolSize* pPoolSizes = nullptr;
    };

    struct VkDescriptorSetAllocateInfo
    {
        std::uint32_t sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        const void* pNext = nullptr;
        void* descriptorPool = nullptr;
        std::uint32_t descriptorSetCount = 0;
        void* const* pSetLayouts = nullptr;
    };

    struct VkDescriptorImageInfo
    {
        void* sampler = nullptr;
        void* imageView = nullptr;
        std::uint32_t imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    };

    struct VkWriteDescriptorSet
    {
        std::uint32_t sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        const void* pNext = nullptr;
        void* dstSet = nullptr;
        std::uint32_t dstBinding = 0;
        std::uint32_t dstArrayElement = 0;
        std::uint32_t descriptorCount = 0;
        std::uint32_t descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        const VkDescriptorImageInfo* pImageInfo = nullptr;
        const void* pBufferInfo = nullptr;
        const void* pTexelBufferView = nullptr;
    };

    struct VkPushConstantRange
    {
        std::uint32_t stageFlags = 0;
        std::uint32_t offset = 0;
        std::uint32_t size = 0;
    };

    struct VkPipelineLayoutCreateInfo
    {
        std::uint32_t sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        const void* pNext = nullptr;
        std::uint32_t flags = 0;
        std::uint32_t setLayoutCount = 0;
        void* const* pSetLayouts = nullptr;
        std::uint32_t pushConstantRangeCount = 0;
        const VkPushConstantRange* pPushConstantRanges = nullptr;
    };

    struct VkPipelineShaderStageCreateInfo
    {
        std::uint32_t sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        const void* pNext = nullptr;
        std::uint32_t flags = 0;
        std::uint32_t stage = 0;
        void* module = nullptr;
        const char* pName = "main";
        const void* pSpecializationInfo = nullptr;
    };

    struct VkPipelineVertexInputStateCreateInfo
    {
        std::uint32_t sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        const void* pNext = nullptr;
        std::uint32_t flags = 0;
        std::uint32_t vertexBindingDescriptionCount = 0;
        const void* pVertexBindingDescriptions = nullptr;
        std::uint32_t vertexAttributeDescriptionCount = 0;
        const void* pVertexAttributeDescriptions = nullptr;
    };

    struct VkPipelineInputAssemblyStateCreateInfo
    {
        std::uint32_t sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        const void* pNext = nullptr;
        std::uint32_t flags = 0;
        std::uint32_t topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        std::uint32_t primitiveRestartEnable = 0;
    };

    struct VkViewport
    {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        float minDepth = 0.0f;
        float maxDepth = 1.0f;
    };

    struct VkPipelineViewportStateCreateInfo
    {
        std::uint32_t sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        const void* pNext = nullptr;
        std::uint32_t flags = 0;
        std::uint32_t viewportCount = 0;
        const VkViewport* pViewports = nullptr;
        std::uint32_t scissorCount = 0;
        const VkRect2D* pScissors = nullptr;
    };

    struct VkPipelineRasterizationStateCreateInfo
    {
        std::uint32_t sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        const void* pNext = nullptr;
        std::uint32_t flags = 0;
        std::uint32_t depthClampEnable = 0;
        std::uint32_t rasterizerDiscardEnable = 0;
        std::uint32_t polygonMode = VK_POLYGON_MODE_FILL;
        std::uint32_t cullMode = VK_CULL_MODE_NONE;
        std::uint32_t frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        std::uint32_t depthBiasEnable = 0;
        float depthBiasConstantFactor = 0.0f;
        float depthBiasClamp = 0.0f;
        float depthBiasSlopeFactor = 0.0f;
        float lineWidth = 1.0f;
    };

    struct VkPipelineMultisampleStateCreateInfo
    {
        std::uint32_t sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        const void* pNext = nullptr;
        std::uint32_t flags = 0;
        std::uint32_t rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        std::uint32_t sampleShadingEnable = 0;
        float minSampleShading = 0.0f;
        const void* pSampleMask = nullptr;
        std::uint32_t alphaToCoverageEnable = 0;
        std::uint32_t alphaToOneEnable = 0;
    };

    struct VkPipelineColorBlendAttachmentState
    {
        std::uint32_t blendEnable = 0;
        std::uint32_t srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        std::uint32_t dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        std::uint32_t colorBlendOp = VK_BLEND_OP_ADD;
        std::uint32_t srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        std::uint32_t dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        std::uint32_t alphaBlendOp = VK_BLEND_OP_ADD;
        std::uint32_t colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    };

    struct VkPipelineColorBlendStateCreateInfo
    {
        std::uint32_t sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        const void* pNext = nullptr;
        std::uint32_t flags = 0;
        std::uint32_t logicOpEnable = 0;
        std::uint32_t logicOp = VK_LOGIC_OP_COPY;
        std::uint32_t attachmentCount = 0;
        const VkPipelineColorBlendAttachmentState* pAttachments = nullptr;
        float blendConstants[4] = {};
    };

    struct VkGraphicsPipelineCreateInfo
    {
        std::uint32_t sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        const void* pNext = nullptr;
        std::uint32_t flags = 0;
        std::uint32_t stageCount = 0;
        const VkPipelineShaderStageCreateInfo* pStages = nullptr;
        const VkPipelineVertexInputStateCreateInfo* pVertexInputState = nullptr;
        const VkPipelineInputAssemblyStateCreateInfo* pInputAssemblyState = nullptr;
        const void* pTessellationState = nullptr;
        const VkPipelineViewportStateCreateInfo* pViewportState = nullptr;
        const VkPipelineRasterizationStateCreateInfo* pRasterizationState = nullptr;
        const VkPipelineMultisampleStateCreateInfo* pMultisampleState = nullptr;
        const void* pDepthStencilState = nullptr;
        const VkPipelineColorBlendStateCreateInfo* pColorBlendState = nullptr;
        const void* pDynamicState = nullptr;
        void* layout = nullptr;
        void* renderPass = nullptr;
        std::uint32_t subpass = 0;
        void* basePipelineHandle = nullptr;
        std::int32_t basePipelineIndex = -1;
    };

    struct VkSubmitInfo
    {
        std::uint32_t sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        const void* pNext = nullptr;
        std::uint32_t waitSemaphoreCount = 0;
        const void* const* pWaitSemaphores = nullptr;
        const std::uint32_t* pWaitDstStageMask = nullptr;
        std::uint32_t commandBufferCount = 0;
        const void* const* pCommandBuffers = nullptr;
        std::uint32_t signalSemaphoreCount = 0;
        const void* const* pSignalSemaphores = nullptr;
    };

    using QueuePresentFn = std::int32_t(__fastcall*)(void* queue, const void* presentInfo);
    using CreateSwapchainFn = std::int32_t(__fastcall*)(void* device, const VkSwapchainCreateInfoKHR* createInfo, const void* allocator, void** swapchain);
    using GetSwapchainImagesFn = std::int32_t(__fastcall*)(void* device, void* swapchain, std::uint32_t* count, void* images);
    using GetDeviceProcAddrFn = void* (__fastcall*)(void* device, const char* name);
    using CreateImageViewFn = std::int32_t(__fastcall*)(void* device, const VkImageViewCreateInfo* createInfo, const void* allocator, void** imageView);
    using DestroyImageViewFn = void(__fastcall*)(void* device, void* imageView, const void* allocator);
    using CreateRenderPassFn = std::int32_t(__fastcall*)(void* device, const VkRenderPassCreateInfo* createInfo, const void* allocator, void** renderPass);
    using DestroyRenderPassFn = void(__fastcall*)(void* device, void* renderPass, const void* allocator);
    using CreateFramebufferFn = std::int32_t(__fastcall*)(void* device, const VkFramebufferCreateInfo* createInfo, const void* allocator, void** framebuffer);
    using DestroyFramebufferFn = void(__fastcall*)(void* device, void* framebuffer, const void* allocator);
    using CreateCommandPoolFn = std::int32_t(__fastcall*)(void* device, const VkCommandPoolCreateInfo* createInfo, const void* allocator, void** commandPool);
    using DestroyCommandPoolFn = void(__fastcall*)(void* device, void* commandPool, const void* allocator);
    using AllocateCommandBuffersFn = std::int32_t(__fastcall*)(void* device, const VkCommandBufferAllocateInfo* allocateInfo, void** commandBuffers);
    using ResetCommandBufferFn = std::int32_t(__fastcall*)(void* commandBuffer, std::uint32_t flags);
    using BeginCommandBufferFn = std::int32_t(__fastcall*)(void* commandBuffer, const VkCommandBufferBeginInfo* beginInfo);
    using EndCommandBufferFn = std::int32_t(__fastcall*)(void* commandBuffer);
    using CmdBeginRenderPassFn = void(__fastcall*)(void* commandBuffer, const VkRenderPassBeginInfo* beginInfo, std::uint32_t contents);
    using CmdEndRenderPassFn = void(__fastcall*)(void* commandBuffer);
    using CmdClearAttachmentsFn = void(__fastcall*)(void* commandBuffer, std::uint32_t attachmentCount, const VkClearAttachment* attachments, std::uint32_t rectCount, const VkClearRect* rects);
    using CreateBufferFn = std::int32_t(__fastcall*)(void* device, const VkBufferCreateInfo* createInfo, const void* allocator, void** buffer);
    using DestroyBufferFn = void(__fastcall*)(void* device, void* buffer, const void* allocator);
    using GetBufferMemoryRequirementsFn = void(__fastcall*)(void* device, void* buffer, VkMemoryRequirements* memoryRequirements);
    using CreateImageFn = std::int32_t(__fastcall*)(void* device, const VkImageCreateInfo* createInfo, const void* allocator, void** image);
    using DestroyImageFn = void(__fastcall*)(void* device, void* image, const void* allocator);
    using GetImageMemoryRequirementsFn = void(__fastcall*)(void* device, void* image, VkMemoryRequirements* memoryRequirements);
    using AllocateMemoryFn = std::int32_t(__fastcall*)(void* device, const VkMemoryAllocateInfo* allocateInfo, const void* allocator, void** memory);
    using FreeMemoryFn = void(__fastcall*)(void* device, void* memory, const void* allocator);
    using BindBufferMemoryFn = std::int32_t(__fastcall*)(void* device, void* buffer, void* memory, std::uint64_t memoryOffset);
    using BindImageMemoryFn = std::int32_t(__fastcall*)(void* device, void* image, void* memory, std::uint64_t memoryOffset);
    using MapMemoryFn = std::int32_t(__fastcall*)(void* device, void* memory, std::uint64_t offset, std::uint64_t size, std::uint32_t flags, void** data);
    using UnmapMemoryFn = void(__fastcall*)(void* device, void* memory);
    using CreateSamplerFn = std::int32_t(__fastcall*)(void* device, const VkSamplerCreateInfo* createInfo, const void* allocator, void** sampler);
    using DestroySamplerFn = void(__fastcall*)(void* device, void* sampler, const void* allocator);
    using CreateShaderModuleFn = std::int32_t(__fastcall*)(void* device, const VkShaderModuleCreateInfo* createInfo, const void* allocator, void** shaderModule);
    using DestroyShaderModuleFn = void(__fastcall*)(void* device, void* shaderModule, const void* allocator);
    using CreateDescriptorSetLayoutFn = std::int32_t(__fastcall*)(void* device, const VkDescriptorSetLayoutCreateInfo* createInfo, const void* allocator, void** setLayout);
    using DestroyDescriptorSetLayoutFn = void(__fastcall*)(void* device, void* setLayout, const void* allocator);
    using CreateDescriptorPoolFn = std::int32_t(__fastcall*)(void* device, const VkDescriptorPoolCreateInfo* createInfo, const void* allocator, void** descriptorPool);
    using DestroyDescriptorPoolFn = void(__fastcall*)(void* device, void* descriptorPool, const void* allocator);
    using AllocateDescriptorSetsFn = std::int32_t(__fastcall*)(void* device, const VkDescriptorSetAllocateInfo* allocateInfo, void** descriptorSets);
    using UpdateDescriptorSetsFn = void(__fastcall*)(void* device, std::uint32_t descriptorWriteCount, const VkWriteDescriptorSet* descriptorWrites, std::uint32_t descriptorCopyCount, const void* descriptorCopies);
    using CreatePipelineLayoutFn = std::int32_t(__fastcall*)(void* device, const VkPipelineLayoutCreateInfo* createInfo, const void* allocator, void** pipelineLayout);
    using DestroyPipelineLayoutFn = void(__fastcall*)(void* device, void* pipelineLayout, const void* allocator);
    using CreateGraphicsPipelinesFn = std::int32_t(__fastcall*)(void* device, void* pipelineCache, std::uint32_t createInfoCount, const VkGraphicsPipelineCreateInfo* createInfos, const void* allocator, void** pipelines);
    using DestroyPipelineFn = void(__fastcall*)(void* device, void* pipeline, const void* allocator);
    using CmdPipelineBarrierFn = void(__fastcall*)(void* commandBuffer, std::uint32_t srcStageMask, std::uint32_t dstStageMask, std::uint32_t dependencyFlags, std::uint32_t memoryBarrierCount, const void* memoryBarriers, std::uint32_t bufferMemoryBarrierCount, const void* bufferMemoryBarriers, std::uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* imageMemoryBarriers);
    using CmdCopyBufferToImageFn = void(__fastcall*)(void* commandBuffer, void* srcBuffer, void* dstImage, std::uint32_t dstImageLayout, std::uint32_t regionCount, const VkBufferImageCopy* regions);
    using CmdBindPipelineFn = void(__fastcall*)(void* commandBuffer, std::uint32_t pipelineBindPoint, void* pipeline);
    using CmdBindDescriptorSetsFn = void(__fastcall*)(void* commandBuffer, std::uint32_t pipelineBindPoint, void* layout, std::uint32_t firstSet, std::uint32_t descriptorSetCount, void* const* descriptorSets, std::uint32_t dynamicOffsetCount, const std::uint32_t* dynamicOffsets);
    using CmdPushConstantsFn = void(__fastcall*)(void* commandBuffer, void* layout, std::uint32_t stageFlags, std::uint32_t offset, std::uint32_t size, const void* values);
    using CmdDrawFn = void(__fastcall*)(void* commandBuffer, std::uint32_t vertexCount, std::uint32_t instanceCount, std::uint32_t firstVertex, std::uint32_t firstInstance);
    using CreateSemaphoreFn = std::int32_t(__fastcall*)(void* device, const VkSemaphoreCreateInfo* createInfo, const void* allocator, void** semaphore);
    using DestroySemaphoreFn = void(__fastcall*)(void* device, void* semaphore, const void* allocator);
    using QueueSubmitFn = std::int32_t(__fastcall*)(void* queue, std::uint32_t submitCount, const VkSubmitInfo* submits, void* fence);
    using DeviceWaitIdleFn = std::int32_t(__fastcall*)(void* device);

    struct UiRenderSnapshot
    {
        uintptr_t source = 0;
        uintptr_t state = 0;
        std::int32_t sourceWidth = 0;
        std::int32_t sourceHeight = 0;
        std::int32_t stateWidth = 0;
        std::int32_t stateHeight = 0;
    };

    struct SwapchainRuntimeInfo
    {
        uintptr_t handle = 0;
        uintptr_t device = 0;
        std::uint32_t format = 0;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint32_t minImageCount = 0;
        std::uint32_t imageCount = 0;
        std::vector<uintptr_t> images;
    };

    struct VulkanRendererFns
    {
        GetSwapchainImagesFn getSwapchainImages = nullptr;
        CreateImageViewFn createImageView = nullptr;
        DestroyImageViewFn destroyImageView = nullptr;
        CreateRenderPassFn createRenderPass = nullptr;
        DestroyRenderPassFn destroyRenderPass = nullptr;
        CreateFramebufferFn createFramebuffer = nullptr;
        DestroyFramebufferFn destroyFramebuffer = nullptr;
        CreateCommandPoolFn createCommandPool = nullptr;
        DestroyCommandPoolFn destroyCommandPool = nullptr;
        AllocateCommandBuffersFn allocateCommandBuffers = nullptr;
        ResetCommandBufferFn resetCommandBuffer = nullptr;
        BeginCommandBufferFn beginCommandBuffer = nullptr;
        EndCommandBufferFn endCommandBuffer = nullptr;
        CmdBeginRenderPassFn cmdBeginRenderPass = nullptr;
        CmdEndRenderPassFn cmdEndRenderPass = nullptr;
        CmdClearAttachmentsFn cmdClearAttachments = nullptr;
        CreateBufferFn createBuffer = nullptr;
        DestroyBufferFn destroyBuffer = nullptr;
        GetBufferMemoryRequirementsFn getBufferMemoryRequirements = nullptr;
        CreateImageFn createImage = nullptr;
        DestroyImageFn destroyImage = nullptr;
        GetImageMemoryRequirementsFn getImageMemoryRequirements = nullptr;
        AllocateMemoryFn allocateMemory = nullptr;
        FreeMemoryFn freeMemory = nullptr;
        BindBufferMemoryFn bindBufferMemory = nullptr;
        BindImageMemoryFn bindImageMemory = nullptr;
        MapMemoryFn mapMemory = nullptr;
        UnmapMemoryFn unmapMemory = nullptr;
        CreateSamplerFn createSampler = nullptr;
        DestroySamplerFn destroySampler = nullptr;
        CreateShaderModuleFn createShaderModule = nullptr;
        DestroyShaderModuleFn destroyShaderModule = nullptr;
        CreateDescriptorSetLayoutFn createDescriptorSetLayout = nullptr;
        DestroyDescriptorSetLayoutFn destroyDescriptorSetLayout = nullptr;
        CreateDescriptorPoolFn createDescriptorPool = nullptr;
        DestroyDescriptorPoolFn destroyDescriptorPool = nullptr;
        AllocateDescriptorSetsFn allocateDescriptorSets = nullptr;
        UpdateDescriptorSetsFn updateDescriptorSets = nullptr;
        CreatePipelineLayoutFn createPipelineLayout = nullptr;
        DestroyPipelineLayoutFn destroyPipelineLayout = nullptr;
        CreateGraphicsPipelinesFn createGraphicsPipelines = nullptr;
        DestroyPipelineFn destroyPipeline = nullptr;
        CmdPipelineBarrierFn cmdPipelineBarrier = nullptr;
        CmdCopyBufferToImageFn cmdCopyBufferToImage = nullptr;
        CmdBindPipelineFn cmdBindPipeline = nullptr;
        CmdBindDescriptorSetsFn cmdBindDescriptorSets = nullptr;
        CmdPushConstantsFn cmdPushConstants = nullptr;
        CmdDrawFn cmdDraw = nullptr;
        CreateSemaphoreFn createSemaphore = nullptr;
        DestroySemaphoreFn destroySemaphore = nullptr;
        QueueSubmitFn queueSubmit = nullptr;
        DeviceWaitIdleFn deviceWaitIdle = nullptr;

        bool Ready() const
        {
            return getSwapchainImages != nullptr &&
                createImageView != nullptr &&
                destroyImageView != nullptr &&
                createRenderPass != nullptr &&
                destroyRenderPass != nullptr &&
                createFramebuffer != nullptr &&
                destroyFramebuffer != nullptr &&
                createCommandPool != nullptr &&
                destroyCommandPool != nullptr &&
                allocateCommandBuffers != nullptr &&
                resetCommandBuffer != nullptr &&
                beginCommandBuffer != nullptr &&
                endCommandBuffer != nullptr &&
                cmdBeginRenderPass != nullptr &&
                cmdEndRenderPass != nullptr &&
                cmdClearAttachments != nullptr &&
                createSemaphore != nullptr &&
                destroySemaphore != nullptr &&
                queueSubmit != nullptr &&
                deviceWaitIdle != nullptr;
        }

        bool TextureReady() const
        {
            return createBuffer != nullptr &&
                destroyBuffer != nullptr &&
                getBufferMemoryRequirements != nullptr &&
                createImage != nullptr &&
                destroyImage != nullptr &&
                getImageMemoryRequirements != nullptr &&
                allocateMemory != nullptr &&
                freeMemory != nullptr &&
                bindBufferMemory != nullptr &&
                bindImageMemory != nullptr &&
                mapMemory != nullptr &&
                unmapMemory != nullptr &&
                createSampler != nullptr &&
                destroySampler != nullptr &&
                createShaderModule != nullptr &&
                destroyShaderModule != nullptr &&
                createDescriptorSetLayout != nullptr &&
                destroyDescriptorSetLayout != nullptr &&
                createDescriptorPool != nullptr &&
                destroyDescriptorPool != nullptr &&
                allocateDescriptorSets != nullptr &&
                updateDescriptorSets != nullptr &&
                createPipelineLayout != nullptr &&
                destroyPipelineLayout != nullptr &&
                createGraphicsPipelines != nullptr &&
                destroyPipeline != nullptr &&
                cmdPipelineBarrier != nullptr &&
                cmdCopyBufferToImage != nullptr &&
                cmdBindPipeline != nullptr &&
                cmdBindDescriptorSets != nullptr &&
                cmdPushConstants != nullptr &&
                cmdDraw != nullptr;
        }
    };

    struct VulkanMinimapRenderer
    {
        bool ready = false;
        uintptr_t device = 0;
        uintptr_t swapchain = 0;
        std::uint32_t format = 0;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint32_t queueFamilyIndex = 0;
        std::vector<uintptr_t> images;
        std::vector<uintptr_t> imageViews;
        std::vector<uintptr_t> framebuffers;
        std::vector<uintptr_t> commandBuffers;
        std::vector<uintptr_t> renderCompleteSemaphores;
        uintptr_t renderPass = 0;
        uintptr_t commandPool = 0;
        uintptr_t frameTextureImage = 0;
        uintptr_t frameTextureMemory = 0;
        uintptr_t frameTextureImageView = 0;
        uintptr_t frameSampler = 0;
        uintptr_t frameStagingBuffer = 0;
        uintptr_t frameStagingMemory = 0;
        uintptr_t frameDescriptorSetLayout = 0;
        uintptr_t frameDescriptorPool = 0;
        uintptr_t frameDescriptorSet = 0;
        uintptr_t framePipelineLayout = 0;
        uintptr_t framePipeline = 0;
        std::uint32_t frameTextureWidth = 0;
        std::uint32_t frameTextureHeight = 0;
        bool frameTextureUploadPending = false;
        bool frameTextureReady = false;
        VulkanRendererFns fns;
    };

    struct RealMapTexture
    {
        bool attempted = false;
        bool loaded = false;
        int width = 0;
        int height = 0;
        std::string path;
        std::vector<std::uint8_t> rgba;
    };

    struct StaticPoi
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        std::uint32_t kind = 0;
    };

    struct StaticPoiCatalog
    {
        bool attempted = false;
        bool loaded = false;
        std::string path;
        std::vector<StaticPoi> pois;
    };

    struct FogOfWarMask
    {
        bool attempted = false;
        bool loaded = false;
        int width = 0;
        int height = 0;
        std::string path;
        std::vector<std::uint8_t> values;
    };

    struct MinimapIcon
    {
        std::uint32_t key = 0;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::vector<std::uint8_t> rgba;
    };

    struct MinimapIconAtlas
    {
        bool attempted = false;
        bool loaded = false;
        std::string path;
        std::vector<MinimapIcon> icons;
    };

    struct MinimapFrameRun
    {
        std::uint16_t x = 0;
        std::uint16_t y = 0;
        std::uint16_t width = 0;
    };

    struct MinimapFrameColorGroup
    {
        std::uint8_t red = 0;
        std::uint8_t green = 0;
        std::uint8_t blue = 0;
        std::vector<MinimapFrameRun> runs;
    };

    struct MinimapFrameAsset
    {
        bool attempted = false;
        bool loaded = false;
        int width = 0;
        int height = 0;
        std::string path;
        std::vector<std::uint8_t> rgba;
        std::vector<MinimapFrameColorGroup> groups;
    };

    ModMetaData g_metaData = {
        "minimap_mod",
        "Internal minimap data bridge for Enshrouded. No external overlay window.",
        "0.4.34",
        "OpenAI + xoker",
        "0.0.3",
        true,
        false
    };

    ModContext* g_modContext = nullptr;
    uintptr_t g_exeBase = 0;
    IterInitFn g_iterInit = nullptr;
    IterNextFn g_iterNext = nullptr;
    Mem::Detour* g_localPlayerUiRenderSetupHook = nullptr;
    Mem::Detour* g_playerWaypointsUiHook = nullptr;
    Mem::Detour* g_mapMarkerVisibilityHook = nullptr;
    Mem::Detour* g_renderPresentFrameHook = nullptr;
    Mem::Detour* g_vulkanDeviceTableInitHook = nullptr;

    std::mutex g_waypointMutex;
    std::vector<CapturedWaypoint> g_waypoints;
    std::mutex g_nearbyMarkerMutex;
    std::vector<CapturedNearbyMarker> g_nearbyMarkers;
    std::mutex g_visibleMapMarkerMutex;
    std::vector<CapturedMapMarkerVisibility> g_visibleMapMarkers;
    std::mutex g_playerPositionMutex;
    CapturedPlayerPosition g_playerPosition;
    std::atomic<uintptr_t> g_playerCameraAddress{ 0 };
    DWORD g_lastPlayerCameraScanTick = 0;
    DWORD g_lastSummaryTick = 0;
    DWORD g_lastNearbySummaryTick = 0;
    DWORD g_lastVisibleMapMarkerSummaryTick = 0;
    DWORD g_lastVisibleMapMarkerPruneTick = 0;
    DWORD g_lastVisibleMapMarkerFaultTick = 0;
    DWORD g_lastPlayerPositionSummaryTick = 0;
    DWORD g_lastRenderFrameSummaryTick = 0;
    DWORD g_lastUiRenderSummaryTick = 0;
    DWORD g_lastVulkanSummaryTick = 0;
    DWORD g_lastVulkanHookSummaryTick = 0;
    DWORD g_lastVulkanPresentHookTick = 0;
    DWORD g_lastVulkanPresentInfoTick = 0;
    DWORD g_lastVulkanSwapchainHookTick = 0;
    DWORD g_lastVulkanSwapchainImagesHookTick = 0;
    DWORD g_lastVulkanScanTick = 0;
    DWORD g_lastVulkanFunctionScanTick = 0;
    DWORD g_lastVulkanRendererLogTick = 0;
    int g_vulkanScanAttempts = 0;
    bool g_vulkanFunctionOffsetsLogged = false;
    std::atomic<uintptr_t> g_vulkanDeviceTable{ 0 };
    std::atomic<uintptr_t> g_patchedVulkanDeviceTable{ 0 };
    std::atomic<uintptr_t> g_originalQueuePresent{ 0 };
    std::atomic<uintptr_t> g_originalCreateSwapchain{ 0 };
    std::atomic<uintptr_t> g_originalGetSwapchainImages{ 0 };
    std::atomic<uintptr_t> g_lastRenderContext{ 0 };
    std::atomic<uintptr_t> g_lastVulkanDevice{ 0 };
    std::atomic<uintptr_t> g_lastGraphicsContext{ 0 };
    std::atomic<uintptr_t> g_lastSwapchainState{ 0 };
    std::atomic<std::uint32_t> g_lastVulkanQueueFamilyIndex{ 0 };
    std::atomic<int> g_visibleMapMarkerFaults{ 0 };
    std::atomic<bool> g_visibleMapMarkerCaptureDisabled{ false };
    std::mutex g_vulkanHookMutex;
    std::mutex g_vulkanFunctionScanMutex;
    std::mutex g_swapchainMutex;
    std::mutex g_rendererMutex;
    std::vector<SwapchainRuntimeInfo> g_swapchains;
    VulkanMinimapRenderer g_renderer;
    std::mutex g_realMapMutex;
    RealMapTexture g_realMap;
    std::mutex g_staticPoiMutex;
    StaticPoiCatalog g_staticPois;
    std::mutex g_fogOfWarMutex;
    FogOfWarMask g_fogOfWar;
    std::mutex g_minimapIconMutex;
    MinimapIconAtlas g_minimapIcons;
    std::mutex g_minimapFrameMutex;
    MinimapFrameAsset g_minimapFrame;
    std::mutex g_playerArrowMutex;
    RealMapTexture g_playerArrow;
    std::atomic<bool> g_worldSessionReady{ false };
    std::atomic<bool> g_gameSessionOnline{ false };
    std::atomic<DWORD> g_lastWorldDataTick{ 0 };
    std::atomic<int> g_minimapZoomStep{ 0 };
    DWORD g_lastSessionLogPollTick = 0;
    std::string g_gameLogPath;

    std::array<std::uint8_t, LOCAL_PLAYER_UI_RENDER_SETUP_STOLEN_SIZE> g_localPlayerUiRenderSetupExpected = {
        0x48, 0x89, 0x7C, 0x24, 0x18,
        0x55,
        0x48, 0x8B, 0xEC,
        0x48, 0x83, 0xEC, 0x50
    };

    std::array<std::uint8_t, PLAYER_WAYPOINTS_UI_STOLEN_SIZE> g_playerWaypointsUiExpected = {
        0x48, 0x89, 0x5C, 0x24, 0x08,
        0x48, 0x89, 0x6C, 0x24, 0x10,
        0x48, 0x89, 0x74, 0x24, 0x18
    };

    std::array<std::uint8_t, KNOWLEDGE_QUERY_MAPMARKER_VISIBILITY_STOLEN_SIZE> g_mapMarkerVisibilityExpected = {
        0x40, 0x55,
        0x41, 0x56,
        0x48, 0x8B, 0xEC,
        0x48, 0x83, 0xEC, 0x78
    };

    std::array<std::uint8_t, KNOWLEDGE_QUERY_MAPMARKER_VISIBILITY_LOOP_STOLEN_SIZE> g_mapMarkerVisibilityLoopExpected = {
        0x48, 0x89, 0x74, 0x24, 0x68
    };

    std::array<std::uint8_t, RENDER_PRESENT_FRAME_STOLEN_SIZE> g_renderPresentFrameExpected = {
        0x4C, 0x89, 0x4C, 0x24, 0x20,
        0x48, 0x89, 0x54, 0x24, 0x10,
        0x55,
        0x53,
        0x56,
        0x57,
        0x41, 0x54
    };

    std::array<std::uint8_t, VULKAN_DEVICE_TABLE_INIT_STOLEN_SIZE> g_vulkanDeviceTableInitExpected = {
        0x48, 0x89, 0x5C, 0x24, 0x08,
        0x48, 0x89, 0x74, 0x24, 0x10,
        0x48, 0x89, 0x7C, 0x24, 0x18
    };

    bool TryInstallVulkanTableHooks(uintptr_t table);
    void RestoreVulkanTableHooks();

    void Log(const std::string& message)
    {
        if (g_modContext != nullptr)
            g_modContext->Log(message.c_str());
    }

    std::string Hex(uintptr_t value)
    {
        std::ostringstream oss;
        oss << "0x" << std::hex << value;
        return oss.str();
    }

    template <typename T>
    T MinValue(T left, T right)
    {
        return left < right ? left : right;
    }

    template <typename T>
    T MaxValue(T left, T right)
    {
        return left > right ? left : right;
    }

    template <typename T>
    T ClampValue(T value, T low, T high)
    {
        return MaxValue(low, MinValue(value, high));
    }

    bool SafeRead(uintptr_t address, void* buffer, std::size_t size)
    {
        __try
        {
            std::memcpy(buffer, reinterpret_cast<const void*>(address), size);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    template <typename T>
    bool SafeReadValue(uintptr_t address, T& outValue)
    {
        return SafeRead(address, &outValue, sizeof(T));
    }

    bool ValidatePrologue(uintptr_t address, const std::uint8_t* expected, std::size_t size, const char* label)
    {
        std::vector<std::uint8_t> current(size, 0);
        if (!SafeRead(address, current.data(), current.size()))
        {
            Log(std::string("[Minimap] failed to read prologue for ") + label);
            return false;
        }

        if (std::memcmp(current.data(), expected, size) != 0)
        {
            std::ostringstream oss;
            oss << "[Minimap] prologue mismatch for " << label
                << " at " << Hex(address - g_exeBase);
            Log(oss.str());
            return false;
        }

        return true;
    }

    bool ActivateHook(Mem::Detour* hook)
    {
        if (hook == nullptr)
            return false;

        if (hook->active)
            return true;

        return hook->activate() || hook->active;
    }

    const char* HookActivationState(Mem::Detour* hook)
    {
        if (hook == nullptr)
            return "missing";

        return hook->active ? "active" : "failed";
    }

    std::vector<std::uint8_t> BuildEntryHookShellcode(const std::uint8_t* stolenBytes, std::size_t stolenSize)
    {
        std::vector<std::uint8_t> code;
        code.reserve(128);

        auto append = [&code](std::initializer_list<std::uint8_t> bytes)
        {
            code.insert(code.end(), bytes.begin(), bytes.end());
        };

        append({ 0x48, 0x83, 0xEC, 0x48 });               // sub rsp, 48h
        append({ 0x48, 0x89, 0x4C, 0x24, 0x20 });         // mov [rsp+20h], rcx
        append({ 0x48, 0x89, 0x54, 0x24, 0x28 });         // mov [rsp+28h], rdx
        append({ 0x4C, 0x89, 0x44, 0x24, 0x30 });         // mov [rsp+30h], r8
        append({ 0x4C, 0x89, 0x4C, 0x24, 0x38 });         // mov [rsp+38h], r9
        append({ 0x48, 0xB8 });                           // mov rax, imm64

        for (int i = 0; i < 8; ++i)
            code.push_back(0x00);

        append({ 0xFF, 0xD0 });                           // call rax
        append({ 0x48, 0x8B, 0x4C, 0x24, 0x20 });         // mov rcx, [rsp+20h]
        append({ 0x48, 0x8B, 0x54, 0x24, 0x28 });         // mov rdx, [rsp+28h]
        append({ 0x4C, 0x8B, 0x44, 0x24, 0x30 });         // mov r8, [rsp+30h]
        append({ 0x4C, 0x8B, 0x4C, 0x24, 0x38 });         // mov r9, [rsp+38h]
        append({ 0x48, 0x83, 0xC4, 0x48 });               // add rsp, 48h

        code.insert(code.end(), stolenBytes, stolenBytes + stolenSize);
        append({ 0xE9, 0x00, 0x00, 0x00, 0x00 });         // jmp back
        return code;
    }

    std::vector<std::uint8_t> BuildMapMarkerVisibilityLoopHookShellcode(const std::uint8_t* stolenBytes, std::size_t stolenSize)
    {
        std::vector<std::uint8_t> code;
        code.reserve(128);

        auto append = [&code](std::initializer_list<std::uint8_t> bytes)
        {
            code.insert(code.end(), bytes.begin(), bytes.end());
        };

        append({ 0x48, 0x83, 0xEC, 0x50 });               // sub rsp, 50h
        append({ 0x48, 0x89, 0x4C, 0x24, 0x20 });         // mov [rsp+20h], rcx
        append({ 0x48, 0x89, 0x54, 0x24, 0x28 });         // mov [rsp+28h], rdx
        append({ 0x4C, 0x89, 0x44, 0x24, 0x30 });         // mov [rsp+30h], r8
        append({ 0x4C, 0x89, 0x4C, 0x24, 0x38 });         // mov [rsp+38h], r9
        append({ 0x48, 0xB8 });                           // mov rax, imm64

        for (int i = 0; i < 8; ++i)
            code.push_back(0x00);

        append({ 0x48, 0x8D, 0x4D, 0xB8 });               // lea rcx, [rbp-48h]
        append({ 0x4C, 0x89, 0xF2 });                     // mov rdx, r14
        append({ 0xFF, 0xD0 });                           // call rax
        append({ 0x48, 0x8B, 0x4C, 0x24, 0x20 });         // mov rcx, [rsp+20h]
        append({ 0x48, 0x8B, 0x54, 0x24, 0x28 });         // mov rdx, [rsp+28h]
        append({ 0x4C, 0x8B, 0x44, 0x24, 0x30 });         // mov r8, [rsp+30h]
        append({ 0x4C, 0x8B, 0x4C, 0x24, 0x38 });         // mov r9, [rsp+38h]
        append({ 0x48, 0x83, 0xC4, 0x50 });               // add rsp, 50h

        code.insert(code.end(), stolenBytes, stolenBytes + stolenSize);
        append({ 0xE9, 0x00, 0x00, 0x00, 0x00 });         // jmp back
        return code;
    }

    template <std::size_t N>
    Mem::Detour* InstallEntryHook(uintptr_t rva, const std::array<std::uint8_t, N>& expected, void* handler, const char* label)
    {
        const uintptr_t address = g_exeBase + rva;
        if (!ValidatePrologue(address, expected.data(), expected.size(), label))
            return nullptr;

        std::vector<std::uint8_t> shellcode = BuildEntryHookShellcode(expected.data(), expected.size());
        auto* detour = new Mem::Detour(address, shellcode.data(), shellcode.size(), false, expected.size() - 5);

        const std::size_t handlerImmediateOffset = 26;
        const std::size_t jumpImmediateOffset = shellcode.size() - 4;
        detour->shellcode->updateValue<std::uint64_t>(handlerImmediateOffset, reinterpret_cast<std::uint64_t>(handler));

        const uintptr_t jumpSource = detour->shellcode->data->address + jumpImmediateOffset + sizeof(std::uint32_t);
        const uintptr_t jumpTarget = address + expected.size();
        detour->shellcode->updateValue<std::uint32_t>(
            jumpImmediateOffset,
            static_cast<std::uint32_t>(jumpTarget - jumpSource)
        );

        return detour;
    }

    Mem::Detour* InstallMapMarkerVisibilityLoopHook(void* handler)
    {
        const uintptr_t address = g_exeBase + RVA_KNOWLEDGE_QUERY_MAPMARKER_VISIBILITY_LOOP;
        if (!ValidatePrologue(address, g_mapMarkerVisibilityLoopExpected.data(), g_mapMarkerVisibilityLoopExpected.size(), "map_marker_visibility_loop"))
            return nullptr;

        std::vector<std::uint8_t> shellcode = BuildMapMarkerVisibilityLoopHookShellcode(
            g_mapMarkerVisibilityLoopExpected.data(),
            g_mapMarkerVisibilityLoopExpected.size()
        );
        auto* detour = new Mem::Detour(address, shellcode.data(), shellcode.size(), false, g_mapMarkerVisibilityLoopExpected.size() - 5);

        const std::size_t handlerImmediateOffset = 26;
        const std::size_t jumpImmediateOffset = shellcode.size() - 4;
        detour->shellcode->updateValue<std::uint64_t>(handlerImmediateOffset, reinterpret_cast<std::uint64_t>(handler));

        const uintptr_t jumpSource = detour->shellcode->data->address + jumpImmediateOffset + sizeof(std::uint32_t);
        const uintptr_t jumpTarget = address + g_mapMarkerVisibilityLoopExpected.size();
        detour->shellcode->updateValue<std::uint32_t>(
            jumpImmediateOffset,
            static_cast<std::uint32_t>(jumpTarget - jumpSource)
        );

        return detour;
    }

    float FixedToWorld(std::int64_t value)
    {
        return static_cast<float>(static_cast<double>(value) * FIXED_32_32_TO_WORLD);
    }

    std::int64_t WorldToFixed(float value)
    {
        return static_cast<std::int64_t>(static_cast<double>(value) / FIXED_32_32_TO_WORLD);
    }

    bool SameWaypoints(const std::vector<CapturedWaypoint>& left, const std::vector<CapturedWaypoint>& right)
    {
        if (left.size() != right.size())
            return false;

        for (std::size_t index = 0; index < left.size(); ++index)
        {
            if (left[index].id != right[index].id ||
                left[index].x != right[index].x ||
                left[index].y != right[index].y ||
                left[index].z != right[index].z ||
                std::memcmp(left[index].raw, right[index].raw, sizeof(left[index].raw)) != 0)
            {
                return false;
            }
        }

        return true;
    }

    bool SameNearbyMarkers(const std::vector<CapturedNearbyMarker>& left, const std::vector<CapturedNearbyMarker>& right)
    {
        if (left.size() != right.size())
            return false;

        for (std::size_t index = 0; index < left.size(); ++index)
        {
            if (left[index].kind != right[index].kind ||
                left[index].hasWorldPosition != right[index].hasWorldPosition ||
                left[index].x != right[index].x ||
                left[index].y != right[index].y ||
                left[index].z != right[index].z ||
                std::memcmp(left[index].raw, right[index].raw, sizeof(left[index].raw)) != 0)
            {
                return false;
            }
        }

        return true;
    }

    bool SameVisibleMapMarkerIdentity(const CapturedMapMarkerVisibility& left, const CapturedMapMarkerVisibility& right)
    {
        if (left.stateAddress != 0 && right.stateAddress != 0 && left.stateAddress == right.stateAddress)
            return true;

        if (left.markerAddress != 0 && right.markerAddress != 0 && left.markerAddress == right.markerAddress)
            return true;

        if (!left.hasWorldPosition || !right.hasWorldPosition)
            return false;

        const float dx = FixedToWorld(left.x) - FixedToWorld(right.x);
        const float dz = FixedToWorld(left.z) - FixedToWorld(right.z);
        if (dx * dx + dz * dz > 16.0f)
            return false;

        return left.kind == right.kind || left.markerId == right.markerId;
    }

    bool SameWaypointPosition(const CapturedWaypoint& left, const CapturedWaypoint& right)
    {
        const float dx = FixedToWorld(left.x) - FixedToWorld(right.x);
        const float dz = FixedToWorld(left.z) - FixedToWorld(right.z);
        return std::abs(dx) < 2.0f && std::abs(dz) < 2.0f;
    }

    void PushWaypointUnique(std::vector<CapturedWaypoint>& waypoints, const CapturedWaypoint& waypoint)
    {
        for (const CapturedWaypoint& existing : waypoints)
        {
            if (SameWaypointPosition(existing, waypoint))
                return;
        }

        waypoints.push_back(waypoint);
    }

    bool IsPlausibleWorldPosition(float x, float y, float z)
    {
        return std::abs(x) < 30000.0f &&
            std::abs(z) < 30000.0f &&
            y > -5000.0f &&
            y < 8000.0f &&
            (std::abs(x) > 1.0f || std::abs(z) > 1.0f);
    }

    bool TryDecodeFixedWorldPosition(const std::uint64_t* raw, std::int64_t& outX, std::int64_t& outY, std::int64_t& outZ)
    {
        for (int first = 0; first <= 2; ++first)
        {
            const std::int64_t x = static_cast<std::int64_t>(raw[first + 0]);
            const std::int64_t y = static_cast<std::int64_t>(raw[first + 1]);
            const std::int64_t z = static_cast<std::int64_t>(raw[first + 2]);
            if (IsPlausibleWorldPosition(FixedToWorld(x), FixedToWorld(y), FixedToWorld(z)))
            {
                outX = x;
                outY = y;
                outZ = z;
                return true;
            }
        }

        return false;
    }

    void NoteWorldData(const char* reason)
    {
        const DWORD now = GetTickCount();
        g_lastWorldDataTick.store(now);

        if (!g_worldSessionReady.exchange(true))
        {
            std::ostringstream oss;
            oss << "[Minimap] world session ready | reason=" << reason;
            Log(oss.str());
        }
    }

    bool HasRecentWorldData()
    {
        const DWORD lastWorldDataTick = g_lastWorldDataTick.load();
        return lastWorldDataTick != 0 && GetTickCount() - lastWorldDataTick <= WORLD_DATA_STALE_MS;
    }

    bool HasFreshPlayerPosition()
    {
        const DWORD now = GetTickCount();
        std::lock_guard<std::mutex> lock(g_playerPositionMutex);
        return g_playerPosition.valid && now - g_playerPosition.lastUpdateTick <= WORLD_DATA_STALE_MS;
    }

    bool ShouldDrawMinimapInWorld()
    {
        CURSORINFO cursorInfo{};
        cursorInfo.cbSize = sizeof(cursorInfo);
        if (GetCursorInfo(&cursorInfo) && (cursorInfo.flags & CURSOR_SHOWING) != 0)
            return false;

        return HasFreshPlayerPosition();
    }

    std::string ResolveGameLogPath()
    {
        char modulePath[MAX_PATH] = {};
        const DWORD length = GetModuleFileNameA(nullptr, modulePath, static_cast<DWORD>(sizeof(modulePath)));
        if (length == 0 || length >= sizeof(modulePath))
            return "enshrouded.log";

        char* slash = std::strrchr(modulePath, '\\');
        if (slash == nullptr)
            return "enshrouded.log";

        *(slash + 1) = '\0';
        return std::string(modulePath) + "enshrouded.log";
    }

    bool ReadFileTail(const std::string& path, DWORD maxBytes, std::string& outText)
    {
        HANDLE file = CreateFileA(
            path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        if (file == INVALID_HANDLE_VALUE)
            return false;

        LARGE_INTEGER fileSize{};
        if (!GetFileSizeEx(file, &fileSize))
        {
            CloseHandle(file);
            return false;
        }

        const LONGLONG bytesToRead64 = MinValue<LONGLONG>(fileSize.QuadPart, static_cast<LONGLONG>(maxBytes));
        if (bytesToRead64 <= 0)
        {
            CloseHandle(file);
            outText.clear();
            return true;
        }

        LARGE_INTEGER distance{};
        distance.QuadPart = fileSize.QuadPart - bytesToRead64;
        if (!SetFilePointerEx(file, distance, nullptr, FILE_BEGIN))
        {
            CloseHandle(file);
            return false;
        }

        outText.assign(static_cast<std::size_t>(bytesToRead64), '\0');
        DWORD bytesRead = 0;
        const BOOL ok = ReadFile(file, &outText[0], static_cast<DWORD>(outText.size()), &bytesRead, nullptr);
        CloseHandle(file);
        if (!ok)
            return false;

        outText.resize(bytesRead);
        return true;
    }

    std::size_t LatestPatternPosition(const std::string& text, std::initializer_list<const char*> patterns)
    {
        std::size_t latest = std::string::npos;
        for (const char* pattern : patterns)
        {
            const std::size_t found = text.rfind(pattern);
            if (found != std::string::npos && (latest == std::string::npos || found > latest))
                latest = found;
        }

        return latest;
    }

    void ResetWorldSessionFromLog()
    {
        const bool wasOnline = g_gameSessionOnline.exchange(false);
        const bool wasReady = g_worldSessionReady.exchange(false);
        g_lastWorldDataTick.store(0);
        g_playerCameraAddress.store(0);
        g_lastPlayerCameraScanTick = 0;

        {
            std::lock_guard<std::mutex> lock(g_playerPositionMutex);
            g_playerPosition = {};
        }
        {
            std::lock_guard<std::mutex> lock(g_visibleMapMarkerMutex);
            g_visibleMapMarkers.clear();
        }
        if (wasOnline || wasReady)
            Log("[Minimap] world session closed | reason=game_session_log");
    }

    void PollGameSessionLog()
    {
        const DWORD now = GetTickCount();
        if (now - g_lastSessionLogPollTick < SESSION_LOG_POLL_MS)
            return;

        g_lastSessionLogPollTick = now;
        if (g_gameLogPath.empty())
            g_gameLogPath = ResolveGameLogPath();

        std::string tail;
        if (!ReadFileTail(g_gameLogPath, SESSION_LOG_TAIL_BYTES, tail))
            return;

        const std::size_t joined = LatestPatternPosition(tail, {
            "[SessionPlayer] finished transition from 'Local' to 'Local_InSession'",
            "[Session] finished transition from 'Lobby' to 'Client_Online'",
            "[play] start creation step LoadingScreenDone"
        });
        const std::size_t left = LatestPatternPosition(tail, {
            "[SessionPlayer] finished transition from 'Local_InSession' to 'Local'",
            "[Session] finished transition from 'Client_Online' to 'Lobby'",
            "[play] start destruction step Session"
        });

        if (joined != std::string::npos && (left == std::string::npos || joined > left))
        {
            const bool wasOnline = g_gameSessionOnline.exchange(true);
            if (!wasOnline)
                Log("[Minimap] game session detected from Enshrouded log");
            return;
        }

        if (left != std::string::npos && (joined == std::string::npos || left > joined))
            ResetWorldSessionFromLog();
    }

    const char* PlayerPositionChannelName(std::uint32_t channel)
    {
        switch (channel)
        {
        case 1:
            return "ui_source";
        case 2:
            return "ui_local30";
        case 3:
            return "ui_state_camera";
        case 20:
            return "cached_client_camera";
        case 21:
            return "waypoint_root_camera";
        case 22:
            return "waypoint_child_camera";
        case 23:
            return "waypoint_direct_camera";
        default:
            return "unknown";
        }
    }

    bool TryScoreAgainstWorldAnchors(float worldX, float worldZ, float& outScore)
    {
        bool hasAnchor = false;
        float bestScore = 1000000000000.0f;

        {
            std::lock_guard<std::mutex> lock(g_waypointMutex);
            for (const CapturedWaypoint& waypoint : g_waypoints)
            {
                const float dx = FixedToWorld(waypoint.x) - worldX;
                const float dz = FixedToWorld(waypoint.z) - worldZ;
                bestScore = MinValue(bestScore, dx * dx + dz * dz);
                hasAnchor = true;
            }
        }

        {
            std::lock_guard<std::mutex> lock(g_nearbyMarkerMutex);
            for (const CapturedNearbyMarker& marker : g_nearbyMarkers)
            {
                if (!marker.hasWorldPosition)
                    continue;

                const float dx = FixedToWorld(marker.x) - worldX;
                const float dz = FixedToWorld(marker.z) - worldZ;
                bestScore = MinValue(bestScore, dx * dx + dz * dz);
                hasAnchor = true;
            }
        }

        outScore = hasAnchor ? bestScore : 0.0f;
        return hasAnchor;
    }

    bool IsAcceptablePlayerPositionCandidate(float worldX, float worldZ, float& outScore)
    {
        const bool hasAnchor = TryScoreAgainstWorldAnchors(worldX, worldZ, outScore);
        if (hasAnchor)
            return outScore <= 12000.0f * 12000.0f;

        if (worldX < -512.0f || worldZ < -512.0f ||
            worldX > REAL_MAP_WORLD_SIZE + 512.0f ||
            worldZ > REAL_MAP_WORLD_SIZE + 512.0f)
        {
            return false;
        }

        const float dx = worldX - REAL_MAP_WORLD_SIZE * 0.5f;
        const float dz = worldZ - REAL_MAP_WORLD_SIZE * 0.5f;
        outScore = dx * dx + dz * dz;
        return true;
    }

    struct PlayerPositionCandidate
    {
        std::int64_t x = 0;
        std::int64_t y = 0;
        std::int64_t z = 0;
        uintptr_t source = 0;
        std::uint32_t offset = 0;
        std::uint32_t channel = 0;
        float score = 0.0f;
        float headingRadians = 0.0f;
        bool hasHeading = false;
        bool valid = false;
    };

    bool TryCapturePlayerPositionFromBlock(uintptr_t base, std::uint32_t channel, std::size_t scanBytes, PlayerPositionCandidate& best)
    {
        if (base == 0 || scanBytes < sizeof(std::uint64_t) * 5)
            return false;

        bool captured = false;
        for (std::size_t offset = 0; offset + sizeof(std::uint64_t) * 5 <= scanBytes; offset += sizeof(std::uint64_t))
        {
            std::uint64_t raw[5] = {};
            if (!SafeRead(base + offset, raw, sizeof(raw)))
                continue;

            std::int64_t x = 0;
            std::int64_t y = 0;
            std::int64_t z = 0;
            if (!TryDecodeFixedWorldPosition(raw, x, y, z))
                continue;

            float score = 0.0f;
            if (!IsAcceptablePlayerPositionCandidate(FixedToWorld(x), FixedToWorld(z), score))
                continue;

            if (!best.valid || score < best.score)
            {
                best.x = x;
                best.y = y;
                best.z = z;
                best.source = base;
                best.offset = static_cast<std::uint32_t>(offset);
                best.channel = channel;
                best.score = score;
                best.valid = true;
            }
            captured = true;
        }

        return captured;
    }

    void PublishPlayerPosition(const CapturedPlayerPosition& position)
    {
        {
            std::lock_guard<std::mutex> lock(g_playerPositionMutex);
            g_playerPosition = position;
        }

        NoteWorldData("player_position");

        const DWORD now = position.lastUpdateTick;
        if (now - g_lastPlayerPositionSummaryTick < 3000)
            return;

        g_lastPlayerPositionSummaryTick = now;

        std::ostringstream oss;
        oss << "[Minimap] player/map position feed"
            << " | channel=" << PlayerPositionChannelName(position.channel)
            << " | source=" << Hex(position.source)
            << " | offset=0x" << std::hex << position.offset << std::dec
            << " | world=("
            << FixedToWorld(position.x) << ", "
            << FixedToWorld(position.y) << ", "
            << FixedToWorld(position.z) << ")";
        Log(oss.str());
    }

    bool TryGetPlayerPosition(CapturedPlayerPosition& outPosition)
    {
        const DWORD now = GetTickCount();
        std::lock_guard<std::mutex> lock(g_playerPositionMutex);
        if (!g_playerPosition.valid || now - g_playerPosition.lastUpdateTick > WORLD_DATA_STALE_MS)
            return false;

        outPosition = g_playerPosition;
        return true;
    }

    bool TryCapturePlayerPositionFromUiRecord(const UiRenderSetupIterationRecord& record)
    {
        PlayerPositionCandidate best{};
        TryCapturePlayerPositionFromBlock(reinterpret_cast<uintptr_t>(record.source), 1, 0x340, best);
        TryCapturePlayerPositionFromBlock(reinterpret_cast<uintptr_t>(record.local30), 2, 0x40, best);
        TryCapturePlayerPositionFromBlock(reinterpret_cast<uintptr_t>(record.state + 0x16158), 3, 0x40, best);

        if (!best.valid)
            return false;

        CapturedPlayerPosition position{};
        position.x = best.x;
        position.y = best.y;
        position.z = best.z;
        position.source = best.source;
        position.offset = best.offset;
        position.channel = best.channel;
        position.lastUpdateTick = GetTickCount();
        position.valid = true;
        PublishPlayerPosition(position);
        return true;
    }

    bool IsLikelyRuntimePointer(uintptr_t value)
    {
        return value > 0x10000 && value < 0x0000800000000000;
    }

    float QuaternionYawFromMemory(const float quaternion[4])
    {
        const float qx = quaternion[0];
        const float qy = quaternion[1];
        const float qz = quaternion[2];
        const float qw = quaternion[3];
        return std::atan2(2.0f * (qw * qy + qx * qz), 1.0f - 2.0f * (qy * qy + qz * qz));
    }

    bool TryScoreCameraPosition(float worldX, float worldZ, std::uint32_t offset, float& outScore)
    {
        if (worldX < -512.0f || worldZ < -512.0f ||
            worldX > REAL_MAP_WORLD_SIZE + 512.0f ||
            worldZ > REAL_MAP_WORLD_SIZE + 512.0f)
        {
            return false;
        }

        outScore = static_cast<float>(offset);
        return true;
    }

    bool TryCaptureClientCameraAt(uintptr_t cameraAddress, uintptr_t source, std::uint32_t offset, std::uint32_t channel, PlayerPositionCandidate& best)
    {
        if (!IsLikelyRuntimePointer(cameraAddress))
            return false;

        std::int64_t x = 0;
        std::int64_t y = 0;
        std::int64_t z = 0;
        if (!SafeReadValue(cameraAddress + 0x00, x) ||
            !SafeReadValue(cameraAddress + 0x08, y) ||
            !SafeReadValue(cameraAddress + 0x10, z))
        {
            return false;
        }

        const float worldX = FixedToWorld(x);
        const float worldY = FixedToWorld(y);
        const float worldZ = FixedToWorld(z);
        if (!IsPlausibleWorldPosition(worldX, worldY, worldZ))
            return false;

        float orientation[4] = {};
        if (!SafeRead(cameraAddress + 0x18, orientation, sizeof(orientation)))
            return false;

        const float quaternionLengthSquared =
            orientation[0] * orientation[0] +
            orientation[1] * orientation[1] +
            orientation[2] * orientation[2] +
            orientation[3] * orientation[3];
        if (!std::isfinite(quaternionLengthSquared) || quaternionLengthSquared < 0.35f || quaternionLengthSquared > 1.75f)
            return false;

        float distance = 0.0f;
        float fovY = 0.0f;
        float aspect = 0.0f;
        float nearPlane = 0.0f;
        float farPlane = 0.0f;
        if (!SafeReadValue(cameraAddress + 0x28, distance) ||
            !SafeReadValue(cameraAddress + 0x2C, fovY) ||
            !SafeReadValue(cameraAddress + 0x30, aspect) ||
            !SafeReadValue(cameraAddress + 0x34, nearPlane) ||
            !SafeReadValue(cameraAddress + 0x38, farPlane))
        {
            return false;
        }

        if (!std::isfinite(distance) || !std::isfinite(fovY) || !std::isfinite(aspect) ||
            !std::isfinite(nearPlane) || !std::isfinite(farPlane))
        {
            return false;
        }

        if (distance < 0.0f || distance > 512.0f ||
            fovY < 0.10f || fovY > 3.20f ||
            aspect < 0.35f || aspect > 5.00f ||
            nearPlane < 0.0f || nearPlane > 32.0f ||
            farPlane <= nearPlane + 1.0f || farPlane < 16.0f || farPlane > 10000000.0f)
        {
            return false;
        }

        float score = 0.0f;
        if (!TryScoreCameraPosition(worldX, worldZ, offset, score))
            return false;

        if (!best.valid || score < best.score)
        {
            const float invLength = 1.0f / std::sqrt(quaternionLengthSquared);
            float normalized[4] = {
                orientation[0] * invLength,
                orientation[1] * invLength,
                orientation[2] * invLength,
                orientation[3] * invLength
            };

            best.x = x;
            best.y = y;
            best.z = z;
            best.source = source;
            best.offset = offset;
            best.channel = channel;
            best.score = score;
            best.headingRadians = QuaternionYawFromMemory(normalized);
            best.hasHeading = std::isfinite(best.headingRadians);
            best.valid = true;
        }

        return true;
    }

    bool TryCaptureCameraPositionFromBlock(uintptr_t base, std::uint32_t channel, std::size_t scanBytes, PlayerPositionCandidate& best)
    {
        if (!IsLikelyRuntimePointer(base) || scanBytes < CLIENT_CAMERA_SIZE)
            return false;

        bool captured = false;
        for (std::size_t offset = 0; offset + CLIENT_CAMERA_SIZE <= scanBytes; offset += sizeof(uintptr_t))
        {
            captured = TryCaptureClientCameraAt(base + offset, base, static_cast<std::uint32_t>(offset), channel, best) || captured;
        }

        return captured;
    }

    bool PublishBestPlayerPosition(const PlayerPositionCandidate& best)
    {
        if (!best.valid)
            return false;

        CapturedPlayerPosition position{};
        position.x = best.x;
        position.y = best.y;
        position.z = best.z;
        position.source = best.source;
        position.offset = best.offset;
        position.channel = best.channel;
        position.headingRadians = best.headingRadians;
        position.hasHeading = best.hasHeading;
        position.lastUpdateTick = GetTickCount();
        position.valid = true;
        PublishPlayerPosition(position);
        return true;
    }

    bool TryRefreshPlayerPositionFromCachedCamera()
    {
        const uintptr_t cachedCamera = g_playerCameraAddress.load();
        if (!IsLikelyRuntimePointer(cachedCamera))
            return false;

        PlayerPositionCandidate best{};
        if (!TryCaptureClientCameraAt(cachedCamera, cachedCamera, 0, 20, best))
        {
            g_playerCameraAddress.store(0);
            return false;
        }

        return PublishBestPlayerPosition(best);
    }

    void ScanCameraRoot(uintptr_t root, PlayerPositionCandidate& best)
    {
        if (!IsLikelyRuntimePointer(root))
            return;

        TryCaptureCameraPositionFromBlock(root, 21, PLAYER_CAMERA_ROOT_SCAN_BYTES, best);

        for (std::size_t pointerOffset = 0; pointerOffset + sizeof(uintptr_t) <= PLAYER_CAMERA_POINTER_SCAN_BYTES; pointerOffset += sizeof(uintptr_t))
        {
            uintptr_t child = 0;
            if (!SafeReadValue(root + pointerOffset, child) || !IsLikelyRuntimePointer(child))
                continue;

            TryCaptureCameraPositionFromBlock(child, 22, PLAYER_CAMERA_CHILD_SCAN_BYTES, best);
        }
    }

    bool TryCapturePlayerCameraFromWaypointRecord(const WaypointsUiIterationRecord& record)
    {
        if (TryRefreshPlayerPositionFromCachedCamera())
            return true;

        const DWORD now = GetTickCount();
        if (now - g_lastPlayerCameraScanTick < PLAYER_CAMERA_SCAN_INTERVAL_MS)
            return false;

        g_lastPlayerCameraScanTick = now;

        PlayerPositionCandidate best{};
        const std::array<uintptr_t, 6> roots = {
            reinterpret_cast<uintptr_t>(record.unknown0),
            reinterpret_cast<uintptr_t>(record.unknown8),
            reinterpret_cast<uintptr_t>(record.state),
            reinterpret_cast<uintptr_t>(record.lookupContext),
            reinterpret_cast<uintptr_t>(record.waypointList),
            reinterpret_cast<uintptr_t>(record.playerList)
        };

        for (uintptr_t root : roots)
            ScanCameraRoot(root, best);

        if (!best.valid)
            return false;

        const uintptr_t cameraAddress = best.source + best.offset;
        if (IsLikelyRuntimePointer(cameraAddress))
            g_playerCameraAddress.store(cameraAddress);

        return PublishBestPlayerPosition(best);
    }

    bool SameMarkerPosition(const CapturedNearbyMarker& left, const CapturedNearbyMarker& right)
    {
        if (!left.hasWorldPosition || !right.hasWorldPosition)
            return std::memcmp(left.raw, right.raw, sizeof(left.raw)) == 0;

        const float dx = FixedToWorld(left.x) - FixedToWorld(right.x);
        const float dz = FixedToWorld(left.z) - FixedToWorld(right.z);
        return std::abs(dx) < 2.0f && std::abs(dz) < 2.0f;
    }

    void PushNearbyMarkerUnique(std::vector<CapturedNearbyMarker>& markers, const CapturedNearbyMarker& marker)
    {
        for (const CapturedNearbyMarker& existing : markers)
        {
            if (SameMarkerPosition(existing, marker))
                return;
        }

        markers.push_back(marker);
    }

    bool TryReadNearbyMarker(uintptr_t entryAddress, std::uint32_t kind, CapturedNearbyMarker& marker)
    {
        marker = {};
        marker.kind = kind;
        if (!SafeRead(entryAddress, marker.raw, sizeof(marker.raw)))
            return false;

        marker.hasWorldPosition = TryDecodeFixedWorldPosition(marker.raw, marker.x, marker.y, marker.z);
        return true;
    }

    std::size_t CountDecodedMarkerPreview(std::uint8_t* entries, std::uint64_t count, std::size_t stride, std::uint32_t kind)
    {
        if (!IsLikelyRuntimePointer(reinterpret_cast<uintptr_t>(entries)) || count == 0 || count > NEARBY_MARKER_MAX_ENTRIES || stride < sizeof(std::uint64_t) * 3)
            return 0;

        const std::uint64_t previewCount = MinValue<std::uint64_t>(count, 5);
        std::size_t decoded = 0;
        for (std::uint64_t index = 0; index < previewCount; ++index)
        {
            CapturedNearbyMarker marker{};
            if (TryReadNearbyMarker(reinterpret_cast<uintptr_t>(entries + index * stride), kind, marker) && marker.hasWorldPosition)
                ++decoded;
        }

        return decoded;
    }

    void AppendNearbyMarkerEntriesWithStride(std::vector<CapturedNearbyMarker>& markers, std::uint8_t* entries, std::uint64_t count, std::uint32_t kind, std::size_t stride)
    {
        if (entries == nullptr || count == 0 || count > NEARBY_MARKER_MAX_ENTRIES)
            return;

        const std::uint64_t limitedCount = MinValue<std::uint64_t>(count, 96);
        markers.reserve(markers.size() + static_cast<std::size_t>(limitedCount));
        for (std::uint64_t index = 0; index < limitedCount; ++index)
        {
            CapturedNearbyMarker marker{};
            if (!TryReadNearbyMarker(reinterpret_cast<uintptr_t>(entries + index * stride), kind, marker))
                continue;

            PushNearbyMarkerUnique(markers, marker);
        }
    }

    void AppendNearbyMarkerEntries(std::vector<CapturedNearbyMarker>& markers, std::uint8_t* entries, std::uint64_t count, std::uint32_t kind)
    {
        AppendNearbyMarkerEntriesWithStride(markers, entries, count, kind, NEARBY_MARKER_ENTRY_STRIDE);
    }

    bool AppendNearbyMarkersFromCandidateArray(std::vector<CapturedNearbyMarker>& markers, std::uint8_t* entries, std::uint64_t count, std::uint32_t kind)
    {
        static const std::size_t kStrideCandidates[] = { 0x20, 0x28, 0x30, 0x38, 0x40, 0x48, 0x50, 0x60, 0x80 };
        for (std::size_t stride : kStrideCandidates)
        {
            if (CountDecodedMarkerPreview(entries, count, stride, kind) < 2)
                continue;

            AppendNearbyMarkerEntriesWithStride(markers, entries, count, kind, stride);
            return true;
        }

        return false;
    }

    void AppendNearbyMarkersFromObjectScan(uintptr_t objectAddress, std::size_t scanStart, std::size_t scanBytes, std::vector<CapturedNearbyMarker>& markers, std::uint32_t kind)
    {
        if (!IsLikelyRuntimePointer(objectAddress))
            return;

        const std::size_t scanEnd = scanStart + scanBytes;
        for (std::size_t pointerOffset = scanStart; pointerOffset + sizeof(uintptr_t) <= scanEnd; pointerOffset += sizeof(uintptr_t))
        {
            std::uint8_t* entries = nullptr;
            if (!SafeReadValue(objectAddress + pointerOffset, entries) ||
                !IsLikelyRuntimePointer(reinterpret_cast<uintptr_t>(entries)))
            {
                continue;
            }

            const std::size_t countStart = pointerOffset + sizeof(uintptr_t);
            const std::size_t countEnd = MinValue(scanEnd, pointerOffset + 0x28);
            for (std::size_t countOffset = countStart; countOffset + sizeof(std::uint32_t) <= countEnd; countOffset += sizeof(std::uint32_t))
            {
                std::uint32_t count32 = 0;
                if (!SafeReadValue(objectAddress + countOffset, count32) || count32 == 0 || count32 > NEARBY_MARKER_MAX_ENTRIES)
                    continue;

                if (AppendNearbyMarkersFromCandidateArray(markers, entries, count32, kind))
                    return;
            }
        }
    }

    void AppendNearbyMarkersFromState(std::uint8_t* state, std::vector<CapturedNearbyMarker>& markers)
    {
        std::uint8_t* entries = nullptr;
        std::uint64_t count = 0;
        if (SafeReadValue(reinterpret_cast<uintptr_t>(state + NEARBY_MARKER_ARRAY_OFFSET), entries) &&
            SafeReadValue(reinterpret_cast<uintptr_t>(state + NEARBY_MARKER_COUNT_OFFSET), count))
        {
            AppendNearbyMarkerEntries(markers, entries, count, 1);
        }

    }

    void AppendNearbyMarkersFromInputList(void* list, std::vector<CapturedNearbyMarker>& markers, std::uint32_t kind)
    {
        if (list == nullptr)
            return;

        std::uint8_t* entries = nullptr;
        std::uint32_t count = 0;
        const uintptr_t listAddress = reinterpret_cast<uintptr_t>(list);
        if (SafeReadValue(listAddress + INPUT_MARKER_ARRAY_OFFSET, entries) &&
            SafeReadValue(listAddress + INPUT_MARKER_COUNT_OFFSET, count))
        {
            AppendNearbyMarkerEntries(markers, entries, count, kind);
        }

        AppendNearbyMarkersFromObjectScan(reinterpret_cast<uintptr_t>(list), 0, DYNAMIC_MARKER_SCAN_BYTES, markers, kind + 10);
    }

    void PublishNearbyMarkers(std::vector<CapturedNearbyMarker>&& markers)
    {
        bool changed = false;
        std::size_t markerCount = 0;
        std::size_t decodedCount = 0;
        CapturedNearbyMarker firstDecoded{};
        bool hasFirstDecoded = false;
        {
            std::lock_guard<std::mutex> lock(g_nearbyMarkerMutex);
            changed = !SameNearbyMarkers(g_nearbyMarkers, markers);
            if (changed)
                g_nearbyMarkers = std::move(markers);

            markerCount = g_nearbyMarkers.size();
            for (const CapturedNearbyMarker& marker : g_nearbyMarkers)
            {
                if (marker.hasWorldPosition)
                {
                    ++decodedCount;
                    if (!hasFirstDecoded)
                    {
                        firstDecoded = marker;
                        hasFirstDecoded = true;
                    }
                }
            }
        }

        const DWORD now = GetTickCount();
        if (now - g_lastNearbySummaryTick < 5000)
            return;

        g_lastNearbySummaryTick = now;

        std::ostringstream oss;
        oss << "[Minimap] internal nearby marker feed: " << markerCount
            << " entries | decoded_world=" << decodedCount;
        if (hasFirstDecoded)
        {
            oss << " | first_kind=" << firstDecoded.kind
                << " | first world=("
                << FixedToWorld(firstDecoded.x) << ", "
                << FixedToWorld(firstDecoded.y) << ", "
                << FixedToWorld(firstDecoded.z) << ")"
                << " | first_raw="
                << std::hex << firstDecoded.raw[0] << ","
                << firstDecoded.raw[1] << ","
                << firstDecoded.raw[2] << ","
                << firstDecoded.raw[3] << ","
                << firstDecoded.raw[4] << std::dec;
        }
        Log(oss.str());
    }

    bool TryInitWaypointRecord(void* ctx, WaypointsUiIterationRecord& record)
    {
        __try
        {
            g_iterInit(ctx, &record, static_cast<std::uint32_t>(sizeof(record)));
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    bool TryInitUiRenderSetupRecord(void* ctx, UiRenderSetupIterationRecord& record)
    {
        __try
        {
            g_iterInit(ctx, &record, static_cast<std::uint32_t>(sizeof(record)));
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    void PublishUiRenderSnapshot(const UiRenderSnapshot& snapshot)
    {
        const DWORD now = GetTickCount();
        if (now - g_lastUiRenderSummaryTick < 5000)
            return;

        g_lastUiRenderSummaryTick = now;

        std::ostringstream oss;
        oss << "[Minimap] internal UI render setup"
            << " | source=" << Hex(snapshot.source)
            << " | state=" << Hex(snapshot.state)
            << " | source_size=" << snapshot.sourceWidth << "x" << snapshot.sourceHeight
            << " | state_size=" << snapshot.stateWidth << "x" << snapshot.stateHeight;
        Log(oss.str());
    }

    bool TryCaptureUiRenderSetup(void* ctx)
    {
        if (g_iterInit == nullptr || ctx == nullptr)
            return false;

        UiRenderSetupIterationRecord record{};
        if (!TryInitUiRenderSetupRecord(ctx, record))
            return false;

        if (record.source == nullptr || record.state == nullptr)
            return false;

        UiRenderSnapshot snapshot{};
        snapshot.source = reinterpret_cast<uintptr_t>(record.source);
        snapshot.state = reinterpret_cast<uintptr_t>(record.state);

        SafeReadValue(reinterpret_cast<uintptr_t>(record.source + 0x30C), snapshot.sourceWidth);
        SafeReadValue(reinterpret_cast<uintptr_t>(record.source + 0x304), snapshot.sourceHeight);
        SafeReadValue(reinterpret_cast<uintptr_t>(record.state + 0x16180), snapshot.stateWidth);
        SafeReadValue(reinterpret_cast<uintptr_t>(record.state + 0x16184), snapshot.stateHeight);

        PublishUiRenderSnapshot(snapshot);
        TryCapturePlayerPositionFromUiRecord(record);
        return true;
    }

    void PublishWaypoints(std::vector<CapturedWaypoint>&& waypoints)
    {
        bool changed = false;
        std::size_t waypointCount = 0;
        CapturedWaypoint firstWaypoint{};
        bool hasFirstWaypoint = false;
        {
            std::lock_guard<std::mutex> lock(g_waypointMutex);
            changed = !SameWaypoints(g_waypoints, waypoints);
            if (changed)
                g_waypoints = std::move(waypoints);

            waypointCount = g_waypoints.size();
            if (!g_waypoints.empty())
            {
                firstWaypoint = g_waypoints.front();
                hasFirstWaypoint = true;
            }
        }

        const DWORD now = GetTickCount();
        if (!changed && now - g_lastSummaryTick < 5000)
            return;

        g_lastSummaryTick = now;

        std::ostringstream oss;
        oss << "[Minimap] internal waypoint feed: " << waypointCount << " active";
        if (hasFirstWaypoint)
        {
            oss << " | first_id=" << firstWaypoint.id
                << " | first world=("
                << FixedToWorld(firstWaypoint.x) << ", "
                << FixedToWorld(firstWaypoint.y) << ", "
                << FixedToWorld(firstWaypoint.z) << ")";
        }
        Log(oss.str());
    }

    bool TryCaptureWaypointsFromPlayerWaypointsUi(void* ctx)
    {
        if (g_iterInit == nullptr || ctx == nullptr)
            return false;

        WaypointsUiIterationRecord record{};
        if (!TryInitWaypointRecord(ctx, record))
            return false;

        TryCapturePlayerCameraFromWaypointRecord(record);

        std::uint8_t* state = record.state;
        if (state == nullptr)
            return false;

        std::vector<CapturedNearbyMarker> nearbyMarkers;
        AppendNearbyMarkersFromState(state, nearbyMarkers);
        AppendNearbyMarkersFromInputList(record.waypointList, nearbyMarkers, 2);
        AppendNearbyMarkersFromInputList(record.playerList, nearbyMarkers, 3);
        PublishNearbyMarkers(std::move(nearbyMarkers));

        std::uint8_t* entries = nullptr;
        std::uint64_t count = 0;
        if (!SafeReadValue(reinterpret_cast<uintptr_t>(state + WAYPOINT_ARRAY_OFFSET), entries) ||
            !SafeReadValue(reinterpret_cast<uintptr_t>(state + WAYPOINT_COUNT_OFFSET), count))
        {
            return false;
        }

        if (entries == nullptr || count == 0 || count > WAYPOINT_MAX_ENTRIES)
        {
            PublishWaypoints({});
            return true;
        }

        std::vector<CapturedWaypoint> waypoints;
        waypoints.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(count, 64)));
        const std::size_t activeFlagOffsets[] = { WAYPOINT_ENTRY_ACTIVE_OFFSET, 0x40, 0x50, 0x58, 0x60 };

        for (std::uint64_t index = 0; index < count; ++index)
        {
            std::uint8_t* entry = entries + (index * WAYPOINT_ENTRY_STRIDE);

            CapturedWaypoint waypoint{};
            SafeRead(reinterpret_cast<uintptr_t>(entry), waypoint.raw, sizeof(waypoint.raw));
            if (!SafeReadValue(reinterpret_cast<uintptr_t>(entry + WAYPOINT_ENTRY_POSITION_OFFSET + 0x00), waypoint.x) ||
                !SafeReadValue(reinterpret_cast<uintptr_t>(entry + WAYPOINT_ENTRY_POSITION_OFFSET + 0x08), waypoint.y) ||
                !SafeReadValue(reinterpret_cast<uintptr_t>(entry + WAYPOINT_ENTRY_POSITION_OFFSET + 0x10), waypoint.z))
            {
                continue;
            }

            SafeReadValue(reinterpret_cast<uintptr_t>(entry), waypoint.id);
            if (!IsPlausibleWorldPosition(FixedToWorld(waypoint.x), FixedToWorld(waypoint.y), FixedToWorld(waypoint.z)))
                continue;

            bool active = waypoint.id != 0;
            for (std::size_t activeOffset : activeFlagOffsets)
            {
                std::uint8_t activeByte = 0;
                if (SafeReadValue(reinterpret_cast<uintptr_t>(entry + activeOffset), activeByte) && activeByte != 0)
                {
                    active = true;
                    break;
                }
            }

            if (!active)
                continue;

            PushWaypointUnique(waypoints, waypoint);
        }

        PublishWaypoints(std::move(waypoints));
        return true;
    }

    void __fastcall CaptureUiRenderSetupHook(void* ctx, void*, void*, void*)
    {
        TryCaptureUiRenderSetup(ctx);
    }

    void __fastcall CaptureWaypointsHook(void* ctx, void*, void*, void*)
    {
        if (!TryCaptureWaypointsFromPlayerWaypointsUi(ctx))
            Log("[Minimap] failed to read internal player_waypoints_ui state");
    }

    void __fastcall CaptureRenderPresentFrameHook(void* renderContext, void*, void* swapchainState, void*)
    {
        const DWORD now = GetTickCount();
        if (now - g_lastRenderFrameSummaryTick < 5000)
            return;

        g_lastRenderFrameSummaryTick = now;

        uintptr_t table = 0;
        uintptr_t device = 0;
        uintptr_t graphicsContext = 0;
        uintptr_t queueState = 0;
        std::uint32_t queueIndex = 0;
        std::uint64_t presentCount = 0;
        uintptr_t presentSwapchains = 0;

        if (renderContext != nullptr)
        {
            SafeReadValue(reinterpret_cast<uintptr_t>(renderContext) + 0x08, table);
            SafeReadValue(reinterpret_cast<uintptr_t>(renderContext) + 0x10, device);
            SafeReadValue(reinterpret_cast<uintptr_t>(renderContext) + 0x28, graphicsContext);
        }

        g_lastRenderContext.store(reinterpret_cast<uintptr_t>(renderContext));
        g_lastVulkanDevice.store(device);
        g_lastGraphicsContext.store(graphicsContext);
        g_lastSwapchainState.store(reinterpret_cast<uintptr_t>(swapchainState));

        if (table != 0)
            TryInstallVulkanTableHooks(table);

        if (graphicsContext != 0)
        {
            SafeReadValue(graphicsContext + 0xD98, queueState);
            SafeReadValue(graphicsContext + 0xDFC, queueIndex);
            g_lastVulkanQueueFamilyIndex.store(queueIndex);
        }

        if (swapchainState != nullptr)
        {
            SafeReadValue(reinterpret_cast<uintptr_t>(swapchainState) + 0x120, presentCount);
            SafeReadValue(reinterpret_cast<uintptr_t>(swapchainState) + 0x118, presentSwapchains);
        }

        std::ostringstream oss;
        oss << "[Minimap] render present frame"
            << " | render_context=" << Hex(reinterpret_cast<uintptr_t>(renderContext))
            << " | table=" << Hex(table)
            << " | device=" << Hex(device)
            << " | graphics_context=" << Hex(graphicsContext)
            << " | queue_state=" << Hex(queueState)
            << " | queue_index=" << queueIndex
            << " | swapchain_state=" << Hex(reinterpret_cast<uintptr_t>(swapchainState))
            << " | present_count=" << presentCount
            << " | present_swapchains=" << Hex(presentSwapchains);
        Log(oss.str());
    }

    bool ReadVulkanTableFunction(uintptr_t table, std::size_t offset, uintptr_t& outValue)
    {
        outValue = 0;
        if (table == 0)
            return false;
        return SafeReadValue(table + offset, outValue);
    }

    bool IsReadablePage(DWORD protect)
    {
        if ((protect & PAGE_GUARD) != 0 || (protect & PAGE_NOACCESS) != 0)
            return false;

        switch (protect & 0xFF)
        {
        case PAGE_READONLY:
        case PAGE_READWRITE:
        case PAGE_WRITECOPY:
        case PAGE_EXECUTE_READ:
        case PAGE_EXECUTE_READWRITE:
        case PAGE_EXECUTE_WRITECOPY:
            return true;
        default:
            return false;
        }
    }

    bool WritePointer(uintptr_t address, uintptr_t value)
    {
        DWORD oldProtect = 0;
        if (!VirtualProtect(reinterpret_cast<void*>(address), sizeof(value), PAGE_READWRITE, &oldProtect))
            return false;

        bool wrote = false;
        __try
        {
            *reinterpret_cast<uintptr_t*>(address) = value;
            wrote = true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            wrote = false;
        }

        DWORD ignored = 0;
        VirtualProtect(reinterpret_cast<void*>(address), sizeof(value), oldProtect, &ignored);
        return wrote;
    }

    SwapchainRuntimeInfo* FindSwapchainLocked(uintptr_t handle)
    {
        auto found = std::find_if(
            g_swapchains.begin(),
            g_swapchains.end(),
            [handle](const SwapchainRuntimeInfo& info)
            {
                return info.handle == handle;
            });

        return found != g_swapchains.end() ? &(*found) : nullptr;
    }

    bool TryCopySwapchainCreateInfo(const VkSwapchainCreateInfoKHR* createInfo, VkSwapchainCreateInfoKHR& outInfo)
    {
        if (createInfo == nullptr)
            return false;

        return SafeReadValue(reinterpret_cast<uintptr_t>(createInfo), outInfo);
    }

    void RememberSwapchainCreate(void* device, const VkSwapchainCreateInfoKHR* createInfo, void* swapchain, std::int32_t result)
    {
        if (result != 0 || createInfo == nullptr || swapchain == nullptr)
            return;

        VkSwapchainCreateInfoKHR info{};
        if (!TryCopySwapchainCreateInfo(createInfo, info))
            return;

        std::lock_guard<std::mutex> lock(g_swapchainMutex);
        if (info.oldSwapchain != nullptr)
        {
            const uintptr_t oldHandle = reinterpret_cast<uintptr_t>(info.oldSwapchain);
            g_swapchains.erase(
                std::remove_if(
                    g_swapchains.begin(),
                    g_swapchains.end(),
                    [oldHandle](const SwapchainRuntimeInfo& existing)
                    {
                        return existing.handle == oldHandle;
                    }),
                g_swapchains.end());
        }

        const uintptr_t handle = reinterpret_cast<uintptr_t>(swapchain);
        SwapchainRuntimeInfo* runtime = FindSwapchainLocked(handle);
        if (runtime == nullptr)
        {
            g_swapchains.push_back({});
            runtime = &g_swapchains.back();
        }

        runtime->handle = handle;
        runtime->device = reinterpret_cast<uintptr_t>(device);
        runtime->format = info.imageFormat;
        runtime->width = info.imageExtent.width;
        runtime->height = info.imageExtent.height;
        runtime->minImageCount = info.minImageCount;
        runtime->imageCount = 0;
        runtime->images.clear();
    }

    void RememberSwapchainImages(void* device, void* swapchain, std::uint32_t* count, void* images, std::int32_t result)
    {
        if (result != 0 || swapchain == nullptr || count == nullptr)
            return;

        std::uint32_t imageCount = 0;
        if (!SafeReadValue(reinterpret_cast<uintptr_t>(count), imageCount) || imageCount > 16)
            return;

        std::vector<uintptr_t> imageHandles;
        if (images != nullptr && imageCount != 0)
        {
            imageHandles.resize(imageCount);
            if (!SafeRead(reinterpret_cast<uintptr_t>(images), imageHandles.data(), sizeof(uintptr_t) * imageHandles.size()))
                imageHandles.clear();
        }

        std::lock_guard<std::mutex> lock(g_swapchainMutex);
        const uintptr_t handle = reinterpret_cast<uintptr_t>(swapchain);
        SwapchainRuntimeInfo* runtime = FindSwapchainLocked(handle);
        if (runtime == nullptr)
        {
            g_swapchains.push_back({});
            runtime = &g_swapchains.back();
            runtime->handle = handle;
        }

        if (device != nullptr)
            runtime->device = reinterpret_cast<uintptr_t>(device);

        runtime->imageCount = imageCount;
        if (!imageHandles.empty())
            runtime->images = std::move(imageHandles);
    }

    bool TryGetSwapchainSnapshot(uintptr_t handle, SwapchainRuntimeInfo& outInfo)
    {
        std::lock_guard<std::mutex> lock(g_swapchainMutex);
        SwapchainRuntimeInfo* runtime = FindSwapchainLocked(handle);
        if (runtime == nullptr)
            return false;

        outInfo = *runtime;
        return true;
    }

    std::size_t FindVulkanTableOffset(uintptr_t table, uintptr_t function)
    {
        if (table == 0 || function == 0)
            return static_cast<std::size_t>(-1);

        for (std::size_t offset = 0; offset <= VULKAN_DEVICE_TABLE_SCAN_LIMIT; offset += sizeof(uintptr_t))
        {
            uintptr_t current = 0;
            if (SafeReadValue(table + offset, current) && current == function)
                return offset;
        }

        return static_cast<std::size_t>(-1);
    }

    void AppendFunctionLookup(std::ostringstream& oss, uintptr_t table, GetDeviceProcAddrFn getDeviceProcAddr, void* device, const char* name)
    {
        const uintptr_t pointer = reinterpret_cast<uintptr_t>(getDeviceProcAddr(device, name));
        const std::size_t offset = FindVulkanTableOffset(table, pointer);

        oss << " | " << name << "=" << Hex(pointer);
        if (offset != static_cast<std::size_t>(-1))
            oss << "@" << Hex(static_cast<uintptr_t>(offset));
        else if (pointer != 0)
            oss << "@external";
        else
            oss << "@missing";
    }

    void TryScanVulkanDeviceFunctions()
    {
        const DWORD now = GetTickCount();
        if (g_vulkanFunctionOffsetsLogged || now - g_lastVulkanFunctionScanTick < 10000)
            return;

        std::lock_guard<std::mutex> lock(g_vulkanFunctionScanMutex);
        if (g_vulkanFunctionOffsetsLogged || now - g_lastVulkanFunctionScanTick < 10000)
            return;

        g_lastVulkanFunctionScanTick = now;

        const uintptr_t table = g_vulkanDeviceTable.load();
        const uintptr_t device = g_lastVulkanDevice.load();
        if (table == 0 || device == 0)
            return;

        uintptr_t getDeviceProcAddrValue = 0;
        if (!ReadVulkanTableFunction(table, VULKAN_TABLE_GET_DEVICE_PROC_ADDR_OFFSET, getDeviceProcAddrValue) ||
            getDeviceProcAddrValue == 0)
        {
            return;
        }

        auto* getDeviceProcAddr = reinterpret_cast<GetDeviceProcAddrFn>(getDeviceProcAddrValue);
        void* deviceHandle = reinterpret_cast<void*>(device);

        const std::array<const char*, 30> names = {
            "vkCreateCommandPool",
            "vkDestroyCommandPool",
            "vkAllocateCommandBuffers",
            "vkFreeCommandBuffers",
            "vkBeginCommandBuffer",
            "vkEndCommandBuffer",
            "vkResetCommandBuffer",
            "vkCreateImageView",
            "vkDestroyImageView",
            "vkCreateRenderPass",
            "vkDestroyRenderPass",
            "vkCreateFramebuffer",
            "vkDestroyFramebuffer",
            "vkCreateShaderModule",
            "vkDestroyShaderModule",
            "vkCreatePipelineLayout",
            "vkDestroyPipelineLayout",
            "vkCreateGraphicsPipelines",
            "vkDestroyPipeline",
            "vkCreateSemaphore",
            "vkDestroySemaphore",
            "vkDeviceWaitIdle",
            "vkQueueSubmit",
            "vkCmdBeginRenderPass",
            "vkCmdEndRenderPass",
            "vkCmdSetViewport",
            "vkCmdSetScissor",
            "vkCmdBindPipeline",
            "vkCmdDraw",
            "vkCmdClearAttachments"
        };

        std::ostringstream oss;
        oss << "[Minimap] Vulkan renderer function offsets"
            << " | table=" << Hex(table)
            << " | device=" << Hex(device)
            << " | getDeviceProcAddr=" << Hex(getDeviceProcAddrValue)
            << "@" << Hex(static_cast<uintptr_t>(VULKAN_TABLE_GET_DEVICE_PROC_ADDR_OFFSET));

        int inLine = 0;
        for (const char* name : names)
        {
            if (inLine == 4)
            {
                Log(oss.str());
                oss.str("");
                oss.clear();
                oss << "[Minimap] Vulkan renderer function offsets";
                inLine = 0;
            }

            AppendFunctionLookup(oss, table, getDeviceProcAddr, deviceHandle, name);
            ++inLine;
        }

        if (inLine != 0)
            Log(oss.str());

        g_vulkanFunctionOffsetsLogged = true;
    }

    template <typename T>
    T ResolveDeviceFunction(GetDeviceProcAddrFn getDeviceProcAddr, void* device, const char* name)
    {
        return reinterpret_cast<T>(getDeviceProcAddr(device, name));
    }

    bool TryLoadVulkanRendererFns(VulkanRendererFns& fns, void* device)
    {
        const uintptr_t table = g_vulkanDeviceTable.load();
        uintptr_t getDeviceProcAddrValue = 0;
        if (!ReadVulkanTableFunction(table, VULKAN_TABLE_GET_DEVICE_PROC_ADDR_OFFSET, getDeviceProcAddrValue) ||
            getDeviceProcAddrValue == 0)
        {
            return false;
        }

        auto* getDeviceProcAddr = reinterpret_cast<GetDeviceProcAddrFn>(getDeviceProcAddrValue);
        const uintptr_t originalGetImages = g_originalGetSwapchainImages.load();
        fns.getSwapchainImages = originalGetImages != 0
            ? reinterpret_cast<GetSwapchainImagesFn>(originalGetImages)
            : ResolveDeviceFunction<GetSwapchainImagesFn>(getDeviceProcAddr, device, "vkGetSwapchainImagesKHR");

        fns.createImageView = ResolveDeviceFunction<CreateImageViewFn>(getDeviceProcAddr, device, "vkCreateImageView");
        fns.destroyImageView = ResolveDeviceFunction<DestroyImageViewFn>(getDeviceProcAddr, device, "vkDestroyImageView");
        fns.createRenderPass = ResolveDeviceFunction<CreateRenderPassFn>(getDeviceProcAddr, device, "vkCreateRenderPass");
        fns.destroyRenderPass = ResolveDeviceFunction<DestroyRenderPassFn>(getDeviceProcAddr, device, "vkDestroyRenderPass");
        fns.createFramebuffer = ResolveDeviceFunction<CreateFramebufferFn>(getDeviceProcAddr, device, "vkCreateFramebuffer");
        fns.destroyFramebuffer = ResolveDeviceFunction<DestroyFramebufferFn>(getDeviceProcAddr, device, "vkDestroyFramebuffer");
        fns.createCommandPool = ResolveDeviceFunction<CreateCommandPoolFn>(getDeviceProcAddr, device, "vkCreateCommandPool");
        fns.destroyCommandPool = ResolveDeviceFunction<DestroyCommandPoolFn>(getDeviceProcAddr, device, "vkDestroyCommandPool");
        fns.allocateCommandBuffers = ResolveDeviceFunction<AllocateCommandBuffersFn>(getDeviceProcAddr, device, "vkAllocateCommandBuffers");
        fns.resetCommandBuffer = ResolveDeviceFunction<ResetCommandBufferFn>(getDeviceProcAddr, device, "vkResetCommandBuffer");
        fns.beginCommandBuffer = ResolveDeviceFunction<BeginCommandBufferFn>(getDeviceProcAddr, device, "vkBeginCommandBuffer");
        fns.endCommandBuffer = ResolveDeviceFunction<EndCommandBufferFn>(getDeviceProcAddr, device, "vkEndCommandBuffer");
        fns.cmdBeginRenderPass = ResolveDeviceFunction<CmdBeginRenderPassFn>(getDeviceProcAddr, device, "vkCmdBeginRenderPass");
        fns.cmdEndRenderPass = ResolveDeviceFunction<CmdEndRenderPassFn>(getDeviceProcAddr, device, "vkCmdEndRenderPass");
        fns.cmdClearAttachments = ResolveDeviceFunction<CmdClearAttachmentsFn>(getDeviceProcAddr, device, "vkCmdClearAttachments");
        fns.createBuffer = ResolveDeviceFunction<CreateBufferFn>(getDeviceProcAddr, device, "vkCreateBuffer");
        fns.destroyBuffer = ResolveDeviceFunction<DestroyBufferFn>(getDeviceProcAddr, device, "vkDestroyBuffer");
        fns.getBufferMemoryRequirements = ResolveDeviceFunction<GetBufferMemoryRequirementsFn>(getDeviceProcAddr, device, "vkGetBufferMemoryRequirements");
        fns.createImage = ResolveDeviceFunction<CreateImageFn>(getDeviceProcAddr, device, "vkCreateImage");
        fns.destroyImage = ResolveDeviceFunction<DestroyImageFn>(getDeviceProcAddr, device, "vkDestroyImage");
        fns.getImageMemoryRequirements = ResolveDeviceFunction<GetImageMemoryRequirementsFn>(getDeviceProcAddr, device, "vkGetImageMemoryRequirements");
        fns.allocateMemory = ResolveDeviceFunction<AllocateMemoryFn>(getDeviceProcAddr, device, "vkAllocateMemory");
        fns.freeMemory = ResolveDeviceFunction<FreeMemoryFn>(getDeviceProcAddr, device, "vkFreeMemory");
        fns.bindBufferMemory = ResolveDeviceFunction<BindBufferMemoryFn>(getDeviceProcAddr, device, "vkBindBufferMemory");
        fns.bindImageMemory = ResolveDeviceFunction<BindImageMemoryFn>(getDeviceProcAddr, device, "vkBindImageMemory");
        fns.mapMemory = ResolveDeviceFunction<MapMemoryFn>(getDeviceProcAddr, device, "vkMapMemory");
        fns.unmapMemory = ResolveDeviceFunction<UnmapMemoryFn>(getDeviceProcAddr, device, "vkUnmapMemory");
        fns.createSampler = ResolveDeviceFunction<CreateSamplerFn>(getDeviceProcAddr, device, "vkCreateSampler");
        fns.destroySampler = ResolveDeviceFunction<DestroySamplerFn>(getDeviceProcAddr, device, "vkDestroySampler");
        fns.createShaderModule = ResolveDeviceFunction<CreateShaderModuleFn>(getDeviceProcAddr, device, "vkCreateShaderModule");
        fns.destroyShaderModule = ResolveDeviceFunction<DestroyShaderModuleFn>(getDeviceProcAddr, device, "vkDestroyShaderModule");
        fns.createDescriptorSetLayout = ResolveDeviceFunction<CreateDescriptorSetLayoutFn>(getDeviceProcAddr, device, "vkCreateDescriptorSetLayout");
        fns.destroyDescriptorSetLayout = ResolveDeviceFunction<DestroyDescriptorSetLayoutFn>(getDeviceProcAddr, device, "vkDestroyDescriptorSetLayout");
        fns.createDescriptorPool = ResolveDeviceFunction<CreateDescriptorPoolFn>(getDeviceProcAddr, device, "vkCreateDescriptorPool");
        fns.destroyDescriptorPool = ResolveDeviceFunction<DestroyDescriptorPoolFn>(getDeviceProcAddr, device, "vkDestroyDescriptorPool");
        fns.allocateDescriptorSets = ResolveDeviceFunction<AllocateDescriptorSetsFn>(getDeviceProcAddr, device, "vkAllocateDescriptorSets");
        fns.updateDescriptorSets = ResolveDeviceFunction<UpdateDescriptorSetsFn>(getDeviceProcAddr, device, "vkUpdateDescriptorSets");
        fns.createPipelineLayout = ResolveDeviceFunction<CreatePipelineLayoutFn>(getDeviceProcAddr, device, "vkCreatePipelineLayout");
        fns.destroyPipelineLayout = ResolveDeviceFunction<DestroyPipelineLayoutFn>(getDeviceProcAddr, device, "vkDestroyPipelineLayout");
        fns.createGraphicsPipelines = ResolveDeviceFunction<CreateGraphicsPipelinesFn>(getDeviceProcAddr, device, "vkCreateGraphicsPipelines");
        fns.destroyPipeline = ResolveDeviceFunction<DestroyPipelineFn>(getDeviceProcAddr, device, "vkDestroyPipeline");
        fns.cmdPipelineBarrier = ResolveDeviceFunction<CmdPipelineBarrierFn>(getDeviceProcAddr, device, "vkCmdPipelineBarrier");
        fns.cmdCopyBufferToImage = ResolveDeviceFunction<CmdCopyBufferToImageFn>(getDeviceProcAddr, device, "vkCmdCopyBufferToImage");
        fns.cmdBindPipeline = ResolveDeviceFunction<CmdBindPipelineFn>(getDeviceProcAddr, device, "vkCmdBindPipeline");
        fns.cmdBindDescriptorSets = ResolveDeviceFunction<CmdBindDescriptorSetsFn>(getDeviceProcAddr, device, "vkCmdBindDescriptorSets");
        fns.cmdPushConstants = ResolveDeviceFunction<CmdPushConstantsFn>(getDeviceProcAddr, device, "vkCmdPushConstants");
        fns.cmdDraw = ResolveDeviceFunction<CmdDrawFn>(getDeviceProcAddr, device, "vkCmdDraw");
        fns.createSemaphore = ResolveDeviceFunction<CreateSemaphoreFn>(getDeviceProcAddr, device, "vkCreateSemaphore");
        fns.destroySemaphore = ResolveDeviceFunction<DestroySemaphoreFn>(getDeviceProcAddr, device, "vkDestroySemaphore");
        fns.queueSubmit = ResolveDeviceFunction<QueueSubmitFn>(getDeviceProcAddr, device, "vkQueueSubmit");
        fns.deviceWaitIdle = ResolveDeviceFunction<DeviceWaitIdleFn>(getDeviceProcAddr, device, "vkDeviceWaitIdle");
        return fns.Ready();
    }

    void LogRendererThrottled(const std::string& message)
    {
        const DWORD now = GetTickCount();
        if (now - g_lastVulkanRendererLogTick < 5000)
            return;

        g_lastVulkanRendererLogTick = now;
        Log(message);
    }

    std::string JoinPath(const std::string& base, const std::string& name);
    std::string GetExecutableDirectory();
    void EnsureMinimapFrameLoaded();

    bool TryReadBinaryFile(const std::string& path, std::vector<std::uint8_t>& bytes)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
            return false;

        const std::streamoff size = file.tellg();
        if (size <= 0 || size > 1024 * 1024)
            return false;

        file.seekg(0, std::ios::beg);
        bytes.resize(static_cast<std::size_t>(size));
        return file.read(reinterpret_cast<char*>(bytes.data()), bytes.size()).good();
    }

    void AddFrameShaderCandidates(std::vector<std::string>& candidates, const std::string& directory, const char* name)
    {
        candidates.push_back(JoinPath(directory, name));
    }

    bool TryReadFrameShader(const char* name, std::vector<std::uint8_t>& bytes)
    {
        std::vector<std::string> candidates;
        if (g_modContext != nullptr && !g_modContext->shroudtopia.mod_folder.empty())
        {
            const std::string modRoot = JoinPath(g_modContext->shroudtopia.mod_folder, "minimap_mod");
            AddFrameShaderCandidates(candidates, modRoot, name);
            AddFrameShaderCandidates(candidates, JoinPath(modRoot, "assets"), name);
        }

        const std::string gameDir = GetExecutableDirectory();
        if (!gameDir.empty())
        {
            const std::string installedModRoot = JoinPath(JoinPath(JoinPath(gameDir, "mods"), "minimap_mod"), "");
            AddFrameShaderCandidates(candidates, installedModRoot, name);
            AddFrameShaderCandidates(candidates, JoinPath(installedModRoot, "assets"), name);
        }

        AddFrameShaderCandidates(candidates, "", name);

        for (const std::string& path : candidates)
        {
            if (TryReadBinaryFile(path, bytes))
                return true;
        }

        return false;
    }

    bool TryCreateShaderModule(VulkanMinimapRenderer& renderer, const std::vector<std::uint8_t>& bytes, void** shaderModule)
    {
        *shaderModule = nullptr;
        if (bytes.empty() || (bytes.size() % sizeof(std::uint32_t)) != 0)
            return false;

        std::vector<std::uint32_t> words(bytes.size() / sizeof(std::uint32_t));
        std::memcpy(words.data(), bytes.data(), bytes.size());

        VkShaderModuleCreateInfo shaderInfo{};
        shaderInfo.codeSize = bytes.size();
        shaderInfo.pCode = words.data();
        return renderer.fns.createShaderModule(
            reinterpret_cast<void*>(renderer.device),
            &shaderInfo,
            nullptr,
            shaderModule) == VK_SUCCESS && *shaderModule != nullptr;
    }

    bool TryAllocateMemoryByTrial(VulkanMinimapRenderer& renderer, const VkMemoryRequirements& requirements, bool requireMappable, void** memory, void** mapped)
    {
        *memory = nullptr;
        if (mapped != nullptr)
            *mapped = nullptr;

        void* device = reinterpret_cast<void*>(renderer.device);
        for (std::uint32_t index = 0; index < 32; ++index)
        {
            if ((requirements.memoryTypeBits & (1u << index)) == 0)
                continue;

            VkMemoryAllocateInfo alloc{};
            alloc.allocationSize = requirements.size;
            alloc.memoryTypeIndex = index;

            void* candidate = nullptr;
            if (renderer.fns.allocateMemory(device, &alloc, nullptr, &candidate) != VK_SUCCESS || candidate == nullptr)
                continue;

            void* mappedCandidate = nullptr;
            if (requireMappable)
            {
                if (renderer.fns.mapMemory(device, candidate, 0, requirements.size, 0, &mappedCandidate) != VK_SUCCESS || mappedCandidate == nullptr)
                {
                    renderer.fns.freeMemory(device, candidate, nullptr);
                    continue;
                }
            }

            *memory = candidate;
            if (mapped != nullptr)
                *mapped = mappedCandidate;
            return true;
        }

        return false;
    }

    bool TryCreateTexturedFrameResourcesLocked(VulkanMinimapRenderer& renderer)
    {
        if (renderer.frameTextureReady)
            return true;
        if (!renderer.fns.TextureReady())
            return false;

        EnsureMinimapFrameLoaded();

        std::vector<std::uint8_t> rgba;
        int textureWidth = 0;
        int textureHeight = 0;
        {
            std::lock_guard<std::mutex> lock(g_minimapFrameMutex);
            if (!g_minimapFrame.loaded || g_minimapFrame.rgba.empty() || g_minimapFrame.width <= 0 || g_minimapFrame.height <= 0)
                return false;

            rgba = g_minimapFrame.rgba;
            textureWidth = g_minimapFrame.width;
            textureHeight = g_minimapFrame.height;
        }

        void* device = reinterpret_cast<void*>(renderer.device);
        const std::uint64_t textureBytes = static_cast<std::uint64_t>(rgba.size());

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.size = textureBytes;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        void* stagingBuffer = nullptr;
        if (renderer.fns.createBuffer(device, &bufferInfo, nullptr, &stagingBuffer) != VK_SUCCESS || stagingBuffer == nullptr)
            return false;

        VkMemoryRequirements bufferRequirements{};
        renderer.fns.getBufferMemoryRequirements(device, stagingBuffer, &bufferRequirements);
        void* stagingMemory = nullptr;
        void* mapped = nullptr;
        if (!TryAllocateMemoryByTrial(renderer, bufferRequirements, true, &stagingMemory, &mapped))
        {
            renderer.fns.destroyBuffer(device, stagingBuffer, nullptr);
            return false;
        }
        std::memcpy(mapped, rgba.data(), rgba.size());
        renderer.fns.unmapMemory(device, stagingMemory);
        if (renderer.fns.bindBufferMemory(device, stagingBuffer, stagingMemory, 0) != VK_SUCCESS)
        {
            renderer.fns.freeMemory(device, stagingMemory, nullptr);
            renderer.fns.destroyBuffer(device, stagingBuffer, nullptr);
            return false;
        }

        VkImageCreateInfo imageInfo{};
        imageInfo.extent.width = static_cast<std::uint32_t>(textureWidth);
        imageInfo.extent.height = static_cast<std::uint32_t>(textureHeight);
        imageInfo.extent.depth = 1;
        void* textureImage = nullptr;
        if (renderer.fns.createImage(device, &imageInfo, nullptr, &textureImage) != VK_SUCCESS || textureImage == nullptr)
        {
            renderer.fns.freeMemory(device, stagingMemory, nullptr);
            renderer.fns.destroyBuffer(device, stagingBuffer, nullptr);
            return false;
        }

        VkMemoryRequirements imageRequirements{};
        renderer.fns.getImageMemoryRequirements(device, textureImage, &imageRequirements);
        void* textureMemory = nullptr;
        if (!TryAllocateMemoryByTrial(renderer, imageRequirements, false, &textureMemory, nullptr) ||
            renderer.fns.bindImageMemory(device, textureImage, textureMemory, 0) != VK_SUCCESS)
        {
            if (textureMemory != nullptr)
                renderer.fns.freeMemory(device, textureMemory, nullptr);
            renderer.fns.destroyImage(device, textureImage, nullptr);
            renderer.fns.freeMemory(device, stagingMemory, nullptr);
            renderer.fns.destroyBuffer(device, stagingBuffer, nullptr);
            return false;
        }

        VkImageViewCreateInfo textureViewInfo{};
        textureViewInfo.image = textureImage;
        textureViewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        textureViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        textureViewInfo.subresourceRange.levelCount = 1;
        textureViewInfo.subresourceRange.layerCount = 1;
        void* textureImageView = nullptr;
        if (renderer.fns.createImageView(device, &textureViewInfo, nullptr, &textureImageView) != VK_SUCCESS || textureImageView == nullptr)
            return false;

        VkSamplerCreateInfo samplerInfo{};
        void* sampler = nullptr;
        if (renderer.fns.createSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS || sampler == nullptr)
            return false;

        VkDescriptorSetLayoutBinding binding{};
        VkDescriptorSetLayoutCreateInfo setLayoutInfo{};
        setLayoutInfo.bindingCount = 1;
        setLayoutInfo.pBindings = &binding;
        void* setLayout = nullptr;
        if (renderer.fns.createDescriptorSetLayout(device, &setLayoutInfo, nullptr, &setLayout) != VK_SUCCESS || setLayout == nullptr)
            return false;

        VkDescriptorPoolSize poolSize{};
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        void* descriptorPool = nullptr;
        if (renderer.fns.createDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS || descriptorPool == nullptr)
            return false;

        void* descriptorSet = nullptr;
        void* setLayoutForAlloc = setLayout;
        VkDescriptorSetAllocateInfo setAlloc{};
        setAlloc.descriptorPool = descriptorPool;
        setAlloc.descriptorSetCount = 1;
        setAlloc.pSetLayouts = &setLayoutForAlloc;
        if (renderer.fns.allocateDescriptorSets(device, &setAlloc, &descriptorSet) != VK_SUCCESS || descriptorSet == nullptr)
            return false;

        VkDescriptorImageInfo imageDescriptor{};
        imageDescriptor.sampler = sampler;
        imageDescriptor.imageView = textureImageView;
        imageDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet write{};
        write.dstSet = descriptorSet;
        write.descriptorCount = 1;
        write.pImageInfo = &imageDescriptor;
        renderer.fns.updateDescriptorSets(device, 1, &write, 0, nullptr);

        std::vector<std::uint8_t> vertexShader;
        std::vector<std::uint8_t> fragmentShader;
        if (!TryReadFrameShader("embervale_minimap_frame.vert.spv", vertexShader) ||
            !TryReadFrameShader("embervale_minimap_frame.frag.spv", fragmentShader))
        {
            return false;
        }

        void* vertexModule = nullptr;
        void* fragmentModule = nullptr;
        if (!TryCreateShaderModule(renderer, vertexShader, &vertexModule) ||
            !TryCreateShaderModule(renderer, fragmentShader, &fragmentModule))
        {
            if (vertexModule != nullptr)
                renderer.fns.destroyShaderModule(device, vertexModule, nullptr);
            if (fragmentModule != nullptr)
                renderer.fns.destroyShaderModule(device, fragmentModule, nullptr);
            return false;
        }

        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushRange.size = sizeof(float) * 8;
        void* setLayoutForPipeline = setLayout;
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &setLayoutForPipeline;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushRange;
        void* pipelineLayout = nullptr;
        if (renderer.fns.createPipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS || pipelineLayout == nullptr)
        {
            renderer.fns.destroyShaderModule(device, vertexModule, nullptr);
            renderer.fns.destroyShaderModule(device, fragmentModule, nullptr);
            return false;
        }

        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertexModule;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragmentModule;

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        VkViewport viewport{};
        viewport.width = static_cast<float>(renderer.width);
        viewport.height = static_cast<float>(renderer.height);
        VkRect2D scissor{};
        scissor.extent.width = renderer.width;
        scissor.extent.height = renderer.height;
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;
        VkPipelineRasterizationStateCreateInfo raster{};
        VkPipelineMultisampleStateCreateInfo multisample{};
        VkPipelineColorBlendAttachmentState blendAttachment{};
        blendAttachment.blendEnable = 1;
        blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        VkPipelineColorBlendStateCreateInfo blend{};
        blend.attachmentCount = 1;
        blend.pAttachments = &blendAttachment;

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = stages;
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &raster;
        pipelineInfo.pMultisampleState = &multisample;
        pipelineInfo.pColorBlendState = &blend;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = reinterpret_cast<void*>(renderer.renderPass);

        void* pipeline = nullptr;
        const bool pipelineOk = renderer.fns.createGraphicsPipelines(device, nullptr, 1, &pipelineInfo, nullptr, &pipeline) == VK_SUCCESS && pipeline != nullptr;
        renderer.fns.destroyShaderModule(device, vertexModule, nullptr);
        renderer.fns.destroyShaderModule(device, fragmentModule, nullptr);
        if (!pipelineOk)
            return false;

        renderer.frameStagingBuffer = reinterpret_cast<uintptr_t>(stagingBuffer);
        renderer.frameStagingMemory = reinterpret_cast<uintptr_t>(stagingMemory);
        renderer.frameTextureImage = reinterpret_cast<uintptr_t>(textureImage);
        renderer.frameTextureMemory = reinterpret_cast<uintptr_t>(textureMemory);
        renderer.frameTextureImageView = reinterpret_cast<uintptr_t>(textureImageView);
        renderer.frameSampler = reinterpret_cast<uintptr_t>(sampler);
        renderer.frameDescriptorSetLayout = reinterpret_cast<uintptr_t>(setLayout);
        renderer.frameDescriptorPool = reinterpret_cast<uintptr_t>(descriptorPool);
        renderer.frameDescriptorSet = reinterpret_cast<uintptr_t>(descriptorSet);
        renderer.framePipelineLayout = reinterpret_cast<uintptr_t>(pipelineLayout);
        renderer.framePipeline = reinterpret_cast<uintptr_t>(pipeline);
        renderer.frameTextureWidth = static_cast<std::uint32_t>(textureWidth);
        renderer.frameTextureHeight = static_cast<std::uint32_t>(textureHeight);
        renderer.frameTextureUploadPending = true;
        renderer.frameTextureReady = true;
        return true;
    }

    void DestroyVulkanMinimapRendererLocked()
    {
        void* device = reinterpret_cast<void*>(g_renderer.device);
        const VulkanRendererFns fns = g_renderer.fns;

        if (device != nullptr && fns.deviceWaitIdle != nullptr)
            fns.deviceWaitIdle(device);

        if (device != nullptr)
        {
            if (g_renderer.framePipeline != 0 && fns.destroyPipeline != nullptr)
                fns.destroyPipeline(device, reinterpret_cast<void*>(g_renderer.framePipeline), nullptr);
            if (g_renderer.framePipelineLayout != 0 && fns.destroyPipelineLayout != nullptr)
                fns.destroyPipelineLayout(device, reinterpret_cast<void*>(g_renderer.framePipelineLayout), nullptr);
            if (g_renderer.frameDescriptorPool != 0 && fns.destroyDescriptorPool != nullptr)
                fns.destroyDescriptorPool(device, reinterpret_cast<void*>(g_renderer.frameDescriptorPool), nullptr);
            if (g_renderer.frameDescriptorSetLayout != 0 && fns.destroyDescriptorSetLayout != nullptr)
                fns.destroyDescriptorSetLayout(device, reinterpret_cast<void*>(g_renderer.frameDescriptorSetLayout), nullptr);
            if (g_renderer.frameSampler != 0 && fns.destroySampler != nullptr)
                fns.destroySampler(device, reinterpret_cast<void*>(g_renderer.frameSampler), nullptr);
            if (g_renderer.frameTextureImageView != 0 && fns.destroyImageView != nullptr)
                fns.destroyImageView(device, reinterpret_cast<void*>(g_renderer.frameTextureImageView), nullptr);
            if (g_renderer.frameTextureImage != 0 && fns.destroyImage != nullptr)
                fns.destroyImage(device, reinterpret_cast<void*>(g_renderer.frameTextureImage), nullptr);
            if (g_renderer.frameTextureMemory != 0 && fns.freeMemory != nullptr)
                fns.freeMemory(device, reinterpret_cast<void*>(g_renderer.frameTextureMemory), nullptr);
            if (g_renderer.frameStagingBuffer != 0 && fns.destroyBuffer != nullptr)
                fns.destroyBuffer(device, reinterpret_cast<void*>(g_renderer.frameStagingBuffer), nullptr);
            if (g_renderer.frameStagingMemory != 0 && fns.freeMemory != nullptr)
                fns.freeMemory(device, reinterpret_cast<void*>(g_renderer.frameStagingMemory), nullptr);

            if (fns.destroySemaphore != nullptr)
            {
                for (uintptr_t semaphore : g_renderer.renderCompleteSemaphores)
                {
                    if (semaphore != 0)
                        fns.destroySemaphore(device, reinterpret_cast<void*>(semaphore), nullptr);
                }
            }

            if (fns.destroyFramebuffer != nullptr)
            {
                for (uintptr_t framebuffer : g_renderer.framebuffers)
                {
                    if (framebuffer != 0)
                        fns.destroyFramebuffer(device, reinterpret_cast<void*>(framebuffer), nullptr);
                }
            }

            if (g_renderer.renderPass != 0 && fns.destroyRenderPass != nullptr)
                fns.destroyRenderPass(device, reinterpret_cast<void*>(g_renderer.renderPass), nullptr);

            if (fns.destroyImageView != nullptr)
            {
                for (uintptr_t imageView : g_renderer.imageViews)
                {
                    if (imageView != 0)
                        fns.destroyImageView(device, reinterpret_cast<void*>(imageView), nullptr);
                }
            }

            if (g_renderer.commandPool != 0 && fns.destroyCommandPool != nullptr)
                fns.destroyCommandPool(device, reinterpret_cast<void*>(g_renderer.commandPool), nullptr);
        }

        g_renderer = VulkanMinimapRenderer{};
    }

    bool FetchRendererSwapchainImages(VulkanMinimapRenderer& renderer, const SwapchainRuntimeInfo& snapshot)
    {
        if (!snapshot.images.empty())
        {
            renderer.images = snapshot.images;
            return true;
        }

        std::uint32_t count = snapshot.imageCount;
        void* device = reinterpret_cast<void*>(renderer.device);
        void* swapchain = reinterpret_cast<void*>(renderer.swapchain);
        if (count == 0)
        {
            if (renderer.fns.getSwapchainImages(device, swapchain, &count, nullptr) != VK_SUCCESS)
                return false;
        }

        if (count == 0 || count > 16)
            return false;

        renderer.images.assign(count, 0);
        return renderer.fns.getSwapchainImages(device, swapchain, &count, renderer.images.data()) == VK_SUCCESS;
    }

    bool CreateRendererCommandPoolLocked(std::uint32_t preferredFamily)
    {
        std::vector<std::uint32_t> candidates;
        candidates.push_back(preferredFamily);
        for (std::uint32_t family = 0; family < 8; ++family)
        {
            if (std::find(candidates.begin(), candidates.end(), family) == candidates.end())
                candidates.push_back(family);
        }

        void* device = reinterpret_cast<void*>(g_renderer.device);
        for (std::uint32_t family : candidates)
        {
            VkCommandPoolCreateInfo commandPoolInfo{};
            commandPoolInfo.queueFamilyIndex = family;

            void* commandPool = nullptr;
            if (g_renderer.fns.createCommandPool(device, &commandPoolInfo, nullptr, &commandPool) == VK_SUCCESS && commandPool != nullptr)
            {
                g_renderer.commandPool = reinterpret_cast<uintptr_t>(commandPool);
                g_renderer.queueFamilyIndex = family;
                return true;
            }
        }

        return false;
    }

    bool BuildVulkanMinimapRendererLocked(const SwapchainRuntimeInfo& snapshot)
    {
        const std::uint32_t fallbackWidth = static_cast<std::uint32_t>(MaxValue(1, GetSystemMetrics(SM_CXSCREEN)));
        const std::uint32_t fallbackHeight = static_cast<std::uint32_t>(MaxValue(1, GetSystemMetrics(SM_CYSCREEN)));
        const std::uint32_t width = snapshot.width != 0 ? snapshot.width : fallbackWidth;
        const std::uint32_t height = snapshot.height != 0 ? snapshot.height : fallbackHeight;
        const std::uint32_t format = snapshot.format != 0 ? snapshot.format : 37;

        if (g_renderer.ready &&
            g_renderer.device == snapshot.device &&
            g_renderer.swapchain == snapshot.handle &&
            g_renderer.format == format &&
            g_renderer.width == width &&
            g_renderer.height == height)
        {
            return true;
        }

        DestroyVulkanMinimapRendererLocked();

        g_renderer.device = snapshot.device;
        g_renderer.swapchain = snapshot.handle;
        g_renderer.format = format;
        g_renderer.width = width;
        g_renderer.height = height;

        void* device = reinterpret_cast<void*>(g_renderer.device);
        if (device == nullptr || g_renderer.swapchain == 0 || !TryLoadVulkanRendererFns(g_renderer.fns, device))
        {
            LogRendererThrottled("[Minimap] Vulkan minimap renderer not ready: missing device functions");
            DestroyVulkanMinimapRendererLocked();
            return false;
        }

        if (!FetchRendererSwapchainImages(g_renderer, snapshot))
        {
            LogRendererThrottled("[Minimap] Vulkan minimap renderer not ready: swapchain images unavailable");
            DestroyVulkanMinimapRendererLocked();
            return false;
        }

        if (g_renderer.images.empty())
        {
            DestroyVulkanMinimapRendererLocked();
            return false;
        }

        if (!CreateRendererCommandPoolLocked(g_lastVulkanQueueFamilyIndex.load()))
        {
            LogRendererThrottled("[Minimap] Vulkan minimap renderer not ready: command pool creation failed");
            DestroyVulkanMinimapRendererLocked();
            return false;
        }

        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = g_renderer.format;
        VkAttachmentReference colorReference{};
        VkSubpassDescription subpass{};
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorReference;
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

        void* renderPass = nullptr;
        if (g_renderer.fns.createRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS || renderPass == nullptr)
        {
            LogRendererThrottled("[Minimap] Vulkan minimap renderer not ready: render pass creation failed");
            DestroyVulkanMinimapRendererLocked();
            return false;
        }
        g_renderer.renderPass = reinterpret_cast<uintptr_t>(renderPass);

        const bool texturedFrameReady = TryCreateTexturedFrameResourcesLocked(g_renderer);
        if (!texturedFrameReady)
            LogRendererThrottled("[Minimap] textured minimap frame unavailable; using clear-rect frame fallback");

        g_renderer.imageViews.assign(g_renderer.images.size(), 0);
        g_renderer.framebuffers.assign(g_renderer.images.size(), 0);
        for (std::size_t index = 0; index < g_renderer.images.size(); ++index)
        {
            VkImageViewCreateInfo imageViewInfo{};
            imageViewInfo.image = reinterpret_cast<void*>(g_renderer.images[index]);
            imageViewInfo.format = g_renderer.format;
            imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageViewInfo.subresourceRange.levelCount = 1;
            imageViewInfo.subresourceRange.layerCount = 1;

            void* imageView = nullptr;
            if (g_renderer.fns.createImageView(device, &imageViewInfo, nullptr, &imageView) != VK_SUCCESS || imageView == nullptr)
            {
                LogRendererThrottled("[Minimap] Vulkan minimap renderer not ready: image view creation failed");
                DestroyVulkanMinimapRendererLocked();
                return false;
            }
            g_renderer.imageViews[index] = reinterpret_cast<uintptr_t>(imageView);

            void* attachment = imageView;
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = &attachment;
            framebufferInfo.width = g_renderer.width;
            framebufferInfo.height = g_renderer.height;
            framebufferInfo.layers = 1;

            void* framebuffer = nullptr;
            if (g_renderer.fns.createFramebuffer(device, &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS || framebuffer == nullptr)
            {
                LogRendererThrottled("[Minimap] Vulkan minimap renderer not ready: framebuffer creation failed");
                DestroyVulkanMinimapRendererLocked();
                return false;
            }
            g_renderer.framebuffers[index] = reinterpret_cast<uintptr_t>(framebuffer);
        }

        g_renderer.commandBuffers.assign(g_renderer.images.size(), 0);
        VkCommandBufferAllocateInfo allocateInfo{};
        allocateInfo.commandPool = reinterpret_cast<void*>(g_renderer.commandPool);
        allocateInfo.commandBufferCount = static_cast<std::uint32_t>(g_renderer.commandBuffers.size());
        if (g_renderer.fns.allocateCommandBuffers(device, &allocateInfo, reinterpret_cast<void**>(g_renderer.commandBuffers.data())) != VK_SUCCESS)
        {
            LogRendererThrottled("[Minimap] Vulkan minimap renderer not ready: command buffer allocation failed");
            DestroyVulkanMinimapRendererLocked();
            return false;
        }

        g_renderer.renderCompleteSemaphores.assign(g_renderer.images.size(), 0);
        VkSemaphoreCreateInfo semaphoreInfo{};
        for (std::size_t index = 0; index < g_renderer.renderCompleteSemaphores.size(); ++index)
        {
            void* semaphore = nullptr;
            if (g_renderer.fns.createSemaphore(device, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS || semaphore == nullptr)
            {
                LogRendererThrottled("[Minimap] Vulkan minimap renderer not ready: semaphore creation failed");
                DestroyVulkanMinimapRendererLocked();
                return false;
            }
            g_renderer.renderCompleteSemaphores[index] = reinterpret_cast<uintptr_t>(semaphore);
        }

        g_renderer.ready = true;

        std::ostringstream oss;
        oss << "[Minimap] Vulkan minimap renderer ready"
            << " | swapchain=" << Hex(g_renderer.swapchain)
            << " | extent=" << g_renderer.width << "x" << g_renderer.height
            << " | format=" << g_renderer.format
            << " | images=" << g_renderer.images.size()
            << " | queue_family=" << g_renderer.queueFamilyIndex
            << " | draw=clear-attachment-widget";
        Log(oss.str());
        return true;
    }

    bool AppendClippedClearRect(VulkanMinimapRenderer& renderer, std::vector<VkClearRect>& rects, int x, int y, int width, int height)
    {
        if (width <= 0 || height <= 0)
            return false;

        const int maxWidth = static_cast<int>(renderer.width);
        const int maxHeight = static_cast<int>(renderer.height);
        if (x < 0)
        {
            width += x;
            x = 0;
        }
        if (y < 0)
        {
            height += y;
            y = 0;
        }
        if (x >= maxWidth || y >= maxHeight)
            return false;

        width = MinValue(width, maxWidth - x);
        height = MinValue(height, maxHeight - y);
        if (width <= 0 || height <= 0)
            return false;

        VkClearRect rect{};
        rect.rect.offset.x = x;
        rect.rect.offset.y = y;
        rect.rect.extent.width = static_cast<std::uint32_t>(width);
        rect.rect.extent.height = static_cast<std::uint32_t>(height);
        rects.push_back(rect);
        return true;
    }

    void CmdClearRects(VulkanMinimapRenderer& renderer, void* commandBuffer, float red, float green, float blue, float alpha, const std::vector<VkClearRect>& rects)
    {
        if (rects.empty())
            return;

        VkClearAttachment attachment{};
        attachment.clearValue.color.float32[0] = red;
        attachment.clearValue.color.float32[1] = green;
        attachment.clearValue.color.float32[2] = blue;
        attachment.clearValue.color.float32[3] = alpha;

        renderer.fns.cmdClearAttachments(commandBuffer, 1, &attachment, static_cast<std::uint32_t>(rects.size()), rects.data());
    }

    void CmdClearRect(VulkanMinimapRenderer& renderer, void* commandBuffer, float red, float green, float blue, float alpha, int x, int y, int width, int height)
    {
        if (width <= 0 || height <= 0)
            return;

        const int maxWidth = static_cast<int>(renderer.width);
        const int maxHeight = static_cast<int>(renderer.height);
        if (x < 0)
        {
            width += x;
            x = 0;
        }
        if (y < 0)
        {
            height += y;
            y = 0;
        }
        if (x >= maxWidth || y >= maxHeight)
            return;

        width = MinValue(width, maxWidth - x);
        height = MinValue(height, maxHeight - y);
        if (width <= 0 || height <= 0)
            return;

        VkClearAttachment attachment{};
        attachment.clearValue.color.float32[0] = red;
        attachment.clearValue.color.float32[1] = green;
        attachment.clearValue.color.float32[2] = blue;
        attachment.clearValue.color.float32[3] = alpha;

        VkClearRect rect{};
        rect.rect.offset.x = x;
        rect.rect.offset.y = y;
        rect.rect.extent.width = static_cast<std::uint32_t>(width);
        rect.rect.extent.height = static_cast<std::uint32_t>(height);
        renderer.fns.cmdClearAttachments(commandBuffer, 1, &attachment, 1, &rect);
    }

    void CmdClearCircle(VulkanMinimapRenderer& renderer, void* commandBuffer, float red, float green, float blue, float alpha, int cx, int cy, int radius, int stripHeight)
    {
        if (radius <= 0)
            return;

        stripHeight = MaxValue(1, stripHeight);
        const int radiusSquared = radius * radius;
        std::vector<VkClearRect> rects;
        rects.reserve(static_cast<std::size_t>((radius * 2) / stripHeight + 3));
        for (int dy = -radius; dy <= radius; dy += stripHeight)
        {
            const int sampleY = MinValue(radius, dy + (stripHeight / 2));
            const int halfWidth = static_cast<int>(std::sqrt(static_cast<float>(MaxValue(0, radiusSquared - sampleY * sampleY))));
            AppendClippedClearRect(renderer, rects, cx - halfWidth, cy + dy, halfWidth * 2 + 1, stripHeight);
        }
        CmdClearRects(renderer, commandBuffer, red, green, blue, alpha, rects);
    }

    void CmdClearDotInCircle(VulkanMinimapRenderer& renderer, void* commandBuffer, int cx, int cy, int radius, int x, int y, int size, float red, float green, float blue)
    {
        const int dx = x - cx;
        const int dy = y - cy;
        if (dx * dx + dy * dy > (radius - size) * (radius - size))
            return;

        CmdClearRect(renderer, commandBuffer, red, green, blue, 1.0f, x - size / 2, y - size / 2, size, size);
    }

    void CmdClearPolyline(VulkanMinimapRenderer& renderer, void* commandBuffer, int cx, int cy, int radius, int x0, int y0, int x1, int y1, int size, float red, float green, float blue)
    {
        const int steps = MaxValue(1, MaxValue(std::abs(x1 - x0), std::abs(y1 - y0)) / MaxValue(1, size / 2));
        for (int step = 0; step <= steps; ++step)
        {
            const float t = static_cast<float>(step) / static_cast<float>(steps);
            const int x = static_cast<int>(static_cast<float>(x0) + static_cast<float>(x1 - x0) * t);
            const int y = static_cast<int>(static_cast<float>(y0) + static_cast<float>(y1 - y0) * t);
            CmdClearDotInCircle(renderer, commandBuffer, cx, cy, radius, x, y, size, red, green, blue);
        }
    }

    void CmdClearMarker(VulkanMinimapRenderer& renderer, void* commandBuffer, int cx, int cy, int radius, int x, int y, float red, float green, float blue)
    {
        CmdClearDotInCircle(renderer, commandBuffer, cx, cy, radius, x, y - 5, 6, red, green, blue);
        CmdClearDotInCircle(renderer, commandBuffer, cx, cy, radius, x - 5, y, 6, red, green, blue);
        CmdClearDotInCircle(renderer, commandBuffer, cx, cy, radius, x, y, 8, 1.0f, 0.94f, 0.72f);
        CmdClearDotInCircle(renderer, commandBuffer, cx, cy, radius, x + 5, y, 6, red, green, blue);
        CmdClearDotInCircle(renderer, commandBuffer, cx, cy, radius, x, y + 5, 6, red, green, blue);
    }

    void CmdClearSolidDiamond(VulkanMinimapRenderer& renderer, void* commandBuffer, int x, int y, int size, float red, float green, float blue, float alpha = 1.0f)
    {
        if (size <= 0)
            return;

        std::vector<VkClearRect> rects;
        rects.reserve(static_cast<std::size_t>(size + 2));
        for (int dy = -size; dy <= size; dy += 2)
        {
            const int halfWidth = size - std::abs(dy);
            AppendClippedClearRect(renderer, rects, x - halfWidth, y + dy, halfWidth * 2 + 1, 2);
        }
        CmdClearRects(renderer, commandBuffer, red, green, blue, alpha, rects);
    }

    void CmdClearDiamond(VulkanMinimapRenderer& renderer, void* commandBuffer, int cx, int cy, int radius, int x, int y, int size, float red, float green, float blue)
    {
        CmdClearSolidDiamond(renderer, commandBuffer, x, y, size, red, green, blue);
        CmdClearDotInCircle(renderer, commandBuffer, cx, cy, radius, x, y, MaxValue(3, size / 2), 1.0f, 0.95f, 0.72f);
    }

    void CmdClearOutlinedDiamond(VulkanMinimapRenderer& renderer, void* commandBuffer, int cx, int cy, int radius, int x, int y, int size, float red, float green, float blue)
    {
        CmdClearSolidDiamond(renderer, commandBuffer, x, y, size + 2, 0.035f, 0.028f, 0.020f);
        CmdClearSolidDiamond(renderer, commandBuffer, x, y, size, red, green, blue);
        CmdClearDotInCircle(renderer, commandBuffer, cx, cy, radius, x, y, MaxValue(3, size / 2), 1.0f, 0.95f, 0.76f);
    }

    void CmdClearVerticalTriangle(VulkanMinimapRenderer& renderer, void* commandBuffer, int centerX, int tipY, int baseY, int baseHalfWidth, float red, float green, float blue)
    {
        const int height = MaxValue(1, baseY - tipY);
        std::vector<VkClearRect> rects;
        rects.reserve(static_cast<std::size_t>(height / 2 + 3));
        for (int y = tipY; y <= baseY; y += 2)
        {
            const float t = static_cast<float>(y - tipY) / static_cast<float>(height);
            const int halfWidth = MaxValue(1, static_cast<int>(t * static_cast<float>(baseHalfWidth)));
            AppendClippedClearRect(renderer, rects, centerX - halfWidth, y, halfWidth * 2 + 1, 2);
        }
        CmdClearRects(renderer, commandBuffer, red, green, blue, 1.0f, rects);
    }

    void CmdClearSmallCircle(VulkanMinimapRenderer& renderer, void* commandBuffer, int cx, int cy, int radius, int x, int y, int drawRadius, float red, float green, float blue, float alpha = 1.0f)
    {
        if (drawRadius <= 0)
            return;

        const int dx = x - cx;
        const int dy = y - cy;
        const int allowedRadius = MaxValue(1, radius - drawRadius);
        if (dx * dx + dy * dy > allowedRadius * allowedRadius)
            return;

        const int radiusSq = drawRadius * drawRadius;
        std::vector<VkClearRect> rects;
        rects.reserve(static_cast<std::size_t>(drawRadius + 2));
        for (int offsetY = -drawRadius; offsetY <= drawRadius; offsetY += 2)
        {
            const int halfWidth = static_cast<int>(std::sqrt(static_cast<float>(MaxValue(0, radiusSq - offsetY * offsetY))));
            AppendClippedClearRect(renderer, rects, x - halfWidth, y + offsetY, halfWidth * 2 + 1, 2);
        }
        CmdClearRects(renderer, commandBuffer, red, green, blue, alpha, rects);
    }

    bool TryDrawMinimapRasterIcon(VulkanMinimapRenderer& renderer, void* commandBuffer, int cx, int cy, int radius, int x, int y, std::uint32_t kind, bool clipped);
    bool HasLoadedMinimapIconAtlas();

    void CmdClearPoiBackplate(VulkanMinimapRenderer& renderer, void* commandBuffer, int cx, int cy, int radius, int x, int y, int size, float red, float green, float blue)
    {
        CmdClearSmallCircle(renderer, commandBuffer, cx, cy, radius, x + 1, y + 2, size + 5, 0.0f, 0.0f, 0.0f, 0.72f);
        CmdClearSmallCircle(renderer, commandBuffer, cx, cy, radius, x, y, size + 4, 0.055f, 0.045f, 0.034f, 0.98f);
        CmdClearSmallCircle(renderer, commandBuffer, cx, cy, radius, x, y, size + 2, red, green, blue, 0.95f);
        CmdClearSmallCircle(renderer, commandBuffer, cx, cy, radius, x, y, size, 0.055f, 0.048f, 0.040f, 0.96f);
    }

    void CmdClearQuestIcon(VulkanMinimapRenderer& renderer, void* commandBuffer, int cx, int cy, int radius, int x, int y, int size, float red, float green, float blue)
    {
        CmdClearPoiBackplate(renderer, commandBuffer, cx, cy, radius, x, y, size, red, green, blue);
        CmdClearSolidDiamond(renderer, commandBuffer, x, y, MaxValue(4, size - 1), red, green, blue);
        CmdClearRect(renderer, commandBuffer, 1.0f, 0.93f, 0.72f, 1.0f, x - 1, y - size + 2, 3, MaxValue(4, size));
        CmdClearSmallCircle(renderer, commandBuffer, cx, cy, radius, x, y + size - 2, 2, 1.0f, 0.93f, 0.72f);
    }

    void CmdClearHomeIcon(VulkanMinimapRenderer& renderer, void* commandBuffer, int cx, int cy, int radius, int x, int y, int size, float red, float green, float blue)
    {
        CmdClearPoiBackplate(renderer, commandBuffer, cx, cy, radius, x, y, size, red, green, blue);
        CmdClearVerticalTriangle(renderer, commandBuffer, x, y - size - 3, y, size, red, green, blue);
        CmdClearRect(renderer, commandBuffer, red, green, blue, 1.0f, x - size + 2, y - 1, MaxValue(3, size * 2 - 3), size);
        CmdClearRect(renderer, commandBuffer, 0.035f, 0.030f, 0.026f, 1.0f, x - 1, y + 1, 3, size - 1);
    }

    void CmdClearFlameIcon(VulkanMinimapRenderer& renderer, void* commandBuffer, int cx, int cy, int radius, int x, int y, int size)
    {
        CmdClearPoiBackplate(renderer, commandBuffer, cx, cy, radius, x, y, size, 1.0f, 0.74f, 0.10f);
        CmdClearSolidDiamond(renderer, commandBuffer, x, y + 1, MaxValue(4, size - 1), 1.0f, 0.66f, 0.08f);
        CmdClearVerticalTriangle(renderer, commandBuffer, x, y - size - 4, y + size - 1, MaxValue(4, size - 2), 1.0f, 0.80f, 0.16f);
        CmdClearVerticalTriangle(renderer, commandBuffer, x, y - size + 1, y + 2, MaxValue(2, size / 2), 1.0f, 0.96f, 0.50f);
    }

    void CmdClearSpireIcon(VulkanMinimapRenderer& renderer, void* commandBuffer, int cx, int cy, int radius, int x, int y, int size, float red, float green, float blue)
    {
        CmdClearPoiBackplate(renderer, commandBuffer, cx, cy, radius, x, y, size, red, green, blue);
        CmdClearVerticalTriangle(renderer, commandBuffer, x, y - size - 5, y + size, MaxValue(4, size - 1), red, green, blue);
        CmdClearRect(renderer, commandBuffer, 1.0f, 0.94f, 0.78f, 1.0f, x - 1, y - size + 1, 3, size * 2 - 1);
        CmdClearRect(renderer, commandBuffer, red, green, blue, 1.0f, x - size, y + size - 1, size * 2 + 1, 3);
    }

    void CmdClearMineIcon(VulkanMinimapRenderer& renderer, void* commandBuffer, int cx, int cy, int radius, int x, int y, int size)
    {
        CmdClearPoiBackplate(renderer, commandBuffer, cx, cy, radius, x, y, size, 0.98f, 0.48f, 0.62f);
        CmdClearPolyline(renderer, commandBuffer, cx, cy, radius, x - size, y + size, x + size, y - size, 3, 1.0f, 0.92f, 0.78f);
        CmdClearPolyline(renderer, commandBuffer, cx, cy, radius, x - size, y - 1, x + size, y - size - 2, 3, 0.98f, 0.56f, 0.68f);
    }

    void CmdClearCampIcon(VulkanMinimapRenderer& renderer, void* commandBuffer, int cx, int cy, int radius, int x, int y, int size)
    {
        CmdClearPoiBackplate(renderer, commandBuffer, cx, cy, radius, x, y, size, 0.90f, 0.66f, 0.36f);
        CmdClearVerticalTriangle(renderer, commandBuffer, x, y - size - 1, y + size, size, 0.98f, 0.92f, 0.78f);
        CmdClearVerticalTriangle(renderer, commandBuffer, x, y, y + size, MaxValue(2, size / 2), 0.055f, 0.045f, 0.034f);
    }

    void CmdClearDangerIcon(VulkanMinimapRenderer& renderer, void* commandBuffer, int cx, int cy, int radius, int x, int y, int size, float red, float green, float blue)
    {
        CmdClearPoiBackplate(renderer, commandBuffer, cx, cy, radius, x, y, size, red, green, blue);
        CmdClearPolyline(renderer, commandBuffer, cx, cy, radius, x - size, y - size, x + size, y + size, 4, red, green, blue);
        CmdClearPolyline(renderer, commandBuffer, cx, cy, radius, x - size, y + size, x + size, y - size, 4, red, green, blue);
        CmdClearSmallCircle(renderer, commandBuffer, cx, cy, radius, x, y, 2, 1.0f, 0.92f, 0.76f);
    }

    void CmdClearWaterIcon(VulkanMinimapRenderer& renderer, void* commandBuffer, int cx, int cy, int radius, int x, int y, int size)
    {
        CmdClearPoiBackplate(renderer, commandBuffer, cx, cy, radius, x, y, size, 0.22f, 0.78f, 1.0f);
        CmdClearSmallCircle(renderer, commandBuffer, cx, cy, radius, x, y + 1, MaxValue(3, size - 2), 0.22f, 0.78f, 1.0f);
        CmdClearVerticalTriangle(renderer, commandBuffer, x, y - size - 3, y + 2, MaxValue(3, size - 3), 0.70f, 0.96f, 1.0f);
    }

    void CmdClearGenericPoiIcon(VulkanMinimapRenderer& renderer, void* commandBuffer, int cx, int cy, int radius, int x, int y, int size, float red, float green, float blue)
    {
        CmdClearPoiBackplate(renderer, commandBuffer, cx, cy, radius, x, y, size, red, green, blue);
        CmdClearOutlinedDiamond(renderer, commandBuffer, cx, cy, radius, x, y, MaxValue(3, size - 2), red, green, blue);
    }

    void CmdClearPoiIcon(VulkanMinimapRenderer& renderer, void* commandBuffer, int cx, int cy, int radius, int x, int y, std::uint32_t kind, bool clipped)
    {
        if (TryDrawMinimapRasterIcon(renderer, commandBuffer, cx, cy, radius, x, y, kind, clipped))
            return;

        const int size = clipped ? 5 : 7;
        if (kind == MAP_MARKER_QUEST_IMPORTANT_KIND)
        {
            CmdClearQuestIcon(renderer, commandBuffer, cx, cy, radius, x, y, clipped ? 8 : 12, 1.0f, 0.74f, 0.10f);
            return;
        }

        if (kind == MAP_MARKER_QUEST_KIND)
        {
            CmdClearQuestIcon(renderer, commandBuffer, cx, cy, radius, x, y, clipped ? 6 : 9, 1.0f, 0.86f, 0.18f);
            return;
        }

        if (kind > 0xFFFFu && HasLoadedMinimapIconAtlas())
            return;

        switch (kind)
        {
        case 10:
            CmdClearQuestIcon(renderer, commandBuffer, cx, cy, radius, x, y, clipped ? 7 : 10, 1.0f, 0.72f, 0.10f);
            break;
        case 2:
        case 40:
            CmdClearQuestIcon(renderer, commandBuffer, cx, cy, radius, x, y, size, 1.0f, 0.24f, 0.18f);
            break;
        case 3:
        case 30:
        case 42:
        case 47:
            CmdClearHomeIcon(renderer, commandBuffer, cx, cy, radius, x, y, size, 0.30f, 0.88f, 1.0f);
            break;
        case 31:
        case 32:
        case 44:
        case 53:
            CmdClearFlameIcon(renderer, commandBuffer, cx, cy, radius, x, y, size);
            break;
        case 33:
        case 45:
        case 54:
            CmdClearWaterIcon(renderer, commandBuffer, cx, cy, radius, x, y, size);
            break;
        case 34:
        case 43:
        case 49:
        case 50:
            CmdClearSpireIcon(renderer, commandBuffer, cx, cy, radius, x, y, size, 0.72f, 0.36f, 0.98f);
            break;
        case 41:
            CmdClearDangerIcon(renderer, commandBuffer, cx, cy, radius, x, y, size, 0.98f, 0.42f, 0.16f);
            break;
        case 46:
            CmdClearMineIcon(renderer, commandBuffer, cx, cy, radius, x, y, size);
            break;
        case 48:
            CmdClearSpireIcon(renderer, commandBuffer, cx, cy, radius, x, y, size, 0.95f, 0.62f, 0.18f);
            break;
        case 51:
            CmdClearCampIcon(renderer, commandBuffer, cx, cy, radius, x, y, size);
            break;
        case 52:
            CmdClearDangerIcon(renderer, commandBuffer, cx, cy, radius, x, y, size, 1.0f, 0.22f, 0.30f);
            break;
        case 55:
            CmdClearGenericPoiIcon(renderer, commandBuffer, cx, cy, radius, x, y, size, 0.84f, 0.78f, 0.58f);
            break;
        default:
            CmdClearGenericPoiIcon(renderer, commandBuffer, cx, cy, radius, x, y, size, 0.78f, 0.60f, 1.0f);
            break;
        }
    }

    void EnsurePlayerArrowLoaded();

    void CmdClearPlayerArrow(VulkanMinimapRenderer& renderer, void* commandBuffer, int cx, int cy, int radius)
    {
        CmdClearSmallCircle(renderer, commandBuffer, cx, cy, radius, cx, cy, 12, 0.02f, 0.13f, 0.16f, 0.34f);
        CmdClearSmallCircle(renderer, commandBuffer, cx, cy, radius, cx, cy + 1, 8, 0.0f, 0.0f, 0.0f, 0.52f);
        CmdClearVerticalTriangle(renderer, commandBuffer, cx, cy - 17, cy + 9, 9, 0.030f, 0.020f, 0.014f);
        CmdClearVerticalTriangle(renderer, commandBuffer, cx, cy - 14, cy + 6, 6, 0.98f, 0.88f, 0.62f);
        CmdClearVerticalTriangle(renderer, commandBuffer, cx, cy - 10, cy + 1, 4, 1.0f, 0.98f, 0.84f);
        CmdClearVerticalTriangle(renderer, commandBuffer, cx, cy + 1, cy + 6, 2, 0.33f, 0.92f, 1.0f);
        CmdClearSmallCircle(renderer, commandBuffer, cx, cy, radius - 10, cx, cy + 4, 1, 0.035f, 0.026f, 0.018f);
    }

    bool TryDrawPremiumPlayerArrow(VulkanMinimapRenderer& renderer, void* commandBuffer, int cx, int cy, int radius)
    {
        EnsurePlayerArrowLoaded();

        std::lock_guard<std::mutex> lock(g_playerArrowMutex);
        if (!g_playerArrow.loaded || g_playerArrow.rgba.empty() || g_playerArrow.width <= 0 || g_playerArrow.height <= 0)
            return false;

        const int targetSize = ClampValue(radius / 4, 26, 34);
        const int left = cx - targetSize / 2;
        const int top = cy - targetSize / 2;
        const int clipRadius = MaxValue(1, radius - 18);
        const int clipRadiusSq = clipRadius * clipRadius;

        for (int dstY = 0; dstY < targetSize; ++dstY)
        {
            const int screenY = top + dstY;
            const int dy = screenY - cy;
            const int srcY = MinValue(
                g_playerArrow.height - 1,
                static_cast<int>((static_cast<std::int64_t>(dstY) * g_playerArrow.height) / targetSize));
            int runStart = -1;
            float runRed = 0.0f;
            float runGreen = 0.0f;
            float runBlue = 0.0f;

            const auto flushRun = [&](int endX)
            {
                if (runStart < 0 || endX <= runStart)
                    return;

                CmdClearRect(renderer, commandBuffer, runRed, runGreen, runBlue, 1.0f, runStart, screenY, endX - runStart, 1);
                runStart = -1;
            };

            for (int dstX = 0; dstX < targetSize; ++dstX)
            {
                const int screenX = left + dstX;
                const int dx = screenX - cx;
                if (dx * dx + dy * dy > clipRadiusSq)
                {
                    flushRun(screenX);
                    continue;
                }

                const int srcX = MinValue(
                    g_playerArrow.width - 1,
                    static_cast<int>((static_cast<std::int64_t>(dstX) * g_playerArrow.width) / targetSize));
                const std::size_t offset =
                    (static_cast<std::size_t>(srcY) * static_cast<std::size_t>(g_playerArrow.width) + static_cast<std::size_t>(srcX)) *
                    4u;
                if (g_playerArrow.rgba[offset + 3] <= 48)
                {
                    flushRun(screenX);
                    continue;
                }

                const float red = static_cast<float>((g_playerArrow.rgba[offset + 0] / 8u) * 8u) / 255.0f;
                const float green = static_cast<float>((g_playerArrow.rgba[offset + 1] / 8u) * 8u) / 255.0f;
                const float blue = static_cast<float>((g_playerArrow.rgba[offset + 2] / 8u) * 8u) / 255.0f;

                if (runStart >= 0 &&
                    std::fabs(runRed - red) < 0.020f &&
                    std::fabs(runGreen - green) < 0.020f &&
                    std::fabs(runBlue - blue) < 0.020f)
                {
                    continue;
                }

                flushRun(screenX);
                runStart = screenX;
                runRed = red;
                runGreen = green;
                runBlue = blue;
            }

            flushRun(left + targetSize);
        }

        return true;
    }

    std::vector<CapturedWaypoint> CopyWaypoints()
    {
        std::lock_guard<std::mutex> lock(g_waypointMutex);
        return g_waypoints;
    }

    std::vector<CapturedNearbyMarker> CopyNearbyMarkers()
    {
        std::lock_guard<std::mutex> lock(g_nearbyMarkerMutex);
        return g_nearbyMarkers;
    }

    std::vector<CapturedMapMarkerVisibility> CopyVisibleMapMarkers()
    {
        const DWORD now = GetTickCount();
        std::lock_guard<std::mutex> lock(g_visibleMapMarkerMutex);

        std::vector<CapturedMapMarkerVisibility> result;
        result.reserve(g_visibleMapMarkers.size());
        for (const CapturedMapMarkerVisibility& marker : g_visibleMapMarkers)
        {
            if (marker.visibility == 0 || now - marker.lastUpdateTick > VISIBLE_MAP_MARKER_STALE_MS)
                continue;

            result.push_back(marker);
        }

        return result;
    }

    std::string JoinPath(const std::string& base, const std::string& name)
    {
        if (base.empty())
            return name;

        const char last = base.back();
        if (last == '\\' || last == '/')
            return base + name;

        return base + "\\" + name;
    }

    std::string GetExecutableDirectory()
    {
        char path[MAX_PATH] = {};
        const DWORD length = GetModuleFileNameA(nullptr, path, static_cast<DWORD>(sizeof(path)));
        if (length == 0 || length >= sizeof(path))
            return {};

        char* slash = std::strrchr(path, '\\');
        if (slash != nullptr)
            *slash = '\0';

        return path;
    }

    bool TryResolveRgbaMapDimensions(std::streamoff byteSize, int& outTextureSize)
    {
        if (byteSize <= 0 || (byteSize % 4) != 0)
            return false;

        const std::uint64_t pixelCount = static_cast<std::uint64_t>(byteSize / 4);
        const int textureSize = static_cast<int>(std::sqrt(static_cast<double>(pixelCount)) + 0.5);
        if (textureSize < REAL_MAP_MIN_TEXTURE_SIZE || textureSize > REAL_MAP_MAX_TEXTURE_SIZE)
            return false;

        if (static_cast<std::uint64_t>(textureSize) * static_cast<std::uint64_t>(textureSize) != pixelCount)
            return false;

        outTextureSize = textureSize;
        return true;
    }

    void AddRealMapCandidates(std::vector<std::string>& candidates, const std::string& directory)
    {
        const char* names[] = {
            "embervale_realmap_1280.rgba",
            "embervale_realmap_1024.rgba",
            "embervale_realmap_768.rgba",
            "embervale_realmap_512.rgba"
        };

        for (const char* name : names)
            candidates.push_back(JoinPath(directory, name));
    }

    bool TryLoadRealMapFromPath(const std::string& path, RealMapTexture& map)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
            return false;

        const std::streamoff size = file.tellg();
        int textureSize = 0;
        if (!TryResolveRgbaMapDimensions(size, textureSize))
            return false;

        file.seekg(0, std::ios::beg);
        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(size));
        if (!file.read(reinterpret_cast<char*>(pixels.data()), pixels.size()))
            return false;

        map.loaded = true;
        map.width = textureSize;
        map.height = textureSize;
        map.path = path;
        map.rgba = std::move(pixels);
        return true;
    }

    void EnsureRealMapLoaded()
    {
        std::lock_guard<std::mutex> lock(g_realMapMutex);
        if (g_realMap.attempted)
            return;

        g_realMap.attempted = true;

        std::vector<std::string> candidates;
        if (g_modContext != nullptr && !g_modContext->shroudtopia.mod_folder.empty())
        {
            const std::string modRoot = JoinPath(g_modContext->shroudtopia.mod_folder, "minimap_mod");
            AddRealMapCandidates(candidates, modRoot);
            AddRealMapCandidates(candidates, JoinPath(modRoot, "assets"));
        }

        const std::string gameDir = GetExecutableDirectory();
        if (!gameDir.empty())
        {
            const std::string installedModRoot = JoinPath(JoinPath(JoinPath(gameDir, "mods"), "minimap_mod"), "");
            AddRealMapCandidates(candidates, installedModRoot);
            AddRealMapCandidates(candidates, JoinPath(installedModRoot, "assets"));
        }

        AddRealMapCandidates(candidates, "");

        for (const std::string& path : candidates)
        {
            if (TryLoadRealMapFromPath(path, g_realMap))
            {
                std::ostringstream oss;
                oss << "[Minimap] loaded real Embervale map texture"
                    << " | path=" << path
                    << " | size=" << g_realMap.width << "x" << g_realMap.height
                    << " | source=UiMapResource 01bcbd07-bdbf-41c0-9999-5d58fb1a3aa1";
                Log(oss.str());
                return;
            }
        }

        Log("[Minimap] real Embervale map texture missing; fake procedural map is disabled");
    }

    bool TryLoadStaticPoisFromPath(const std::string& path, StaticPoiCatalog& catalog)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
            return false;

        const std::streamoff size = file.tellg();
        if (size < 12)
            return false;

        file.seekg(0, std::ios::beg);
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        if (!file.read(reinterpret_cast<char*>(bytes.data()), bytes.size()))
            return false;

        const char expectedMagicV1[8] = { 'E', 'M', 'P', 'O', 'I', '0', '0', '1' };
        const char expectedMagicV2[8] = { 'E', 'M', 'P', 'O', 'I', '0', '0', '2' };
        if (std::memcmp(bytes.data(), expectedMagicV1, sizeof(expectedMagicV1)) != 0 &&
            std::memcmp(bytes.data(), expectedMagicV2, sizeof(expectedMagicV2)) != 0)
        {
            return false;
        }

        std::uint32_t count = 0;
        std::memcpy(&count, bytes.data() + 8, sizeof(count));
        if (count == 0 || count > STATIC_POI_MAX_ENTRIES)
            return false;

        const std::size_t expectedSize = 12 + static_cast<std::size_t>(count) * STATIC_POI_ENTRY_SIZE;
        if (bytes.size() != expectedSize)
            return false;

        std::vector<StaticPoi> pois;
        pois.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index)
        {
            const std::uint8_t* entry = bytes.data() + 12 + static_cast<std::size_t>(index) * STATIC_POI_ENTRY_SIZE;
            StaticPoi poi{};
            std::memcpy(&poi.x, entry + 0, sizeof(poi.x));
            std::memcpy(&poi.y, entry + 4, sizeof(poi.y));
            std::memcpy(&poi.z, entry + 8, sizeof(poi.z));
            std::memcpy(&poi.kind, entry + 12, sizeof(poi.kind));

            if (!std::isfinite(poi.x) || !std::isfinite(poi.y) || !std::isfinite(poi.z))
                continue;

            if (poi.x < -512.0f || poi.z < -512.0f ||
                poi.x > REAL_MAP_WORLD_SIZE + 768.0f ||
                poi.z > REAL_MAP_WORLD_SIZE + 768.0f)
            {
                continue;
            }

            pois.push_back(poi);
        }

        if (pois.empty())
            return false;

        catalog.loaded = true;
        catalog.path = path;
        catalog.pois = std::move(pois);
        return true;
    }

    void EnsureStaticPoisLoaded()
    {
        std::lock_guard<std::mutex> lock(g_staticPoiMutex);
        if (g_staticPois.attempted)
            return;

        g_staticPois.attempted = true;

        std::vector<std::string> candidates;
        if (g_modContext != nullptr && !g_modContext->shroudtopia.mod_folder.empty())
        {
            const std::string modRoot = JoinPath(g_modContext->shroudtopia.mod_folder, "minimap_mod");
            candidates.push_back(JoinPath(modRoot, "embervale_pois.bin"));
            candidates.push_back(JoinPath(JoinPath(modRoot, "assets"), "embervale_pois.bin"));
        }

        const std::string gameDir = GetExecutableDirectory();
        if (!gameDir.empty())
        {
            candidates.push_back(JoinPath(JoinPath(JoinPath(gameDir, "mods"), "minimap_mod"), "embervale_pois.bin"));
            candidates.push_back(JoinPath(JoinPath(JoinPath(JoinPath(gameDir, "mods"), "minimap_mod"), "assets"), "embervale_pois.bin"));
        }

        candidates.push_back("embervale_pois.bin");

        for (const std::string& path : candidates)
        {
            if (TryLoadStaticPoisFromPath(path, g_staticPois))
            {
                std::ostringstream oss;
                oss << "[Minimap] loaded real Embervale POI catalog"
                    << " | path=" << path
                    << " | count=" << g_staticPois.pois.size()
                    << " | source=SceneResource/MapMarker asset extraction";
                Log(oss.str());
                return;
            }
        }

        Log("[Minimap] real Embervale POI catalog missing; runtime marker feed only");
    }

    bool TryLoadFogOfWarFromPath(const std::string& path, FogOfWarMask& mask)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
            return false;

        const std::streamoff size = file.tellg();
        if (size < 16)
            return false;

        file.seekg(0, std::ios::beg);
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        if (!file.read(reinterpret_cast<char*>(bytes.data()), bytes.size()))
            return false;

        const char expectedMagic[8] = { 'E', 'M', 'F', 'O', 'W', '0', '0', '1' };
        if (std::memcmp(bytes.data(), expectedMagic, sizeof(expectedMagic)) != 0)
            return false;

        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::memcpy(&width, bytes.data() + 8, sizeof(width));
        std::memcpy(&height, bytes.data() + 12, sizeof(height));
        if (width < 16 || height < 16 || width > 2048 || height > 2048)
            return false;

        const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        if (bytes.size() != 16 + pixelCount)
            return false;

        mask.attempted = true;
        mask.loaded = true;
        mask.width = static_cast<int>(width);
        mask.height = static_cast<int>(height);
        mask.path = path;
        mask.values.assign(bytes.begin() + 16, bytes.end());
        return true;
    }

    void EnsureFogOfWarLoaded()
    {
        std::lock_guard<std::mutex> lock(g_fogOfWarMutex);
        if (g_fogOfWar.attempted)
            return;

        g_fogOfWar.attempted = true;

        std::vector<std::string> candidates;
        if (g_modContext != nullptr && !g_modContext->shroudtopia.mod_folder.empty())
        {
            const std::string modRoot = JoinPath(g_modContext->shroudtopia.mod_folder, "minimap_mod");
            candidates.push_back(JoinPath(modRoot, "embervale_fowr.bin"));
            candidates.push_back(JoinPath(JoinPath(modRoot, "assets"), "embervale_fowr.bin"));
        }

        const std::string gameDir = GetExecutableDirectory();
        if (!gameDir.empty())
        {
            candidates.push_back(JoinPath(JoinPath(JoinPath(gameDir, "mods"), "minimap_mod"), "embervale_fowr.bin"));
            candidates.push_back(JoinPath(JoinPath(JoinPath(JoinPath(gameDir, "mods"), "minimap_mod"), "assets"), "embervale_fowr.bin"));
        }

        candidates.push_back("embervale_fowr.bin");

        for (const std::string& path : candidates)
        {
            if (TryLoadFogOfWarFromPath(path, g_fogOfWar))
            {
                std::ostringstream oss;
                oss << "[Minimap] loaded Embervale fog-of-war mask"
                    << " | path=" << path
                    << " | size=" << g_fogOfWar.width << "x" << g_fogOfWar.height
                    << " | source=FOWR save blob";
                Log(oss.str());
                return;
            }
        }

        Log("[Minimap] fog-of-war mask missing; static POI visibility cannot be save-filtered");
    }

    bool IsWorldPointRevealedByFogOfWar(float worldX, float worldZ)
    {
        EnsureFogOfWarLoaded();

        std::lock_guard<std::mutex> lock(g_fogOfWarMutex);
        if (!g_fogOfWar.loaded || g_fogOfWar.values.empty() || g_fogOfWar.width <= 0 || g_fogOfWar.height <= 0)
            return true;

        const int baseX = ClampValue(static_cast<int>((worldX / REAL_MAP_WORLD_SIZE) * static_cast<float>(g_fogOfWar.width)), 0, g_fogOfWar.width - 1);
        const int baseY = ClampValue(static_cast<int>((worldZ / REAL_MAP_WORLD_SIZE) * static_cast<float>(g_fogOfWar.height)), 0, g_fogOfWar.height - 1);

        std::uint8_t best = 0;
        for (int dy = -1; dy <= 1; ++dy)
        {
            const int y = ClampValue(baseY + dy, 0, g_fogOfWar.height - 1);
            for (int dx = -1; dx <= 1; ++dx)
            {
                const int x = ClampValue(baseX + dx, 0, g_fogOfWar.width - 1);
                const std::size_t offset = static_cast<std::size_t>(y) * static_cast<std::size_t>(g_fogOfWar.width) + static_cast<std::size_t>(x);
                best = MaxValue(best, g_fogOfWar.values[offset]);
            }
        }

        return best >= 12;
    }

    void AddMinimapIconCandidates(std::vector<std::string>& candidates, const std::string& directory)
    {
        candidates.push_back(JoinPath(directory, "embervale_minimap_icons.bin"));
    }

    void AddPlayerArrowCandidates(std::vector<std::string>& candidates, const std::string& directory)
    {
        candidates.push_back(JoinPath(directory, "embervale_player_arrow.rgba"));
    }

    bool TryLoadMinimapIconsFromPath(const std::string& path, MinimapIconAtlas& atlas)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
            return false;

        const std::streamoff fileSize = file.tellg();
        if (fileSize < 12)
            return false;

        file.seekg(0, std::ios::beg);
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(fileSize));
        if (!file.read(reinterpret_cast<char*>(bytes.data()), bytes.size()))
            return false;

        const char expectedMagic[8] = { 'E', 'M', 'I', 'C', 'O', '0', '0', '1' };
        if (std::memcmp(bytes.data(), expectedMagic, sizeof(expectedMagic)) != 0)
            return false;

        std::uint32_t count = 0;
        std::memcpy(&count, bytes.data() + 8, sizeof(count));
        if (count == 0 || count > MINIMAP_ICON_MAX_ENTRIES)
            return false;

        std::size_t cursor = 12;
        std::vector<MinimapIcon> icons;
        icons.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index)
        {
            if (cursor + 12 > bytes.size())
                return false;

            MinimapIcon icon{};
            std::memcpy(&icon.key, bytes.data() + cursor + 0, sizeof(icon.key));
            std::memcpy(&icon.width, bytes.data() + cursor + 4, sizeof(icon.width));
            std::memcpy(&icon.height, bytes.data() + cursor + 8, sizeof(icon.height));
            cursor += 12;

            if (icon.width < MINIMAP_ICON_MIN_SIZE || icon.width > MINIMAP_ICON_MAX_SIZE ||
                icon.height < MINIMAP_ICON_MIN_SIZE || icon.height > MINIMAP_ICON_MAX_SIZE)
            {
                return false;
            }

            const std::size_t pixelBytes =
                static_cast<std::size_t>(icon.width) *
                static_cast<std::size_t>(icon.height) *
                4u;
            if (pixelBytes == 0 || cursor + pixelBytes > bytes.size())
                return false;

            icon.rgba.assign(bytes.begin() + static_cast<std::ptrdiff_t>(cursor), bytes.begin() + static_cast<std::ptrdiff_t>(cursor + pixelBytes));
            cursor += pixelBytes;
            icons.push_back(std::move(icon));
        }

        if (cursor != bytes.size() || icons.empty())
            return false;

        atlas.loaded = true;
        atlas.path = path;
        atlas.icons = std::move(icons);
        return true;
    }

    void EnsureMinimapIconsLoaded()
    {
        std::lock_guard<std::mutex> lock(g_minimapIconMutex);
        if (g_minimapIcons.attempted)
            return;

        g_minimapIcons.attempted = true;

        std::vector<std::string> candidates;
        if (g_modContext != nullptr && !g_modContext->shroudtopia.mod_folder.empty())
        {
            const std::string modRoot = JoinPath(g_modContext->shroudtopia.mod_folder, "minimap_mod");
            AddMinimapIconCandidates(candidates, modRoot);
            AddMinimapIconCandidates(candidates, JoinPath(modRoot, "assets"));
        }

        const std::string gameDir = GetExecutableDirectory();
        if (!gameDir.empty())
        {
            const std::string installedModRoot = JoinPath(JoinPath(JoinPath(gameDir, "mods"), "minimap_mod"), "");
            AddMinimapIconCandidates(candidates, installedModRoot);
            AddMinimapIconCandidates(candidates, JoinPath(installedModRoot, "assets"));
        }

        AddMinimapIconCandidates(candidates, "");

        for (const std::string& path : candidates)
        {
            if (TryLoadMinimapIconsFromPath(path, g_minimapIcons))
            {
                std::ostringstream oss;
                oss << "[Minimap] loaded real map marker icon atlas"
                    << " | path=" << path
                    << " | count=" << g_minimapIcons.icons.size()
                    << " | source=MapMarkerRegistryResource/UiTextureResource";
                Log(oss.str());
                return;
            }
        }

        Log("[Minimap] real marker icon atlas missing; legacy primitive marker fallback remains active");
    }

    bool TryLoadPlayerArrowFromPath(const std::string& path, RealMapTexture& arrow)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
            return false;

        const std::streamoff size = file.tellg();
        int textureSize = 0;
        if (!TryResolveRgbaMapDimensions(size, textureSize))
            return false;

        file.seekg(0, std::ios::beg);
        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(size));
        if (!file.read(reinterpret_cast<char*>(pixels.data()), pixels.size()))
            return false;

        arrow.loaded = true;
        arrow.width = textureSize;
        arrow.height = textureSize;
        arrow.path = path;
        arrow.rgba = std::move(pixels);
        return true;
    }

    void EnsurePlayerArrowLoaded()
    {
        std::lock_guard<std::mutex> lock(g_playerArrowMutex);
        if (g_playerArrow.attempted)
            return;

        g_playerArrow.attempted = true;

        std::vector<std::string> candidates;
        if (g_modContext != nullptr && !g_modContext->shroudtopia.mod_folder.empty())
        {
            const std::string modRoot = JoinPath(g_modContext->shroudtopia.mod_folder, "minimap_mod");
            AddPlayerArrowCandidates(candidates, modRoot);
            AddPlayerArrowCandidates(candidates, JoinPath(modRoot, "assets"));
        }

        const std::string gameDir = GetExecutableDirectory();
        if (!gameDir.empty())
        {
            const std::string installedModRoot = JoinPath(JoinPath(JoinPath(gameDir, "mods"), "minimap_mod"), "");
            AddPlayerArrowCandidates(candidates, installedModRoot);
            AddPlayerArrowCandidates(candidates, JoinPath(installedModRoot, "assets"));
        }

        AddPlayerArrowCandidates(candidates, "");

        for (const std::string& path : candidates)
        {
            if (TryLoadPlayerArrowFromPath(path, g_playerArrow))
            {
                std::ostringstream oss;
                oss << "[Minimap] loaded premium player arrow"
                    << " | path=" << path
                    << " | size=" << g_playerArrow.width << "x" << g_playerArrow.height;
                Log(oss.str());
                return;
            }
        }

        Log("[Minimap] premium player arrow missing; vector fallback enabled");
    }

    bool TryResolveFrameRgbaDimensions(std::streamoff byteSize, int& outTextureSize)
    {
        if (byteSize <= 0 || (byteSize % 4) != 0)
            return false;

        const std::uint64_t pixelCount = static_cast<std::uint64_t>(byteSize / 4);
        const int textureSize = static_cast<int>(std::sqrt(static_cast<double>(pixelCount)) + 0.5);
        if (textureSize < MINIMAP_FRAME_MIN_SIZE || textureSize > MINIMAP_FRAME_MAX_SIZE)
            return false;

        if (static_cast<std::uint64_t>(textureSize) * static_cast<std::uint64_t>(textureSize) != pixelCount)
            return false;

        outTextureSize = textureSize;
        return true;
    }

    void AddMinimapFrameCandidates(std::vector<std::string>& candidates, const std::string& directory)
    {
        const char* names[] = {
            "embervale_minimap_frame.rgba",
            "embervale_minimap_frame_512.rgba",
            "embervale_minimap_frame_384.rgba",
            "embervale_minimap_frame_320.rgba"
        };

        for (const char* name : names)
            candidates.push_back(JoinPath(directory, name));
    }

    bool TryLoadMinimapFrameFromPath(const std::string& path, MinimapFrameAsset& asset)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
            return false;

        const std::streamoff fileSize = file.tellg();
        int textureSize = 0;
        if (!TryResolveFrameRgbaDimensions(fileSize, textureSize))
            return false;

        file.seekg(0, std::ios::beg);
        std::vector<std::uint8_t> rgba(static_cast<std::size_t>(fileSize));
        if (!file.read(reinterpret_cast<char*>(rgba.data()), rgba.size()))
            return false;

        constexpr int kColorLevels = 64;
        constexpr int kColorMapSize = kColorLevels * kColorLevels * kColorLevels;
        std::vector<int> colorToGroup(kColorMapSize, -1);
        std::vector<MinimapFrameColorGroup> groups;
        groups.reserve(96);

        const auto quantize = [](std::uint8_t value) -> std::uint8_t
        {
            return static_cast<std::uint8_t>(value >> 2);
        };
        const auto expand = [](std::uint8_t value) -> std::uint8_t
        {
            return static_cast<std::uint8_t>((static_cast<unsigned int>(value) * 255u + 31u) / 63u);
        };
        const auto makeKey = [](std::uint8_t red, std::uint8_t green, std::uint8_t blue) -> int
        {
            return (static_cast<int>(red) << 12) | (static_cast<int>(green) << 6) | static_cast<int>(blue);
        };
        const auto appendRun = [&](int y, int startX, int endX, int colorKey)
        {
            if (endX <= startX || colorKey < 0 || colorKey >= kColorMapSize)
                return;

            int groupIndex = colorToGroup[static_cast<std::size_t>(colorKey)];
            if (groupIndex < 0)
            {
                const std::uint8_t red = static_cast<std::uint8_t>((colorKey >> 12) & 0x3F);
                const std::uint8_t green = static_cast<std::uint8_t>((colorKey >> 6) & 0x3F);
                const std::uint8_t blue = static_cast<std::uint8_t>(colorKey & 0x3F);

                MinimapFrameColorGroup group{};
                group.red = expand(red);
                group.green = expand(green);
                group.blue = expand(blue);
                groups.push_back(std::move(group));
                groupIndex = static_cast<int>(groups.size() - 1);
                colorToGroup[static_cast<std::size_t>(colorKey)] = groupIndex;
            }

            MinimapFrameRun run{};
            run.x = static_cast<std::uint16_t>(startX);
            run.y = static_cast<std::uint16_t>(y);
            run.width = static_cast<std::uint16_t>(endX - startX);
            groups[static_cast<std::size_t>(groupIndex)].runs.push_back(run);
        };

        for (int y = 0; y < textureSize; ++y)
        {
            int runStart = -1;
            int runKey = -1;
            for (int x = 0; x < textureSize; ++x)
            {
                const std::size_t offset =
                    (static_cast<std::size_t>(y) * static_cast<std::size_t>(textureSize) + static_cast<std::size_t>(x)) *
                    4u;
                const std::uint8_t alpha = rgba[offset + 3];
                if (alpha <= 40)
                {
                    appendRun(y, runStart, x, runKey);
                    runStart = -1;
                    runKey = -1;
                    continue;
                }

                const int colorKey = makeKey(
                    quantize(rgba[offset + 0]),
                    quantize(rgba[offset + 1]),
                    quantize(rgba[offset + 2]));

                if (runStart >= 0 && runKey == colorKey)
                    continue;

                appendRun(y, runStart, x, runKey);
                runStart = x;
                runKey = colorKey;
            }
            appendRun(y, runStart, textureSize, runKey);
        }

        if (groups.empty())
            return false;

        asset.loaded = true;
        asset.width = textureSize;
        asset.height = textureSize;
        asset.path = path;
        asset.rgba = std::move(rgba);
        asset.groups = std::move(groups);
        return true;
    }

    void EnsureMinimapFrameLoaded()
    {
        std::lock_guard<std::mutex> lock(g_minimapFrameMutex);
        if (g_minimapFrame.attempted)
            return;

        g_minimapFrame.attempted = true;

        std::vector<std::string> candidates;
        if (g_modContext != nullptr && !g_modContext->shroudtopia.mod_folder.empty())
        {
            const std::string modRoot = JoinPath(g_modContext->shroudtopia.mod_folder, "minimap_mod");
            AddMinimapFrameCandidates(candidates, modRoot);
            AddMinimapFrameCandidates(candidates, JoinPath(modRoot, "assets"));
        }

        const std::string gameDir = GetExecutableDirectory();
        if (!gameDir.empty())
        {
            const std::string installedModRoot = JoinPath(JoinPath(JoinPath(gameDir, "mods"), "minimap_mod"), "");
            AddMinimapFrameCandidates(candidates, installedModRoot);
            AddMinimapFrameCandidates(candidates, JoinPath(installedModRoot, "assets"));
        }

        AddMinimapFrameCandidates(candidates, "");

        for (const std::string& path : candidates)
        {
            if (TryLoadMinimapFrameFromPath(path, g_minimapFrame))
            {
                std::size_t runCount = 0;
                for (const MinimapFrameColorGroup& group : g_minimapFrame.groups)
                    runCount += group.runs.size();

                std::ostringstream oss;
                oss << "[Minimap] loaded raster minimap frame"
                    << " | path=" << path
                    << " | size=" << g_minimapFrame.width << "x" << g_minimapFrame.height
                    << " | colors=" << g_minimapFrame.groups.size()
                    << " | runs=" << runCount;
                Log(oss.str());
                return;
            }
        }

        Log("[Minimap] raster minimap frame missing; using vector fallback frame");
    }

    bool HasLoadedMinimapFrame()
    {
        EnsureMinimapFrameLoaded();
        std::lock_guard<std::mutex> lock(g_minimapFrameMutex);
        return g_minimapFrame.loaded;
    }

    bool HasLoadedMinimapIconAtlas()
    {
        std::lock_guard<std::mutex> lock(g_minimapIconMutex);
        return g_minimapIcons.loaded;
    }

    bool TryCopyMinimapIconExact(std::uint32_t key, MinimapIcon& outIcon)
    {
        EnsureMinimapIconsLoaded();

        std::lock_guard<std::mutex> lock(g_minimapIconMutex);
        if (!g_minimapIcons.loaded)
            return false;

        for (const MinimapIcon& icon : g_minimapIcons.icons)
        {
            if (icon.key == key)
            {
                outIcon = icon;
                return true;
            }
        }

        return false;
    }

    std::uint32_t ResolveMinimapIconKey(std::uint32_t key)
    {
        return key;
    }

    bool TryCopyMinimapIcon(std::uint32_t key, MinimapIcon& outIcon)
    {
        const std::uint32_t resolvedKey = ResolveMinimapIconKey(key);
        if (TryCopyMinimapIconExact(resolvedKey, outIcon))
            return true;

        return resolvedKey != key && TryCopyMinimapIconExact(key, outIcon);
    }

    bool TryDrawMinimapRasterIcon(VulkanMinimapRenderer& renderer, void* commandBuffer, int cx, int cy, int radius, int x, int y, std::uint32_t kind, bool clipped)
    {
        MinimapIcon icon{};
        if (!TryCopyMinimapIcon(kind, icon))
            return false;

        const int targetWidth = clipped ? MaxValue(18, static_cast<int>(icon.width * 3u / 4u)) : static_cast<int>(icon.width);
        const int targetHeight = clipped ? MaxValue(18, static_cast<int>(icon.height * 3u / 4u)) : static_cast<int>(icon.height);
        const int left = x - targetWidth / 2;
        const int top = y - targetHeight / 2;
        const int radiusSq = radius * radius;

        for (int dstY = 0; dstY < targetHeight; ++dstY)
        {
            const int screenY = top + dstY;
            const int dy = screenY - cy;
            const std::uint32_t srcY = MinValue<std::uint32_t>(
                icon.height - 1u,
                static_cast<std::uint32_t>((static_cast<std::uint64_t>(dstY) * icon.height) / static_cast<std::uint32_t>(targetHeight)));
            int runStart = -1;
            float runRed = 0.0f;
            float runGreen = 0.0f;
            float runBlue = 0.0f;

            const auto flushRun = [&](int endX)
            {
                if (runStart < 0 || endX <= runStart)
                    return;

                CmdClearRect(renderer, commandBuffer, runRed, runGreen, runBlue, 1.0f, runStart, screenY, endX - runStart, 1);
                runStart = -1;
            };

            for (int dstX = 0; dstX < targetWidth; ++dstX)
            {
                const int screenX = left + dstX;
                const int dx = screenX - cx;
                if (dx * dx + dy * dy > radiusSq)
                {
                    flushRun(screenX);
                    continue;
                }

                const std::uint32_t srcX = MinValue<std::uint32_t>(
                    icon.width - 1u,
                    static_cast<std::uint32_t>((static_cast<std::uint64_t>(dstX) * icon.width) / static_cast<std::uint32_t>(targetWidth)));
                const std::size_t offset =
                    (static_cast<std::size_t>(srcY) * static_cast<std::size_t>(icon.width) + static_cast<std::size_t>(srcX)) *
                    4u;
                const std::uint8_t alpha = icon.rgba[offset + 3];
                if (alpha <= 24)
                {
                    flushRun(screenX);
                    continue;
                }

                const float red = static_cast<float>(icon.rgba[offset + 0]) / 255.0f;
                const float green = static_cast<float>(icon.rgba[offset + 1]) / 255.0f;
                const float blue = static_cast<float>(icon.rgba[offset + 2]) / 255.0f;

                if (runStart >= 0 &&
                    std::fabs(runRed - red) < 0.002f &&
                    std::fabs(runGreen - green) < 0.002f &&
                    std::fabs(runBlue - blue) < 0.002f)
                {
                    continue;
                }

                flushRun(screenX);
                runStart = screenX;
                runRed = red;
                runGreen = green;
                runBlue = blue;
            }

            flushRun(left + targetWidth);
        }

        return true;
    }

    bool SampleRealMapColor(float worldX, float worldZ, float& red, float& green, float& blue)
    {
        std::lock_guard<std::mutex> lock(g_realMapMutex);
        if (!g_realMap.loaded || g_realMap.rgba.empty())
            return false;

        const float u = ClampValue(worldX / REAL_MAP_WORLD_SIZE, 0.0f, 1.0f);
        const float v = ClampValue(1.0f - (worldZ / REAL_MAP_WORLD_SIZE), 0.0f, 1.0f);
        const int x = ClampValue(static_cast<int>(u * static_cast<float>(g_realMap.width - 1)), 0, g_realMap.width - 1);
        const int y = ClampValue(static_cast<int>(v * static_cast<float>(g_realMap.height - 1)), 0, g_realMap.height - 1);
        const std::size_t offset = (static_cast<std::size_t>(y) * static_cast<std::size_t>(g_realMap.width) + static_cast<std::size_t>(x)) * 4;

        red = static_cast<float>(g_realMap.rgba[offset + 0]) / 255.0f;
        green = static_cast<float>(g_realMap.rgba[offset + 1]) / 255.0f;
        blue = static_cast<float>(g_realMap.rgba[offset + 2]) / 255.0f;
        return true;
    }

    struct MinimapWorldPoint
    {
        float x = 0.0f;
        float z = 0.0f;
        std::uint32_t kind = 0;
    };

    bool TryResolveKnownMarkerIconKey(std::uint32_t candidate, std::uint32_t& outKey)
    {
        if (candidate <= 0xFFFFu)
            return false;

        EnsureMinimapIconsLoaded();

        std::lock_guard<std::mutex> lock(g_minimapIconMutex);
        if (!g_minimapIcons.loaded)
            return false;

        for (const MinimapIcon& icon : g_minimapIcons.icons)
        {
            if (icon.key == candidate)
            {
                outKey = candidate;
                return true;
            }
        }

        return false;
    }

    bool IsKnownMarkerIconKeyLocked(std::uint32_t candidate)
    {
        if (candidate <= 0xFFFFu || !g_minimapIcons.loaded)
            return false;

        for (const MinimapIcon& icon : g_minimapIcons.icons)
        {
            if (icon.key == candidate)
                return true;
        }

        return false;
    }

    bool IsSpecialMapMarkerKind(std::uint32_t candidate)
    {
        return candidate == MAP_MARKER_QUEST_IMPORTANT_KIND ||
            candidate == MAP_MARKER_QUEST_KIND;
    }

    bool TryReadMapMarkerTypeFromRawWords(const std::uint64_t* words, std::size_t wordCount, std::uint32_t& outKind)
    {
        if (words == nullptr || wordCount < 3)
            return false;

        outKind = static_cast<std::uint32_t>(words[2] & 0xFFFFFFFFu);
        return outKind != 0;
    }

    std::uint32_t ResolveIconKeyFromRawWordsIfAtlasLoaded(const std::uint64_t* words, std::size_t wordCount, std::uint32_t fallback)
    {
        std::lock_guard<std::mutex> lock(g_minimapIconMutex);
        if (!g_minimapIcons.loaded)
        {
            std::uint32_t markerType = 0;
            if (TryReadMapMarkerTypeFromRawWords(words, wordCount, markerType) && IsSpecialMapMarkerKind(markerType))
                return markerType;
            return fallback;
        }

        std::uint32_t markerType = 0;
        if (TryReadMapMarkerTypeFromRawWords(words, wordCount, markerType) &&
            (IsSpecialMapMarkerKind(markerType) || IsKnownMarkerIconKeyLocked(markerType)))
        {
            return markerType;
        }

        if (IsKnownMarkerIconKeyLocked(fallback))
            return fallback;

        for (std::size_t index = 0; index < wordCount; ++index)
        {
            const std::uint32_t low = static_cast<std::uint32_t>(words[index] & 0xFFFFFFFFu);
            const std::uint32_t high = static_cast<std::uint32_t>((words[index] >> 32u) & 0xFFFFFFFFu);
            if (IsKnownMarkerIconKeyLocked(low))
                return low;
            if (IsKnownMarkerIconKeyLocked(high))
                return high;
        }

        return fallback;
    }

    std::uint32_t ResolveIconKeyFromRawWords(const std::uint64_t* words, std::size_t wordCount, std::uint32_t fallback)
    {
        EnsureMinimapIconsLoaded();

        std::lock_guard<std::mutex> lock(g_minimapIconMutex);
        if (!g_minimapIcons.loaded)
        {
            std::uint32_t markerType = 0;
            if (TryReadMapMarkerTypeFromRawWords(words, wordCount, markerType) && IsSpecialMapMarkerKind(markerType))
                return markerType;
            return fallback;
        }

        std::uint32_t markerType = 0;
        if (TryReadMapMarkerTypeFromRawWords(words, wordCount, markerType) &&
            (IsSpecialMapMarkerKind(markerType) || IsKnownMarkerIconKeyLocked(markerType)))
        {
            return markerType;
        }

        if (IsKnownMarkerIconKeyLocked(fallback))
            return fallback;

        for (std::size_t index = 0; index < wordCount; ++index)
        {
            const std::uint32_t low = static_cast<std::uint32_t>(words[index] & 0xFFFFFFFFu);
            const std::uint32_t high = static_cast<std::uint32_t>((words[index] >> 32u) & 0xFFFFFFFFu);
            if (IsKnownMarkerIconKeyLocked(low))
                return low;
            if (IsKnownMarkerIconKeyLocked(high))
                return high;
        }

        return fallback;
    }

    std::uint32_t ResolveCapturedWaypointKind(const CapturedWaypoint& waypoint)
    {
        std::uint32_t key = 0;
        if (TryResolveKnownMarkerIconKey(waypoint.id, key))
            return key;

        return ResolveIconKeyFromRawWords(waypoint.raw, sizeof(waypoint.raw) / sizeof(waypoint.raw[0]), 10);
    }

    std::uint32_t ResolveCapturedMarkerKind(const CapturedNearbyMarker& marker)
    {
        return ResolveIconKeyFromRawWords(marker.raw, sizeof(marker.raw) / sizeof(marker.raw[0]), marker.kind);
    }

    std::uint32_t ResolveCapturedVisibleMapMarkerKind(const CapturedMapMarkerVisibility& marker)
    {
        return ResolveIconKeyFromRawWords(marker.raw, sizeof(marker.raw) / sizeof(marker.raw[0]), marker.kind);
    }

    bool TryInitMapMarkerVisibilityRecord(void* ctx, MapMarkerVisibilityIterationRecord& record)
    {
        __try
        {
            g_iterInit(ctx, &record, static_cast<std::uint32_t>(sizeof(record)));
            return g_iterNext(ctx, &record, static_cast<std::uint32_t>(sizeof(record)));
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    bool IsPlausibleMapMarkerPosition(float x, float y, float z)
    {
        return IsPlausibleWorldPosition(x, y, z) &&
            x >= -512.0f &&
            z >= -512.0f &&
            x <= REAL_MAP_WORLD_SIZE + 768.0f &&
            z <= REAL_MAP_WORLD_SIZE + 768.0f;
    }

    bool TryCaptureMarkerWorldPositionFromBlock(uintptr_t base, std::size_t scanBytes, std::int64_t& outX, std::int64_t& outY, std::int64_t& outZ, std::size_t& bestOffset)
    {
        if (!IsLikelyRuntimePointer(base) || scanBytes < sizeof(std::uint64_t) * 5)
            return false;

        bool found = false;
        for (std::size_t offset = 0; offset + sizeof(std::uint64_t) * 5 <= scanBytes; offset += sizeof(std::uint64_t))
        {
            std::uint64_t raw[5] = {};
            if (!SafeRead(base + offset, raw, sizeof(raw)))
                continue;

            std::int64_t x = 0;
            std::int64_t y = 0;
            std::int64_t z = 0;
            if (!TryDecodeFixedWorldPosition(raw, x, y, z))
                continue;

            if (!IsPlausibleMapMarkerPosition(FixedToWorld(x), FixedToWorld(y), FixedToWorld(z)))
                continue;

            if (!found || offset < bestOffset)
            {
                outX = x;
                outY = y;
                outZ = z;
                bestOffset = offset;
                found = true;
            }
        }

        return found;
    }

    bool TryCaptureMarkerWorldPositionFromPointerChildren(uintptr_t root, std::int64_t& outX, std::int64_t& outY, std::int64_t& outZ, std::size_t& bestOffset)
    {
        if (!IsLikelyRuntimePointer(root))
            return false;

        bool found = false;
        for (std::size_t pointerOffset = 0; pointerOffset < 0x40; pointerOffset += sizeof(uintptr_t))
        {
            uintptr_t child = 0;
            if (!SafeReadValue(root + pointerOffset, child) || !IsLikelyRuntimePointer(child))
                continue;

            std::size_t childOffset = 0;
            if (!TryCaptureMarkerWorldPositionFromBlock(child, 0x100, outX, outY, outZ, childOffset))
                continue;

            const std::size_t combinedOffset = 0x1000 + pointerOffset * 0x100 + childOffset;
            if (!found || combinedOffset < bestOffset)
            {
                bestOffset = combinedOffset;
                found = true;
            }
        }

        return found;
    }

    bool TryResolveVisibleMapMarkerWorldPosition(const MapMarkerVisibilityIterationRecord& record, CapturedMapMarkerVisibility& marker)
    {
        std::size_t bestOffset = static_cast<std::size_t>(-1);
        bool found = false;

        const auto tryRoot = [&](uintptr_t root, std::size_t bytes)
        {
            std::size_t offset = bestOffset;
            std::int64_t x = 0;
            std::int64_t y = 0;
            std::int64_t z = 0;
            if (TryCaptureMarkerWorldPositionFromBlock(root, bytes, x, y, z, offset) && (!found || offset < bestOffset))
            {
                marker.x = x;
                marker.y = y;
                marker.z = z;
                bestOffset = offset;
                found = true;
            }
        };

        const uintptr_t markerAddress = reinterpret_cast<uintptr_t>(record.marker);
        const uintptr_t entityAddress = reinterpret_cast<uintptr_t>(record.entity);
        const uintptr_t unknownAddress = reinterpret_cast<uintptr_t>(record.unknown0);
        const uintptr_t stateAddress = reinterpret_cast<uintptr_t>(record.visibilityState);

        tryRoot(markerAddress, 0x100);
        tryRoot(entityAddress, 0x180);
        tryRoot(unknownAddress, 0x100);
        tryRoot(stateAddress, 0x60);

        marker.hasWorldPosition = found;
        return found;
    }

    void PruneVisibleMapMarkersLocked(DWORD now)
    {
        if (now - g_lastVisibleMapMarkerPruneTick < VISIBLE_MAP_MARKER_PRUNE_MS)
            return;

        g_lastVisibleMapMarkerPruneTick = now;
        g_visibleMapMarkers.erase(
            std::remove_if(g_visibleMapMarkers.begin(), g_visibleMapMarkers.end(), [now](const CapturedMapMarkerVisibility& marker)
            {
                return now - marker.lastUpdateTick > VISIBLE_MAP_MARKER_STALE_MS;
            }),
            g_visibleMapMarkers.end());
    }

    void PublishVisibleMapMarker(CapturedMapMarkerVisibility&& marker)
    {
        const DWORD now = GetTickCount();
        marker.lastUpdateTick = now;

        std::size_t visibleCount = 0;
        std::size_t positionedCount = 0;
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(g_visibleMapMarkerMutex);
            PruneVisibleMapMarkersLocked(now);

            auto existing = std::find_if(g_visibleMapMarkers.begin(), g_visibleMapMarkers.end(), [&marker](const CapturedMapMarkerVisibility& current)
            {
                return SameVisibleMapMarkerIdentity(current, marker);
            });

            if (existing != g_visibleMapMarkers.end())
            {
                const bool hadPosition = existing->hasWorldPosition;
                if (!marker.hasWorldPosition && hadPosition)
                {
                    marker.x = existing->x;
                    marker.y = existing->y;
                    marker.z = existing->z;
                    marker.hasWorldPosition = true;
                }

                changed = existing->visibility != marker.visibility ||
                    existing->kind != marker.kind ||
                    existing->hasWorldPosition != marker.hasWorldPosition ||
                    existing->x != marker.x ||
                    existing->z != marker.z;
                *existing = marker;
            }
            else if (marker.visibility != 0 && g_visibleMapMarkers.size() < VISIBLE_MAP_MARKER_MAX_ENTRIES)
            {
                g_visibleMapMarkers.push_back(marker);
                changed = true;
            }

            for (const CapturedMapMarkerVisibility& current : g_visibleMapMarkers)
            {
                if (current.visibility == 0 || now - current.lastUpdateTick > VISIBLE_MAP_MARKER_STALE_MS)
                    continue;

                ++visibleCount;
                if (current.hasWorldPosition)
                    ++positionedCount;
            }
        }

        if (!changed || now - g_lastVisibleMapMarkerSummaryTick < 5000)
            return;

        g_lastVisibleMapMarkerSummaryTick = now;
        std::ostringstream oss;
        oss << "[Minimap] real map marker visibility feed"
            << " | visible=" << visibleCount
            << " | positioned=" << positionedCount
            << " | kind=0x" << std::hex << marker.kind << std::dec
            << " | visibility=" << static_cast<int>(marker.visibility);
        if (marker.hasWorldPosition)
        {
            oss << " | world=("
                << FixedToWorld(marker.x) << ", "
                << FixedToWorld(marker.y) << ", "
                << FixedToWorld(marker.z) << ")";
        }
        Log(oss.str());
    }

    bool TryPublishMapMarkerVisibilityRecord(const MapMarkerVisibilityIterationRecord& record)
    {
        if (record.marker == nullptr || record.visibilityState == nullptr)
            return false;

        CapturedMapMarkerVisibility marker{};
        marker.markerAddress = reinterpret_cast<uintptr_t>(record.marker);
        marker.stateAddress = reinterpret_cast<uintptr_t>(record.visibilityState);
        marker.entityAddress = reinterpret_cast<uintptr_t>(record.entity);

        if (!SafeRead(marker.markerAddress, marker.raw, sizeof(marker.raw)))
            return false;

        SafeReadValue(marker.stateAddress, marker.visibility);
        marker.markerId = static_cast<std::uint32_t>(marker.raw[0] & 0xFFFFFFFFu);
        TryReadMapMarkerTypeFromRawWords(marker.raw, sizeof(marker.raw) / sizeof(marker.raw[0]), marker.markerType);
        marker.kind = ResolveIconKeyFromRawWordsIfAtlasLoaded(marker.raw, sizeof(marker.raw) / sizeof(marker.raw[0]), marker.markerType != 0 ? marker.markerType : marker.markerId);

        CapturedMapMarkerVisibility existing{};
        bool hasExistingPosition = false;
        {
            std::lock_guard<std::mutex> lock(g_visibleMapMarkerMutex);
            auto existingIt = std::find_if(g_visibleMapMarkers.begin(), g_visibleMapMarkers.end(), [&marker](const CapturedMapMarkerVisibility& current)
            {
                return SameVisibleMapMarkerIdentity(current, marker);
            });
            if (existingIt != g_visibleMapMarkers.end() && existingIt->hasWorldPosition)
            {
                existing = *existingIt;
                hasExistingPosition = true;
            }
        }

        if (hasExistingPosition)
        {
            marker.x = existing.x;
            marker.y = existing.y;
            marker.z = existing.z;
            marker.hasWorldPosition = true;
        }
        else if (marker.visibility != 0)
        {
            TryResolveVisibleMapMarkerWorldPosition(record, marker);
        }

        PublishVisibleMapMarker(std::move(marker));
        g_visibleMapMarkerFaults.store(0, std::memory_order_relaxed);
        return true;
    }

    bool TryCaptureMapMarkerVisibility(void* ctx)
    {
        if (g_iterInit == nullptr || g_iterNext == nullptr || ctx == nullptr)
            return false;

        MapMarkerVisibilityIterationRecord record{};
        if (!TryInitMapMarkerVisibilityRecord(ctx, record))
            return false;

        return TryPublishMapMarkerVisibilityRecord(record);
    }

    void __fastcall CaptureMapMarkerVisibilityHook(void* ctx, void*, void*, void*)
    {
        TryCaptureMapMarkerVisibility(ctx);
    }

    void RegisterVisibleMapMarkerCaptureFault()
    {
        const int faults = g_visibleMapMarkerFaults.fetch_add(1, std::memory_order_relaxed) + 1;
        if (faults >= 3)
            g_visibleMapMarkerCaptureDisabled.store(true, std::memory_order_relaxed);

        const DWORD now = GetTickCount();
        if (now - g_lastVisibleMapMarkerFaultTick < 5000)
            return;

        g_lastVisibleMapMarkerFaultTick = now;
        std::ostringstream oss;
        oss << "[Minimap] map marker visibility capture fault"
            << " | faults=" << faults
            << " | disabled=" << (g_visibleMapMarkerCaptureDisabled.load(std::memory_order_relaxed) ? 1 : 0);
        Log(oss.str());
    }

    void __fastcall CaptureMapMarkerVisibilityRecordHook(MapMarkerVisibilityIterationRecord* recordPtr, void*)
    {
        if (g_visibleMapMarkerCaptureDisabled.load(std::memory_order_relaxed) || recordPtr == nullptr)
            return;

        MapMarkerVisibilityIterationRecord record{};
        bool copied = false;
        __try
        {
            record = *recordPtr;
            copied = true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            RegisterVisibleMapMarkerCaptureFault();
            return;
        }

        if (copied)
            TryPublishMapMarkerVisibilityRecord(record);
    }

    bool IsPersonalMarkerKind(std::uint32_t kind)
    {
        return kind == 1 || kind == 2 || kind == 10 || kind == 11 || kind == 12;
    }

    bool IsRuntimeMarkerKind(std::uint32_t kind)
    {
        return kind == 1 || kind == 2 || kind == 3 || kind == 11 || kind == 12 || kind == 13;
    }

    bool IsVisibleMapMarkerDrawable(const CapturedMapMarkerVisibility& marker)
    {
        if (marker.visibility < 2 || !marker.hasWorldPosition)
            return false;

        const std::uint32_t kind = ResolveCapturedVisibleMapMarkerKind(marker);
        if (kind <= 0xFFFFu || IsSpecialMapMarkerKind(kind))
            return true;

        std::uint32_t knownKey = 0;
        return TryResolveKnownMarkerIconKey(kind, knownKey);
    }

    void PushWorldPointUnique(std::vector<MinimapWorldPoint>& points, const MinimapWorldPoint& point)
    {
        for (const MinimapWorldPoint& existing : points)
        {
            const float dx = existing.x - point.x;
            const float dz = existing.z - point.z;
            if (dx * dx + dz * dz < 16.0f)
                return;
        }

        points.push_back(point);
    }

    bool HasVisibleMapMarkerNear(const std::vector<MinimapWorldPoint>& visibleMarkers, float x, float z, std::uint32_t kind);

    void BuildWorldPoints(
        std::vector<MinimapWorldPoint>& points,
        const std::vector<CapturedWaypoint>& waypoints,
        const std::vector<CapturedNearbyMarker>& nearbyMarkers,
        const std::vector<CapturedMapMarkerVisibility>& visibleMapMarkers)
    {
        std::vector<MinimapWorldPoint> visibleMarkerPoints;
        visibleMarkerPoints.reserve(visibleMapMarkers.size());
        for (const CapturedMapMarkerVisibility& marker : visibleMapMarkers)
        {
            if (!IsVisibleMapMarkerDrawable(marker))
                continue;

            visibleMarkerPoints.push_back({ FixedToWorld(marker.x), FixedToWorld(marker.z), ResolveCapturedVisibleMapMarkerKind(marker) });
        }

        for (const CapturedWaypoint& waypoint : waypoints)
        {
            PushWorldPointUnique(points, { FixedToWorld(waypoint.x), FixedToWorld(waypoint.z), ResolveCapturedWaypointKind(waypoint) });
        }

        for (const CapturedNearbyMarker& marker : nearbyMarkers)
        {
            if (!marker.hasWorldPosition)
                continue;

            const std::uint32_t resolvedKind = ResolveCapturedMarkerKind(marker);
            if (!IsRuntimeMarkerKind(marker.kind) && resolvedKind <= 0xFFFFu)
                continue;

            if (!visibleMarkerPoints.empty() &&
                !IsPersonalMarkerKind(marker.kind) &&
                !HasVisibleMapMarkerNear(visibleMarkerPoints, FixedToWorld(marker.x), FixedToWorld(marker.z), resolvedKind))
            {
                continue;
            }

            PushWorldPointUnique(points, { FixedToWorld(marker.x), FixedToWorld(marker.z), resolvedKind });
        }

        for (const MinimapWorldPoint& marker : visibleMarkerPoints)
        {
            PushWorldPointUnique(points, marker);
        }
    }

    bool HasVisibleRuntimeMarkerNear(const std::vector<MinimapWorldPoint>& genericRuntimeMarkers, float x, float z)
    {
        constexpr float kMatchRadiusWorld = 72.0f;
        constexpr float kMatchRadiusSq = kMatchRadiusWorld * kMatchRadiusWorld;
        for (const MinimapWorldPoint& marker : genericRuntimeMarkers)
        {
            const float dx = marker.x - x;
            const float dz = marker.z - z;
            if (dx * dx + dz * dz <= kMatchRadiusSq)
                return true;
        }

        return false;
    }

    bool HasRuntimeMarkersNearCenter(const std::vector<MinimapWorldPoint>& genericRuntimeMarkers, float centerX, float centerZ, float radiusWorld)
    {
        const float usableRadius = radiusWorld + 96.0f;
        const float usableRadiusSq = usableRadius * usableRadius;
        for (const MinimapWorldPoint& marker : genericRuntimeMarkers)
        {
            const float dx = marker.x - centerX;
            const float dz = marker.z - centerZ;
            if (dx * dx + dz * dz <= usableRadiusSq)
                return true;
        }

        return false;
    }

    bool HasWorldPointsNearCenter(const std::vector<MinimapWorldPoint>& markers, float centerX, float centerZ, float radiusWorld)
    {
        const float usableRadius = radiusWorld + 96.0f;
        const float usableRadiusSq = usableRadius * usableRadius;
        for (const MinimapWorldPoint& marker : markers)
        {
            const float dx = marker.x - centerX;
            const float dz = marker.z - centerZ;
            if (dx * dx + dz * dz <= usableRadiusSq)
                return true;
        }

        return false;
    }

    bool HasVisibleMapMarkerNear(const std::vector<MinimapWorldPoint>& visibleMarkers, float x, float z, std::uint32_t kind)
    {
        constexpr float kMatchRadiusWorld = 56.0f;
        constexpr float kMatchRadiusSq = kMatchRadiusWorld * kMatchRadiusWorld;
        for (const MinimapWorldPoint& marker : visibleMarkers)
        {
            const float dx = marker.x - x;
            const float dz = marker.z - z;
            if (dx * dx + dz * dz > kMatchRadiusSq)
                continue;

            if (kind > 0xFFFFu)
            {
                if (marker.kind == kind)
                    return true;
                continue;
            }

            return true;
        }

        return false;
    }

    void AppendVisibleStaticPoiPoints(
        std::vector<MinimapWorldPoint>& points,
        const std::vector<CapturedNearbyMarker>& nearbyMarkers,
        const std::vector<CapturedMapMarkerVisibility>& visibleMapMarkers,
        float centerX,
        float centerZ,
        float radiusWorld)
    {
        EnsureStaticPoisLoaded();

        std::vector<MinimapWorldPoint> genericRuntimeMarkers;
        genericRuntimeMarkers.reserve(nearbyMarkers.size());
        for (const CapturedNearbyMarker& marker : nearbyMarkers)
        {
            if (!marker.hasWorldPosition || !IsRuntimeMarkerKind(marker.kind))
                continue;

            const std::uint32_t resolvedKind = ResolveCapturedMarkerKind(marker);
            if (resolvedKind > 0xFFFFu)
                continue;

            genericRuntimeMarkers.push_back({ FixedToWorld(marker.x), FixedToWorld(marker.z), resolvedKind });
        }

        std::vector<MinimapWorldPoint> visibleMarkerPoints;
        visibleMarkerPoints.reserve(visibleMapMarkers.size());
        for (const CapturedMapMarkerVisibility& marker : visibleMapMarkers)
        {
            if (!IsVisibleMapMarkerDrawable(marker))
                continue;

            visibleMarkerPoints.push_back({ FixedToWorld(marker.x), FixedToWorld(marker.z), ResolveCapturedVisibleMapMarkerKind(marker) });
        }

        const bool requireRuntimeMatch = HasRuntimeMarkersNearCenter(genericRuntimeMarkers, centerX, centerZ, radiusWorld);
        const bool requireVisibilityMatch = HasWorldPointsNearCenter(visibleMarkerPoints, centerX, centerZ, radiusWorld);

        struct StaticPoiCandidate
        {
            MinimapWorldPoint point;
            float distanceSq = 0.0f;
        };

        std::vector<StaticPoiCandidate> candidates;
        {
            std::lock_guard<std::mutex> lock(g_staticPoiMutex);
            if (!g_staticPois.loaded)
                return;

            const float radiusSq = radiusWorld * radiusWorld;
            candidates.reserve(MinValue(g_staticPois.pois.size(), STATIC_POI_MAX_DRAWN));
            for (const StaticPoi& poi : g_staticPois.pois)
            {
                if (!IsWorldPointRevealedByFogOfWar(poi.x, poi.z))
                    continue;

                const float dx = poi.x - centerX;
                const float dz = poi.z - centerZ;
                const float distanceSq = dx * dx + dz * dz;
                if (distanceSq > radiusSq)
                    continue;

                if (requireVisibilityMatch && !HasVisibleMapMarkerNear(visibleMarkerPoints, poi.x, poi.z, poi.kind))
                    continue;

                if (requireRuntimeMatch && !HasVisibleRuntimeMarkerNear(genericRuntimeMarkers, poi.x, poi.z))
                    continue;

                candidates.push_back({ { poi.x, poi.z, poi.kind }, distanceSq });
            }
        }

        std::sort(candidates.begin(), candidates.end(), [](const StaticPoiCandidate& left, const StaticPoiCandidate& right)
        {
            return left.distanceSq < right.distanceSq;
        });

        const std::size_t count = MinValue(candidates.size(), STATIC_POI_MAX_DRAWN);
        for (std::size_t index = 0; index < count; ++index)
        {
            points.push_back(candidates[index].point);
        }
    }

    bool ComputeMapCenter(const std::vector<MinimapWorldPoint>& points, float& centerX, float& centerZ)
    {
        if (points.empty())
            return false;

        if (points.size() == 1)
        {
            centerX = points[0].x - 260.0f;
            centerZ = points[0].z + 170.0f;
            return true;
        }

        float sumX = 0.0f;
        float sumZ = 0.0f;
        for (const MinimapWorldPoint& point : points)
        {
            sumX += point.x;
            sumZ += point.z;
        }

        centerX = sumX / static_cast<float>(points.size());
        centerZ = sumZ / static_cast<float>(points.size());
        return true;
    }

    bool ProjectWorldToMinimap(float worldX, float worldZ, float centerX, float centerZ, float unitsPerPixel, float headingRadians, int cx, int cy, int radius, int& outX, int& outY, bool& clipped)
    {
        const float dx = (worldX - centerX) / unitsPerPixel;
        const float dz = (worldZ - centerZ) / unitsPerPixel;
        const float cosHeading = std::cos(headingRadians);
        const float sinHeading = std::sin(headingRadians);
        const float northUpX = dx;
        const float northUpY = -dz;
        float screenX = cosHeading * northUpX + sinHeading * northUpY;
        float screenY = -sinHeading * northUpX + cosHeading * northUpY;
        const float distance = std::sqrt(dx * dx + dz * dz);
        clipped = distance > static_cast<float>(radius - 18);
        if (clipped && distance > 0.01f)
        {
            const float scale = static_cast<float>(radius - 18) / distance;
            screenX *= scale;
            screenY *= scale;
        }

        outX = cx + static_cast<int>(screenX);
        outY = cy + static_cast<int>(screenY);
        return true;
    }

    void DrawRealMap(VulkanMinimapRenderer& renderer, void* commandBuffer, int cx, int cy, int radius, float centerX, float centerZ, float unitsPerPixel, float headingRadians)
    {
        EnsureRealMapLoaded();

        const int innerRadius = MaxValue(8, radius);
        const float cosHeading = std::cos(headingRadians);
        const float sinHeading = std::sin(headingRadians);
        std::lock_guard<std::mutex> lock(g_realMapMutex);
        const bool hasMap = g_realMap.loaded && !g_realMap.rgba.empty();
        const auto quantizeColor = [](float value) -> float
        {
            constexpr float kSteps = 34.0f;
            return std::round(ClampValue(value, 0.0f, 1.0f) * kSteps) / kSteps;
        };

        for (int y = -innerRadius; y <= innerRadius; ++y)
        {
            int runStart = 0;
            float runRed = 0.0f;
            float runGreen = 0.0f;
            float runBlue = 0.0f;
            bool inRun = false;

            const auto flushRun = [&](int endX)
            {
                if (!inRun || endX <= runStart)
                    return;

                CmdClearRect(renderer, commandBuffer, runRed, runGreen, runBlue, 1.0f, cx + runStart, cy + y, endX - runStart, 1);
                inRun = false;
            };

            for (int x = -innerRadius; x <= innerRadius; ++x)
            {
                const int distanceSq = x * x + y * y;
                if (distanceSq > innerRadius * innerRadius)
                {
                    flushRun(x);
                    continue;
                }

                float red = 0.045f;
                float green = 0.048f;
                float blue = 0.042f;
                if (hasMap)
                {
                    const float northUpX = cosHeading * static_cast<float>(x) - sinHeading * static_cast<float>(y);
                    const float northUpY = sinHeading * static_cast<float>(x) + cosHeading * static_cast<float>(y);
                    const float worldX = centerX + northUpX * unitsPerPixel;
                    const float worldZ = centerZ - northUpY * unitsPerPixel;
                    const float u = ClampValue(worldX / REAL_MAP_WORLD_SIZE, 0.0f, 1.0f);
                    const float v = ClampValue(1.0f - (worldZ / REAL_MAP_WORLD_SIZE), 0.0f, 1.0f);
                    const float mapFx = u * static_cast<float>(g_realMap.width - 1);
                    const float mapFy = v * static_cast<float>(g_realMap.height - 1);
                    const int x0 = ClampValue(static_cast<int>(std::floor(mapFx)), 0, g_realMap.width - 1);
                    const int y0 = ClampValue(static_cast<int>(std::floor(mapFy)), 0, g_realMap.height - 1);
                    const int x1 = ClampValue(x0 + 1, 0, g_realMap.width - 1);
                    const int y1 = ClampValue(y0 + 1, 0, g_realMap.height - 1);
                    const float tx = mapFx - static_cast<float>(x0);
                    const float ty = mapFy - static_cast<float>(y0);

                    const auto sampleChannel = [&](int x, int y, int channel) -> float
                    {
                        const std::size_t offset = (static_cast<std::size_t>(y) * static_cast<std::size_t>(g_realMap.width) + static_cast<std::size_t>(x)) * 4;
                        return static_cast<float>(g_realMap.rgba[offset + static_cast<std::size_t>(channel)]) / 255.0f;
                    };

                    const float redTop = sampleChannel(x0, y0, 0) * (1.0f - tx) + sampleChannel(x1, y0, 0) * tx;
                    const float redBottom = sampleChannel(x0, y1, 0) * (1.0f - tx) + sampleChannel(x1, y1, 0) * tx;
                    const float greenTop = sampleChannel(x0, y0, 1) * (1.0f - tx) + sampleChannel(x1, y0, 1) * tx;
                    const float greenBottom = sampleChannel(x0, y1, 1) * (1.0f - tx) + sampleChannel(x1, y1, 1) * tx;
                    const float blueTop = sampleChannel(x0, y0, 2) * (1.0f - tx) + sampleChannel(x1, y0, 2) * tx;
                    const float blueBottom = sampleChannel(x0, y1, 2) * (1.0f - tx) + sampleChannel(x1, y1, 2) * tx;

                    red = redTop * (1.0f - ty) + redBottom * ty;
                    green = greenTop * (1.0f - ty) + greenBottom * ty;
                    blue = blueTop * (1.0f - ty) + blueBottom * ty;

                    red = ClampValue((red - 0.42f) * 1.20f + 0.42f, 0.0f, 1.0f);
                    green = ClampValue((green - 0.42f) * 1.22f + 0.42f, 0.0f, 1.0f);
                    blue = ClampValue((blue - 0.42f) * 1.16f + 0.42f, 0.0f, 1.0f);

                    const float distance = std::sqrt(static_cast<float>(distanceSq)) / static_cast<float>(innerRadius);
                    const float vignette = 1.02f - 0.34f * distance * distance;
                    red = ClampValue(red * vignette + 0.004f, 0.0f, 1.0f);
                    green = ClampValue(green * vignette + 0.004f, 0.0f, 1.0f);
                    blue = ClampValue(blue * vignette + 0.004f, 0.0f, 1.0f);
                }

                red = quantizeColor(red);
                green = quantizeColor(green);
                blue = quantizeColor(blue);

                if (inRun &&
                    std::fabs(runRed - red) < 0.0001f &&
                    std::fabs(runGreen - green) < 0.0001f &&
                    std::fabs(runBlue - blue) < 0.0001f)
                {
                    continue;
                }

                flushRun(x);
                runStart = x;
                runRed = red;
                runGreen = green;
                runBlue = blue;
                inRun = true;
            }

            flushRun(innerRadius + 1);
        }
    }

    void CmdClearRingBand(VulkanMinimapRenderer& renderer, void* commandBuffer, float red, float green, float blue, float alpha, int cx, int cy, int outerRadius, int innerRadius, int stripHeight)
    {
        if (outerRadius <= 0)
            return;

        innerRadius = MaxValue(0, innerRadius);
        stripHeight = MaxValue(1, stripHeight);
        if (outerRadius <= innerRadius)
            outerRadius = innerRadius + 1;

        const int outerSq = outerRadius * outerRadius;
        const int innerSq = innerRadius * innerRadius;
        std::vector<VkClearRect> rects;
        rects.reserve(static_cast<std::size_t>(((outerRadius * 2) / stripHeight + 3) * 2));
        for (int dy = -outerRadius; dy <= outerRadius; dy += stripHeight)
        {
            const int sampleY = MinValue(outerRadius, dy + (stripHeight / 2));
            const int outerHalf = static_cast<int>(std::sqrt(static_cast<float>(MaxValue(0, outerSq - sampleY * sampleY))));
            const int innerHalf = std::abs(sampleY) < innerRadius
                ? static_cast<int>(std::sqrt(static_cast<float>(MaxValue(0, innerSq - sampleY * sampleY))))
                : -1;

            if (innerHalf <= 0)
            {
                AppendClippedClearRect(renderer, rects, cx - outerHalf, cy + dy, outerHalf * 2 + 1, stripHeight);
                continue;
            }

            AppendClippedClearRect(renderer, rects, cx - outerHalf, cy + dy, MaxValue(1, outerHalf - innerHalf), stripHeight);
            AppendClippedClearRect(renderer, rects, cx + innerHalf, cy + dy, MaxValue(1, outerHalf - innerHalf), stripHeight);
        }
        CmdClearRects(renderer, commandBuffer, red, green, blue, alpha, rects);
    }

    void CmdClearFreeLine(VulkanMinimapRenderer& renderer, void* commandBuffer, int x0, int y0, int x1, int y1, int size, float red, float green, float blue)
    {
        const int steps = MaxValue(1, MaxValue(std::abs(x1 - x0), std::abs(y1 - y0)));
        const int half = MaxValue(1, size / 2);
        std::vector<VkClearRect> rects;
        rects.reserve(static_cast<std::size_t>(steps / MaxValue(1, half) + 2));
        for (int step = 0; step <= steps; step += MaxValue(1, half))
        {
            const float t = static_cast<float>(step) / static_cast<float>(steps);
            const int x = static_cast<int>(static_cast<float>(x0) + static_cast<float>(x1 - x0) * t);
            const int y = static_cast<int>(static_cast<float>(y0) + static_cast<float>(y1 - y0) * t);
            AppendClippedClearRect(renderer, rects, x - half, y - half, half * 2 + 1, half * 2 + 1);
        }
        CmdClearRects(renderer, commandBuffer, red, green, blue, 1.0f, rects);
    }

    int MinimapRasterFrameExtra(int radius)
    {
        return ClampValue((radius * 45) / 100, 46, 64);
    }

    int MinimapRasterMapRadius(int radius, int frameExtra)
    {
        return MaxValue(radius, (((radius + frameExtra) * 188) + 255) / 256 + 3);
    }

    bool TryDrawMinimapRasterFrame(VulkanMinimapRenderer& renderer, void* commandBuffer, int cx, int cy, int radius, int frameExtra)
    {
        EnsureMinimapFrameLoaded();

        std::lock_guard<std::mutex> lock(g_minimapFrameMutex);
        if (!g_minimapFrame.loaded || g_minimapFrame.width <= 0 || g_minimapFrame.height <= 0)
            return false;

        const int targetSize = (radius + frameExtra) * 2;
        const int visualCenterYOffset = -MaxValue(1, (targetSize + 128) / 256);
        const int left = cx - targetSize / 2;
        const int top = cy - targetSize / 2 + visualCenterYOffset;
        constexpr std::size_t kChunkSize = 1024;

        for (const MinimapFrameColorGroup& group : g_minimapFrame.groups)
        {
            std::vector<VkClearRect> rects;
            rects.reserve(kChunkSize);

            const auto flush = [&]()
            {
                if (rects.empty())
                    return;

                CmdClearRects(
                    renderer,
                    commandBuffer,
                    static_cast<float>(group.red) / 255.0f,
                    static_cast<float>(group.green) / 255.0f,
                    static_cast<float>(group.blue) / 255.0f,
                    1.0f,
                    rects);
                rects.clear();
            };

            for (const MinimapFrameRun& run : group.runs)
            {
                const int x0 = left + (static_cast<int>(run.x) * targetSize) / g_minimapFrame.width;
                const int x1 = left + ((static_cast<int>(run.x) + static_cast<int>(run.width)) * targetSize + g_minimapFrame.width - 1) / g_minimapFrame.width;
                const int y0 = top + (static_cast<int>(run.y) * targetSize) / g_minimapFrame.height;
                const int y1 = top + ((static_cast<int>(run.y) + 1) * targetSize + g_minimapFrame.height - 1) / g_minimapFrame.height;
                AppendClippedClearRect(renderer, rects, x0, y0, MaxValue(1, x1 - x0), MaxValue(1, y1 - y0));

                if (rects.size() >= kChunkSize)
                    flush();
            }

            flush();
        }

        return true;
    }

    void RecordFrameTextureUploadIfNeeded(VulkanMinimapRenderer& renderer, void* commandBuffer)
    {
        if (!renderer.frameTextureReady || !renderer.frameTextureUploadPending)
            return;

        void* image = reinterpret_cast<void*>(renderer.frameTextureImage);
        void* buffer = reinterpret_cast<void*>(renderer.frameStagingBuffer);
        if (image == nullptr || buffer == nullptr)
            return;

        VkImageMemoryBarrier toTransfer{};
        toTransfer.srcAccessMask = 0;
        toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toTransfer.image = image;
        toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        toTransfer.subresourceRange.levelCount = 1;
        toTransfer.subresourceRange.layerCount = 1;
        renderer.fns.cmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &toTransfer);

        VkBufferImageCopy copy{};
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.layerCount = 1;
        copy.imageExtent.width = renderer.frameTextureWidth;
        copy.imageExtent.height = renderer.frameTextureHeight;
        copy.imageExtent.depth = 1;
        renderer.fns.cmdCopyBufferToImage(
            commandBuffer,
            buffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &copy);

        VkImageMemoryBarrier toShader{};
        toShader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toShader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        toShader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toShader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        toShader.image = image;
        toShader.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        toShader.subresourceRange.levelCount = 1;
        toShader.subresourceRange.layerCount = 1;
        renderer.fns.cmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &toShader);

        renderer.frameTextureUploadPending = false;
    }

    bool TryDrawMinimapTexturedFrame(VulkanMinimapRenderer& renderer, void* commandBuffer, int cx, int cy, int radius, int frameExtra, float headingRadians)
    {
        if (!renderer.frameTextureReady || renderer.framePipeline == 0 || renderer.framePipelineLayout == 0 || renderer.frameDescriptorSet == 0)
            return false;

        const int targetSize = (radius + frameExtra) * 2;
        const int visualCenterYOffset = -MaxValue(1, (targetSize + 128) / 256);
        const int left = cx - targetSize / 2;
        const int top = cy - targetSize / 2 + visualCenterYOffset;
        const int right = left + targetSize;
        const int bottom = top + targetSize;

        const float cosHeading = std::cos(headingRadians);
        const float sinHeading = std::sin(headingRadians);
        const float push[8] = {
            (static_cast<float>(left) / static_cast<float>(renderer.width)) * 2.0f - 1.0f,
            (static_cast<float>(top) / static_cast<float>(renderer.height)) * 2.0f - 1.0f,
            (static_cast<float>(right) / static_cast<float>(renderer.width)) * 2.0f - 1.0f,
            (static_cast<float>(bottom) / static_cast<float>(renderer.height)) * 2.0f - 1.0f,
            cosHeading,
            sinHeading,
            static_cast<float>(renderer.height) / static_cast<float>(renderer.width),
            static_cast<float>(renderer.width) / static_cast<float>(renderer.height)
        };

        void* descriptorSet = reinterpret_cast<void*>(renderer.frameDescriptorSet);
        renderer.fns.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, reinterpret_cast<void*>(renderer.framePipeline));
        renderer.fns.cmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            reinterpret_cast<void*>(renderer.framePipelineLayout),
            0,
            1,
            &descriptorSet,
            0,
            nullptr);
        renderer.fns.cmdPushConstants(
            commandBuffer,
            reinterpret_cast<void*>(renderer.framePipelineLayout),
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(push),
            push);
        renderer.fns.cmdDraw(commandBuffer, 6, 1, 0, 0);
        return true;
    }

    void CmdClearCompassLetter(VulkanMinimapRenderer& renderer, void* commandBuffer, char letter, int x, int y, int size, float red, float green, float blue)
    {
        const int w = MaxValue(10, size);
        const int h = MaxValue(14, size + 4);
        const int t = MaxValue(2, size / 6);
        const int left = x - w / 2;
        const int top = y - h / 2;
        const int right = left + w;
        const int bottom = top + h;
        const int midY = y;

        switch (letter)
        {
        case 'N':
            CmdClearRect(renderer, commandBuffer, red, green, blue, 1.0f, left, top, t, h);
            CmdClearRect(renderer, commandBuffer, red, green, blue, 1.0f, right - t, top, t, h);
            CmdClearFreeLine(renderer, commandBuffer, left + t, bottom - 1, right - t, top, t, red, green, blue);
            break;
        case 'E':
            CmdClearRect(renderer, commandBuffer, red, green, blue, 1.0f, left, top, t, h);
            CmdClearRect(renderer, commandBuffer, red, green, blue, 1.0f, left, top, w, t);
            CmdClearRect(renderer, commandBuffer, red, green, blue, 1.0f, left, midY - t / 2, w - 2, t);
            CmdClearRect(renderer, commandBuffer, red, green, blue, 1.0f, left, bottom - t, w, t);
            break;
        case 'S':
            CmdClearRect(renderer, commandBuffer, red, green, blue, 1.0f, left + 1, top, w - 1, t);
            CmdClearRect(renderer, commandBuffer, red, green, blue, 1.0f, left, top, t, h / 2);
            CmdClearRect(renderer, commandBuffer, red, green, blue, 1.0f, left + 1, midY - t / 2, w - 1, t);
            CmdClearRect(renderer, commandBuffer, red, green, blue, 1.0f, right - t, midY, t, h / 2);
            CmdClearRect(renderer, commandBuffer, red, green, blue, 1.0f, left, bottom - t, w - 1, t);
            break;
        case 'W':
            CmdClearFreeLine(renderer, commandBuffer, left, top, left + w / 4, bottom, t, red, green, blue);
            CmdClearFreeLine(renderer, commandBuffer, left + w / 4, bottom, x, top + h / 3, t, red, green, blue);
            CmdClearFreeLine(renderer, commandBuffer, x, top + h / 3, left + (w * 3) / 4, bottom, t, red, green, blue);
            CmdClearFreeLine(renderer, commandBuffer, left + (w * 3) / 4, bottom, right, top, t, red, green, blue);
            break;
        default:
            break;
        }
    }

    void CmdClearCompassBadge(VulkanMinimapRenderer& renderer, void* commandBuffer, int x, int y, char letter)
    {
        CmdClearSolidDiamond(renderer, commandBuffer, x + 2, y + 3, 17, 0.0f, 0.0f, 0.0f, 0.58f);
        CmdClearSolidDiamond(renderer, commandBuffer, x, y, 16, 0.76f, 0.57f, 0.30f);
        CmdClearSolidDiamond(renderer, commandBuffer, x, y, 13, 0.080f, 0.062f, 0.044f);
        CmdClearCircle(renderer, commandBuffer, 0.82f, 0.63f, 0.34f, 1.0f, x, y, 9, 2);
        CmdClearCircle(renderer, commandBuffer, 0.026f, 0.024f, 0.022f, 1.0f, x, y, 7, 2);
        CmdClearCompassLetter(renderer, commandBuffer, letter, x, y + 1, letter == 'W' ? 13 : 12, 0.96f, 0.86f, 0.63f);
    }

    void CmdClearRimJewel(VulkanMinimapRenderer& renderer, void* commandBuffer, int x, int y)
    {
        CmdClearCircle(renderer, commandBuffer, 0.0f, 0.0f, 0.0f, 0.50f, x + 1, y + 1, 7, 2);
        CmdClearSolidDiamond(renderer, commandBuffer, x, y, 6, 0.76f, 0.56f, 0.27f);
        CmdClearSolidDiamond(renderer, commandBuffer, x, y, 4, 0.045f, 0.115f, 0.125f);
        CmdClearSolidDiamond(renderer, commandBuffer, x, y, 3, 0.32f, 0.90f, 0.96f);
        CmdClearSolidDiamond(renderer, commandBuffer, x, y, 1, 0.86f, 1.0f, 1.0f);
    }

    void CmdClearCompassFrameBase(VulkanMinimapRenderer& renderer, void* commandBuffer, int cx, int cy, int radius)
    {
        CmdClearRingBand(renderer, commandBuffer, 0.0f, 0.0f, 0.0f, 0.50f, cx + 2, cy + 3, radius + 15, radius + 2, 4);
        CmdClearRingBand(renderer, commandBuffer, 0.016f, 0.015f, 0.014f, 1.0f, cx, cy, radius + 13, radius + 1, 3);
        CmdClearRingBand(renderer, commandBuffer, 0.45f, 0.32f, 0.17f, 1.0f, cx, cy, radius + 13, radius + 12, 2);
        CmdClearRingBand(renderer, commandBuffer, 0.086f, 0.072f, 0.054f, 1.0f, cx, cy, radius + 11, radius + 6, 2);
        CmdClearRingBand(renderer, commandBuffer, 0.28f, 0.20f, 0.11f, 1.0f, cx, cy, radius + 6, radius + 5, 2);
        CmdClearRingBand(renderer, commandBuffer, 0.026f, 0.025f, 0.023f, 1.0f, cx, cy, radius + 3, radius + 1, 2);
    }

    void CmdClearCompassFrameOverlay(VulkanMinimapRenderer& renderer, void* commandBuffer, int cx, int cy, int radius)
    {
        CmdClearRingBand(renderer, commandBuffer, 0.90f, 0.71f, 0.38f, 1.0f, cx, cy, radius + 14, radius + 13, 2);
        CmdClearRingBand(renderer, commandBuffer, 0.25f, 0.17f, 0.085f, 1.0f, cx, cy, radius + 11, radius + 10, 2);
        CmdClearRingBand(renderer, commandBuffer, 0.76f, 0.56f, 0.28f, 1.0f, cx, cy, radius + 5, radius + 4, 2);
        CmdClearRingBand(renderer, commandBuffer, 0.96f, 0.78f, 0.45f, 1.0f, cx, cy, radius + 1, radius, 2);
        CmdClearRingBand(renderer, commandBuffer, 0.040f, 0.045f, 0.043f, 1.0f, cx, cy, radius - 6, radius - 8, 2);
        CmdClearRingBand(renderer, commandBuffer, 0.45f, 0.41f, 0.32f, 1.0f, cx, cy, radius - 40, radius - 41, 2);

        constexpr float kPi = 3.14159265358979323846f;
        for (int index = 0; index < 32; ++index)
        {
            const bool cardinalTick = (index % 8) == 0;
            const bool major = (index % 4) == 0;
            const float angle = (static_cast<float>(index) / 32.0f) * 2.0f * kPi;
            const int outer = radius + 11;
            const int inner = cardinalTick ? radius + 2 : (major ? radius + 5 : radius + 8);
            const int x0 = cx + static_cast<int>(std::sin(angle) * static_cast<float>(inner));
            const int y0 = cy - static_cast<int>(std::cos(angle) * static_cast<float>(inner));
            const int x1 = cx + static_cast<int>(std::sin(angle) * static_cast<float>(outer));
            const int y1 = cy - static_cast<int>(std::cos(angle) * static_cast<float>(outer));
            CmdClearFreeLine(renderer, commandBuffer, x0, y0, x1, y1, cardinalTick ? 2 : 1, major ? 0.70f : 0.46f, major ? 0.52f : 0.34f, major ? 0.28f : 0.17f);
        }

        const int badge = radius + 13;
        CmdClearCompassBadge(renderer, commandBuffer, cx, cy - badge, 'N');
        CmdClearCompassBadge(renderer, commandBuffer, cx + badge, cy, 'E');
        CmdClearCompassBadge(renderer, commandBuffer, cx, cy + badge, 'S');
        CmdClearCompassBadge(renderer, commandBuffer, cx - badge, cy, 'W');

        const int jewel = radius + 9;
        const std::array<float, 4> jewelAngles = {
            kPi * 0.25f,
            kPi * 0.75f,
            kPi * 1.25f,
            kPi * 1.75f
        };
        for (const float angle : jewelAngles)
        {
            const int x = cx + static_cast<int>(std::sin(angle) * static_cast<float>(jewel));
            const int y = cy - static_cast<int>(std::cos(angle) * static_cast<float>(jewel));
            CmdClearRimJewel(renderer, commandBuffer, x, y);
        }
    }

    void DrawMinimapWidget(VulkanMinimapRenderer& renderer, void* commandBuffer)
    {
        const int shortEdge = static_cast<int>(MinValue(renderer.width, renderer.height));
        const int radius = ClampValue(shortEdge / 11, 104, 142);
        const int marginX = MaxValue(58, static_cast<int>(renderer.width) / 58);
        const int marginY = MaxValue(52, static_cast<int>(renderer.height) / 42);
        const int cx = static_cast<int>(renderer.width) - marginX - radius;
        const int cy = marginY + radius;
        const int zoomStep = ClampValue(g_minimapZoomStep.load(), -3, 3);
        const float zoom = std::pow(1.32f, static_cast<float>(zoomStep));
        const float unitsPerPixel = MINIMAP_BASE_UNITS_PER_PIXEL / zoom;

        const std::vector<CapturedWaypoint> waypoints = CopyWaypoints();
        const std::vector<CapturedNearbyMarker> nearbyMarkers = CopyNearbyMarkers();
        const std::vector<CapturedMapMarkerVisibility> visibleMapMarkers = CopyVisibleMapMarkers();
        std::vector<MinimapWorldPoint> points;
        BuildWorldPoints(points, waypoints, nearbyMarkers, visibleMapMarkers);

        CapturedPlayerPosition playerPosition{};
        if (!TryGetPlayerPosition(playerPosition))
            return;

        const float centerX = FixedToWorld(playerPosition.x);
        const float centerZ = FixedToWorld(playerPosition.z);
        const float mapHeading = playerPosition.hasHeading ? playerPosition.headingRadians : 0.0f;
        AppendVisibleStaticPoiPoints(points, nearbyMarkers, visibleMapMarkers, centerX, centerZ, unitsPerPixel * static_cast<float>(radius) * STATIC_POI_DRAW_RADIUS_FACTOR);

        const bool hasRasterFrame = HasLoadedMinimapFrame();
        const int frameExtra = MinimapRasterFrameExtra(radius);
        const int frameMapRadius = hasRasterFrame ? MinimapRasterMapRadius(radius, frameExtra) : radius - 6;
        if (!hasRasterFrame)
            CmdClearCompassFrameBase(renderer, commandBuffer, cx, cy, radius);

        DrawRealMap(renderer, commandBuffer, cx, cy, frameMapRadius, centerX, centerZ, unitsPerPixel, mapHeading);

        if (hasRasterFrame)
        {
            if (!TryDrawMinimapTexturedFrame(renderer, commandBuffer, cx, cy, radius, frameExtra, mapHeading))
                TryDrawMinimapRasterFrame(renderer, commandBuffer, cx, cy, radius, frameExtra);
        }
        else
            CmdClearCompassFrameOverlay(renderer, commandBuffer, cx, cy, radius);

        const int pointProjectionRadius = hasRasterFrame ? frameMapRadius : radius;
        const int pointClipRadius = hasRasterFrame ? frameMapRadius : radius - 14;
        for (const MinimapWorldPoint& point : points)
        {
            int px = cx;
            int py = cy;
            bool clipped = false;
            ProjectWorldToMinimap(point.x, point.z, centerX, centerZ, unitsPerPixel, mapHeading, cx, cy, pointProjectionRadius, px, py, clipped);

            CmdClearPoiIcon(renderer, commandBuffer, cx, cy, pointClipRadius, px, py, point.kind, clipped);
        }

        if (!TryDrawPremiumPlayerArrow(renderer, commandBuffer, cx, cy, radius))
            CmdClearPlayerArrow(renderer, commandBuffer, cx, cy, radius);
    }

    bool RecordVulkanMinimapCommandLocked(std::uint32_t imageIndex)
    {
        if (!g_renderer.ready || imageIndex >= g_renderer.commandBuffers.size() || imageIndex >= g_renderer.framebuffers.size())
            return false;

        void* commandBuffer = reinterpret_cast<void*>(g_renderer.commandBuffers[imageIndex]);
        if (g_renderer.fns.resetCommandBuffer(commandBuffer, 0) != VK_SUCCESS)
            return false;

        VkCommandBufferBeginInfo beginInfo{};
        if (g_renderer.fns.beginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
            return false;

        RecordFrameTextureUploadIfNeeded(g_renderer, commandBuffer);

        VkRenderPassBeginInfo renderPassBegin{};
        renderPassBegin.renderPass = reinterpret_cast<void*>(g_renderer.renderPass);
        renderPassBegin.framebuffer = reinterpret_cast<void*>(g_renderer.framebuffers[imageIndex]);
        renderPassBegin.renderArea.offset.x = 0;
        renderPassBegin.renderArea.offset.y = 0;
        renderPassBegin.renderArea.extent.width = g_renderer.width;
        renderPassBegin.renderArea.extent.height = g_renderer.height;

        g_renderer.fns.cmdBeginRenderPass(commandBuffer, &renderPassBegin, 0);
        DrawMinimapWidget(g_renderer, commandBuffer);
        g_renderer.fns.cmdEndRenderPass(commandBuffer);
        return g_renderer.fns.endCommandBuffer(commandBuffer) == VK_SUCCESS;
    }

    bool TryReadPresentTargets(const VkPresentInfoKHR& info, uintptr_t& firstSwapchain, std::uint32_t& imageIndex)
    {
        firstSwapchain = 0;
        imageIndex = 0;

        if (info.swapchainCount == 0 || info.swapchainCount > 8 || info.pSwapchains == nullptr || info.pImageIndices == nullptr)
            return false;

        return SafeReadValue(reinterpret_cast<uintptr_t>(info.pSwapchains), firstSwapchain) &&
            SafeReadValue(reinterpret_cast<uintptr_t>(info.pImageIndices), imageIndex) &&
            firstSwapchain != 0;
    }

    bool TrySubmitVulkanMinimap(void* queue, const VkPresentInfoKHR& info, uintptr_t firstSwapchain, std::uint32_t imageIndex, VkPresentInfoKHR& adjustedInfo, const void** adjustedWaitSemaphore)
    {
        if (queue == nullptr || firstSwapchain == 0 || info.waitSemaphoreCount > 8)
            return false;

        TryRefreshPlayerPositionFromCachedCamera();
        if (!ShouldDrawMinimapInWorld())
            return false;

        if (info.waitSemaphoreCount != 0 && info.pWaitSemaphores == nullptr)
            return false;

        SwapchainRuntimeInfo snapshot{};
        if (!TryGetSwapchainSnapshot(firstSwapchain, snapshot))
        {
            snapshot.handle = firstSwapchain;
            snapshot.device = g_lastVulkanDevice.load();
        }

        if (snapshot.device == 0)
            return false;

        std::lock_guard<std::mutex> lock(g_rendererMutex);
        if (!BuildVulkanMinimapRendererLocked(snapshot))
            return false;

        if (imageIndex >= g_renderer.commandBuffers.size() || imageIndex >= g_renderer.renderCompleteSemaphores.size())
            return false;

        if (!RecordVulkanMinimapCommandLocked(imageIndex))
        {
            LogRendererThrottled("[Minimap] Vulkan minimap draw skipped: command recording failed");
            return false;
        }

        std::vector<std::uint32_t> waitStages(info.waitSemaphoreCount, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        const void* commandBuffer = reinterpret_cast<void*>(g_renderer.commandBuffers[imageIndex]);
        const void* signalSemaphore = reinterpret_cast<void*>(g_renderer.renderCompleteSemaphores[imageIndex]);

        VkSubmitInfo submitInfo{};
        submitInfo.waitSemaphoreCount = info.waitSemaphoreCount;
        submitInfo.pWaitSemaphores = info.pWaitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages.empty() ? nullptr : waitStages.data();
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &signalSemaphore;

        const std::int32_t submitResult = g_renderer.fns.queueSubmit(queue, 1, &submitInfo, nullptr);
        if (submitResult != VK_SUCCESS)
        {
            std::ostringstream oss;
            oss << "[Minimap] Vulkan minimap draw skipped: queue submit failed"
                << " | result=" << submitResult
                << " | image_index=" << imageIndex;
            LogRendererThrottled(oss.str());
            return false;
        }

        adjustedWaitSemaphore[0] = signalSemaphore;
        adjustedInfo = info;
        adjustedInfo.waitSemaphoreCount = 1;
        adjustedInfo.pWaitSemaphores = adjustedWaitSemaphore;
        return true;
    }

    void UpdateMinimapZoomHotkeys()
    {
        int step = g_minimapZoomStep.load();
        bool changed = false;
        if ((GetAsyncKeyState(VK_ADD) & 0x0001) != 0 ||
            (GetAsyncKeyState(VK_OEM_PLUS) & 0x0001) != 0)
        {
            step = ClampValue(step + 1, -3, 3);
            changed = true;
        }

        if ((GetAsyncKeyState(VK_SUBTRACT) & 0x0001) != 0 ||
            (GetAsyncKeyState(VK_OEM_MINUS) & 0x0001) != 0)
        {
            step = ClampValue(step - 1, -3, 3);
            changed = true;
        }

        if (changed)
        {
            g_minimapZoomStep.store(step);
            std::ostringstream oss;
            oss << "[Minimap] zoom step=" << step;
            Log(oss.str());
        }
    }

    void LogVulkanPresentInfo(void* queue, const void* presentInfo)
    {
        const DWORD now = GetTickCount();
        if (now - g_lastVulkanPresentInfoTick < 5000)
            return;

        g_lastVulkanPresentInfoTick = now;

        VkPresentInfoKHR info{};
        const bool hasPresentInfo = presentInfo != nullptr &&
            SafeReadValue(reinterpret_cast<uintptr_t>(presentInfo), info);

        uintptr_t firstSwapchain = 0;
        std::uint32_t imageIndex = 0;
        bool hasImageIndex = false;
        if (hasPresentInfo && info.swapchainCount != 0 && info.swapchainCount <= 8)
        {
            SafeReadValue(reinterpret_cast<uintptr_t>(info.pSwapchains), firstSwapchain);
            if (info.pImageIndices != nullptr)
                hasImageIndex = SafeReadValue(reinterpret_cast<uintptr_t>(info.pImageIndices), imageIndex);
        }

        SwapchainRuntimeInfo swapchain{};
        const bool knownSwapchain = firstSwapchain != 0 && TryGetSwapchainSnapshot(firstSwapchain, swapchain);

        std::ostringstream oss;
        oss << "[Minimap] vkQueuePresentKHR hook"
            << " | queue=" << Hex(reinterpret_cast<uintptr_t>(queue))
            << " | present_info=" << Hex(reinterpret_cast<uintptr_t>(presentInfo))
            << " | render_context=" << Hex(g_lastRenderContext.load())
            << " | device=" << Hex(g_lastVulkanDevice.load())
            << " | graphics_context=" << Hex(g_lastGraphicsContext.load())
            << " | swapchain_state=" << Hex(g_lastSwapchainState.load());

        if (hasPresentInfo)
        {
            oss << " | wait_semaphores=" << info.waitSemaphoreCount
                << " | swapchain_count=" << info.swapchainCount
                << " | first_swapchain=" << Hex(firstSwapchain);
            if (hasImageIndex)
                oss << " | image_index=" << imageIndex;
        }
        else
        {
            oss << " | present_info_read=failed";
        }

        if (knownSwapchain)
        {
            oss << " | swapchain_format=" << swapchain.format
                << " | extent=" << swapchain.width << "x" << swapchain.height
                << " | images=" << swapchain.imageCount
                << " | cached_images=" << swapchain.images.size();
        }

        Log(oss.str());
    }

    void LogVulkanSwapchainCreate(void* device, const VkSwapchainCreateInfoKHR* createInfo, void* swapchain, std::int32_t result)
    {
        const DWORD now = GetTickCount();
        if (now - g_lastVulkanSwapchainHookTick < 5000)
            return;

        g_lastVulkanSwapchainHookTick = now;

        VkSwapchainCreateInfoKHR info{};
        const bool hasCreateInfo = TryCopySwapchainCreateInfo(createInfo, info);

        std::ostringstream oss;
        oss << "[Minimap] vkCreateSwapchainKHR"
            << " | result=" << result
            << " | device=" << Hex(reinterpret_cast<uintptr_t>(device))
            << " | swapchain=" << Hex(reinterpret_cast<uintptr_t>(swapchain));
        if (hasCreateInfo)
        {
            oss << " | format=" << info.imageFormat
                << " | extent=" << info.imageExtent.width << "x" << info.imageExtent.height
                << " | min_images=" << info.minImageCount
                << " | present_mode=" << info.presentMode
                << " | sharing_mode=" << info.imageSharingMode
                << " | queue_family_count=" << info.queueFamilyIndexCount;
        }
        Log(oss.str());
    }

    std::int32_t __fastcall HookCreateSwapchain(void* device, const VkSwapchainCreateInfoKHR* createInfo, const void* allocator, void** swapchain)
    {
        if (device != nullptr)
            g_lastVulkanDevice.store(reinterpret_cast<uintptr_t>(device));

        const uintptr_t original = g_originalCreateSwapchain.load();
        auto* originalFn = reinterpret_cast<CreateSwapchainFn>(original);
        const std::int32_t result = originalFn != nullptr ? originalFn(device, createInfo, allocator, swapchain) : -1;
        void* swapchainHandle = swapchain != nullptr ? *swapchain : nullptr;
        RememberSwapchainCreate(device, createInfo, swapchainHandle, result);
        LogVulkanSwapchainCreate(device, createInfo, swapchainHandle, result);
        return result;
    }

    std::int32_t __fastcall HookGetSwapchainImages(void* device, void* swapchain, std::uint32_t* count, void* images)
    {
        if (device != nullptr)
            g_lastVulkanDevice.store(reinterpret_cast<uintptr_t>(device));

        const uintptr_t original = g_originalGetSwapchainImages.load();
        auto* originalFn = reinterpret_cast<GetSwapchainImagesFn>(original);
        const std::int32_t result = originalFn != nullptr ? originalFn(device, swapchain, count, images) : -1;
        RememberSwapchainImages(device, swapchain, count, images, result);

        const DWORD now = GetTickCount();
        if (now - g_lastVulkanSwapchainImagesHookTick >= 5000)
        {
            g_lastVulkanSwapchainImagesHookTick = now;
            std::uint32_t imageCount = 0;
            if (count != nullptr)
                SafeReadValue(reinterpret_cast<uintptr_t>(count), imageCount);

            std::ostringstream oss;
            oss << "[Minimap] vkGetSwapchainImagesKHR"
                << " | result=" << result
                << " | device=" << Hex(reinterpret_cast<uintptr_t>(device))
                << " | swapchain=" << Hex(reinterpret_cast<uintptr_t>(swapchain))
                << " | count=" << imageCount
                << " | images_ptr=" << Hex(reinterpret_cast<uintptr_t>(images));
            Log(oss.str());
        }

        return result;
    }

    std::int32_t __fastcall HookQueuePresent(void* queue, const void* presentInfo)
    {
        TryScanVulkanDeviceFunctions();
        LogVulkanPresentInfo(queue, presentInfo);

        const uintptr_t original = g_originalQueuePresent.load();
        auto* originalFn = reinterpret_cast<QueuePresentFn>(original);
        if (originalFn == nullptr)
            return -1;

        VkPresentInfoKHR info{};
        uintptr_t firstSwapchain = 0;
        std::uint32_t imageIndex = 0;
        VkPresentInfoKHR adjustedInfo{};
        const void* adjustedWaitSemaphore[1] = {};

        const bool canDraw = presentInfo != nullptr &&
            SafeReadValue(reinterpret_cast<uintptr_t>(presentInfo), info) &&
            TryReadPresentTargets(info, firstSwapchain, imageIndex) &&
            TrySubmitVulkanMinimap(queue, info, firstSwapchain, imageIndex, adjustedInfo, adjustedWaitSemaphore);

        return originalFn(queue, canDraw ? &adjustedInfo : presentInfo);
    }

    bool HookVulkanTableFunction(uintptr_t table, std::size_t offset, void* hook, std::atomic<uintptr_t>& original, const char* label)
    {
        const uintptr_t slot = table + offset;
        uintptr_t current = 0;
        if (!SafeReadValue(slot, current) || current == 0)
            return false;

        const uintptr_t hookAddress = reinterpret_cast<uintptr_t>(hook);
        if (current == hookAddress)
            return original.load() != 0;

        uintptr_t expectedZero = 0;
        original.compare_exchange_strong(expectedZero, current);

        if (!WritePointer(slot, hookAddress))
        {
            std::ostringstream oss;
            oss << "[Minimap] failed to hook Vulkan table " << label
                << " at " << Hex(slot);
            Log(oss.str());
            return false;
        }

        return true;
    }

    bool TryInstallVulkanTableHooks(uintptr_t table)
    {
        if (table == 0)
            return false;

        std::lock_guard<std::mutex> lock(g_vulkanHookMutex);
        if (g_patchedVulkanDeviceTable.load() == table &&
            g_originalQueuePresent.load() != 0 &&
            g_originalCreateSwapchain.load() != 0 &&
            g_originalGetSwapchainImages.load() != 0)
        {
            return true;
        }

        const bool createOk = HookVulkanTableFunction(
            table,
            VULKAN_TABLE_CREATE_SWAPCHAIN_OFFSET,
            reinterpret_cast<void*>(&HookCreateSwapchain),
            g_originalCreateSwapchain,
            "vkCreateSwapchainKHR"
        );
        const bool imagesOk = HookVulkanTableFunction(
            table,
            VULKAN_TABLE_GET_SWAPCHAIN_IMAGES_OFFSET,
            reinterpret_cast<void*>(&HookGetSwapchainImages),
            g_originalGetSwapchainImages,
            "vkGetSwapchainImagesKHR"
        );
        const bool presentOk = HookVulkanTableFunction(
            table,
            VULKAN_TABLE_QUEUE_PRESENT_OFFSET,
            reinterpret_cast<void*>(&HookQueuePresent),
            g_originalQueuePresent,
            "vkQueuePresentKHR"
        );

        if (createOk || imagesOk || presentOk)
            g_patchedVulkanDeviceTable.store(table);

        const DWORD now = GetTickCount();
        if (now - g_lastVulkanHookSummaryTick >= 5000)
        {
            g_lastVulkanHookSummaryTick = now;
            std::ostringstream oss;
            oss << "[Minimap] Vulkan table hooks"
                << " | table=" << Hex(table)
                << " | create_swapchain=" << (createOk ? "hooked" : "missing")
                << " | get_swapchain_images=" << (imagesOk ? "hooked" : "missing")
                << " | queue_present=" << (presentOk ? "hooked" : "missing")
                << " | no_external_overlay=true";
            Log(oss.str());
        }

        return createOk && imagesOk && presentOk;
    }

    void RestoreVulkanTableFunction(uintptr_t table, std::size_t offset, void* hook, std::atomic<uintptr_t>& original)
    {
        const uintptr_t originalValue = original.load();
        if (table == 0 || originalValue == 0)
            return;

        const uintptr_t slot = table + offset;
        uintptr_t current = 0;
        if (!SafeReadValue(slot, current))
            return;

        if (current == reinterpret_cast<uintptr_t>(hook))
            WritePointer(slot, originalValue);

        original.store(0);
    }

    void RestoreVulkanTableHooks()
    {
        std::lock_guard<std::mutex> lock(g_vulkanHookMutex);
        const uintptr_t table = g_patchedVulkanDeviceTable.load();
        RestoreVulkanTableFunction(table, VULKAN_TABLE_CREATE_SWAPCHAIN_OFFSET, reinterpret_cast<void*>(&HookCreateSwapchain), g_originalCreateSwapchain);
        RestoreVulkanTableFunction(table, VULKAN_TABLE_GET_SWAPCHAIN_IMAGES_OFFSET, reinterpret_cast<void*>(&HookGetSwapchainImages), g_originalGetSwapchainImages);
        RestoreVulkanTableFunction(table, VULKAN_TABLE_QUEUE_PRESENT_OFFSET, reinterpret_cast<void*>(&HookQueuePresent), g_originalQueuePresent);
        g_patchedVulkanDeviceTable.store(0);
    }

    bool LooksLikeVulkanDeviceTable(uintptr_t candidate, uintptr_t vulkanModule, uintptr_t vkGetInstanceProcAddr)
    {
        uintptr_t module = 0;
        uintptr_t getInstanceProcAddr = 0;
        uintptr_t createInstance = 0;
        uintptr_t getDeviceProcAddr = 0;
        uintptr_t createSwapchain = 0;
        uintptr_t queuePresent = 0;

        if (!SafeReadValue(candidate + 0x10, module) ||
            !SafeReadValue(candidate + 0x18, getInstanceProcAddr) ||
            !SafeReadValue(candidate + 0x20, createInstance) ||
            !SafeReadValue(candidate + 0x78, getDeviceProcAddr) ||
            !SafeReadValue(candidate + VULKAN_TABLE_CREATE_SWAPCHAIN_OFFSET, createSwapchain) ||
            !SafeReadValue(candidate + VULKAN_TABLE_QUEUE_PRESENT_OFFSET, queuePresent))
        {
            return false;
        }

        return module == vulkanModule &&
            getInstanceProcAddr == vkGetInstanceProcAddr &&
            createInstance != 0 &&
            getDeviceProcAddr != 0 &&
            createSwapchain != 0 &&
            queuePresent != 0;
    }

    bool TryFindExistingVulkanDeviceTable()
    {
        if (g_vulkanDeviceTable.load() != 0)
            return true;

        HMODULE vulkanModule = GetModuleHandleA("vulkan-1.dll");
        if (vulkanModule == nullptr)
            return false;

        FARPROC getInstanceProcAddr = GetProcAddress(vulkanModule, "vkGetInstanceProcAddr");
        if (getInstanceProcAddr == nullptr)
            return false;

        SYSTEM_INFO systemInfo{};
        GetSystemInfo(&systemInfo);

        uintptr_t cursor = reinterpret_cast<uintptr_t>(systemInfo.lpMinimumApplicationAddress);
        const uintptr_t maximum = reinterpret_cast<uintptr_t>(systemInfo.lpMaximumApplicationAddress);
        constexpr std::size_t TABLE_MIN_SIZE = VULKAN_TABLE_QUEUE_PRESENT_OFFSET + sizeof(uintptr_t);

        MEMORY_BASIC_INFORMATION info{};
        while (cursor < maximum && VirtualQuery(reinterpret_cast<const void*>(cursor), &info, sizeof(info)) == sizeof(info))
        {
            const uintptr_t regionBase = reinterpret_cast<uintptr_t>(info.BaseAddress);
            const uintptr_t next = regionBase + info.RegionSize;

            if (info.State == MEM_COMMIT &&
                info.Type == MEM_PRIVATE &&
                info.RegionSize >= TABLE_MIN_SIZE &&
                IsReadablePage(info.Protect))
            {
                const uintptr_t first = (regionBase + 7) & ~uintptr_t(7);
                const uintptr_t last = next > TABLE_MIN_SIZE ? next - TABLE_MIN_SIZE : first;
                for (uintptr_t candidate = first; candidate <= last; candidate += sizeof(uintptr_t))
                {
                    if (LooksLikeVulkanDeviceTable(
                        candidate,
                        reinterpret_cast<uintptr_t>(vulkanModule),
                        reinterpret_cast<uintptr_t>(getInstanceProcAddr)))
                    {
                        g_vulkanDeviceTable.store(candidate);
                        std::ostringstream oss;
                        oss << "[Minimap] found existing Vulkan device table at " << Hex(candidate);
                        Log(oss.str());
                        return true;
                    }
                }
            }

            if (next <= cursor)
                break;
            cursor = next;
        }

        return false;
    }

    void ProbeVulkanTable()
    {
        if (g_vulkanDeviceTable.load() == 0)
        {
            const DWORD now = GetTickCount();
            if (g_vulkanScanAttempts < 3 && now - g_lastVulkanScanTick >= 10000)
            {
                g_lastVulkanScanTick = now;
                ++g_vulkanScanAttempts;
                TryFindExistingVulkanDeviceTable();
            }
        }

        const uintptr_t table = g_vulkanDeviceTable.load();
        if (table == 0)
            return;

        TryInstallVulkanTableHooks(table);

        uintptr_t queueSubmit = 0;
        uintptr_t createSwapchain = 0;
        uintptr_t getSwapchainImages = 0;
        uintptr_t acquireNextImage = 0;
        uintptr_t queuePresent = 0;

        if (!ReadVulkanTableFunction(table, VULKAN_TABLE_QUEUE_SUBMIT_OFFSET, queueSubmit) ||
            !ReadVulkanTableFunction(table, VULKAN_TABLE_CREATE_SWAPCHAIN_OFFSET, createSwapchain) ||
            !ReadVulkanTableFunction(table, VULKAN_TABLE_GET_SWAPCHAIN_IMAGES_OFFSET, getSwapchainImages) ||
            !ReadVulkanTableFunction(table, VULKAN_TABLE_ACQUIRE_NEXT_IMAGE_OFFSET, acquireNextImage) ||
            !ReadVulkanTableFunction(table, VULKAN_TABLE_QUEUE_PRESENT_OFFSET, queuePresent))
        {
            return;
        }

        const DWORD now = GetTickCount();
        if (now - g_lastVulkanSummaryTick < 5000)
            return;

        g_lastVulkanSummaryTick = now;

        std::ostringstream oss;
        oss << "[Minimap] Vulkan table"
            << " | table=" << Hex(table)
            << " | queueSubmit=" << Hex(queueSubmit)
            << " | createSwapchain=" << Hex(createSwapchain)
            << " | getSwapchainImages=" << Hex(getSwapchainImages)
            << " | acquireNextImage=" << Hex(acquireNextImage)
            << " | queuePresent=" << Hex(queuePresent);
        Log(oss.str());
    }

    void __fastcall CaptureVulkanDeviceTableInitHook(void* table, void*, void*, void*)
    {
        if (table == nullptr)
            return;

        const uintptr_t previous = g_vulkanDeviceTable.exchange(reinterpret_cast<uintptr_t>(table));
        TryInstallVulkanTableHooks(reinterpret_cast<uintptr_t>(table));
        if (previous != reinterpret_cast<uintptr_t>(table))
        {
            std::ostringstream oss;
            oss << "[Minimap] captured Vulkan device table candidate at "
                << Hex(reinterpret_cast<uintptr_t>(table));
            Log(oss.str());
        }
    }

    class MinimapMod : public Mod
    {
    public:
        void Load(ModContext* modContext) override
        {
            if (!modContext->game.isClient)
            {
                modContext->Log("[Minimap] client-only mod; skipping load on server");
                return;
            }

            g_modContext = modContext;
            g_exeBase = reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr));
            g_iterInit = reinterpret_cast<IterInitFn>(g_exeBase + RVA_ITER_INIT);
            g_iterNext = reinterpret_cast<IterNextFn>(g_exeBase + RVA_ITER_NEXT);

            g_localPlayerUiRenderSetupHook = InstallEntryHook(
                RVA_LOCAL_PLAYER_UI_RENDER_SETUP,
                g_localPlayerUiRenderSetupExpected,
                reinterpret_cast<void*>(&CaptureUiRenderSetupHook),
                "local_player_ui_render_setup"
            );

            g_playerWaypointsUiHook = InstallEntryHook(
                RVA_PLAYER_WAYPOINTS_UI,
                g_playerWaypointsUiExpected,
                reinterpret_cast<void*>(&CaptureWaypointsHook),
                "player_waypoints_ui"
            );

            g_mapMarkerVisibilityHook = InstallMapMarkerVisibilityLoopHook(
                reinterpret_cast<void*>(&CaptureMapMarkerVisibilityRecordHook)
            );

            g_renderPresentFrameHook = InstallEntryHook(
                RVA_RENDER_PRESENT_FRAME,
                g_renderPresentFrameExpected,
                reinterpret_cast<void*>(&CaptureRenderPresentFrameHook),
                "render_present_frame"
            );

            g_vulkanDeviceTableInitHook = InstallEntryHook(
                RVA_VULKAN_DEVICE_TABLE_INIT,
                g_vulkanDeviceTableInitExpected,
                reinterpret_cast<void*>(&CaptureVulkanDeviceTableInitHook),
                "vulkan_device_table_init"
            );

            std::ostringstream oss;
            oss << "[Minimap] load complete | mode=internal_data_bridge"
                << " | external_overlay=disabled"
                << " | ui_hook=" << (g_localPlayerUiRenderSetupHook != nullptr ? "ready" : "missing")
                << " | waypoint_hook=" << (g_playerWaypointsUiHook != nullptr ? "ready" : "missing")
                << " | marker_visibility_hook=" << (g_mapMarkerVisibilityHook != nullptr ? "ready" : "missing")
                << " | render_hook=" << (g_renderPresentFrameHook != nullptr ? "ready" : "missing")
                << " | vulkan_probe=" << (g_vulkanDeviceTableInitHook != nullptr ? "ready" : "missing");
            modContext->Log(oss.str().c_str());
            loaded = true;
        }

        void Unload(ModContext* modContext) override
        {
            {
                std::lock_guard<std::mutex> lock(g_rendererMutex);
                DestroyVulkanMinimapRendererLocked();
            }
            RestoreVulkanTableHooks();

            if (g_localPlayerUiRenderSetupHook != nullptr)
            {
                g_localPlayerUiRenderSetupHook->deactivate();
                delete g_localPlayerUiRenderSetupHook;
                g_localPlayerUiRenderSetupHook = nullptr;
            }

            if (g_playerWaypointsUiHook != nullptr)
            {
                g_playerWaypointsUiHook->deactivate();
                delete g_playerWaypointsUiHook;
                g_playerWaypointsUiHook = nullptr;
            }

            if (g_mapMarkerVisibilityHook != nullptr)
            {
                g_mapMarkerVisibilityHook->deactivate();
                delete g_mapMarkerVisibilityHook;
                g_mapMarkerVisibilityHook = nullptr;
            }

            if (g_renderPresentFrameHook != nullptr)
            {
                g_renderPresentFrameHook->deactivate();
                delete g_renderPresentFrameHook;
                g_renderPresentFrameHook = nullptr;
            }

            if (g_vulkanDeviceTableInitHook != nullptr)
            {
                g_vulkanDeviceTableInitHook->deactivate();
                delete g_vulkanDeviceTableInitHook;
                g_vulkanDeviceTableInitHook = nullptr;
            }

            {
                std::lock_guard<std::mutex> lock(g_waypointMutex);
                g_waypoints.clear();
            }
            {
                std::lock_guard<std::mutex> lock(g_nearbyMarkerMutex);
                g_nearbyMarkers.clear();
            }
            {
                std::lock_guard<std::mutex> lock(g_visibleMapMarkerMutex);
                g_visibleMapMarkers.clear();
            }
            {
                std::lock_guard<std::mutex> lock(g_playerPositionMutex);
                g_playerPosition = {};
            }
            {
                std::lock_guard<std::mutex> lock(g_swapchainMutex);
                g_swapchains.clear();
            }

            g_worldSessionReady.store(false);
            g_gameSessionOnline.store(false);
            g_lastWorldDataTick.store(0);
            g_playerCameraAddress.store(0);
            g_lastPlayerCameraScanTick = 0;
            g_gameLogPath.clear();
            g_lastSessionLogPollTick = 0;
            g_vulkanDeviceTable.store(0);
            g_lastRenderContext.store(0);
            g_lastVulkanDevice.store(0);
            g_lastGraphicsContext.store(0);
            g_lastSwapchainState.store(0);
            g_lastVulkanQueueFamilyIndex.store(0);
            g_vulkanScanAttempts = 0;
            g_lastVulkanScanTick = 0;
            g_lastVulkanFunctionScanTick = 0;
            g_vulkanFunctionOffsetsLogged = false;
            g_iterInit = nullptr;
            g_iterNext = nullptr;
            g_modContext = nullptr;
            active = false;
            loaded = false;
            modContext->Log("[Minimap] unloaded");
        }

        void Activate(ModContext* modContext) override
        {
            const bool uiOk = ActivateHook(g_localPlayerUiRenderSetupHook);
            const bool waypointOk = ActivateHook(g_playerWaypointsUiHook);
            const bool markerVisibilityOk = ActivateHook(g_mapMarkerVisibilityHook);
            const bool renderOk = ActivateHook(g_renderPresentFrameHook);
            const bool vulkanOk = ActivateHook(g_vulkanDeviceTableInitHook);
            active = uiOk || waypointOk || markerVisibilityOk || renderOk || vulkanOk;
            if (active)
                TryFindExistingVulkanDeviceTable();

            std::ostringstream oss;
            oss << "[Minimap] activate internal bridge"
                << " | ui_hook=" << HookActivationState(g_localPlayerUiRenderSetupHook)
                << " | waypoint_hook=" << HookActivationState(g_playerWaypointsUiHook)
                << " | marker_visibility_hook=" << HookActivationState(g_mapMarkerVisibilityHook)
                << " | render_hook=" << HookActivationState(g_renderPresentFrameHook)
                << " | vulkan_probe=" << HookActivationState(g_vulkanDeviceTableInitHook)
                << " | external_overlay=disabled";
            modContext->Log(oss.str().c_str());
        }

        void Deactivate(ModContext* modContext) override
        {
            {
                std::lock_guard<std::mutex> lock(g_rendererMutex);
                DestroyVulkanMinimapRendererLocked();
            }

            if (g_localPlayerUiRenderSetupHook != nullptr)
                g_localPlayerUiRenderSetupHook->deactivate();

            if (g_playerWaypointsUiHook != nullptr)
                g_playerWaypointsUiHook->deactivate();

            if (g_mapMarkerVisibilityHook != nullptr)
                g_mapMarkerVisibilityHook->deactivate();

            if (g_renderPresentFrameHook != nullptr)
                g_renderPresentFrameHook->deactivate();

            if (g_vulkanDeviceTableInitHook != nullptr)
                g_vulkanDeviceTableInitHook->deactivate();

            g_worldSessionReady.store(false);
            g_gameSessionOnline.store(false);
            g_lastWorldDataTick.store(0);
            g_playerCameraAddress.store(0);
            g_lastPlayerCameraScanTick = 0;
            {
                std::lock_guard<std::mutex> lock(g_visibleMapMarkerMutex);
                g_visibleMapMarkers.clear();
            }
            {
                std::lock_guard<std::mutex> lock(g_playerPositionMutex);
                g_playerPosition = {};
            }
            active = false;
            modContext->Log("[Minimap] deactivated");
        }

        void Update(ModContext*) override
        {
            if (active)
            {
                PollGameSessionLog();
                UpdateMinimapZoomHotkeys();
                ProbeVulkanTable();
            }
        }

        ModMetaData GetMetaData() override
        {
            return g_metaData;
        }
    };
}

extern "C" __declspec(dllexport) Mod* CreateModInstance()
{
    return new MinimapMod();
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID)
{
    return TRUE;
}
