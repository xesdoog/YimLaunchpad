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

#include <condition_variable>
#include <future>
#include <thread>
#include <queue>
#include <utility>


namespace YLP
{
	using Task = std::function<void()>;

	struct ScheduledTask
	{
		std::chrono::steady_clock::time_point executeAt;
		Task task;

		bool operator>(const ScheduledTask& other) const noexcept
		{
			return executeAt > other.executeAt;
		}
	};

	class ThreadManager : public Singleton<ThreadManager>
	{
		friend class Singleton<ThreadManager>;

	private:
		ThreadManager() = default;
		~ThreadManager()
		{
			ShutdownImpl();
		}

	public:
		static void Init(size_t threadCount = std::thread::hardware_concurrency())
		{
			GetInstance().InitImpl(threadCount);
		}

		static void Shutdown()
		{
			GetInstance().ShutdownImpl();
		}

		static void Destroy()
		{
			Shutdown();
		}

		static void Run(Task task)
		{
			GetInstance().Enqueue(std::move(task));
		}

		static void RunDetached(Task task);

		static void RunDelayed(Task task, std::chrono::milliseconds delay = 1ms)
		{
			GetInstance().EnqueueDelayed(std::move(task), delay);
		}

		static void RunRepeating(Task task, std::chrono::milliseconds interval = 1ms)
		{
			GetInstance().EnqueueRepeating(std::move(task), interval);
		}

		static void Start(size_t count)
		{
			GetInstance().StartImpl(count);
		}

		static void Enqueue(Task task)
		{
			GetInstance().EnqueueImpl(task);
		}

		static void EnqueueDelayed(Task task, std::chrono::milliseconds delay = 1ms)
		{
			GetInstance().EnqueueDelayedImpl(task, delay);
		}

		static void EnqueueRepeating(Task task, std::chrono::milliseconds interval = 1ms)
		{
			GetInstance().EnqueueRepeatingImpl(task, interval);
		}

	private:
		void InitImpl(size_t threadCount = std::thread::hardware_concurrency());
		void ShutdownImpl();
		void StartImpl(size_t count);
		void EnqueueImpl(Task task);
		void EnqueueDelayedImpl(Task task, std::chrono::milliseconds delay);
		void EnqueueRepeatingImpl(Task task, std::chrono::milliseconds interval);
		void WorkerLoop();

	private:
		std::atomic<bool> m_StopFlag{false};
		std::vector<std::thread> m_Workers;

		std::priority_queue<
		    ScheduledTask,
		    std::vector<ScheduledTask>,
		    std::greater<> >
		    m_TaskQueue;

		std::mutex m_Mutex;
		std::condition_variable m_CV;
	};
}
