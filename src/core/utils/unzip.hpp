// YLP Project - GPL-3.0-or-later
// See LICENSE file or <https://www.gnu.org/licenses/> for details.


#pragma once
#include <thirdparty/zip_file.hpp>


namespace YLP
{
	inline bool Unzip(const std::filesystem::path& zipPath, const std::filesystem::path& outDir)
	{
		mz_zip_archive zip{};
		if (!exists(zipPath))
			return false;

		std::ifstream file(zipPath, std::ios::binary | std::ios::ate);
		if (!file)
			return false;

		auto size = file.tellg();
		std::vector<uint8_t> buffer(size);
		file.seekg(0);
		file.read(reinterpret_cast<char*>(buffer.data()), size);
		file.close();

		memset(&zip, 0, sizeof(zip));

		if (!mz_zip_reader_init_mem(&zip, buffer.data(), buffer.size(), 0))
			return false;

		create_directories(outDir);

		const int fileCount = (int)mz_zip_reader_get_num_files(&zip);
		for (int i = 0; i < fileCount; i++)
		{
			mz_zip_archive_file_stat fileStat{};
			if (!mz_zip_reader_file_stat(&zip, i, &fileStat))
				continue;

			const std::string filename = fileStat.m_filename;
			const auto destPath = outDir / filename;

			if (filename.ends_with('/'))
			{
				create_directories(destPath);
				continue;
			}

			create_directories(destPath.parent_path());
			if (!mz_zip_reader_is_file_a_directory(&zip, i))
			{
				if (!mz_zip_reader_extract_to_file(&zip, i, destPath.string().c_str(), 0))
				{
				}
			}
		}
		mz_zip_reader_end(&zip);
		return true;
	}
}
