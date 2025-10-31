#include <common.hpp>
#include "core/gui/renderer.hpp"


using namespace YLP;

// SHUT UP ABOUT ANNOTATION MISMATCH! **insert "what do you want" meme here**
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
    g_Instance = GetModuleHandle(nullptr);
    std::filesystem::path log_path;
    char cwd[MAX_PATH];
    DWORD result = GetCurrentDirectoryA(sizeof(cwd), cwd);
    if (result != 0 && result <= sizeof(cwd))
    {
        log_path = (std::filesystem::path)cwd / "cout.log"; // temporary
        Logger::Init(log_path);
    }

    LOG_INFO("==== YLP C++ ====");
    if (!Renderer::Init())
        return 1;

    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        Renderer::Draw();
    }

    Renderer::Destroy();
    return 0;
}
