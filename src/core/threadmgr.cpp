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


#include "threadmgr.hpp"

namespace YLP
{
	void ThreadManager::InitImpl(size_t threadCount)
	{
		Start(threadCount ? threadCount : 2);
	}

	void ThreadManager::ShutdownImpl()
	{
		m_StopFlag.store(true);
		m_CV.notify_all();

		for (auto& t : m_Workers)
			if (t.joinable())
				t.join();

		m_Workers.clear();
	}

	void ThreadManager::RunDetached(Task task)
	{
		std::thread([task = std::move(task)] {
			try
			{
				task();
			}
			catch (const std::exception& e)
			{
				LOG_ERROR("Detached thread exception: {}", e.what());
			}
		}).detach();
	}

	void ThreadManager::StartImpl(size_t count)
	{
		for (size_t i = 0; i < count; ++i)
			m_Workers.emplace_back([this] {
				WorkerLoop();
			});
	}

	void ThreadManager::EnqueueImpl(Task task)
	{
		{
			std::scoped_lock lock(m_Mutex);
			m_TaskQueue.push({std::chrono::steady_clock::now(), std::move(task)});
		}
		m_CV.notify_one();
	}

	void ThreadManager::EnqueueDelayedImpl(Task task, std::chrono::milliseconds delay)
	{
		{
			std::scoped_lock lock(m_Mutex);
			m_TaskQueue.push({std::chrono::steady_clock::now() + delay, std::move(task)});
		}
		m_CV.notify_one();
	}

	void ThreadManager::EnqueueRepeatingImpl(Task task, std::chrono::milliseconds interval)
	{
		EnqueueDelayed([=, this]() {
			if (!m_StopFlag.load())
			{
				try
				{
					task();
				}
				catch (const std::exception& e)
				{
					Logger::Log(Logger::eLogLevel::Error, "Repeating task exception: {}", e.what());
				}
				EnqueueRepeating(task, interval);
			}
		},
		    interval);
	}

	void ThreadManager::WorkerLoop()
	{
		while (!m_StopFlag.load())
		{
			std::unique_lock lock(m_Mutex);
			if (m_TaskQueue.empty())
			{
				m_CV.wait(lock, [this] {
					return m_StopFlag.load() || !m_TaskQueue.empty();
				});
			}
			else
			{
				auto now = std::chrono::steady_clock::now();
				auto& next = m_TaskQueue.top();
				if (now >= next.executeAt)
				{
					auto task = std::move(next.task);
					m_TaskQueue.pop();
					lock.unlock();

					try
					{
						task();
					}
					catch (const std::exception& e)
					{
						Logger::Log(Logger::eLogLevel::Error, "Task exception: {}", e.what());
					}
				}
				else
				{
					m_CV.wait_until(lock, next.executeAt);
				}
			}
		}
	}
}
