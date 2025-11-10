// YLP Project - GPL-3.0
// See LICENSE file or <https://www.gnu.org/licenses/> for details.


#include "pointers.hpp"


namespace YLP
{
	std::shared_ptr<ProcessMonitor> g_ProcLegacy = nullptr;
	std::shared_ptr<ProcessMonitor> g_ProcEnhanced = nullptr;

	void Pointers::Init()
	{
		g_ProcLegacy = std::make_shared<ProcessMonitor>(
		    "GTA5.exe",
		    MonitorLegacy,
		    1s,
		    [this](ProcessScanner& scanner) {
			    {
				    auto gvov = scanner.FindPattern("8B C3 33 D2 C6 44 24 20", "Game Version");
				    if (gvov)
				    {
					    Legacy.GameVersion = gvov.Add(0x24).Rip().Read<std::string>();
					    Legacy.OnlineVersion = gvov.Add(0x24).Rip().Add(0x20).Read<std::string>();
				    }
			    }
			    {
				    auto gs = scanner.FindPattern("83 3D ? ? ? ? ? 75 17 8B 43 20 25", "Game State");
				    if (gs)
					    Legacy.GameState = gs.Add(0x2).Rip().Add(0x1);
			    }
			    {
				    auto glt = scanner.FindPattern("8B 05 ? ? ? ? 89 ? 48 8D 4D C8", "Game Time");
				    if (glt)
					    Legacy.GameTime = glt.Add(0x2).Rip();
			    }
		    },
		    g_YimV1);

		g_ProcEnhanced = std::make_shared<ProcessMonitor>(
		    "GTA5_Enhanced.exe",
		    MonitorEnhanced,
		    1s,
		    [this](ProcessScanner& scanner) {
			    {
				    auto gvov = scanner.FindPattern("4C 8D 0D ? ? ? ? 48 8D 5C 24 ? 48 89 D9 48 89 FA", "Game Version");
				    if (gvov)
				    {
					    Enhanced.GameVersion = gvov.Add(0x3).Rip().Read<std::string>();
					    Enhanced.OnlineVersion = gvov.Add(0x47).Add(3).Rip().Read<std::string>();
				    }
			    }
			    {
				    auto glt = scanner.FindPattern("3B 2D ? ? ? ? 76 ? 89 D9", "Game Time");
				    if (glt)
					    Enhanced.GameTime = glt.Add(0x2).Rip();
			    }
		    },
		    g_YimV2);
	}
}
