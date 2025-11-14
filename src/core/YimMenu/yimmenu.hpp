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


#pragma once

#include <core/gui/notifier.hpp>


namespace YLP
{
	enum eYimVersion : uint8_t
	{
		YimMenuV1 = 1,
		YimMenuV2
	};

	class YimMenu final
	{
	public:
		YimMenu() = default;
		~YimMenu() = default;
		YimMenu(YimMenu&&) noexcept = default;
		YimMenu& operator=(YimMenu&&) noexcept = default;
		YimMenu(const YimMenu&) = delete;
		YimMenu& operator=(const YimMenu&) = delete;

		explicit operator bool() const
		{
			return m_Exists;
		}

		enum eMenuViewState : uint8_t
		{
			Idle,
			Error,
			Checking,
			Downloading,
			PendingUpdate,
		};

		eMenuViewState GetState() const
		{
			return m_State;
		}

		void ResetState()
		{
			m_State = Idle;
		}

		void Notify(const std::string& msg, Notifier::eNotificationLevel level = Notifier::Info, bool log = false)
		{
			std::string title = std::format("[{}]", m_Name);
			Notifier::Add(title, msg, level);
			
			if (log)
				Logger::Log(static_cast<Logger::eLogLevel>(level), msg);
		}

		bool SanityCheck()
		{
			if (m_Exists && !IO::Exists(m_DllPath))
			{
				m_State = Error;
				m_Exists = false;
				Notify("The DLL file was removed! Please check your anti-virus settings.", Notifier::Error, true);
				return false;
			}
			return m_Exists;
		}

		void Init(eYimVersion version,
		    const std::filesystem::path& path,
		    const std::string& url,
		    const std::pair<std::wstring, std::wstring>& releaseUrl,
		    const std::pair<std::wstring, std::wstring>& downloadUrl)
		{
			m_Version = version;
			m_BasePath = path;
			m_Url = url;
			m_ReleaseUrl = releaseUrl;
			m_DownloadUrl = downloadUrl;
			m_Name = version == YimMenuV1 ? "YimMenu" : "YimMenuV2";
			m_DllName = m_Name + ".dll";
			m_TargetProcess = version == YimMenuV1 ? "GTA5.exe" : "GTA5_Enhanced.exe";
			m_DllPath = path / m_DllName;
			m_Exists = IO::Exists(m_DllPath);
			m_ChecksumPath = path / (m_Name + ".sha256");

			if (m_Exists && !ReadChecksum())
				UpdateChecksum();
		}

		bool ReadChecksum()
		{
			if (!IO::Exists(m_ChecksumPath))
				return false;

			std::ifstream f(m_ChecksumPath);
			if (!f.is_open())
			{
				LOG_WARN("{}: Failed to read SHA256 checksum from file.", m_Name);
				return false;
			}

			std::getline(f, m_Checksum);
			f.close();
			return true;
		}

		void UpdateChecksum()
		{
			if (!SanityCheck())
				return;

			m_Checksum = Utils::CalcSha256(m_DllPath);
			std::ofstream f(m_ChecksumPath);
			if (f.is_open())
			{
				f << m_Checksum;
				f.close();
			}
		}

		void GetCommitHash()
		{
			m_LastCommitHash = Utils::RegexMatchHtml(
			    m_ReleaseUrl.first,
			    m_ReleaseUrl.second,
			    R"(/commit/([0-9a-f]+)/hovercard)",
			    1);
		}

		void Download()
		{
			std::scoped_lock lock(m_Mutex);

			if (!IO::Exists(g_ProjectPath))
				return;

			if (!IO::Exists(m_DllPath.parent_path()))
				IO::CreateFolders(m_DllPath.parent_path());

			try
			{
				LOG_INFO("{}: Downloading latest Nightly release...", m_Name);
				m_State = Downloading;
				auto response = Utils::HttpRequest(m_DownloadUrl.first, m_DownloadUrl.second, {}, &m_DllPath, &m_DownloadProgress);
				if (!response.success)
				{
					LOG_ERROR("{}: Failed to download DLL!\n\tHTTP status: {}", m_Name, response.status);
					ResetState();
					return;
				}

				UpdateChecksum();
				GetCommitHash();
				ResetState();
				m_Exists = IO::Exists(m_DllPath);
				LOG_INFO("{}: Done.", m_Name);
			}
			catch (const std::exception& e)
			{
				LOG_ERROR("An exception has occured: {}", e.what());
			}
		}

		void CheckForUpdates()
		{
			std::scoped_lock lock(m_Mutex);
			m_State = Checking;

			if (!SanityCheck())
				return;

			try
			{
				LOG_INFO("[{}]: Checking for updates...", m_Name);
				auto remote_sha = Utils::RegexMatchHtml(
				    m_ReleaseUrl.first,
				    m_ReleaseUrl.second,
				    R"(data-snippet-clipboard-copy-content=\"([0-9a-fA-F]{64})[^"]*?\.dll)",
				    1);

				if (m_Checksum.empty())
					UpdateChecksum();

				bool pendingUpdate = !m_Checksum.empty() && remote_sha != m_Checksum;
				if (!pendingUpdate)
				{
					LOG_INFO("[{}]: No new releases found.", m_Name);
					m_State = Idle;
				}
				else
				{
					Notify("A new release is out!", Notifier::Info, true);
					m_State = PendingUpdate;
				}

				if (m_LastCommitHash.empty())
					GetCommitHash();
			}
			catch (const std::exception& e)
			{
				LOG_ERROR("[{}]: An exception has occured: {}", m_Name, e.what());
			}
		}

		PsUtils::InjectResult Inject()
		{
			if (!SanityCheck())
				return PsUtils::InjectResult::Err("File not found!", 2);

			const auto result = PsUtils::Inject(m_TargetProcess, m_DllPath);
			m_IsInjected = result.success;
			return result;
		}

	private:
		bool m_Exists{false};

		std::filesystem::path m_BasePath{};
		std::filesystem::path m_ChecksumPath{};
		std::pair<std::wstring, std::wstring> m_ReleaseUrl{};
		std::pair<std::wstring, std::wstring> m_DownloadUrl{};
		std::mutex m_Mutex;

	public:
		bool mutable m_IsInjected{false};
		float m_DownloadProgress{0.f};
		eYimVersion m_Version{YimMenuV1};
		eMenuViewState m_State{Idle};

		std::string m_Name{};
		std::string m_DllName{};
		std::string m_Checksum{};
		std::string m_LastCommitHash{};
		std::string m_TargetProcess{};
		std::string m_Url{};
		std::filesystem::path m_DllPath{};
	};

	class YimMenuHandler : public Singleton<YimMenuHandler>
	{
		friend class Singleton<YimMenuHandler>;

	private:
		YimMenuHandler() = default;
		~YimMenuHandler() = default;

		void InitImpl()
		{
			if (m_Initialized)
				return;

			{
				std::scoped_lock lock(m_Mutex);
				m_V1.Init(
				    YimMenuV1,
				    g_ProjectPath / "YimMenu",
				    "https://github.com/Mr-X-GTA/YimMenu",
				    {L"github.com", L"/Mr-X-GTA/YimMenu/releases/tag/nightly"},
				    {L"github.com", L"/Mr-X-GTA/YimMenu/releases/download/nightly/YimMenu.dll"});
			}

			// if (Config().autoMonitorFlags & MonitorLegacy) // should be default behavior otherwise what's the point?
			ThreadManager::RunDelayed([this] {
				m_V1.CheckForUpdates();
			},
			    2s);

			if (Config().supportsV2)
			{
				{
					std::scoped_lock lock(m_Mutex);
					m_V2.Init(
					    YimMenuV2,
					    g_ProjectPath / "YimMenu",
					    "https://github.com/YimMenu/YimMenuV2",
					    {L"github.com", L"/YimMenu/YimMenuV2/releases/tag/nightly"},
					    {L"github.com", L"/YimMenu/YimMenuV2/releases/download/nightly/YimMenuV2.dll"});
				}

				// if (Config().autoMonitorFlags & MonitorEnhanced)
				ThreadManager::RunDelayed([this] {
					m_V2.CheckForUpdates();
				},
				    3s);
			}

			m_Initialized = true;
		}

	private:
		std::filesystem::path m_BasePath;
		std::mutex m_Mutex;
		bool m_Initialized = false;

		YimMenu m_V1;
		YimMenu m_V2;

	public:
		static void Init()
		{
			GetInstance().InitImpl();
		}

		static YimMenu& Get(eYimVersion ver) noexcept
		{
			switch (ver)
			{
			case YimMenuV1:
				return GetInstance().m_V1;
			case YimMenuV2:
				return GetInstance().m_V2;
			default:
				return GetInstance().m_V1;
			}
		}

		static void Download(eYimVersion ver)
		{
			Get(ver).Download();
		}

		static void CheckForUpdates(eYimVersion ver)
		{
			Get(ver).CheckForUpdates();
		}
	};

	inline YimMenu& g_YimV1 = YimMenuHandler::Get(YimMenuV1);
	inline YimMenu& g_YimV2 = YimMenuHandler::Get(YimMenuV2);
}
