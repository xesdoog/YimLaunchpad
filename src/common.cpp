#include "common.hpp"

#ifndef STB_IMAGE_IMPLEMENTATION
	#define STB_IMAGE_IMPLEMENTATION
#endif
#include <thirdparty/stb_image.h>


namespace YLP
{
	extern HINSTANCE g_Instance{nullptr};
	extern HWND g_Hwnd{0};
	extern std::filesystem::path g_ProjectPath{};
	extern std::filesystem::path g_YimPath{};
	extern std::filesystem::path g_YimV2Path{};
	extern std::atomic<bool> g_Running{false};
}
