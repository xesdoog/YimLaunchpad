#pragma once

#include <gl/GL.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_opengl3.h>


namespace YLP
{
	class Renderer final
	{
	private:
		Renderer() {};

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

		static bool IsFocused()
		{
			return GetInstance().IsFocusedImpl();
		}

		static ImVec2 GetWindowSize()
		{
			return GetInstance().GetWindowSizeImpl();
		}

		static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	private:
		void DestroyImpl();
		bool InitImpl();
		void DrawImpl();
		void SetResizingImpl(LPARAM lParam);
		bool IsFocusedImpl() const;

		ImVec2 GetWindowSizeImpl();

		WNDCLASSEX m_WndClass;
		HWND m_HWND;
		HDC m_HDC;
		HGLRC m_HGLRC;

		int m_Width = 800;
		int m_Height = 700;

		bool m_Initialized = false;
		bool m_ResizePending = false;

		std::chrono::steady_clock::time_point m_LastResizeTime;

		LRESULT WndProcImpl(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

		static Renderer& GetInstance()
		{
			static Renderer i{};
			return i;
		}
	};
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
