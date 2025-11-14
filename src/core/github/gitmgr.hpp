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
#include "repository.hpp"


namespace YLP
{
	class GitHubManager : public Singleton<GitHubManager>
	{
		friend class Singleton<GitHubManager>;

	private:
		GitHubManager() = default;

	public:
		~GitHubManager()
		{
			SaveCache();
		}

		enum class eLoadState
		{
			NONE,
			LOADING,
			READY,
			FAILED
		};

		enum class eSortMode
		{
			NAME,
			STARS,
			COMMIT,
			INSTALLED
		};

		eSortMode m_SortMode = eSortMode::STARS;
		mutable std::shared_mutex m_Mutex{};

		static void Init()
		{
			GetInstance().InitImpl();
		}

		static void LoadETag()
		{
			GetInstance().LoadETagImpl();
		}

		static bool LoadCache(bool log = true)
		{
			return GetInstance().LoadCacheImpl(log);
		}

		static void SaveCache()
		{
			GetInstance().SaveCacheImpl();
		}

		static void FetchRepositories()
		{
			GetInstance().FetchRepositoriesImpl();
		}

		static void RefreshRepositories()
		{
			GetInstance().RefreshRepositoriesImpl();
		}

		static void SortRepositories()
		{
			GetInstance().SortRepositoriesImpl();
		}

		static const std::map<std::string, Repository>& GetRepos()
		{
			return GetInstance().m_Repos;
		}

		static const std::vector<const Repository*>& GetSortedRepos()
		{
			return GetInstance().m_SortedView;
		}

		static void SetSortMode(const eSortMode mode)
		{
			auto old = GetInstance().m_SortMode;
			GetInstance().m_SortMode = mode;

			if (old != mode)
				SortRepositories();
		}

		static const eSortMode GetSortMode()
		{
			return GetInstance().m_SortMode;
		}

		static const eLoadState GetState()
		{
			return GetInstance().m_State.load();
		}

		static std::shared_lock<std::shared_mutex> GetMutex()
		{
			return std::shared_lock(GetInstance().m_Mutex);
		}

		static void DownloadRepository(const std::string& name)
		{
			GetInstance().DownloadRepositoryImpl(name);
		}


		static void MoveRepository(const std::string& name, std::filesystem::path src, std::filesystem::path dest)
		{
			GetInstance().MoveRepositoryImpl(name, src, dest);
		}

		static void RemoveRepository(const std::string& name)
		{
			GetInstance().RemoveRepositoryImpl(name);
		}

		static bool IsAuthenticated()
		{
			return !GetInstance().m_AuthToken.empty();
		}

		static const bool IsScriptInstalled(const std::string& name);

	private:
		void InitImpl();
		void LoadETagImpl();
		bool LoadCacheImpl(bool log = true);
		void SaveCacheImpl();
		void FetchRepositoriesImpl();
		void RefreshRepositoriesImpl();
		void SortRepositoriesImpl();
		void DownloadRepositoryImpl(const std::string& name);
		void MoveRepositoryImpl(const std::string& name, std::filesystem::path src, std::filesystem::path dest);
		void RemoveRepositoryImpl(const std::string& name);

		std::string m_OrgName;
		std::string m_AuthToken;
		std::string m_RepoETag;
		std::filesystem::path m_CacheFile;
		std::map<std::string, Repository> m_Repos;
		std::vector<const Repository*> m_SortedView;

		std::atomic<eLoadState> m_State = eLoadState::NONE;

		static inline std::filesystem::path m_ScriptsPath;

		static inline const std::vector<std::string> m_IgnoreList = {
		    "YimLLS",
		    "Example",
		    "submission",
		    "SAMURAI-Scenarios",
		    "SAMURAI-Animations"};
	};
}
