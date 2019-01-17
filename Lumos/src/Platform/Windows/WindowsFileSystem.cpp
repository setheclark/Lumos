#include "LM.h"
#include "System/FileSystem.h"


#ifndef LINUX
#include <Windows.h>

namespace Lumos 
{

	/*void CALLBACK FileIOCompletionRoutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
	{
	}*/

	static HANDLE OpenFileForReading(const String& path)
	{
		return CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
	}

	static int64 GetFileSizeInternal(const HANDLE file)
	{
		LARGE_INTEGER size;
		GetFileSizeEx(file, &size);
		return size.QuadPart;
	}

	static bool ReadFileInternal(const HANDLE file, void* buffer, const int64 size)
	{
		OVERLAPPED ol = { 0 };
		return ReadFileEx(file, buffer, static_cast<DWORD>(size), &ol, nullptr) != 0;
	}

	bool FileSystem::FileExists(const String& path)
	{
		const DWORD result = GetFileAttributes(path.c_str());
		return !(result == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND);
	}

	int64 FileSystem::GetFileSize(const String& path)
	{
		const HANDLE file = OpenFileForReading(path);
		if (file == INVALID_HANDLE_VALUE)
			return -1;
		int64 result = GetFileSizeInternal(file);
		CloseHandle(file);
		
		return result;
	}

	bool FileSystem::ReadFile(const String& path, void* buffer, int64 size)
	{
		const HANDLE file = OpenFileForReading(path);
		if (file == INVALID_HANDLE_VALUE)
			return false;

		if (size < 0)
			size = GetFileSizeInternal(file);

		bool result = ReadFileInternal(file, buffer, size);
		CloseHandle(file);
		return result;
	}

	byte* FileSystem::ReadFile(const String& path)
	{
		const HANDLE file = OpenFileForReading(path);
		const int64 size = GetFileSizeInternal(file);
		byte* buffer = new byte[static_cast<uint>(size)];
		const bool result = ReadFileInternal(file, buffer, size);
		CloseHandle(file);
		if (!result)
			delete[] buffer;
		return result ? buffer : nullptr;
	}

	String FileSystem::ReadTextFile(const String& path)
	{
		const HANDLE file = OpenFileForReading(path);
		const int64 size = GetFileSizeInternal(file);
		String result(static_cast<uint>(size), 0);
		const bool success = ReadFileInternal(file, &result[0], size);
		CloseHandle(file);
		if (success)
		{
			// Strip carriage returns
			result.erase(std::remove(result.begin(), result.end(), '\r'), result.end());
		}
		return success ? result : String();
	}

	bool FileSystem::WriteFile(const String& path, byte* buffer)
	{
		const HANDLE file = CreateFile(path.c_str(), GENERIC_WRITE, NULL, nullptr, CREATE_NEW | OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file == INVALID_HANDLE_VALUE)
			return false;

		const int64 size = GetFileSizeInternal(file);
		DWORD written;
		const bool result = ::WriteFile(file, buffer, static_cast<DWORD>(size), &written, nullptr) != 0;
		CloseHandle(file);
		return result;
	}

	bool FileSystem::WriteTextFile(const String& path, const String& text)
	{
		return WriteFile(path, (byte*)&text[0]);
	}
}

#endif