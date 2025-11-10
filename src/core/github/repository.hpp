// YLP Project - GPL-3.0
// See LICENSE file or <https://www.gnu.org/licenses/> for details.


#pragma once


namespace YLP
{
	struct Repository
	{
		std::string name;
		std::string description;
		std::string lastUpdate;
		std::string htmlUrl;
		std::chrono::system_clock::time_point lastChecked{};

		bool isStarred = false;
		bool isInstalled = false;
		bool isDisabled = false;
		bool isDownloading = false;
		bool isPendingUpdate = false;

		int stars = 0;
		float downloadProgress = 0.f;
		std::filesystem::path currentPath;

		bool is_outdated() const
		{
			if (lastChecked.time_since_epoch().count() == 0)
				return false;

			std::tm tm = {};
			std::istringstream ss(lastUpdate);
			ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
			auto pushedTime = std::chrono::system_clock::from_time_t(_mkgmtime(&tm));

			return lastChecked < pushedTime;
		}
	};

	inline void to_json(nlohmann::json& j, const Repository& r)
	{
		j = nlohmann::json{
		    {"name", r.name},
		    {"description", r.description},
		    {"lastUpdate", r.lastUpdate},
		    {"stars", r.stars},
		    {"isStarred", r.isStarred},
		    {"isInstalled", r.isInstalled},
		    {"isDisabled", r.isDisabled},
		    {"htmlUrl", r.htmlUrl},
		    {"lastChecked", std::chrono::system_clock::to_time_t(r.lastChecked)},
		};
	}

	inline void from_json(const nlohmann::json& j, Repository& r)
	{
		j.at("name").get_to(r.name);
		j.at("description").get_to(r.description);
		j.at("lastUpdate").get_to(r.lastUpdate);
		j.at("stars").get_to(r.stars);
		j.at("isStarred").get_to(r.isStarred);
		j.at("isInstalled").get_to(r.isInstalled);
		j.at("isDisabled").get_to(r.isDisabled);
		j.at("htmlUrl").get_to(r.htmlUrl);

		if (j.contains("lastChecked") && !j["lastChecked"].is_null())
		{
			std::time_t t = j["lastChecked"].get<std::time_t>();
			r.lastChecked = std::chrono::system_clock::from_time_t(t);
		}
		else
		{
			r.lastChecked = std::chrono::system_clock::time_point{};
		}
	}
}
