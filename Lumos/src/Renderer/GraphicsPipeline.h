#pragma once

#include "LM.h"
#include "Maths/Frustum.h"

namespace Lumos
{
	class Scene;
	class ForwardRenderer;
	class DeferredRenderer;
	class RenderList;
	class TextureDepthArray;
	class ShadowRenderer;

    namespace graphics
    {
        namespace api
        {
			class IMGUIRenderer;
		}
	}

	struct TimeStep;

	class LUMOS_EXPORT GraphicsPipeline
    {
	public:

		GraphicsPipeline();
		~GraphicsPipeline();

		void RenderScene();

		bool Init(uint width, uint height);
		void InitialiseDefaults();

		inline Scene* GetScene() const { return m_pScene; }

		void Reset();
		void SetScene(Scene* scene) { m_pScene = scene; };

		void SetScreenSize(uint width, uint height) { m_ScreenTexWidth = width; m_ScreenTexHeight = height; }
		uint GetScreenWidth()  const { return m_ScreenTexWidth; }
		uint GetScreenHeight() const { return m_ScreenTexHeight; }

		void OnResize(uint width, uint height);

	protected:

		Scene* m_pScene;
		std::unique_ptr<ForwardRenderer>				m_ForwardRenderer;
		std::unique_ptr<DeferredRenderer>				m_DeferredRenderer;
		std::unique_ptr<graphics::api::IMGUIRenderer>   m_IMGUIRenderer;

		uint	m_ScreenTexWidth, m_ScreenTexHeight;
	};
}