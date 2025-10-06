/*
* Copyright (c) 2021-2025, NVIDIA CORPORATION. All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*/

#if DONUT_WITH_DLSS && DONUT_WITH_VULKAN

#include <vulkan/vulkan.h>
#include <nvsdk_ngx_vk.h>
#include <nvsdk_ngx_helpers_vk.h>

#include <donut/render/DLSS.h>
#include <donut/engine/View.h>
#include <donut/core/log.h>
#include <nvrhi/vulkan.h>

using namespace donut;
using namespace donut::render;

static void NVSDK_CONV NgxLogCallback(const char* message, NVSDK_NGX_Logging_Level loggingLevel, NVSDK_NGX_Feature sourceComponent)
{
    log::info("NGX: %s", message);
}

class DLSS_VK : public DLSS
{
public:
    DLSS_VK(nvrhi::IDevice* device, donut::engine::ShaderFactory& shaderFactory,
        std::string const& directoryWithExecutable, uint32_t applicationID)
        : DLSS(device, shaderFactory)
    {
        VkInstance vkInstance = device->getNativeObject(nvrhi::ObjectTypes::VK_Instance);
        VkPhysicalDevice vkPhysicalDevice = device->getNativeObject(nvrhi::ObjectTypes::VK_PhysicalDevice);
        VkDevice vkDevice= device->getNativeObject(nvrhi::ObjectTypes::VK_Device);

        std::wstring executablePathW;
        executablePathW.assign(directoryWithExecutable.begin(), directoryWithExecutable.end());

        NVSDK_NGX_FeatureCommonInfo featureCommonInfo = {};
        featureCommonInfo.LoggingInfo.LoggingCallback = NgxLogCallback;
        featureCommonInfo.LoggingInfo.MinimumLoggingLevel = NVSDK_NGX_LOGGING_LEVEL_OFF;
        featureCommonInfo.LoggingInfo.DisableOtherLoggingSinks = true;

        NVSDK_NGX_Result result = NVSDK_NGX_VULKAN_Init(applicationID,
            executablePathW.c_str(), vkInstance, vkPhysicalDevice, vkDevice, nullptr, nullptr, &featureCommonInfo);

        if (result != NVSDK_NGX_Result_Success)
        {
            log::warning("Cannot initialize NGX, Result = 0x%08x (%ls)", result, GetNGXResultAsString(result));
            return;
        }
        
        result = NVSDK_NGX_VULKAN_GetCapabilityParameters(&m_parameters);

        if (result != NVSDK_NGX_Result_Success)
            return;

        int dlssAvailable = 0;
        result = m_parameters->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &dlssAvailable);
        if (result != NVSDK_NGX_Result_Success || !dlssAvailable)
        {
            result = NVSDK_NGX_Result_Fail;
            NVSDK_NGX_Parameter_GetI(m_parameters, NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult, (int*)&result);
            log::warning("NVIDIA DLSS is not available on this system, FeatureInitResult = 0x%08x (%ls)",
                result, GetNGXResultAsString(result));
            return;
        }

        m_featureSupported = true;
    }

    void SetRenderSize(
        uint32_t inputWidth, uint32_t inputHeight,
        uint32_t outputWidth, uint32_t outputHeight) override
    {
        if (!m_featureSupported)
            return;

        if (m_inputWidth == inputWidth && m_inputHeight == inputHeight && m_outputWidth == outputWidth && m_outputHeight == outputHeight)
            return;
        
        if (m_dlssHandle)
        {
            m_device->waitForIdle();
            NVSDK_NGX_VULKAN_ReleaseFeature(m_dlssHandle);
            m_dlssHandle = nullptr;
        }

        m_featureCommandList->open();
        VkCommandBuffer vkCmdBuf = m_featureCommandList->getNativeObject(nvrhi::ObjectTypes::VK_CommandBuffer);

        NVSDK_NGX_DLSS_Create_Params dlssParams = {};
        dlssParams.Feature.InWidth = inputWidth;
        dlssParams.Feature.InHeight = inputHeight;
        dlssParams.Feature.InTargetWidth = outputWidth;
        dlssParams.Feature.InTargetHeight = outputHeight;
        dlssParams.Feature.InPerfQualityValue = NVSDK_NGX_PerfQuality_Value_MaxQuality;
        dlssParams.InFeatureCreateFlags =
            NVSDK_NGX_DLSS_Feature_Flags_IsHDR |
            NVSDK_NGX_DLSS_Feature_Flags_DepthInverted |
            NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;

        NVSDK_NGX_Result result = NGX_VULKAN_CREATE_DLSS_EXT(vkCmdBuf, 1, 1, &m_dlssHandle, m_parameters, &dlssParams);

        m_featureCommandList->close();
        m_device->executeCommandList(m_featureCommandList);

        if (result != NVSDK_NGX_Result_Success)
        {
            log::warning("Failed to create a DLSS feautre, Result = 0x%08x (%ls)", result, GetNGXResultAsString(result));
            return;
        }

        m_isAvailable = true;

        m_inputWidth = inputWidth;
        m_inputHeight = inputHeight;
        m_outputWidth = outputWidth;
        m_outputHeight = outputHeight;
    }

    static void FillTextureResource(NVSDK_NGX_Resource_VK& resource, nvrhi::ITexture* texture)
    {
        const nvrhi::TextureDesc& desc = texture->getDesc();
        resource.ReadWrite = desc.isUAV;
        resource.Type = NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW;

        auto& viewInfo = resource.Resource.ImageViewInfo;
        viewInfo.Image = texture->getNativeObject(nvrhi::ObjectTypes::VK_Image);
        viewInfo.ImageView = texture->getNativeView(nvrhi::ObjectTypes::VK_ImageView);
        viewInfo.Format = VkFormat(nvrhi::vulkan::convertFormat(desc.format));
        viewInfo.Width = desc.width;
        viewInfo.Height = desc.height;
        viewInfo.SubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.SubresourceRange.baseArrayLayer = 0;
        viewInfo.SubresourceRange.layerCount = 1;
        viewInfo.SubresourceRange.baseMipLevel = 0;
        viewInfo.SubresourceRange.levelCount = 1;
    }
    
    void Evaluate(
        nvrhi::ICommandList* commandList,
        const EvaluateParameters& params,
        const donut::engine::PlanarView& view) override
    {
        if (!m_isAvailable)
            return;

        bool const useExposureBuffer = params.exposureBuffer != nullptr && params.exposureScale != 0.f;
        
        if (useExposureBuffer)
        {
            ComputeExposure(commandList, params.exposureBuffer, params.exposureScale);
        }

        VkCommandBuffer vkCmdBuf = commandList->getNativeObject(nvrhi::ObjectTypes::VK_CommandBuffer);

        NVSDK_NGX_Resource_VK inColorResource;
        NVSDK_NGX_Resource_VK outColorResource;
        NVSDK_NGX_Resource_VK depthResource;
        NVSDK_NGX_Resource_VK motionVectorResource;
        NVSDK_NGX_Resource_VK exposureResource;
        FillTextureResource(inColorResource, params.inputColorTexture);
        FillTextureResource(outColorResource, params.outputColorTexture);
        FillTextureResource(depthResource, params.depthTexture);
        FillTextureResource(motionVectorResource, params.motionVectorsTexture);
        if (useExposureBuffer)
        {
            FillTextureResource(exposureResource, m_exposureTexture);
        }

        commandList->setTextureState(params.inputColorTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        commandList->setTextureState(params.outputColorTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);
        commandList->setTextureState(params.depthTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        commandList->setTextureState(params.motionVectorsTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        if (useExposureBuffer)
        {
            commandList->setTextureState(m_exposureTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        }
        commandList->commitBarriers();
        
        NVSDK_NGX_VK_DLSS_Eval_Params evalParams = {};
        evalParams.Feature.pInColor = &inColorResource;
        evalParams.Feature.pInOutput = &outColorResource;
        evalParams.Feature.InSharpness = params.sharpness;
        evalParams.pInDepth = &depthResource;
        evalParams.pInMotionVectors = &motionVectorResource;
        evalParams.pInExposureTexture = useExposureBuffer ? &exposureResource : nullptr;
        evalParams.InReset = params.resetHistory;
        evalParams.InJitterOffsetX = view.GetPixelOffset().x;
        evalParams.InJitterOffsetY = view.GetPixelOffset().y;
        evalParams.InRenderSubrectDimensions.Width = view.GetViewExtent().width();
        evalParams.InRenderSubrectDimensions.Height = view.GetViewExtent().height();

        NVSDK_NGX_Result result = NGX_VULKAN_EVALUATE_DLSS_EXT(vkCmdBuf, m_dlssHandle, m_parameters, &evalParams);

        commandList->clearState();

        if (result != NVSDK_NGX_Result_Success)
        {
            log::warning("Failed to evaluate DLSS feature: 0x%08x", result);
            return;
        }
    }

    ~DLSS_VK() override
    {
        if (m_dlssHandle)
        {
            NVSDK_NGX_VULKAN_ReleaseFeature(m_dlssHandle);
            m_dlssHandle = nullptr;
        }

        if (m_parameters)
        {
            NVSDK_NGX_VULKAN_DestroyParameters(m_parameters);
            m_parameters = nullptr;
        }

        VkDevice vkDevice = m_device->getNativeObject(nvrhi::ObjectTypes::VK_Device);
        NVSDK_NGX_VULKAN_Shutdown1(vkDevice);
    }
};

std::unique_ptr<DLSS> DLSS::CreateVK(nvrhi::IDevice* device, donut::engine::ShaderFactory& shaderFactory,
    std::string const& directoryWithExecutable, uint32_t applicationID)
{
    return std::make_unique<DLSS_VK>(device, shaderFactory, directoryWithExecutable, applicationID);
}

void DLSS::GetRequiredVulkanExtensions(std::vector<std::string>& instanceExtensions, std::vector<std::string>& deviceExtensions)
{
    unsigned int instanceExtCount = 0;
    unsigned int deviceExtCount = 0;
    const char** pInstanceExtensions = nullptr;
    const char** pDeviceExtensions = nullptr;
    NVSDK_NGX_VULKAN_RequiredExtensions(&instanceExtCount, &pInstanceExtensions, &deviceExtCount, &pDeviceExtensions);

    for (unsigned int i = 0; i < instanceExtCount; i++)
    {
        instanceExtensions.push_back(pInstanceExtensions[i]);
    }

    for (unsigned int i = 0; i < deviceExtCount; i++)
    {
        // VK_EXT_buffer_device_address is incompatible with Vulkan 1.2 and causes a validation error
        if (!strcmp(pDeviceExtensions[i], VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME))
            continue;

        deviceExtensions.push_back(pDeviceExtensions[i]);
    }
}

#endif
