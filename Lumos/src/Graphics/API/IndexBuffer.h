#pragma once



#include "Graphics/API/VertexBuffer.h"

namespace Lumos
{
	namespace Graphics
	{
		class CommandBuffer;

		class LUMOS_EXPORT IndexBuffer
		{
		public:
			virtual ~IndexBuffer() = default;
			virtual void Bind(CommandBuffer* commandBuffer = nullptr) const = 0;
			virtual void Unbind() const = 0;

			virtual u32 GetCount() const = 0;
			virtual u32 GetSize() const { return 0; }
			virtual void SetCount(u32 m_index_count) = 0;
		public:
			static IndexBuffer* Create(u16* data, u32 count, BufferUsage bufferUsage = BufferUsage::STATIC);
			static IndexBuffer* Create(u32* data, u32 count, BufferUsage bufferUsage = BufferUsage::STATIC);
            
        protected:
            static IndexBuffer* (*Create16Func)(u16*, u32, BufferUsage);
            static IndexBuffer* (*CreateFunc)(u32*, u32, BufferUsage);

		};
	}
}
