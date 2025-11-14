// Copyright (C) 2025 SAMURAI (xesdoog) & Contributors
// This file is part of YLP.
//
// YLP is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// YLP is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with YLP.  If not, see <https://www.gnu.org/licenses/>.


#pragma once

#include <gl/GL.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_opengl3.h>

namespace YLP
{
	enum eTextureRequestType : uint8_t
	{
		RequestTypeMemory,
		RequestTypeFile,
		RequestTypeRgba, // for process list icons (if I ever implement them)
	};

	class Renderer : public Singleton<Renderer>
	{
		friend class Singleton<Renderer>;

	private:
		Renderer() = default;

	public:
		~Renderer();

		Renderer(const Renderer&) = delete;
		Renderer(Renderer&&) noexcept = delete;
		Renderer& operator=(const Renderer&) = delete;
		Renderer& operator=(Renderer&&) noexcept = delete;

		static void Destroy()
		{
			GetInstance().DestroyImpl();
		}

		static bool Init()
		{
			return GetInstance().InitImpl();
		}

		static void Draw()
		{
			return GetInstance().DrawImpl();
		};

		static void SetResizing(LPARAM lParam)
		{
			GetInstance().SetResizingImpl(lParam);
		}

		static const std::atomic<bool> IsFocused()
		{
			return GetInstance().IsFocusedImpl();
		}

		static HWND GetWindowHandle()
		{
			return GetInstance().m_HWND;
		}

		static ImVec2 GetWindowSize() noexcept
		{
			return GetInstance().GetWindowSizeImpl();
		}

		static ImTextureID LoadTextureFromFile(const std::filesystem::path& filepath)
		{
			return GetInstance().LoadTextureFromFileImpl(filepath);
		}

		static ImTextureID LoadTextureFromMemory(const unsigned char* data, size_t size, const std::string& name)
		{
			return GetInstance().LoadTextureFromMemoryImpl(data, size, name);
		}

		static ImTextureID LoadRawTexture(const unsigned char* rgbaData, int w, int h, const std::string& name)
		{
			return GetInstance().LoadRawTextureImpl(rgbaData, w, h, name);
		}

		static void RequestTexture(const std::string& name,
		    eTextureRequestType requestType,
		    ImTextureID outImTexture,
		    const unsigned char* textureData,
		    size_t dataSize,
		    const std::filesystem::path& filePath = "");

		static void LoadPendingTextures()
		{
			GetInstance().LoadPendingTexturesImpl();
		}

		static void ReleaseTexture(const std::string& name)
		{
			GetInstance().ReleaseTextureImpl(name);
		}

		static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	private:
		void DestroyImpl();
		bool InitImpl();
		void DrawImpl();
		void SetResizingImpl(LPARAM lParam);

		std::atomic<bool> IsFocusedImpl() const;

		ImVec2 GetWindowSizeImpl() noexcept;
		ImTextureID LoadTextureFromFileImpl(const std::filesystem::path& filepath);
		ImTextureID LoadTextureFromMemoryImpl(const unsigned char* data, size_t size, const std::string& name);
		ImTextureID LoadRawTextureImpl(const unsigned char* rgbaData, int w, int h, const std::string& name);
		void ReleaseTextureImpl(const std::string& name);
		void LoadPendingTexturesImpl();

		WNDCLASSEX m_WndClass;
		HWND m_HWND;
		HDC m_HDC;
		HGLRC m_HGLRC;

		bool m_Initialized = false;
		bool m_ResizePending = false;
		int m_Width = 680;
		int m_Height = 720;

		std::chrono::steady_clock::time_point m_LastResizeTime;

		LRESULT WndProcImpl(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

		struct PendingTexture
		{
			const std::string m_Name;
			uint8_t m_RequestType;
			ImTextureID m_OuTexture;
			const unsigned char* m_Data;
			size_t m_DataSize;
			const std::filesystem::path m_Path;
			int m_Width;
			int m_Height;
		};

		struct Texture
		{
			std::string m_Name;
			GLuint m_Id;
			ImTextureID m_ImGuiId;
			int m_RefCount = 1;
		};

		std::mutex m_TextureMutex;
		std::vector<PendingTexture> m_PendingTextures{};
		std::vector<Texture> m_Textures;

		static Renderer& GetInstance()
		{
			static Renderer i{};
			return i;
		}
	};
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
