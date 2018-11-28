#include "../Common/RayTracingApplication.h"

#include <cstring>

class TutorialApplication : public RayTracingApplication
{
public:
    VkDeviceMemory _topASMemory = VK_NULL_HANDLE;
    VkAccelerationStructureNV _topAS = VK_NULL_HANDLE;
    VkDeviceMemory _bottomASMemory = VK_NULL_HANDLE;
    VkAccelerationStructureNV _bottomAS = VK_NULL_HANDLE;

public:
    TutorialApplication();
    virtual ~TutorialApplication();

    virtual void Init() override;

    void CreateAccelerationStructures();         // Tutorial 02
};

TutorialApplication::TutorialApplication()
{
    _appName = "VkRay Tutorial 02: Building Acceleration Structure";
    _deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    _deviceExtensions.push_back(VK_NV_RAY_TRACING_EXTENSION_NAME);
}

TutorialApplication::~TutorialApplication()
{
    if (_topAS)
    {
        vkDestroyAccelerationStructureNV(_device, _topAS, nullptr);
    }
    if (_topASMemory)
    {
        vkFreeMemory(_device, _topASMemory, nullptr);
    }
    if (_bottomAS)
    {
        vkDestroyAccelerationStructureNV(_device, _bottomAS, nullptr);
    }
    if (_bottomASMemory)
    {
        vkFreeMemory(_device, _bottomASMemory, nullptr);
    }
}

void TutorialApplication::Init()
{
    InitRayTracing();

    CreateAccelerationStructures();              // Tutorial 02
}

// ============================================================
// Tutorial 02: Create RayTracing Acceleration Structures
// ============================================================
void TutorialApplication::CreateAccelerationStructures()
{
    // ============================================================
    // 1. CREATE GEOMETRY
    // Convert vertex/index data into buffers and then use the
    // buffers to create VkGeometryNV struct
    // ============================================================

    // Notice that vertex/index buffers have to be alive while
    // geometry is used because it references them
    BufferResource vertexBuffer;
    BufferResource indexBuffer;
    std::vector<VkGeometryNV> geometries;

    {
        struct Vertex
        {
            float X, Y, Z;
        };

        std::vector<Vertex> vertices
        {
            { -0.5f, -0.5f, 0.0f },
            { +0.0f, +0.5f, 0.0f },
            { +0.5f, -0.5f, 0.0f }
        };
        const uint32_t vertexCount = (uint32_t)vertices.size();
        const VkDeviceSize vertexSize = sizeof(Vertex);
        const VkDeviceSize vertexBufferSize = vertexCount * vertexSize;

        std::vector<uint16_t> indices
        {
            { 0, 1, 2 }
        };
        const uint32_t indexCount = (uint32_t)indices.size();
        const VkDeviceSize indexSize = sizeof(uint16_t);
        const VkDeviceSize indexBufferSize = indexCount * indexSize;

        VkResult code;
        bool copied;

        code = vertexBuffer.Create(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        NVVK_CHECK_ERROR(code, "rt vertexBuffer.Create");
        copied = vertexBuffer.CopyToBufferUsingMapUnmap(vertices.data(), vertexBufferSize);
        if (!copied)
        {
            ExitError("Failed to copy vertex buffer");
        }

        code = indexBuffer.Create(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        NVVK_CHECK_ERROR(code, "rt indexBuffer.Create");
        copied = indexBuffer.CopyToBufferUsingMapUnmap(indices.data(), indexBufferSize);
        if (!copied)
        {
            ExitError("Failed to copy index buffer");
        }

        VkGeometryNV geometry;
        geometry.sType = VK_STRUCTURE_TYPE_GEOMETRY_NV;
        geometry.pNext = nullptr;
        geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_NV;
        geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV;
        geometry.geometry.triangles.pNext = nullptr;
        geometry.geometry.triangles.vertexData = vertexBuffer.Buffer;
        geometry.geometry.triangles.vertexOffset = 0;
        geometry.geometry.triangles.vertexCount = vertexCount;
        geometry.geometry.triangles.vertexStride = vertexSize;
        geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        geometry.geometry.triangles.indexData = indexBuffer.Buffer;
        geometry.geometry.triangles.indexOffset = 0;
        geometry.geometry.triangles.indexCount = indexCount;
        geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT16;
        geometry.geometry.triangles.transformData = VK_NULL_HANDLE;
        geometry.geometry.triangles.transformOffset = 0;
        geometry.geometry.aabbs = { };
        geometry.geometry.aabbs.sType = VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV;
        geometry.flags = 0;

        geometries.emplace_back(geometry);
    }

    // ============================================================
    // 2. CREATE BOTTOM LEVEL ACCELERATION STRUCTURES
    // Bottom level AS corresponds to the geometry.
    // ============================================================

    auto CreateAccelerationStructure = [&](VkAccelerationStructureTypeNV type, uint32_t geometryCount,
        VkGeometryNV* geometries, uint32_t instanceCount, VkAccelerationStructureNV& AS, VkDeviceMemory& memory)
    {
        VkAccelerationStructureCreateInfoNV accelerationStructureInfo;
        accelerationStructureInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV;
        accelerationStructureInfo.pNext = nullptr;
        accelerationStructureInfo.compactedSize = 0;
        accelerationStructureInfo.info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
        accelerationStructureInfo.info.pNext = NULL;
        accelerationStructureInfo.info.type = type;
        accelerationStructureInfo.info.flags = 0;
        accelerationStructureInfo.info.instanceCount = instanceCount;
        accelerationStructureInfo.info.geometryCount = geometryCount;
        accelerationStructureInfo.info.pGeometries = geometries;

        VkResult code = vkCreateAccelerationStructureNV(_device, &accelerationStructureInfo, nullptr, &AS);
        NVVK_CHECK_ERROR(code, "vkCreateAccelerationStructureNV");

        VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo;
        memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
        memoryRequirementsInfo.pNext = nullptr;
        memoryRequirementsInfo.accelerationStructure = AS;
        memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV;

        VkMemoryRequirements2 memoryRequirements;
        vkGetAccelerationStructureMemoryRequirementsNV(_device, &memoryRequirementsInfo, &memoryRequirements);

        VkMemoryAllocateInfo memoryAllocateInfo;
        memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memoryAllocateInfo.pNext = nullptr;
        memoryAllocateInfo.allocationSize = memoryRequirements.memoryRequirements.size;
        memoryAllocateInfo.memoryTypeIndex = ResourceBase::GetMemoryType(memoryRequirements.memoryRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        code = vkAllocateMemory(_device, &memoryAllocateInfo, nullptr, &memory);
        NVVK_CHECK_ERROR(code, "rt AS vkAllocateMemory");

        VkBindAccelerationStructureMemoryInfoNV bindInfo;
        bindInfo.sType = VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV;
        bindInfo.pNext = nullptr;
        bindInfo.accelerationStructure = AS;
        bindInfo.memory = memory;
        bindInfo.memoryOffset = 0;
        bindInfo.deviceIndexCount = 0;
        bindInfo.pDeviceIndices = nullptr;

        code = vkBindAccelerationStructureMemoryNV(_device, 1, &bindInfo);
        NVVK_CHECK_ERROR(code, "vkBindAccelerationStructureMemoryNV");
    };

    CreateAccelerationStructure(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV,
        (uint32_t)geometries.size(), geometries.data(), 0,
        _bottomAS, _bottomASMemory);

    // ============================================================
    // 3. CREATE INSTANCE BUFFER
    // There can be many instances of the single geometry. Create
    // instances using various transforms.
    // ============================================================

    BufferResource instanceBuffer;

    {
        uint64_t accelerationStructureHandle;
        VkResult code = vkGetAccelerationStructureHandleNV(_device, _bottomAS, sizeof(uint64_t), &accelerationStructureHandle);
        NVVK_CHECK_ERROR(code, "vkGetAccelerationStructureHandleNV");

        float transform[12] =
        {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
        };

        VkGeometryInstance instance;
        memcpy(instance.transform, transform, sizeof(instance.transform));
        instance.instanceId = 0;
        instance.mask = 0xff;
        instance.instanceOffset = 0;
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NV;
        instance.accelerationStructureHandle = accelerationStructureHandle;

        uint32_t instanceBufferSize = (uint32_t)sizeof(VkGeometryInstance);

        code = instanceBuffer.Create(instanceBufferSize, VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        NVVK_CHECK_ERROR(code, "rt instanceBuffer.Create");
        instanceBuffer.CopyToBufferUsingMapUnmap(&instance, instanceBufferSize);
    }

    // ============================================================
    // 4. CREATE TOP LEVEL ACCELERATION STRUCTURES
    // Top level AS encompasses bottom level acceleration structures.
    // ============================================================

    CreateAccelerationStructure(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV,
        0, nullptr, 1,
        _topAS, _topASMemory);

    // ============================================================
    // 5. BUILD ACCELERATION STRUCTURES
    // Finally fill acceleration structures using all the data.
    // ============================================================

    auto GetScratchBufferSize = [&](VkAccelerationStructureNV handle)
    {
        VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo;
        memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
        memoryRequirementsInfo.pNext = nullptr;
        memoryRequirementsInfo.accelerationStructure = handle;
        memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_NV;

        VkMemoryRequirements2 memoryRequirements;
        vkGetAccelerationStructureMemoryRequirementsNV(_device, &memoryRequirementsInfo, &memoryRequirements);

        VkDeviceSize result = memoryRequirements.memoryRequirements.size;
        return result;
    };

    {
        VkDeviceSize bottomAccelerationStructureBufferSize = GetScratchBufferSize(_bottomAS);
        VkDeviceSize topAccelerationStructureBufferSize = GetScratchBufferSize(_topAS);
        VkDeviceSize scratchBufferSize = std::max(bottomAccelerationStructureBufferSize, topAccelerationStructureBufferSize);

        BufferResource scratchBuffer;
        scratchBuffer.Create(scratchBufferSize, VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);


        VkCommandBufferAllocateInfo commandBufferAllocateInfo;
        commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAllocateInfo.pNext = nullptr;
        commandBufferAllocateInfo.commandPool = _commandPool;
        commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferAllocateInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkResult code = vkAllocateCommandBuffers(_device, &commandBufferAllocateInfo, &commandBuffer);
        NVVK_CHECK_ERROR(code, "rt vkAllocateCommandBuffers");

        VkCommandBufferBeginInfo beginInfo;
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.pNext = nullptr;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = nullptr;
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        VkMemoryBarrier memoryBarrier;
        memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        memoryBarrier.pNext = nullptr;
        memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;
        memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;


        {
            VkAccelerationStructureInfoNV asInfo;
            asInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
            asInfo.pNext = NULL;
            asInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;
            asInfo.flags = 0;
            asInfo.instanceCount = 0;
            asInfo.geometryCount = (uint32_t)geometries.size();
            asInfo.pGeometries = &geometries[0];

            vkCmdBuildAccelerationStructureNV(commandBuffer, &asInfo, VK_NULL_HANDLE, 0, VK_FALSE, _bottomAS, VK_NULL_HANDLE, scratchBuffer.Buffer, 0);
        }

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, 0, 1, &memoryBarrier, 0, 0, 0, 0);

        {
            VkAccelerationStructureInfoNV asInfo;
            asInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
            asInfo.pNext = NULL;
            asInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV;
            asInfo.flags = 0;
            asInfo.instanceCount = 1;
            asInfo.geometryCount = 0;
            asInfo.pGeometries = nullptr;

            vkCmdBuildAccelerationStructureNV(commandBuffer, &asInfo, instanceBuffer.Buffer, 0, VK_FALSE, _topAS, VK_NULL_HANDLE, scratchBuffer.Buffer, 0);
        }

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV, 0, 1, &memoryBarrier, 0, 0, 0, 0);

        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo;
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = nullptr;
        submitInfo.waitSemaphoreCount = 0;
        submitInfo.pWaitSemaphores = nullptr;
        submitInfo.pWaitDstStageMask = nullptr;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        submitInfo.signalSemaphoreCount = 0;
        submitInfo.pSignalSemaphores = nullptr;

        vkQueueSubmit(_queuesInfo.Graphics.Queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(_queuesInfo.Graphics.Queue);
        vkFreeCommandBuffers(_device, _commandPool, 1, &commandBuffer);
    }
}

int main(int argc, const char* argv[])
{
    RunApplication<TutorialApplication>();
	return 0;
}