#include "../../../common.hpp"
#include "FA_Min.hpp"
#include "JetBrainsMono.hpp"
#include "JetBrainsMonoBold.hpp"


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
    static const ImWchar icons_range[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };

    io.Fonts->AddFontFromMemoryCompressedTTF(fa_data, fa_size, size, &icon_cfg, icons_range);
}

void Fonts::Load(ImGuiIO& io)
{
    ImFontConfig cfg;
    cfg.OversampleH = 2;
    cfg.OversampleV = 2;
    cfg.PixelSnapH = true;

    Small = io.Fonts->AddFontFromMemoryCompressedTTF(jbm_data, jbm_size, 14.0f, &cfg);
    MergeIcons(io, 14.0f);

    Regular = io.Fonts->AddFontFromMemoryCompressedTTF(jbm_data, jbm_size, 18.0f, &cfg);
    MergeIcons(io, 18.0f);

    Bold = io.Fonts->AddFontFromMemoryCompressedTTF(jbmb_data, jbmb_size, 18.0f, &cfg);
    MergeIcons(io, 18.0f);

    Title = io.Fonts->AddFontFromMemoryCompressedTTF(jbmb_data, jbmb_size, 22.0f, &cfg);
    MergeIcons(io, 22.0f);
}
