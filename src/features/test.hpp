static void DrawTest()
{
    ImGui::PushFont(Fonts::Title);
    ImGui::Text("Title Font");
    ImGui::PopFont();

    ImGui::PushFont(Fonts::Bold);
    ImGui::Text("Bold Font");
    ImGui::PopFont();

    ImGui::PushFont(Fonts::Regular);
    ImGui::Text("Regular Font");
    ImGui::PopFont();

    ImGui::PushFont(Fonts::Small);
    ImGui::Text("Tiny Octocat %s", ICON_FA_GITHUB);
    ImGui::PopFont();

    ImGui::Text(ICON_FA_DOWNLOAD);

    ImGuiHelpers::Spinner("Loading...");
}
