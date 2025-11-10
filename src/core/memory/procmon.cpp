// Copyright (C) 2025 SAMURAI (xesdoog) & Contributors
// This file is part of YLP.
//
// YLP is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// YLP is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with YLP.  If not, see <https://www.gnu.org/licenses/>.


#include "procmon.hpp"
#include <core/gui/gui.hpp>


namespace YLP
{
	ProcessMonitor::ProcessMonitor(const std::string& processName,
	    uint8_t targetMonitorMode,
	    std::chrono::milliseconds startDelay,
	    OnProcessFoundCallback onFound,
	    YimMenu& menu) :
	    m_ProcessName(processName),
	    m_Scanner(std::make_unique<ProcessScanner>(processName)),
	    m_OnFound(std::move(onFound))
	{
		m_Menu = &menu;
		m_MonitorMode = targetMonitorMode;

		if (Config().autoMonitorFlags & m_MonitorMode)
			Start(startDelay);
	}

	ProcessMonitor::~ProcessMonitor()
	{
		Stop();
	};

	void ProcessMonitor::Start(std::chrono::milliseconds delay)
	{
		if (m_Running)
			return;

		m_Running = true;

		ThreadManager::RunDelayed([this]() {
			MonitorLoop();
		},
		    delay);
	}

	void ProcessMonitor::Stop()
	{
		m_Running.store(false);
	}

	bool ProcessMonitor::IsRunning() const
	{
		return m_Running.load();
	};

	void ProcessMonitor::CheckBE()
	{
		if (std::chrono::system_clock::now() - m_LastBEcheckTime > 10s)
		{
			m_HeBeWatchin = IsBattlEyeRunning();
			m_LastBEcheckTime = std::chrono::system_clock::now();
		}
	}

	bool ProcessMonitor::IsProcessRunning() const
	{
		return m_Found.load();
	};

	bool ProcessMonitor::WaitForProcessStart(int timeoutMs)
	{
		return Utils::WaitUntil([&] {
			return m_Scanner && m_Scanner->GetBaseAddress();
		},
		    timeoutMs,
		    100);
	}

	bool ProcessMonitor::WaitForModuleLoad(const std::string& dllName, int timeoutMs)
	{
		return Utils::WaitUntil([&] {
			return m_Scanner && m_Scanner->IsModuleLoaded(dllName);
		},
		    timeoutMs,
		    200);
	}

	bool ProcessMonitor::WaitForGameReady(int timeoutMs)
	{
		return Utils::WaitUntil([&] {
			HWND hwnd = PsUtils::GetHwndFromPid(m_Scanner->GetProcessID());
			if (!IsWindow(hwnd))
				return false;

			std::this_thread::sleep_for(2s);
			return true;
		},
		    timeoutMs,
		    200);
	}

	bool ProcessMonitor::IsBattlEyeRunning() const
	{
		return PsUtils::IsServiceRunning(L"BEService");
	}

	bool ProcessMonitor::AutoInject()
	{
		if (m_MonitorMode == MonitorEnhanced) // not supported yet
			return false;

		if (!m_Scanner || !m_Menu)
			return false;

		if (m_HeBeWatchin)
		{
			LOG_ERROR("ProcMon: BattlEye detected! Auto-inject skipped.");
			return false;
		}

		if (m_Scanner->IsModuleLoaded(m_Menu->m_DllName))
		{
			LOG_WARN("ProcMon: Module was already injected. Auto-inject skipped.");
			return false;
		}

		LOG_INFO("ProcMon: Preparing to auto-inject...");

		auto& pointers = (m_MonitorMode == MonitorLegacy) ? g_Pointers.Legacy : g_Pointers.Enhanced;
		if (!pointers.GameTime || !pointers.GameState)
		{
			LOG_ERROR("ProcMon: Auto-inject failed! Game pointers not ready.");
			return false;
		}

		bool attempted_inject = false;
		std::string last_log;

		while (g_Running && m_Running && !attempted_inject)
		{
			int32_t gametime = pointers.GameTime.Read<int32_t>();
			int8_t gamestate = pointers.GameState.Read<int8_t>();

			if (gametime < 10000)
			{
				std::this_thread::sleep_for(50ms);
				continue;
			}

			if (gamestate == 5)
			{
				std::string msg = "ProcMon: Auto-injecting in 3 seconds...";
				if (msg != last_log)
				{
					LOG_INFO(msg);
					last_log = msg;
				}

				std::this_thread::sleep_for(3s);
				m_Menu->Inject();
				attempted_inject = true;
			}
			else if (gamestate == 0 || gamestate > 5)
			{
				LOG_WARN("ProcMon: Too late to inject! You can still manually inject at your own risk.");
				break;
			}
			else
			{
				std::string msg = "ProcMon: Waiting for the landing page...";
				if (msg != last_log)
				{
					LOG_INFO(msg);
					last_log = msg;
				}

				std::this_thread::sleep_for(100ms);
			}
		}

		if (attempted_inject)
			return OnPostInject();

		return false;
	}

	bool ProcessMonitor::OnPostInject()
	{
		const auto& dllName = m_Menu->m_DllName;
		LOG_INFO("ProcMon: Checking module load for {}...", dllName);

		if (!WaitForModuleLoad(dllName, 10000))
		{
			LOG_WARN("ProcMon: Module {} not detected after injection.", dllName);
			return false;
		}

		LOG_INFO("ProcMon: Module {} loaded successfully. Checking process stability after injection...", dllName);	
		auto result = PsUtils::WaitForProcessExit(m_Scanner->GetProcessHandle(), 10000);
		
		if (result.has_value())
		{
			DWORD exitCode = result.value();
			LOG_WARN("ProcMon: Possible crash detected! Exit Code: 0x{:X} ({})", exitCode, PsUtils::TranslateError(exitCode));
			return false;
		}

		LOG_INFO("ProcMon: Everything seems fine.");

		if (Config().autoExit)
		{
			LOG_INFO("YLP: Shutting down in 3 seconds...");
			GUI::ToggleDisableUI(true);
			std::this_thread::sleep_for(3s);
			m_Running.store(false);
			m_Running.notify_all();
			PostMessageW(g_Hwnd, WM_CLOSE, 0, 0);
		}

		return true;
	}

	void ProcessMonitor::MonitorLoop()
	{
		if (m_MonitorMode == MonitorEnhanced && !Config().supportsV2)
			return;

		if (!(Config().autoMonitorFlags & m_MonitorMode))
			return;

		if (IsBattlEyeRunning())
		{
			LOG_WARN("ProcMon: Standing down until the all-seeing Bastian looks away. (BattlEye detected!)");
			m_HeBeWatchin.store(true);
			m_LastBEcheckTime = std::chrono::system_clock::now();
			return;
		}

		LOG_INFO("ProcMon: Started monitoring '{}'", m_ProcessName);

		try
		{
			while (m_Running && g_Running)
			{
				CheckBE();

				if (m_HeBeWatchin)
					std::this_thread::sleep_for(1s);
				else
				{
					if (!m_Scanner)
					{
						std::scoped_lock lock(m_Mutex);
						m_Scanner = std::make_unique<ProcessScanner>(m_ProcessName);
					}

					if (!m_Found && m_Scanner && m_Scanner->FindProcess(m_ProcessName))
					{
						m_Found = true;
						LOG_INFO("ProcMon: Found process '{}'", m_ProcessName);

						if (m_OnFound)
						{
							try
							{
								WaitForGameReady(20000);
								m_OnFound(*m_Scanner);

								if (m_Menu && Config().autoInject)
									AutoInject();
							}
							catch (const std::exception& e)
							{
								LOG_ERROR("ProcMon: Exception in callback: {}", e.what());
							}
						}
					}
					else if (m_Found && !m_Scanner->IsProcessRunning())
					{
						LOG_INFO("ProcMon: Process '{}' was terminated.", m_ProcessName);
						{
							std::scoped_lock lock(m_Mutex);
							m_Found = false;
							m_Scanner.reset();

							auto& pointers = (m_MonitorMode == MonitorLegacy) ? g_Pointers.Legacy : g_Pointers.Enhanced;
							pointers.Reset();
							std::this_thread::sleep_for(5s);
						}
					}
				}
				std::this_thread::sleep_for(250ms);
			}

			LOG_INFO("ProcMon: Stopped monitoring '{}'", m_ProcessName);
			m_Running = false;
			m_Scanner.reset();
		}
		catch (std::exception& e)
		{
			LOG_ERROR("ProcMon: Caught unhandled exception: {}", e.what());
		}
	}
}
