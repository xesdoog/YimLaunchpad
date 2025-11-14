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


#include <thirdparty/stb_image.h>
#include <resources/res.h>

#include "renderer.hpp"
#include "gui.hpp"


namespace YLP
{
	Renderer::~Renderer()
	{
		if (m_Initialized)
			DestroyImpl();
	}

	void Renderer::DestroyImpl()
	{
		if (!m_Initialized)
			return;

		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		wglMakeCurrent(nullptr, nullptr);
		wglDeleteContext(m_HGLRC);
		ReleaseDC(m_HWND, m_HDC);
		UnregisterClass(m_WndClass.lpszClassName, g_Instance);

		for (const auto& texture : m_Textures)
		{
			glDeleteTextures(1, &texture.m_Id);
		}

		m_Textures.clear();
		m_Initialized = false;
	}

	bool Renderer::InitImpl()
	{
		if (m_Initialized)
			return true;

		LPCSTR YLP = "YLP";
		m_WndClass = {
		    sizeof(WNDCLASSEX),
		    CS_CLASSDC,
		    WndProc,
		    0L,
		    0L,
		    g_Instance,
		    nullptr,
		    nullptr,
		    nullptr,
		    nullptr,
		    YLP,
		    nullptr};

		ATOM regResult = RegisterClassEx(&m_WndClass);
		if (regResult == 0)
		{
			DWORD err = GetLastError();
			LOG_ERROR("RegisterClassExW failed with code {}", err);
			return false;
		}


		auto& cfg = Settings::Get();
		m_Width = cfg.windowWidth;
		m_Height = cfg.windowHeight;
		int x, y;

		if (cfg.windowX == -1 || cfg.windowY == -1)
		{
			RECT desktop;
			const HWND hDesktop = GetDesktopWindow();
			GetClientRect(hDesktop, &desktop);
			x = (desktop.right - m_Width) / 2;
			y = (desktop.bottom - m_Height) / 2;
		}
		else
		{
			x = cfg.windowX;
			y = cfg.windowY;
		}

		m_HWND = CreateWindow(
		    m_WndClass.lpszClassName,
		    YLP,
		    WS_OVERLAPPEDWINDOW,
		    x, y,
		    m_Width,
		    m_Height,
		    nullptr,
		    nullptr,
		    m_WndClass.hInstance,
		    nullptr);

		if (!m_HWND)
		{
			DWORD err = GetLastError();
			LOG_ERROR("CreateWindowW failed with error code {}", err);
			return false;
		}

		g_Hwnd = m_HWND;

		HICON hIcon = static_cast<HICON>(LoadImageW(
		    GetModuleHandle("YLP.exe"),
		    MAKEINTRESOURCEW(IDI_APPICON),
		    IMAGE_ICON,
		    0,
		    0,
		    LR_DEFAULTSIZE | LR_SHARED));

		if (hIcon)
		{
			SendMessage(m_HWND, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
			SendMessage(m_HWND, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
		}
		else
			LOG_WARN("Failed to load window icon!");


		ShowWindow(m_HWND, SW_SHOWDEFAULT);
		UpdateWindow(m_HWND);

		m_HDC = GetDC(m_HWND);
		PIXELFORMATDESCRIPTOR pfd{
		    .nSize = sizeof(pfd),
		    .nVersion = 1,
		    .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
		    .iPixelType = PFD_TYPE_RGBA,
		    .cColorBits = 32,
		    .cDepthBits = 24,
		    .cStencilBits = 8,
		    .iLayerType = PFD_MAIN_PLANE,
		};

		int pf = ChoosePixelFormat(m_HDC, &pfd);
		if (pf == 0)
		{
			LOG_ERROR("ChoosePixelFormat failed!");
			return false;
		}

		if (!SetPixelFormat(m_HDC, pf, &pfd))
		{
			LOG_ERROR("SetPixelFormat failed!");
			return false;
		}

		m_HGLRC = wglCreateContext(m_HDC);
		if (!m_HGLRC)
		{
			LOG_ERROR("wglCreateContext failed!");
			return false;
		}

		wglMakeCurrent(m_HDC, m_HGLRC);
		if (wglGetCurrentContext() == nullptr)
		{
			LOG_ERROR("Failed to initialize OpenGL context!");
			return false;
		}

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		RECT rect;
		GetClientRect(m_HWND, &rect);

		m_Width = rect.right - rect.left;
		m_Height = rect.bottom - rect.top;

		ImGui_ImplWin32_Init(m_HWND);
		ImGui_ImplOpenGL3_Init("#version 130");

		LoadPendingTextures();
		GUI::Init();

		m_Initialized = true;
		return true;
	}

	void Renderer::SetResizingImpl(LPARAM lParam)
	{
		m_Width = LOWORD(lParam);
		m_Height = HIWORD(lParam);
		m_ResizePending = true;
		m_LastResizeTime = std::chrono::steady_clock::now();
	}

	std::atomic<bool> Renderer::IsFocusedImpl() const
	{
		WINDOWPLACEMENT wp{0};
		GetWindowPlacement(m_HWND, &wp);

		const bool minimized = (wp.showCmd == SW_SHOWMINIMIZED);
		const bool focused = (GetForegroundWindow() == m_HWND);

		return focused && !minimized;
	}

	LRESULT Renderer::WndProcImpl(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
			return true;

		switch (msg)
		{
		case WM_SIZE:
		{
			if (wParam != SIZE_MINIMIZED)
				SetResizing(lParam);
			return 0;
		}
		case WM_GETMINMAXINFO:
		{
			auto mmi = reinterpret_cast<MINMAXINFO*>(lParam);
			mmi->ptMinTrackSize.x = 600;
			mmi->ptMinTrackSize.y = 500;
			return 0;
		}
		case WM_SYSCOMMAND:
			if ((wParam & 0xFFF0) == SC_KEYMENU)
				return 0;
			break;
		case WM_CLOSE:
		{
			WINDOWPLACEMENT wp{};
			wp.length = sizeof(WINDOWPLACEMENT);
			if (GetWindowPlacement(hwnd, &wp))
			{
				RECT& rc = wp.rcNormalPosition;
				auto& cfg = Settings::Get();
				cfg.windowWidth = rc.right - rc.left;
				cfg.windowHeight = rc.bottom - rc.top;
				cfg.windowX = rc.left;
				cfg.windowY = rc.top;
			}
			DestroyWindow(hwnd);
			return 0;
		}
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		}
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	void Renderer::DrawImpl()
	{
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		GUI::Draw();

		if (m_ResizePending)
		{
			auto now = std::chrono::steady_clock::now();
			auto elapsed = duration_cast<std::chrono::milliseconds>(now - m_LastResizeTime).count();
			if (elapsed > 100)
			{
				m_ResizePending = false;
				ImGuiIO& io = ImGui::GetIO();
				io.DisplaySize = ImVec2((float)m_Width, (float)m_Height);
				glViewport(0, 0, m_Width, m_Height);
			}
		}

		ImGui::Render();
		glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		SwapBuffers(m_HDC);
	}

	ImVec2 Renderer::GetWindowSizeImpl() noexcept
	{
		return ImVec2((float)m_Width, (float)m_Height);
	}

	ImTextureID Renderer::LoadTextureFromFileImpl(const std::filesystem::path& filepath)
	{
		std::string name = filepath.filename().string();

		for (auto& tex : m_Textures)
		{
			if (tex.m_Name == name)
			{
				tex.m_RefCount++;
				return tex.m_ImGuiId;
			}
		}

		int w, h, channels;
		unsigned char* data = stbi_load(filepath.string().c_str(), &w, &h, &channels, 4);
		if (!data)
		{
			LOG_ERROR("Failed to load image from {}", filepath.string());
			return 0;
		}

		GLuint tex;
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		stbi_image_free(data);

		ImTextureID imguiId = static_cast<ImTextureID>(static_cast<intptr_t>(tex));
		m_Textures.push_back({name, tex, imguiId, 1});
		LOG_DEBUG("Loaded texture {}", name);

		return imguiId;
	}

	ImTextureID Renderer::LoadTextureFromMemoryImpl(const unsigned char* data, size_t size, const std::string& name)
	{
		for (auto& tex : m_Textures)
		{
			if (tex.m_Name == name)
			{
				tex.m_RefCount++;
				return tex.m_ImGuiId;
			}
		}

		int w, h, channels;
		unsigned char* pdata = stbi_load_from_memory(data, size, &w, &h, &channels, 4);
		if (!pdata)
		{
			LOG_ERROR("Failed to load image: {}", name);
			stbi_image_free(pdata);
			return NULL;
		}

		GLuint tex;
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pdata);
		glBindTexture(GL_TEXTURE_2D, 0);
		stbi_image_free(pdata);

		ImTextureID imguiId = static_cast<ImTextureID>(static_cast<intptr_t>(tex));
		m_Textures.push_back({name, tex, imguiId, 1});

		LOG_DEBUG("Loaded texture {}", name);
		return imguiId;
	}

	ImTextureID Renderer::LoadRawTextureImpl(const unsigned char* rgbaData, int w, int h, const std::string& name)
	{
		for (auto& tex : m_Textures)
		{
			if (tex.m_Name == name)
			{
				tex.m_RefCount++;
				return tex.m_ImGuiId;
			}
		}

		GLuint tex;
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgbaData);

		ImTextureID imguiId = static_cast<ImTextureID>(static_cast<intptr_t>(tex));
		m_Textures.push_back({name, tex, imguiId, 1});
		return imguiId;
	}

	void Renderer::ReleaseTextureImpl(const std::string& name)
	{
		for (auto it = m_Textures.begin(); it != m_Textures.end(); ++it)
		{
			if (it->m_Name == name)
			{
				if (--it->m_RefCount <= 0)
				{
					glDeleteTextures(1, &it->m_Id);
					m_Textures.erase(it);
				}
				return;
			}
		}
	}

	void Renderer::RequestTexture(const std::string& name,
	    eTextureRequestType requestType,
	    ImTextureID ImTexture,
	    const unsigned char* textureData,
	    size_t dataSize,
	    const std::filesystem::path& filePath)
	{
		std::scoped_lock lock(GetInstance().m_TextureMutex);
		GetInstance().m_PendingTextures.push_back({name, requestType, ImTexture, textureData, dataSize, filePath});
	}

	void Renderer::LoadPendingTexturesImpl()
	{
		ThreadManager::RunDetached([this]() {
			std::scoped_lock lock(m_TextureMutex);

			for (auto& tex : m_PendingTextures)
			{
				switch (tex.m_RequestType)
				{
				case RequestTypeMemory:
					tex.m_OuTexture = LoadTextureFromMemory(tex.m_Data, tex.m_DataSize, tex.m_Name);
					break;
				case RequestTypeFile:
					tex.m_OuTexture = LoadTextureFromFile(tex.m_Path);
					break;
				case RequestTypeRgba:
					tex.m_OuTexture = LoadRawTexture(tex.m_Data, tex.m_Width, tex.m_Height, tex.m_Name);
					break;
				default:
					break;
				}
			}
		});
	}

	LRESULT CALLBACK Renderer::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		return GetInstance().WndProcImpl(hwnd, msg, wparam, lparam);
	}
}
