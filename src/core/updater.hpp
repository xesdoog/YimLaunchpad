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
#pragma comment(lib, "Version.lib")

#include <winver.h>


namespace YLP
{
	class Updater
	{
	public:
		Updater() = default;
		~Updater() noexcept = default;

	private:
		struct Version
		{
			int major = 0;
			int minor = 0;
			int patch = 0;
			int build = 0;

			std::string ToString() const
			{
				std::ostringstream oss;
				oss << major << "." << minor << "." << patch << "." << build;
				return oss.str();
			}

			constexpr explicit operator bool() const noexcept
			{
				return major != 0;
			}

			constexpr bool operator<(const Version& other) const noexcept
			{
				if (major != other.major)
					return major < other.major;
				if (minor != other.minor)
					return minor < other.minor;
				if (patch != other.patch)
					return patch < other.patch;
				return build < other.build;
			}

			constexpr bool operator>(const Version& other) const noexcept
			{
				return other < *this;
			}

			constexpr bool operator!=(const Version& other) const noexcept
			{
				return major != other.major || minor != other.minor || patch != other.patch || build != other.build;
			}

			constexpr bool operator==(const Version& other) const noexcept
			{
				return major == other.major && minor == other.minor && patch == other.patch && build == other.build;
			}
		};

	public:
		enum UpdateState : uint8_t
		{
			Idle,
			Checking,
			Pending,
			Downloading,
			Ready,
			Error,
		};

		UpdateState GetState() const noexcept
		{
			return m_State;
		}

		float GetProgress() const noexcept
		{
			return m_DownloadProgress;
		}

		Version GetLocalVersion();
		Version GetRemoteVersion();

		void Check();
		void Download();
		void Update();
		void Reset();

	private:
		Version m_LocalVersion{};
		std::atomic<UpdateState> m_State{Idle};
		std::pair<std::wstring, std::wstring> m_ReleaseUrl{L"github.com", L"/xesdoog/YLP/releases/latest"};
		float m_DownloadProgress{0.f};
	};

	inline Updater YLPUpdater;
}
