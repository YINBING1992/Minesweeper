#include <Windows.h>
#include <TlHelp32.h>
#include <iostream>
#include <vector>

uintptr_t g_baseAddress = 0;
HANDLE g_hProcess = NULL;


typedef struct TagInfoMap
{
	int m_nRow;
	int m_nMinCount;
	int m_nCol;


}TAGINFOMAP, * PTAGINFOMAP;

uintptr_t GetModuleBaseAddress(DWORD processId, const wchar_t* moduleName) {
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);

	if (hSnapshot == INVALID_HANDLE_VALUE) {
		return 0;
	}

	MODULEENTRY32 moduleEntry;
	moduleEntry.dwSize = sizeof(MODULEENTRY32);

	if (Module32First(hSnapshot, &moduleEntry)) {
		do {
			if (_wcsicmp(moduleEntry.szModule, moduleName) == 0) {
				CloseHandle(hSnapshot);
				return reinterpret_cast<uintptr_t>(moduleEntry.modBaseAddr);
			}
		} while (Module32Next(hSnapshot, &moduleEntry));
	}

	CloseHandle(hSnapshot);
	return 0;
}

TAGINFOMAP readInfoMap(_In_ HANDLE hProcess, uintptr_t baseAddress)
{
	// 获取宽度，高度和雷区数量
	TAGINFOMAP infoMap = {};

	std::vector<uintptr_t> offsets = { 0x03116DF0, 0x370, 0x30, 0x30 };
	for (uintptr_t offset : offsets) {
		ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(baseAddress + offset), &baseAddress, sizeof(baseAddress), NULL);
	}
	ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(baseAddress + 0x10), &infoMap, sizeof(infoMap), NULL);

	return infoMap;

}



uintptr_t uIsStart(_In_ HANDLE hProcess, uintptr_t baseAddress)
{

	// 计算内存地址
	std::vector<uintptr_t> offsets = { 0x03116DF0, 0x430, 0xa0, 0xd8 };
	for (uintptr_t offset : offsets) {
		ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(baseAddress + offset), &baseAddress, sizeof(baseAddress), NULL);
	}

	return baseAddress;
}


uintptr_t MinListPoint(_In_ HANDLE hProcess, uintptr_t baseAddress)
{
	// 计算内存地址
	std::vector<uintptr_t> offsets = { 0x03116DF0, 0x430, 0xa0, 0x70,0x10,0x10,0x10 };
	for (uintptr_t offset : offsets) {
		ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(baseAddress + offset), &baseAddress, sizeof(baseAddress), NULL);
	}

	return baseAddress;
}


uintptr_t MinListOnePoint(_In_ HANDLE hProcess, uintptr_t baseAddress, int offsets)
{

	ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(baseAddress + 0x10), &baseAddress, sizeof(baseAddress), NULL);
	ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(baseAddress + 0x20 + offsets * 8), &baseAddress, sizeof(baseAddress), NULL);

	return baseAddress;
}

DWORD WINAPI MyThreadFunction(LPVOID lpParam)
{
	uintptr_t mapMinListPointOld = 0;
	uintptr_t mapMinListPoint = 0;
	//读取是否开始了
	while (true)
	{
		uintptr_t uStart = uIsStart(g_hProcess, g_baseAddress);
		if (uStart != 0)
		{

			mapMinListPoint = MinListPoint(g_hProcess, g_baseAddress);
			if (mapMinListPointOld != mapMinListPoint)
			{
				mapMinListPointOld = mapMinListPoint;

				//读取行和列和个数
				TAGINFOMAP infoMap = readInfoMap(g_hProcess, g_baseAddress);
				printf("\n");
				printf("新游戏开始===>>>有  %d行  %d列,雷的数量为%d个", infoMap.m_nRow, infoMap.m_nCol, infoMap.m_nMinCount);
				printf("\n");
				for (int y = infoMap.m_nRow - 1; y > -1; y--)
				{
					uintptr_t mapMinListLinePoint = MinListOnePoint(g_hProcess, mapMinListPoint, y);
					for (int x = 0; x < infoMap.m_nCol; x++)
					{
						uintptr_t mapMinListOnePoint = MinListOnePoint(g_hProcess, mapMinListLinePoint, x);

						int nMin = 0;
						ReadProcessMemory(g_hProcess, reinterpret_cast<LPCVOID>(mapMinListOnePoint + 0x1c), &nMin, sizeof(nMin), NULL);
						if (nMin)
						{
							printf("1");
						}
						else
						{
							printf("0");
						}

					}
					printf("\n");
				}
			}


		}
		Sleep(1000);
	}


	return 0;
}
int main() {

	const wchar_t* processName = L"Minesweeper.exe";
	const wchar_t* moduleName = L"GameAssembly.dll";


	DWORD processId = 0;

	HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	if (hProcessSnap != INVALID_HANDLE_VALUE) {
		PROCESSENTRY32 processEntry;
		processEntry.dwSize = sizeof(PROCESSENTRY32);

		if (Process32First(hProcessSnap, &processEntry)) {
			do {
				if (_wcsicmp(processEntry.szExeFile, processName) == 0) {
					processId = processEntry.th32ProcessID;
					break;
				}
			} while (Process32Next(hProcessSnap, &processEntry));
		}

		CloseHandle(hProcessSnap);
	}

	if (processId != 0) {
		g_baseAddress = GetModuleBaseAddress(processId, moduleName);

		if (g_baseAddress != 0) {
			std::wcout << L"The base address of " << moduleName << L" in " << processName << L" is 0x" << std::hex << g_baseAddress << std::endl;
			//std::wcout << L"success" << std::endl;
		}
		else {
			std::wcout << L"Module " << moduleName << L" not found in process " << processName << std::endl;
		}
	}
	else {
		std::wcout << L"Process " << processName << L" not found" << std::endl;
	}


	// 打开游戏进程
	g_hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
	if (g_hProcess == NULL) {
		std::cerr << "Failed to open the process. Error code: " << GetLastError() << std::endl;
		return 1;
	}

	//创建线程
	HANDLE  hThread = CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size  
		MyThreadFunction,       // thread function name
		0,          // argument to thread function 
		0,                      // use default creation flags 
		0);   // returns the thread identifier 

	WaitForSingleObject(hThread, INFINITE);
	CloseHandle(g_hProcess);

	return 0;
}
