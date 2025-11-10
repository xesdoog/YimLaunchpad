// YLP Project - GPL-3.0
// See LICENSE file or <https://www.gnu.org/licenses/> for details.


#pragma once


namespace YLP
{
	class Pointer
	{
	public:
		Pointer() = default;
		~Pointer() = default;

		inline Pointer(HANDLE hProcess, uintptr_t address) :
		    m_ProcessHandle(hProcess),
		    m_Address(address)
		{
		}

		template<typename T>
		inline T Read(size_t maxLength = 64) const
		{
			if constexpr (std::is_same_v<T, std::string>)
			{
				std::string buffer(maxLength, '\0');
				SIZE_T bytesRead = 0;

				if (ReadProcessMemory(m_ProcessHandle,
				        reinterpret_cast<LPCVOID>(m_Address),
				        buffer.data(),
				        maxLength - 1,
				        &bytesRead))
				{
					buffer.resize(strnlen(buffer.c_str(), bytesRead));
					return buffer;
				}

				LOG_ERROR("Failed to read string at 0x{:X}", m_Address);
				return std::string{};
			}
			else
			{
				T value{};
				SIZE_T bytesRead = 0;

				if (!ReadProcessMemory(m_ProcessHandle,
				        reinterpret_cast<LPCVOID>(m_Address),
				        &value,
				        sizeof(T),
				        &bytesRead))
				{
					LOG_ERROR("Failed to read memory at 0x{:X}", m_Address);
				}

				return value;
			}
		}

		inline Pointer Add(int32_t offset)
		{
			return Pointer(m_ProcessHandle, m_Address + offset);
		}

		inline Pointer Sub(int32_t offset)
		{
			return Pointer(m_ProcessHandle, m_Address - offset);
		}

		inline Pointer Rip()
		{
			int32_t rel = Read<int32_t>();
			return Pointer(m_ProcessHandle, m_Address + rel + 4);
		}

		explicit operator bool() const
		{
			return m_Address != 0;
		}

	private:
		uintptr_t m_Address = 0;
		HANDLE m_ProcessHandle = INVALID_HANDLE_VALUE;
	};
}
