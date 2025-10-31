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

        m_Initialized = false;
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(m_HGLRC);
        ReleaseDC(m_HWND, m_HDC);
        UnregisterClass(m_WndClass.lpszClassName, g_Instance);
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
            nullptr
        };

        ATOM regResult = RegisterClassEx(&m_WndClass);
        if (regResult == 0)
        {
            DWORD err = GetLastError();
            LOG_ERROR("RegisterClassExW failed with code %s", err);
            return false;
        }
        
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        int screenPosX = (int)((screenWidth - m_Width) / 2);
        int screenPosY = (int)((screenHeight - m_Height) / 2);
        m_HWND = CreateWindow(
            m_WndClass.lpszClassName,
            YLP,
            WS_OVERLAPPEDWINDOW,
            screenPosX, screenPosY,
            m_Width, m_Height,
            nullptr, nullptr,
            m_WndClass.hInstance,
            nullptr
        );

        if (!m_HWND)
        {
            DWORD err = GetLastError();
            LOG_ERROR("CreateWindowW failed with error code %s", err);
            return false;
        }

        ShowWindow(m_HWND, SW_SHOWDEFAULT);
        UpdateWindow(m_HWND);

        m_HDC = GetDC(m_HWND);
        PIXELFORMATDESCRIPTOR pfd = {};
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cDepthBits = 24;
        pfd.cStencilBits = 8;
        pfd.iLayerType = PFD_MAIN_PLANE;

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
            if (wParam != SIZE_MINIMIZED)
                SetResizing(lParam);
            return 0;

        case WM_SYSCOMMAND:
            if ((wParam & 0xFFF0) == SC_KEYMENU)
                return 0;
            break;

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

    ImVec2 Renderer::GetWindowSizeImpl()
    {
        return ImVec2((float)m_Width, (float)m_Height);
    }


    LRESULT CALLBACK Renderer::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        return GetInstance().WndProcImpl(hwnd, msg, wparam, lparam);
    }
}
