#include "Precompiled.h"
#include "GLTexture.h"
#include "Platform/OpenGL/GL.h"
#include "Platform/OpenGL/GLTools.h"
#include "Platform/OpenGL/GLShader.h"
#include "Utilities/LoadImage.h"

namespace Lumos
{
	namespace Graphics
	{
		GLTexture2D::GLTexture2D()
			: m_Width(0)
			, m_Height(0)
		{
			glGenTextures(1, &m_Handle);
		}

		GLTexture2D::GLTexture2D(u32 width, u32 height, void* data, TextureParameters parameters, TextureLoadOptions loadOptions)
			: m_FileName("")
			, m_Name("")
			, m_Parameters(parameters)
			, m_LoadOptions(loadOptions)
			, m_Width(width)
			, m_Height(height)
		{
			m_Name = "";
			m_Parameters = parameters;
			m_LoadOptions = loadOptions;
			m_Handle = Load(data);
		}

		GLTexture2D::GLTexture2D(const std::string& name, const std::string& filename, const TextureParameters parameters, const TextureLoadOptions loadOptions)
			: m_FileName(filename)
			, m_Name(name)
			, m_Parameters(parameters)
			, m_LoadOptions(loadOptions)
		{
			m_Handle = Load(nullptr);
		}

		GLTexture2D::~GLTexture2D()
		{
			GLCall(glDeleteTextures(1, &m_Handle));
		}

		u32 GLTexture2D::LoadTexture(void* data) const
		{
			u32 handle;
			GLCall(glGenTextures(1, &handle));
			GLCall(glBindTexture(GL_TEXTURE_2D, handle));
			GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_Parameters.minFilter == TextureFilter::LINEAR ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST));
			GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_Parameters.magFilter == TextureFilter::LINEAR ? GL_LINEAR : GL_NEAREST));
			GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GLTools::TextureWrapToGL(m_Parameters.wrap)));
			GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GLTools::TextureWrapToGL(m_Parameters.wrap)));

			u32 format = GLTools::TextureFormatToGL(m_Parameters.format);
			GLCall(glTexImage2D(GL_TEXTURE_2D, 0, format, m_Width, m_Height, 0, GLTools::TextureFormatToInternalFormat(format), isHDR ? GL_FLOAT : GL_UNSIGNED_BYTE, data ? data : NULL));
			GLCall(glGenerateMipmap(GL_TEXTURE_2D));
#ifdef LUMOS_DEBUG
			GLCall(glBindTexture(GL_TEXTURE_2D, 0));
#endif

			return handle;
		}

		u32 GLTexture2D::Load(void* data)
		{
			u8* pixels = nullptr;

			if(data != nullptr)
			{
				pixels = reinterpret_cast<u8*>(data);
			}
			else
			{
				if(m_FileName != "")
				{
					pixels = LoadTextureData();
				}
			}

			u32 handle = LoadTexture(pixels);

			return handle;
		}

		void GLTexture2D::SetData(const void* pixels)
		{
			GLCall(glBindTexture(GL_TEXTURE_2D, m_Handle));
			GLCall(glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height, GLTools::TextureFormatToGL(m_Parameters.format), GL_UNSIGNED_BYTE, pixels));
			GLCall(glGenerateMipmap(GL_TEXTURE_2D));
		}

		void GLTexture2D::Bind(u32 slot) const
		{
			GLCall(glActiveTexture(GL_TEXTURE0 + slot));
			GLCall(glBindTexture(GL_TEXTURE_2D, m_Handle));
		}

		void GLTexture2D::Unbind(u32 slot) const
		{
			GLCall(glActiveTexture(GL_TEXTURE0 + slot));
			GLCall(glBindTexture(GL_TEXTURE_2D, 0));
		}

		void GLTexture2D::BuildTexture(const TextureFormat internalformat, u32 width, u32 height, bool depth, bool samplerShadow)
		{
			m_Width = width;
			m_Height = height;
			m_Name = "Texture Attachment";

			u32 Format = GLTools::TextureFormatToGL(internalformat);
			u32 Format2 = GLTools::TextureFormatToInternalFormat(Format);

			glBindTexture(GL_TEXTURE_2D, m_Handle);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

			if(samplerShadow)
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
#ifndef LUMOS_PLATFORM_MOBILE
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
				glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE, GL_INTENSITY);
#endif
			}

			glTexImage2D(GL_TEXTURE_2D, 0, Format, width, height, 0, Format2, depth ? GL_UNSIGNED_BYTE : GL_FLOAT, nullptr);
		}

		u8* GLTexture2D::LoadTextureData()
		{
			u8* pixels = nullptr;
			if(m_FileName != "NULL")
			{
				u32 bits;
				pixels = Lumos::LoadImageFromFile(m_FileName.c_str(), &m_Width, &m_Height, &bits, &isHDR, !m_LoadOptions.flipY);
				m_Parameters.format = BitsToTextureFormat(bits);
			}
			return pixels;
		};

		GLTextureCube::GLTextureCube(u32 size)
			: m_Size(size)
			, m_Bits(0)
			, m_NumMips(0)
			, m_Format()
		{
			m_NumMips = Texture::CalculateMipMapCount(size, size);

			glGenTextures(1, &m_Handle);
			glBindTexture(GL_TEXTURE_CUBE_MAP, m_Handle);
			//glTexStorage2D(m_Handle, m_NumMips, GL_RGBA16F, m_Size, m_Size);

#ifndef LUMOS_PLATFORM_MOBILE
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP);
#endif
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			// set textures
			for(int i = 0; i < 6; ++i)
				glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA16F, size, size, 0, GL_RGBA, GL_FLOAT, nullptr);

			//GLCall(glGenerateMipmap(GL_TEXTURE_CUBE_MAP));
			m_Width = size;
			m_Height = size;
		}

		GLTextureCube::GLTextureCube(const std::string& filepath)
			: m_Width(0)
			, m_Height(0)
			, m_Size(0)
			, m_NumMips(0)
			, m_Bits(0)
			, m_Format()
		{
			m_Files[0] = filepath;
			m_Handle = LoadFromSingleFile();
		}

		GLTextureCube::GLTextureCube(const std::string* files)
		{
			for(u32 i = 0; i < 6; i++)
				m_Files[i] = files[i];
			m_Handle = LoadFromMultipleFiles();
		}

		GLTextureCube::GLTextureCube(const std::string* files, u32 mips, const InputFormat format)
		{
			m_NumMips = mips;
			for(u32 i = 0; i < mips; i++)
				m_Files[i] = files[i];
			if(format == InputFormat::VERTICAL_CROSS)
				m_Handle = LoadFromVCross(mips);
		}

		GLTextureCube::~GLTextureCube()
		{
			GLCall(glDeleteTextures(m_NumMips, &m_Handle));
		}

		void GLTextureCube::Bind(u32 slot) const
		{
			GLCall(glActiveTexture(GL_TEXTURE0 + slot));
			GLCall(glBindTexture(GL_TEXTURE_CUBE_MAP, m_Handle));
		}

		void GLTextureCube::Unbind(u32 slot) const
		{
			GLCall(glActiveTexture(GL_TEXTURE0 + slot));
			GLCall(glBindTexture(GL_TEXTURE_CUBE_MAP, 0));
		}

		u32 GLTextureCube::LoadFromSingleFile()
		{
			// TODO: Implement
			return 0;
		}

		u32 GLTextureCube::LoadFromMultipleFiles()
		{
			const std::string& xpos = m_Files[0];
			const std::string& xneg = m_Files[1];
			const std::string& ypos = m_Files[2];
			const std::string& yneg = m_Files[3];
			const std::string& zpos = m_Files[4];
			const std::string& zneg = m_Files[5];

			m_Parameters.format = TextureFormat::RGBA;

			u32 width, height, bits;
			bool isHDR = false;
			u8* xp = Lumos::LoadImageFromFile(xpos, &width, &height, &bits, &isHDR, true);
			u8* xn = Lumos::LoadImageFromFile(xneg, &width, &height, &bits, &isHDR, true);
			u8* yp = Lumos::LoadImageFromFile(ypos, &width, &height, &bits, &isHDR, true);
			u8* yn = Lumos::LoadImageFromFile(yneg, &width, &height, &bits, &isHDR, true);
			u8* zp = Lumos::LoadImageFromFile(zpos, &width, &height, &bits, &isHDR, true);
			u8* zn = Lumos::LoadImageFromFile(zneg, &width, &height, &bits, &isHDR, true);

			u32 result;
			GLCall(glGenTextures(1, &result));
			GLCall(glBindTexture(GL_TEXTURE_CUBE_MAP, result));
			GLCall(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR));
			GLCall(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
			GLCall(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
			GLCall(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

			u32 internalFormat = GLTools::TextureFormatToGL(m_Parameters.format);
			u32 format = internalFormat;

			GLCall(glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, xp));
			GLCall(glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, xn));

			GLCall(glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, yp));
			GLCall(glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, yn));

			GLCall(glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, zp));
			GLCall(glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, zn));

			GLCall(glGenerateMipmap(GL_TEXTURE_CUBE_MAP));

			delete[] xp;
			delete[] xn;
			delete[] yp;
			delete[] yn;
			delete[] zp;
			delete[] zn;

			return result;
		}

		u32 GLTextureCube::LoadFromVCross(u32 mips)
		{
			u32 srcWidth, srcHeight, bits;
			u8*** cubeTextureData = new u8 * *[mips];
			for(u32 i = 0; i < mips; i++)
				cubeTextureData[i] = new u8* [6];

			u32* faceWidths = new u32[mips];
			u32* faceHeights = new u32[mips];

			for(u32 m = 0; m < mips; m++)
			{
				bool isHDR = false;
				u8* data = Lumos::LoadImageFromFile(m_Files[m], &srcWidth, &srcHeight, &bits, &isHDR, !m_LoadOptions.flipY);
				m_Parameters.format = BitsToTextureFormat(bits);
				u32 stride = bits / 8;

				u32 face = 0;
				u32 faceWidth = srcWidth / 3;
				u32 faceHeight = srcHeight / 4;
				faceWidths[m] = faceWidth;
				faceHeights[m] = faceHeight;
				for(u32 cy = 0; cy < 4; cy++)
				{
					for(u32 cx = 0; cx < 3; cx++)
					{
						if(cy == 0 || cy == 2 || cy == 3)
						{
							if(cx != 1)
								continue;
						}

						cubeTextureData[m][face] = new u8[faceWidth * faceHeight * stride];

						for(u32 y = 0; y < faceHeight; y++)
						{
							u32 offset = y;
							if(face == 5)
								offset = faceHeight - (y + 1);
							u32 yp = cy * faceHeight + offset;
							for(u32 x = 0; x < faceWidth; x++)
							{
								offset = x;
								if(face == 5)
									offset = faceWidth - (x + 1);
								u32 xp = cx * faceWidth + offset;
								cubeTextureData[m][face][(x + y * faceWidth) * stride + 0] = data[(xp + yp * srcWidth) * stride + 0];
								cubeTextureData[m][face][(x + y * faceWidth) * stride + 1] = data[(xp + yp * srcWidth) * stride + 1];
								cubeTextureData[m][face][(x + y * faceWidth) * stride + 2] = data[(xp + yp * srcWidth) * stride + 2];
								if(stride >= 4)
									cubeTextureData[m][face][(x + y * faceWidth) * stride + 3] = data[(xp + yp * srcWidth) * stride + 3];
							}
						}
						face++;
					}
				}
				delete[] data;
			}

			u32 result;
			GLCall(glGenTextures(1, &result));
			GLCall(glBindTexture(GL_TEXTURE_CUBE_MAP, result));
			GLCall(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR));
			GLCall(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
			GLCall(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
			GLCall(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
			GLCall(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE));

			u32 internalFormat = GLTools::TextureFormatToGL(m_Parameters.format);
			u32 format = GLTools::TextureFormatToInternalFormat(internalFormat);
			for(u32 m = 0; m < mips; m++)
			{
				GLCall(glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, m, internalFormat, faceWidths[m], faceHeights[m], 0, format, GL_UNSIGNED_BYTE, cubeTextureData[m][3]));
				GLCall(glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, m, internalFormat, faceWidths[m], faceHeights[m], 0, format, GL_UNSIGNED_BYTE, cubeTextureData[m][1]));

				GLCall(glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, m, internalFormat, faceWidths[m], faceHeights[m], 0, format, GL_UNSIGNED_BYTE, cubeTextureData[m][0]));
				GLCall(glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, m, internalFormat, faceWidths[m], faceHeights[m], 0, format, GL_UNSIGNED_BYTE, cubeTextureData[m][4]));

				GLCall(glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, m, internalFormat, faceWidths[m], faceHeights[m], 0, format, GL_UNSIGNED_BYTE, cubeTextureData[m][2]));
				GLCall(glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, m, internalFormat, faceWidths[m], faceHeights[m], 0, format, GL_UNSIGNED_BYTE, cubeTextureData[m][5]));
			}
			GLCall(glBindTexture(GL_TEXTURE_CUBE_MAP, 0));

			for(u32 m = 0; m < mips; m++)
			{
				for(u32 f = 0; f < 6; f++)
				{
					delete[] cubeTextureData[m][f];
				}
				delete[] cubeTextureData[m];
			}
			delete[] cubeTextureData;
			delete[] faceHeights;
			delete[] faceWidths;

			return result;
		}

		GLTextureDepth::GLTextureDepth(u32 width, u32 height)
			: m_Width(width)
			, m_Height(height)
		{
			GLCall(glGenTextures(1, &m_Handle));

			Init();
		}

		GLTextureDepth::~GLTextureDepth()
		{
			GLCall(glDeleteTextures(1, &m_Handle));
		}

		void GLTextureDepth::Bind(u32 slot) const
		{
			GLCall(glActiveTexture(GL_TEXTURE0 + slot));
			GLCall(glBindTexture(GL_TEXTURE_2D, m_Handle));
		}

		void GLTextureDepth::Unbind(u32 slot) const
		{
			GLCall(glActiveTexture(GL_TEXTURE0 + slot));
			GLCall(glBindTexture(GL_TEXTURE_2D, 0));
		}

		void GLTextureDepth::Init()
		{
			GLCall(glBindTexture(GL_TEXTURE_2D, m_Handle));

			GLCall(glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, m_Width, m_Height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL));
			GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
			GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
			GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
			GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
			GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL));
#ifndef LUMOS_PLATFORM_MOBILE
			GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE));
#endif
			GLCall(glBindTexture(GL_TEXTURE_2D, 0));
		}

		void GLTextureDepth::Resize(u32 width, u32 height)
		{
			m_Width = width;
			m_Height = height;

			Init();
		}

		GLTextureDepthArray::GLTextureDepthArray(u32 width, u32 height, u32 count)
			: m_Width(width)
			, m_Height(height)
			, m_Count(count)
		{
			GLTextureDepthArray::Init();
		}

		GLTextureDepthArray::~GLTextureDepthArray()
		{
			GLCall(glDeleteTextures(1, &m_Handle));
		}

		void GLTextureDepthArray::Bind(u32 slot) const
		{
			GLCall(glActiveTexture(GL_TEXTURE0 + slot));
			GLCall(glBindTexture(GL_TEXTURE_2D_ARRAY, m_Handle));
		}

		void GLTextureDepthArray::Unbind(u32 slot) const
		{
			GLCall(glActiveTexture(GL_TEXTURE0 + slot));
			GLCall(glBindTexture(GL_TEXTURE_2D_ARRAY, 0));
		}

		void GLTextureDepthArray::Init()
		{
			GLCall(glGenTextures(1, &m_Handle));
			GLCall(glBindTexture(GL_TEXTURE_2D_ARRAY, m_Handle));

			GLCall(glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT, m_Width, m_Height, m_Count, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, nullptr));

			GLCall(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT));
			GLCall(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT));
			GLCall(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
			GLCall(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
#ifndef LUMOS_PLATFORM_MOBILE
			GLCall(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_NONE));
			//GLCall(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_DEPTH_TEXTURE_MODE, GL_INTENSITY));
#endif
			GLCall(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL));
			GLCall(glBindTexture(GL_TEXTURE_2D_ARRAY, 0));
		}

		void GLTextureDepthArray::Resize(u32 width, u32 height, u32 count)
		{
			m_Width = width;
			m_Height = height;
			m_Count = count;

			GLCall(glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT, m_Width, m_Height, m_Count, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, nullptr));

			GLCall(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT));
			GLCall(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT));
			GLCall(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
			GLCall(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
#ifndef LUMOS_PLATFORM_MOBILE
			GLCall(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_NONE));
			GLCall(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_DEPTH_TEXTURE_MODE, GL_INTENSITY));
#endif
			GLCall(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL));
			GLCall(glBindTexture(GL_TEXTURE_2D_ARRAY, 0));
		}

		Texture2D* GLTexture2D::CreateFuncGL()
		{
			return new GLTexture2D();
		}

		Texture2D* GLTexture2D::CreateFromSourceFuncGL(u32 width, u32 height, void* data, TextureParameters parameters, TextureLoadOptions loadoptions)
		{
			return new GLTexture2D(width, height, data, parameters, loadoptions);
		}

		Texture2D* GLTexture2D::CreateFromFileFuncGL(const std::string& name, const std::string& filename, TextureParameters parameters, TextureLoadOptions loadoptions)
		{
			return new GLTexture2D(name, filename, parameters, loadoptions);
		}

		TextureCube* GLTextureCube::CreateFuncGL(u32 size)
		{
			return new GLTextureCube(size);
		}

		TextureCube* GLTextureCube::CreateFromFileFuncGL(const std::string& filepath)
		{
			return new GLTextureCube(filepath);
		}

		TextureCube* GLTextureCube::CreateFromFilesFuncGL(const std::string* files)
		{
			return new GLTextureCube(files);
		}

		TextureCube* GLTextureCube::CreateFromVCrossFuncGL(const std::string* files, u32 mips, InputFormat format)
		{
			return new GLTextureCube(files, mips, format);
		}

		TextureDepth* GLTextureDepth::CreateFuncGL(u32 width, u32 height)
		{
			return new GLTextureDepth(width, height);
		}

		TextureDepthArray* GLTextureDepthArray::CreateFuncGL(u32 width, u32 height, u32 count)
		{
			return new GLTextureDepthArray(width, height, count);
		}

		void GLTexture2D::MakeDefault()
		{
			CreateFunc = CreateFuncGL;
			CreateFromFileFunc = CreateFromFileFuncGL;
			CreateFromSourceFunc = CreateFromSourceFuncGL;
		}

		void GLTextureCube::MakeDefault()
		{
			CreateFunc = CreateFuncGL;
			CreateFromFileFunc = CreateFromFileFuncGL;
			CreateFromFilesFunc = CreateFromFilesFuncGL;
			CreateFromVCrossFunc = CreateFromVCrossFuncGL;
		}

		void GLTextureDepth::MakeDefault()
		{
			CreateFunc = CreateFuncGL;
		}

		void GLTextureDepthArray::MakeDefault()
		{
			CreateFunc = CreateFuncGL;
		}
	}
}
