// YLP Project - GPL-3.0
// See LICENSE file or <https://www.gnu.org/licenses/> for details.


#pragma once

#include "procmon.hpp"


namespace YLP
{	struct GTAPointers
	{
		Pointer GameState{};
		Pointer GameTime{};
		std::string GameVersion{};
		std::string OnlineVersion{};

		void Reset() noexcept
		{
			*this = {};
		}
	};

	struct Pointers
	{
		GTAPointers Legacy;
		GTAPointers Enhanced;
		void Init();

		void Reset() noexcept
		{
			Legacy.Reset();
			Enhanced.Reset();
		}
	};

	extern std::shared_ptr<ProcessMonitor> g_ProcLegacy;
	extern std::shared_ptr<ProcessMonitor> g_ProcEnhanced;

	inline Pointers g_Pointers;
}
