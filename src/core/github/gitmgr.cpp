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


#include <core/utils/unzip.hpp>
#include "gitmgr.hpp"


using namespace std::chrono_literals;

namespace YLP
{
	void GitHubManager::InitImpl()
	{
		m_OrgName = "YimMenu-Lua";
		m_AuthToken = ""; // TODO: add optional OAuth (we no longer need it)
		m_ScriptsPath = g_YimPath / "scripts";
		m_CacheFile = g_ProjectPath / "repo_cache.json";

		ThreadManager::RunDelayed([this] {
			LoadETag();
			FetchRepositories();
		},
		    400ms);
	}

	void GitHubManager::LoadETagImpl()
	{
		if (!std::filesystem::exists(m_CacheFile))
			return;

		std::ifstream file(m_CacheFile);
		if (!file.is_open())
			return;

		nlohmann::json j;
		file >> j;

		if (j.empty())
			return;

		m_RepoETag = j["ETag"];
	}

	bool GitHubManager::LoadCacheImpl(bool log)
	{
		if (!std::filesystem::exists(m_CacheFile))
			return false;

		std::ifstream file(m_CacheFile);
		if (!file.is_open())
			return false;

		nlohmann::json j;
		file >> j;
		file.close();

		if (j.empty())
		{
			LOG_WARN("Lua repository cache is empty!");
			return false;
		}

		std::map<std::string, Repository> newRepos;
		m_RepoETag = j["ETag"];

		auto& repositories = j["repositories"];
		if (repositories.empty())
		{
			LOG_WARN("Lua repository cache is empty!");
			return false;
		}

		bool should_update_cache = false;
		for (auto& [name, repo] : repositories.items())
		{
			Repository script = repo.get<Repository>();
			std::filesystem::path localPath = m_ScriptsPath / name;
			std::filesystem::path disabledPath = m_ScriptsPath / "disabled" / name;
			bool installed = IsScriptInstalled(name);
			bool disabled = (IO::Exists(disabledPath) && IO::HasLuaFiles(disabledPath) && !IO::Exists(localPath));

			if (script.isInstalled && !installed)
			{
				script.isDisabled  = false;
				script.isInstalled = false;
				script.currentPath = "";
				script.lastChecked = std::chrono::system_clock::time_point{};

				should_update_cache = true;
			}

			if (script.isInstalled != installed)
			{
				script.isInstalled = installed;
				should_update_cache = true;
			}

			if (script.isDisabled != disabled)
			{
				script.isDisabled = disabled;
				should_update_cache = true;
			}

			script.isPendingUpdate = script.is_outdated();

			if (installed)
				script.currentPath = disabled ? disabledPath : localPath;
			else
				script.currentPath = "";

			newRepos[name] = std::move(script);
		}

		{
			std::unique_lock lock(m_Mutex);
			m_Repos.swap(newRepos);

			if (should_update_cache)
				SaveCache();
		}

		if (log)
			LOG_INFO("Loaded {} Lua repositories from cache", m_Repos.size());

		return true;
	}

	void GitHubManager::SaveCacheImpl()
	{
		if (m_Repos.empty())
			return;

		nlohmann::json j;
		j["ETag"] = m_RepoETag;
		auto& repos = j["repositories"];

		for (auto& [key, val] : m_Repos)
			repos[key] = val;

		std::ofstream file(m_CacheFile);
		file << j.dump(2);
		file.close();
	}

	void GitHubManager::FetchRepositoriesImpl()
	{
		try
		{
			LOG_INFO("Fetching Lua repositories from https://github.com/YimMenu-Lua");
			m_State = eLoadState::LOADING;
			std::wstring host = L"api.github.com";
			std::wstring path = L"/orgs/" + std::wstring(m_OrgName.begin(), m_OrgName.end()) + L"/repos?per_page=100";
			std::vector<std::wstring> headers = {
				L"User-Agent: YLP-GitHubClient\r\n",
				L"Accept: application/vnd.github+json\r\n",
				L"If-None-Match: " + std::wstring(m_RepoETag.begin(), m_RepoETag.end()) + L"\r\n",
			};

			auto response = Utils::HttpRequest(host, path, headers);
			if (!response.success)
			{
				if (response.status == 304)
				{
					LOG_DEBUG("Repository cache is still valid. Loading it...");
					if (LoadCache())
					{
						SortRepositories();
						m_State = eLoadState::READY;
						return;
					}
					else
						LOG_ERROR("Failed to load repository cache! Attempting to refetch...");
				}
				else
				{
					LOG_ERROR("GitHub request failed with HTTP {}", response.status);
					m_State = eLoadState::FAILED;
					return;
				}
			}

			if (response.body.empty())
			{
				LOG_ERROR("Empty GitHub response!");
				m_State = eLoadState::FAILED;
				return;
			}

			auto j = nlohmann::json::parse(response.body, nullptr, false);
			if (j.is_discarded())
			{
				LOG_ERROR("Failed to parse GitHub response!");
				m_State = eLoadState::FAILED;
				return;
			}

			m_RepoETag = response.eTag;
			std::map<std::string, Repository> newRepos;

			for (auto& repo : j)
			{
				std::string name = repo["name"];
				if (std::find(m_IgnoreList.begin(), m_IgnoreList.end(), name) != m_IgnoreList.end())
				{
					LOG_INFO("Skipping repository {}", name);
					continue;
				}

				Repository script;

				script.name = name;
				script.stars = repo.value("stargazers_count", 0);
				script.lastUpdate = repo.value("pushed_at", "");
				script.description = repo.value("description", "");
				script.isInstalled = IsScriptInstalled(name);
				script.htmlUrl = std::format("https://github.com/{}/{}", m_OrgName, name);

				std::filesystem::path enabledPath = m_ScriptsPath / name;
				std::filesystem::path disabledPath = m_ScriptsPath / "disabled" / name;
				script.isDisabled = IO::Exists(disabledPath) && IO::HasLuaFiles(disabledPath) && !IO::Exists(enabledPath);
				script.currentPath = "";

				if (script.isInstalled)
					script.currentPath = script.isDisabled ? disabledPath : enabledPath;

				newRepos[name] = script;
			}

			{
				std::unique_lock lock(m_Mutex);
				m_Repos.swap(newRepos);
			}

			LOG_INFO("Fetched {} repositories from GitHub", m_Repos.size());
			SaveCache();
			SortRepositories();
			m_State = eLoadState::READY;
		}
		catch (const std::exception& e)
		{
			LOG_ERROR("Exception caught while fetching repositories from Github! {}", e.what());
		}
	}

	void GitHubManager::RefreshRepositoriesImpl()
	{
		ThreadManager::RunDetached([this]() {
			std::map<std::string, Repository> newRepos;
			try
			{
				FetchRepositories();
				SaveCache();
				m_State = eLoadState::READY;
			}
			catch (const std::exception& e)
			{
				LOG_ERROR("Failed to refresh repositories! {}", e.what());
				m_State = eLoadState::FAILED;
			}
		});
	}

	void GitHubManager::SortRepositoriesImpl()
	{
		std::vector<const Repository*> sorted;
		{
			std::shared_lock lock(m_Mutex);
			sorted.reserve(m_Repos.size());
			for (auto& [name, repo] : m_Repos)
			{
				if (m_SortMode == eSortMode::INSTALLED && !repo.isInstalled)
					continue;

				sorted.push_back(&repo);
			}
		}

		std::sort(sorted.begin(), sorted.end(), [mode = m_SortMode](auto a, auto b) {
			switch (mode)
			{
			case eSortMode::NAME:   return a->name < b->name;
			case eSortMode::STARS:  return a->stars > b->stars;
			case eSortMode::COMMIT: return a->lastUpdate > b->lastUpdate;
			default:                return false;
			}
		});

		{
			std::unique_lock lock(m_Mutex);
			m_SortedView.swap(sorted);
		}
	}

	void GitHubManager::MoveRepositoryImpl(const std::string& name, std::filesystem::path src, std::filesystem::path dest)
	{
		std::scoped_lock lock(m_Mutex);
		auto it = m_Repos.find(name);
		if (it == m_Repos.end())
			return;

		auto& repo = it->second;
		if (repo.isDownloading)
			return;

		IO::Rename(src, dest);
		repo.isDisabled = dest.parent_path().filename().string() == "disabled";
		repo.currentPath = dest;
		SaveCache();
	}

	void GitHubManager::RemoveRepositoryImpl(const std::string& name)
	{
		std::scoped_lock lock(m_Mutex);
		auto it = m_Repos.find(name);
		if (it == m_Repos.end())
			return;

		auto& repo = it->second;
		if (repo.isDownloading)
			return;

		IO::RemoveAll(repo.currentPath);

		repo.isDisabled = false;
		repo.isInstalled = false;
		repo.currentPath = "";
		repo.lastChecked = std::chrono::system_clock::time_point{};
		SaveCache();
	}

	void GitHubManager::DownloadRepositoryImpl(const std::string& name)
	{
		ThreadManager::RunDetached([this, name]() mutable {
			auto it = m_Repos.find(name);
			if (it == m_Repos.end())
				return;

			auto& repo = it->second;

			{
				std::scoped_lock lock(m_Mutex);
				if (repo.isDownloading)
					return;

				repo.isDownloading = true;
				repo.downloadProgress = 0.f;
			}

			const auto cacheDir = g_ProjectPath / "downloads_cache";
			if (!IO::Exists(cacheDir))
				IO::CreateFolders(cacheDir);

			const auto zipPath = cacheDir / (name + ".zip");
			const auto extractDir = cacheDir / (name + "_extracted");
			const auto installDir = m_ScriptsPath / name;
			const std::wstring host = L"github.com";
			const std::wstring path = L"/"
				+ std::wstring(m_OrgName.begin(), m_OrgName.end())
				+ L"/" + std::wstring(name.begin(), name.end())
				+ L"/archive/refs/heads/main.zip";

			auto response = Utils::HttpRequest(host, path, {}, &zipPath, &repo.downloadProgress);
			if (!response.success)
			{
				LOG_ERROR("Failed to download repository: {}", name);
				std::scoped_lock lock(m_Mutex);
				repo.isDownloading = false;
				repo.downloadProgress = 0.f;
				return;
			}

			LOG_INFO("Downloaded repository ZIP to {}. Extracting...", zipPath.string());

			if (!Unzip(zipPath, extractDir))
			{
				LOG_ERROR("Failed to extract repository: {}", repo.name);
				std::scoped_lock lock(m_Mutex);
				repo.isDownloading = false;
				repo.downloadProgress = 0.f;
				return;
			}

			std::filesystem::path repoRoot;
			for (auto& entry : std::filesystem::directory_iterator(extractDir))
			{
				if (std::filesystem::is_directory(entry.path()))
				{
					repoRoot = entry.path();
					break;
				}
			}

			if (repoRoot.empty())
			{
				LOG_ERROR("Could not locate extracted repository root folder in {}", extractDir.string());
				std::scoped_lock lock(m_Mutex);
				repo.isDownloading = false;
				repo.downloadProgress = 0.f;
				return;
			}

			IO::FilterLuaFiles(repoRoot);

			// The human torch was denied a bank loan
			if (IO::Exists(installDir))
				IO::RemoveAll(installDir);

			IO::Rename(repoRoot, installDir);

			for (auto& entry : std::filesystem::recursive_directory_iterator(installDir))
			{
				if (std::filesystem::is_directory(entry.path()) && (entry.path().filename().string().substr(0, 1) == "."))
				{
					IO::RemoveAll(entry.path());
					break;
				}
			}

			LOG_INFO("Installed repository to {}. Performing cleanup...", installDir.string());

			IO::RemoveAll(extractDir);
			IO::Remove(zipPath);

			{
				std::scoped_lock lock(m_Mutex);
				repo.downloadProgress = 0.f;
				repo.isDownloading = false;
				repo.isInstalled = true;
				repo.lastChecked = std::chrono::system_clock::now();
				repo.currentPath = m_ScriptsPath / name;
			}

			SaveCache();
			LOG_INFO("Done.");
		});
	}

	const bool GitHubManager::IsScriptInstalled(const std::string& name)
	{
		auto enabled_path = m_ScriptsPath / name;
		auto disabled_path = m_ScriptsPath / "disabled" / name;
		return (IO::Exists(enabled_path) && IO::HasLuaFiles(enabled_path)) || (IO::Exists(disabled_path) && IO::HasLuaFiles(disabled_path));
	}

}
