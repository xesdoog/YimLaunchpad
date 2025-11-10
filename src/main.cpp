#include <common.hpp>
#include <core/gui/renderer.hpp>
#include <core/github/gitmgr.hpp>
#include <core/YimMenu/yimmenu.hpp>
#include <core/memory/pointers.hpp>


using namespace YLP;

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
	GlobalMutex lock("YLPCPPMUTEX");
	if (!lock.IsOwned())
	{
		HWND hExHWND = FindWindowA(nullptr, "YLP");
		if (hExHWND)
			SetForegroundWindow(hExHWND);

		MsgBox::Error(L"Warning", L"YLP is already running!");
		return 1;
	}

	auto appdata = std::filesystem::path(std::getenv("appdata"));
	g_Instance = GetModuleHandle(nullptr);
	g_ProjectPath = appdata / "YLP";
	g_YimPath = appdata / "YimMenu";
	g_YimV2Path = appdata / "YimMenuV2";

	if (!std::filesystem::exists(g_ProjectPath))
		std::filesystem::create_directories(g_ProjectPath);

	Logger::Init(g_ProjectPath / "cout.log");
	LOG_INFO("========== Initializing YLP ==========");

	Settings::Init(g_ProjectPath / "settings.json");
	ThreadManager::Init(4);

	if (!std::filesystem::exists(g_YimPath))
		LOG_INFO("User does not seem to have used YimMenu before.");

	if (!std::filesystem::exists(g_YimV2Path))
		LOG_INFO("User does not seem to have used YimMenu V2 before.");

	if (!Renderer::Init())
	{
		Renderer::Destroy();
		ThreadManager::Shutdown();
		Settings::Destroy();
		return 1;
	}

	GitHubManager::Init();
	YimMenuHandler::Init();

	g_Pointers.Init();
	g_Running = true;

	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			continue;
		}

		Renderer::Draw();
	}

	g_Running = false;
	Renderer::Destroy();
	ThreadManager::Shutdown();
	Settings::Destroy();

	return 0;
}
