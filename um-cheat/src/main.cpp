# include <iostream>
# include <windows.h>;
# include <tlhelp32.h>;


static DWORD get_process_id(const wchar_t* process_name) {
	DWORD process_id = 0;

	HANDLE snap_shot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	if (snap_shot == INVALID_HANDLE_VALUE) 
		return process_id;

		PROCESSENTRY32W process_entry = {};
		process_entry.dwSize = sizeof(decltype(process_entry));


		if (Process32FirstW(snap_shot, &process_entry) == TRUE) {

			if (_wcsicmp(process_name, process_entry.szExeFile) == 0)
				process_id = process_entry.th32ProcessID;
			else {
				while (Process32NextW(snap_shot, &process_entry) == TRUE) {
					if (_wcsicmp(process_name, process_entry.szExeFile) == 0) {
						process_id = process_entry.th32ProcessID;
						break;
					}
				}
			}

		}

		CloseHandle(snap_shot);

		return process_id;
	}


static std::uintptr_t get_module_base(const DWORD pid, const wchar_t* module_name) {
		std::uintptr_t module_base = 0;


		HANDLE snap_shot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
		if (snap_shot == INVALID_HANDLE_VALUE) {
			return module_base;
		}
		MODULEENTRY32W module_entry = {};
		module_entry.dwSize = sizeof(module_entry);
		if (Module32FirstW(snap_shot, &module_entry) == TRUE) {
			if (_wcsicmp(module_name, module_entry.szModule) != 0) {
				module_base = reinterpret_cast<std::uintptr_t >(module_entry.modBaseAddr);
			}
			else {
				while (Module32NextW(snap_shot, &module_entry) == TRUE) {
                    if (_wcsicmp(module_name, module_entry.szModule) != 0) {
						module_base = reinterpret_cast<std::uintptr_t >(module_entry.modBaseAddr);
						break;
					}
				}
			}
		}

		return module_base;


	}




namespace driver {
	namespace codes {

		// used to setup Driver
		// Control codes used to setup Driver
		constexpr ULONG attach = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x696, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);

		// Control code for read operation
		constexpr ULONG read = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x697, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);

		// Control code for write operation
		constexpr ULONG write = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x698, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
	}

	struct Request {
		HANDLE process_id;

		PVOID Target;

		PVOID buffer;

		SIZE_T size;
		SIZE_T return_size;

	};



	bool attach_to_process(HANDLE driver_handle, const DWORD pid)
	{
		Request request;
		request.process_id = reinterpret_cast<HANDLE>(pid);
		/*DWORD bytes_returned = 0;*/
		return DeviceIoControl(driver_handle, codes::attach, &request, sizeof(request), &request, sizeof(request) , nullptr, nullptr);
	}

	template <class T>
	T read_memory(HANDLE driver_handle, const std::uintptr_t addr) {
		T temp = {};

		Request request;
		request.Target = reinterpret_cast<PVOID>(addr);
		request.buffer = &temp;
		request.size = sizeof(T);

		DeviceIoControl(driver_handle, codes::read, &request, sizeof(request), &request, sizeof(request), nullptr, nullptr);

		return temp;
		
	}

	template <class T>
	void write_memory(HANDLE driver_handle, const std::uintptr_t addr, const T& value) {
		Request request;
		request.Target = reinterpret_cast<PVOID>(addr);
		request.buffer = (PVOID)&value;	
		request.size = sizeof(T);
		DeviceIoControl(driver_handle, codes::write, &request, sizeof(request), &request, sizeof(request), nullptr, nullptr);

	}


}







int main() {
	

	const DWORD pid = get_process_id(L"notepad.exe");

	if (pid == 0) {
		std::cout << "[-] Failed to get process id. \n";
		std::cin.get();
		return 1;
	}

	const HANDLE driver = CreateFile(L"\\\\.\\Cexy-Driver", GENERIC_READ , 0 , nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL , nullptr);


	if (driver == INVALID_HANDLE_VALUE) {
		std::cout << "[-] Failed to get driver handle. \n";
		std::cin.get();
		return 1;
	}


	if (!driver::attach_to_process(driver, pid) == true) 
		std::cout << "attach successfully. \n";
		

	CloseHandle(driver);

	std::cin.get();
	return 0;
}  