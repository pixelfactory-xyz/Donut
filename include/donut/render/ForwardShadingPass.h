/*
* Copyright (c) 2014-2024, NVIDIA CORPORATION. All rights reserved.
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

#pragma once

#include <donut/engine/View.h>
#include <donut/engine/SceneTypes.h>
#include <donut/render/GeometryPasses.h>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace donut::engine
{
    class ShaderFactory;
    class Light;
    class CommonRenderPasses;
    class FramebufferFactory;
    class MaterialBindingCache;
    struct Material;
    struct LightProbe;
}

namespace donut::render
{
    struct ForwardShadingPassPipelineKey
    {
        engine::MaterialDomain domain = engine::MaterialDomain::Opaque;
        nvrhi::RasterCullMode cullMode = nvrhi::RasterCullMode::Back;
        bool frontCounterClockwise = false;
        bool reverseDepth = false;
        nvrhi::VariableRateShadingState shadingRateState{};

        bool operator==(const ForwardShadingPassPipelineKey& other) const
        {
            return domain == other.domain &&
                    cullMode == other.cullMode &&
                    frontCounterClockwise == other.frontCounterClockwise &&
                    reverseDepth == other.reverseDepth &&
                    shadingRateState == other.shadingRateState;
        }

        bool operator!=(const ForwardShadingPassPipelineKey& other) const
        {
            return !(*this == other);
        }
    };
}

namespace std
{
    template<>
    struct hash<std::pair<nvrhi::ITexture*, nvrhi::ITexture*>>
    {
        size_t operator()(const std::pair<nvrhi::ITexture*, nvrhi::ITexture*>& v) const noexcept
        {
            auto h = hash<nvrhi::ITexture*>();
            return h(v.first) ^ (h(v.second) << 8);
        }
    };

    template<> struct hash<donut::render::ForwardShadingPassPipelineKey>
    {
        std::size_t operator()(donut::render::ForwardShadingPassPipelineKey const& key) const noexcept
        {
            size_t hash = 0;
            nvrhi::hash_combine(hash, key.domain);
            nvrhi::hash_combine(hash, key.cullMode);
            nvrhi::hash_combine(hash, key.frontCounterClockwise);
            nvrhi::hash_combine(hash, key.reverseDepth);
            nvrhi::hash_combine(hash, key.shadingRateState);
            return hash;
        }
    };
}

namespace donut::render
{
    class ForwardShadingPass : public IGeometryPass
    {
    public:

        class Context : public GeometryPassContext
        {
        public:
            nvrhi::BindingSetHandle shadingBindingSet;
            nvrhi::BindingSetHandle inputBindingSet;
            ForwardShadingPassPipelineKey keyTemplate;

            uint32_t positionOffset = 0;
            uint32_t texCoordOffset = 0;
            uint32_t normalOffset = 0;
            uint32_t tangentOffset = 0;
        };

        struct CreateParameters
        {
            std::shared_ptr<engine::MaterialBindingCache> materialBindings;
            bool singlePassCubemap = false;
            bool trackLiveness = true;

            // Switches between loading vertex data through the Input Assembler (true) or buffer SRVs (false).
            // Using Buffer SRVs is often faster.
            bool useInputAssembler = false;

            uint32_t numConstantBufferVersions = 16;
        };


    protected:
        nvrhi::DeviceHandle m_Device;
        nvrhi::InputLayoutHandle m_InputLayout;
        nvrhi::ShaderHandle m_VertexShader;
        nvrhi::ShaderHandle m_PixelShader;
        nvrhi::ShaderHandle m_PixelShaderTransmissive;
        nvrhi::ShaderHandle m_GeometryShader;
        nvrhi::SamplerHandle m_ShadowSampler;
        nvrhi::BindingLayoutHandle m_ViewBindingLayout;
        nvrhi::BindingSetHandle m_ViewBindingSet;
        nvrhi::BindingLayoutHandle m_ShadingBindingLayout;
        nvrhi::BindingLayoutHandle m_InputBindingLayout;
        engine::ViewType::Enum m_SupportedViewTypes = engine::ViewType::PLANAR;
        nvrhi::BufferHandle m_ForwardViewCB;
        nvrhi::BufferHandle m_ForwardLightCB;
        bool m_TrackLiveness = true;
        bool m_IsDX11 = false;
        bool m_UseInputAssembler = false;
        std::mutex m_Mutex;

        std::unordered_map<ForwardShadingPassPipelineKey, nvrhi::GraphicsPipelineHandle> m_Pipelines;
        std::unordered_map<std::pair<nvrhi::ITexture*, nvrhi::ITexture*>, nvrhi::BindingSetHandle> m_ShadingBindingSets;
        std::unordered_map<const engine::BufferGroup*, nvrhi::BindingSetHandle> m_InputBindingSets;
        
        std::shared_ptr<engine::CommonRenderPasses> m_CommonPasses;
        std::shared_ptr<engine::MaterialBindingCache> m_MaterialBindings;
        
        virtual nvrhi::ShaderHandle CreateVertexShader(engine::ShaderFactory& shaderFactory, const CreateParameters& params);
        virtual nvrhi::ShaderHandle CreateGeometryShader(engine::ShaderFactory& shaderFactory, const CreateParameters& params);
        virtual nvrhi::ShaderHandle CreatePixelShader(engine::ShaderFactory& shaderFactory, const CreateParameters& params, bool transmissiveMaterial);
        virtual nvrhi::InputLayoutHandle CreateInputLayout(nvrhi::IShader* vertexShader, const CreateParameters& params);
        virtual nvrhi::BindingLayoutHandle CreateViewBindingLayout();
        virtual nvrhi::BindingSetHandle CreateViewBindingSet();
        virtual nvrhi::BindingLayoutHandle CreateShadingBindingLayout();
        virtual nvrhi::BindingSetHandle CreateShadingBindingSet(nvrhi::ITexture* shadowMapTexture, nvrhi::ITexture* diffuse, nvrhi::ITexture* specular, nvrhi::ITexture* environmentBrdf);
        virtual nvrhi::BindingLayoutHandle CreateInputBindingLayout();
        virtual nvrhi::BindingSetHandle CreateInputBindingSet(const engine::BufferGroup* bufferGroup);
        virtual std::shared_ptr<engine::MaterialBindingCache> CreateMaterialBindingCache(engine::CommonRenderPasses& commonPasses);
        virtual nvrhi::GraphicsPipelineHandle CreateGraphicsPipeline(ForwardShadingPassPipelineKey const& key, nvrhi::FramebufferInfo const& framebufferInfo);
        nvrhi::BindingSetHandle GetOrCreateInputBindingSet(const engine::BufferGroup* bufferGroup);

    public:
        ForwardShadingPass(
            nvrhi::IDevice* device,
            std::shared_ptr<engine::CommonRenderPasses> commonPasses);

        virtual void Init(
            engine::ShaderFactory& shaderFactory,
            const CreateParameters& params);

        void ResetBindingCache();
        
        virtual void PrepareLights(
            Context& context,
            nvrhi::ICommandList* commandList,
            const std::vector<std::shared_ptr<engine::Light>>& lights,
            dm::float3 ambientColorTop,
            dm::float3 ambientColorBottom,
            const std::vector<std::shared_ptr<engine::LightProbe>>& lightProbes);

        // IGeometryPass implementation

        [[nodiscard]] engine::ViewType::Enum GetSupportedViewTypes() const override;
        void SetupView(GeometryPassContext& context, nvrhi::ICommandList* commandList, const engine::IView* view, const engine::IView* viewPrev) override;
        bool SetupMaterial(GeometryPassContext& context, const engine::Material* material, nvrhi::RasterCullMode cullMode, nvrhi::GraphicsState& state) override;
        void SetupInputBuffers(GeometryPassContext& context, const engine::BufferGroup* buffers, nvrhi::GraphicsState& state) override;
        void SetPushConstants(GeometryPassContext& context, nvrhi::ICommandList* commandList, nvrhi::GraphicsState& state, nvrhi::DrawArguments& args) override;
    };

}
