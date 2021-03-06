#include "Precompiled.h"
#include "GridRenderer.h"
#include "Graphics/API/Shader.h"
#include "Graphics/API/Framebuffer.h"
#include "Graphics/API/Texture.h"
#include "Graphics/API/UniformBuffer.h"
#include "Graphics/API/Renderer.h"
#include "Graphics/API/CommandBuffer.h"
#include "Graphics/API/Swapchain.h"
#include "Graphics/API/RenderPass.h"
#include "Graphics/API/Pipeline.h"
#include "Graphics/GBuffer.h"
#include "Graphics/Mesh.h"
#include "Graphics/MeshFactory.h"
#include "Graphics/RenderManager.h"
#include "Scene/Scene.h"
#include "Core/Application.h"
#include "Graphics/Camera/Camera.h"
#include "Maths/Transform.h"

#include <imgui/imgui.h>

namespace Lumos
{
	namespace Graphics
	{
		GridRenderer::GridRenderer(u32 width, u32 height)
			: m_UniformBuffer(nullptr)
			, m_UniformBufferFrag(nullptr)
		{
			m_Pipeline = nullptr;
            
			IRenderer::SetScreenBufferSize(width, height);
			GridRenderer::Init();
            
			m_GridRes = 1.4f;
			m_GridSize = 500.0f;
		}
        
		GridRenderer::~GridRenderer()
		{
			delete m_Quad;
			delete m_UniformBuffer;
			delete m_UniformBufferFrag;
			delete[] m_VSSystemUniformBuffer;
            delete[] m_PSSystemUniformBuffer;
		}
        
		void GridRenderer::RenderScene(Scene* scene)
		{
			m_CurrentBufferID = 0;
			if(!m_RenderTexture)
				m_CurrentBufferID = Renderer::GetSwapchain()->GetCurrentBufferId();
            
			Begin();
            
			SetSystemUniforms(m_Shader.get());
            
            m_Pipeline->Bind(m_CommandBuffers[m_CurrentBufferID].get());
            
            m_CurrentDescriptorSets[0] = m_Pipeline->GetDescriptorSet();
            
			m_Quad->GetVertexBuffer()->Bind(m_CommandBuffers[m_CurrentBufferID].get(), m_Pipeline.get());
			m_Quad->GetIndexBuffer()->Bind(m_CommandBuffers[m_CurrentBufferID].get());
            
			Renderer::BindDescriptorSets(m_Pipeline.get(), m_CommandBuffers[m_CurrentBufferID].get(), 0, m_CurrentDescriptorSets);
			Renderer::DrawIndexed(m_CommandBuffers[m_CurrentBufferID].get(), DrawType::TRIANGLE, m_Quad->GetIndexBuffer()->GetCount());
            
			m_Quad->GetVertexBuffer()->Unbind();
			m_Quad->GetIndexBuffer()->Unbind();
            
			End();
            
			if(!m_RenderTexture)
				Renderer::Present((m_CommandBuffers[Renderer::GetSwapchain()->GetCurrentBufferId()].get()));
		}
        
		enum VSSystemUniformIndices : i32
		{
			VSSystemUniformIndex_InverseProjectionViewMatrix = 0,
			VSSystemUniformIndex_Size
		};
        
		void GridRenderer::Init()
		{
			m_Shader = Ref<Graphics::Shader>(Shader::CreateFromFile("Grid", "/CoreShaders/"));
			m_Quad = Graphics::CreatePlane(5000.0f, 5000.f, Maths::Vector3(0.0f, 1.0f, 0.0f));
            
			// Vertex shader System uniforms
			m_VSSystemUniformBufferSize = sizeof(Maths::Matrix4);
			m_VSSystemUniformBuffer = new u8[m_VSSystemUniformBufferSize];
			memset(m_VSSystemUniformBuffer, 0, m_VSSystemUniformBufferSize);
            
			m_PSSystemUniformBufferSize = sizeof(UniformBufferObjectFrag);
			m_PSSystemUniformBuffer = new u8[m_PSSystemUniformBufferSize];
			memset(m_PSSystemUniformBuffer, 0, m_PSSystemUniformBufferSize);
            
			m_CommandBuffers.resize(Renderer::GetSwapchain()->GetSwapchainBufferCount());
            
			for(auto& commandBuffer : m_CommandBuffers)
			{
				commandBuffer = Ref<Graphics::CommandBuffer>(Graphics::CommandBuffer::Create());
				commandBuffer->Init(true);
			}
            
			m_RenderPass = Ref<Graphics::RenderPass>(Graphics::RenderPass::Create());
			AttachmentInfo textureTypes[1] =
            {
                {TextureType::COLOUR, TextureFormat::RGBA32},
            };
            
			Graphics::RenderpassInfo renderpassCI;
			renderpassCI.attachmentCount = 1;
			renderpassCI.textureType = textureTypes;
			renderpassCI.clear = false;
            
			m_RenderPass->Init(renderpassCI);
            
			CreateGraphicsPipeline();
			UpdateUniformBuffer();
			CreateFramebuffers();
            
            m_CurrentDescriptorSets.resize(1);
		}
        
		void GridRenderer::Begin()
		{
			m_CommandBuffers[m_CurrentBufferID]->BeginRecording();
			m_CommandBuffers[m_CurrentBufferID]->UpdateViewport(m_ScreenBufferWidth, m_ScreenBufferHeight);
            
			m_RenderPass->BeginRenderpass(m_CommandBuffers[m_CurrentBufferID].get(), Maths::Vector4(0.0f), m_Framebuffers[m_CurrentBufferID].get(), Graphics::INLINE, m_ScreenBufferWidth, m_ScreenBufferHeight);
		}
        
		void GridRenderer::BeginScene(Scene* scene, Camera* overrideCamera, Maths::Transform* overrideCameraTransform)
		{
			auto& registry = scene->GetRegistry();
            
			if(overrideCamera)
			{
				m_Camera = overrideCamera;
				m_CameraTransform = overrideCameraTransform;
			}
			else
			{
				auto cameraView = registry.view<Camera>();
				if(!cameraView.empty())
				{
					m_Camera = &cameraView.get<Camera>(cameraView.front());
					m_CameraTransform = registry.try_get<Maths::Transform>(cameraView.front());
				}
			}
            
			if(!m_Camera || !m_CameraTransform)
				return;
            
			auto proj = m_Camera->GetProjectionMatrix();
            
			UniformBufferObjectFrag test;
			test.res = m_GridRes;
			test.scale = m_GridSize;
			test.cameraPos = m_CameraTransform->GetWorldPosition();
			test.maxDistance = m_MaxDistance;
            
			auto invViewProj = proj * m_CameraTransform->GetWorldMatrix().Inverse();
			memcpy(m_VSSystemUniformBuffer, &invViewProj, sizeof(Maths::Matrix4));
			memcpy(m_PSSystemUniformBuffer, &test, sizeof(UniformBufferObjectFrag));
		}
        
		void GridRenderer::End()
		{
			m_RenderPass->EndRenderpass(m_CommandBuffers[m_CurrentBufferID].get());
			m_CommandBuffers[m_CurrentBufferID]->EndRecording();
            
			if(m_RenderTexture)
				m_CommandBuffers[m_CurrentBufferID]->Execute(true);
		}
        
		void GridRenderer::SetSystemUniforms(Shader* shader) const
		{
			m_UniformBuffer->SetData(sizeof(UniformBufferObject), *&m_VSSystemUniformBuffer);
			m_UniformBufferFrag->SetData(sizeof(UniformBufferObjectFrag), *&m_PSSystemUniformBuffer);
		}
        
		void GridRenderer::OnImGui()
		{
			ImGui::TextUnformatted("Grid Renderer");
            
			if(ImGui::TreeNode("Parameters"))
			{
				ImGui::DragFloat("Resolution", &m_GridRes, 1.0f, 0.0f, 10.0f);
				ImGui::DragFloat("Scale", &m_GridSize, 1.0f, 1.0f, 10000.0f);
				ImGui::DragFloat("Max Distance", &m_MaxDistance, 1.0f, 1.0f, 10000.0f);
                
				ImGui::TreePop();
			}
		}
        
		void GridRenderer::OnResize(u32 width, u32 height)
		{
			m_Framebuffers.clear();
            
			SetScreenBufferSize(width, height);
            
			UpdateUniformBuffer();
			CreateFramebuffers();
		}
        
		void GridRenderer::CreateGraphicsPipeline()
		{
			std::vector<Graphics::DescriptorPoolInfo> poolInfo =
            {
                {Graphics::DescriptorType::UNIFORM_BUFFER, 2}};
            
			std::vector<Graphics::DescriptorLayoutInfo> layoutInfo =
            {
                {Graphics::DescriptorType::UNIFORM_BUFFER, Graphics::ShaderType::VERTEX, 0},
                {Graphics::DescriptorType::UNIFORM_BUFFER, Graphics::ShaderType::FRAGMENT, 1}};
            
			auto attributeDescriptions = Vertex::getAttributeDescriptions();
            
			std::vector<Graphics::DescriptorLayout> descriptorLayouts;
            
			Graphics::DescriptorLayout sceneDescriptorLayout;
			sceneDescriptorLayout.count = static_cast<u32>(layoutInfo.size());
			sceneDescriptorLayout.layoutInfo = layoutInfo.data();
            
			descriptorLayouts.push_back(sceneDescriptorLayout);
            
			Graphics::PipelineInfo pipelineCI;
			pipelineCI.pipelineName = "GridRenderer";
			pipelineCI.shader = m_Shader.get();
			pipelineCI.renderpass = m_RenderPass.get();
			pipelineCI.numVertexLayout = static_cast<u32>(attributeDescriptions.size());
			pipelineCI.descriptorLayouts = descriptorLayouts;
			pipelineCI.vertexLayout = attributeDescriptions.data();
			pipelineCI.numLayoutBindings = static_cast<u32>(poolInfo.size());
			pipelineCI.typeCounts = poolInfo.data();
			pipelineCI.strideSize = sizeof(Vertex);
			pipelineCI.numColorAttachments = 1;
			pipelineCI.polygonMode = Graphics::PolygonMode::Fill;
			pipelineCI.cullMode = Graphics::CullMode::NONE;
			pipelineCI.transparencyEnabled = true;
			pipelineCI.depthBiasEnabled = false;
			pipelineCI.maxObjects = 1;
            
			m_Pipeline = Ref<Graphics::Pipeline>(Graphics::Pipeline::Create(pipelineCI));
		}
        
		void GridRenderer::UpdateUniformBuffer()
		{
			if(m_UniformBuffer == nullptr)
			{
				m_UniformBuffer = Graphics::UniformBuffer::Create();
				uint32_t bufferSize = static_cast<uint32_t>(sizeof(UniformBufferObject));
				m_UniformBuffer->Init(bufferSize, nullptr);
			}
            
			std::vector<Graphics::BufferInfo> bufferInfos;
            
			Graphics::BufferInfo bufferInfo;
			bufferInfo.name = "UniformBufferObject";
			bufferInfo.buffer = m_UniformBuffer;
			bufferInfo.offset = 0;
			bufferInfo.size = sizeof(UniformBufferObject);
			bufferInfo.type = Graphics::DescriptorType::UNIFORM_BUFFER;
			bufferInfo.binding = 0;
			bufferInfo.shaderType = ShaderType::VERTEX;
			bufferInfo.systemUniforms = true;
            
			if(m_UniformBufferFrag == nullptr)
			{
				m_UniformBufferFrag = Graphics::UniformBuffer::Create();
				uint32_t bufferSize = static_cast<uint32_t>(sizeof(UniformBufferObjectFrag));
				m_UniformBufferFrag->Init(bufferSize, nullptr);
			}
            
			Graphics::BufferInfo bufferInfo2;
			bufferInfo2.name = "UniformBuffer";
			bufferInfo2.buffer = m_UniformBufferFrag;
			bufferInfo2.offset = 0;
			bufferInfo2.size = sizeof(UniformBufferObjectFrag);
			bufferInfo2.type = Graphics::DescriptorType::UNIFORM_BUFFER;
			bufferInfo2.binding = 1;
			bufferInfo2.shaderType = ShaderType::FRAGMENT;
			bufferInfo2.systemUniforms = false;
            
			bufferInfos.push_back(bufferInfo);
			bufferInfos.push_back(bufferInfo2);
			if(m_Pipeline != nullptr)
				m_Pipeline->GetDescriptorSet()->Update(bufferInfos);
		}
        
		void GridRenderer::SetRenderTarget(Texture* texture, bool rebuildFramebuffer)
		{
			m_RenderTexture = texture;
            
			if(!rebuildFramebuffer)
				return;
            
			m_Framebuffers.clear();
            
			CreateFramebuffers();
		}
        
		void GridRenderer::CreateFramebuffers()
		{
			TextureType attachmentTypes[1];
			attachmentTypes[0] = TextureType::COLOUR;
            
			Texture* attachments[1];
			FramebufferInfo bufferInfo{};
			bufferInfo.width = m_ScreenBufferWidth;
			bufferInfo.height = m_ScreenBufferHeight;
			bufferInfo.attachmentCount = 1;
			bufferInfo.renderPass = m_RenderPass.get();
			bufferInfo.attachmentTypes = attachmentTypes;
            
			if(m_RenderTexture)
			{
				attachments[0] = m_RenderTexture;
				bufferInfo.attachments = attachments;
				bufferInfo.screenFBO = false;
				m_Framebuffers.emplace_back(Framebuffer::Create(bufferInfo));
			}
			else
			{
				for(uint32_t i = 0; i < Renderer::GetSwapchain()->GetSwapchainBufferCount(); i++)
				{
					bufferInfo.screenFBO = true;
					attachments[0] = Renderer::GetSwapchain()->GetImage(i);
					bufferInfo.attachments = attachments;
                    
					m_Framebuffers.emplace_back(Framebuffer::Create(bufferInfo));
				}
			}
		}
	}
}
