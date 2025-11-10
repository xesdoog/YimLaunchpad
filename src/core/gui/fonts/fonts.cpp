#if defined(__GNUC__) || defined(__clang__)
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wnull-character"
#elif defined(_MSC_VER)
	#pragma warning(push)
	#pragma warning(disable : 4820)
#endif

#include "IconFont.hpp"
#include "JetBrainsMono.hpp"
#include "JetBrainsMonoBold.hpp"

#if defined(__GNUC__) || defined(__clang__)
	#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
	#pragma warning(pop)
#endif


#include "fonts.hpp"


ImFont* Fonts::Small = nullptr;
ImFont* Fonts::Regular = nullptr;
ImFont* Fonts::Bold = nullptr;
ImFont* Fonts::Title = nullptr;

static void MergeIcons(ImGuiIO& io, float size)
{
	ImFontConfig icon_cfg;
	icon_cfg.MergeMode = true;
	icon_cfg.PixelSnapH = true;
	icon_cfg.GlyphMinAdvanceX = 16.0f;
	icon_cfg.GlyphOffset = ImVec2(0, 2);
	static const ImWchar icons_range[] = {ICON_MIN, ICON_MAX, 0};

	io.Fonts->AddFontFromMemoryCompressedTTF(
	    icon_font_compressed_data,
	    icon_font_compressed_size,
	    size,
	    &icon_cfg,
	    icons_range);
}

void Fonts::Load(ImGuiIO& io)
{
	ImFontConfig cfg;
	cfg.OversampleH = 2;
	cfg.OversampleV = 2;
	cfg.PixelSnapH = true;

	Small = io.Fonts->AddFontFromMemoryCompressedTTF(jbm_data, jbm_size, 15.0f, &cfg);
	MergeIcons(io, 13.0f);

	Regular = io.Fonts->AddFontFromMemoryCompressedTTF(jbm_data, jbm_size, 18.0f, &cfg);
	MergeIcons(io, 17.0f);

	Bold = io.Fonts->AddFontFromMemoryCompressedTTF(jbmb_data, jbmb_size, 18.0f, &cfg);
	MergeIcons(io, 17.0f);

	Title = io.Fonts->AddFontFromMemoryCompressedTTF(jbmb_data, jbmb_size, 22.0f, &cfg);
	MergeIcons(io, 22.0f);
}
