# include <ntifs.h>


// #include <ntddk.h>

extern "C" { 
	
	//undocumented windows internal functions (exported by ntoskrnl)

	NTKERNELAPI NTSTATUS IoCreateDriver(PUNICODE_STRING DriverName, PDRIVER_INITIALIZE InitializationFunction);
	NTKERNELAPI NTSTATUS MmCopyVirtualMemory(PEPROCESS SourceProcess, PVOID SourceAddress, PEPROCESS TargetProcess, PVOID TargetAddress, SIZE_T BufferSize, KPROCESSOR_MODE PreviousMode, PSIZE_T ReturnSize);
}






void debug_print(PCSTR text) {
#ifndef DEBUG
	UNREFERENCED_PARAMETER(text);
#endif // DEBUG
	KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, text));

}

namespace driver {
	namespace codes {

		// used to setup Driver
        // Control codes used to setup Driver
        constexpr ULONG attach = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x696 , METHOD_BUFFERED , FILE_SPECIAL_ACCESS);

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

	NTSTATUS create(PDEVICE_OBJECT device_object, PIRP irp) { // irp = I/O request packet 
		UNREFERENCED_PARAMETER(device_object);
		IoCompleteRequest(irp, IO_NO_INCREMENT);	
		return irp->IoStatus.Status;
	}


	NTSTATUS close(PDEVICE_OBJECT device_object, PIRP irp) { // irp = I/O request packet 
		UNREFERENCED_PARAMETER(device_object);
		IoCompleteRequest(irp, IO_NO_INCREMENT);
		return irp->IoStatus.Status;

	}



	// note : todo
	NTSTATUS device_control(PDEVICE_OBJECT device_object, PIRP irp) { // irp = I/O request packet 
		UNREFERENCED_PARAMETER(device_object);
		debug_print("[+] Device control called. \n");

		IoCompleteRequest(irp, IO_NO_INCREMENT);

		NTSTATUS status = STATUS_UNSUCCESSFUL;	

		// we need this to determine the type of code pased to the driver

		PIO_STACK_LOCATION stack_irp = IoGetCurrentIrpStackLocation(irp);	

		// Access the request object sent by the user

		auto request = reinterpret_cast<Request*>(irp->AssociatedIrp.SystemBuffer);	

		if (stack_irp == nullptr || request == nullptr) {
			IoCompleteRequest(irp, IO_NO_INCREMENT);
			return status;
		}

		// get the process id of the target process
		static PEPROCESS target_process = nullptr;
		
		const ULONG control_code = stack_irp->Parameters.DeviceIoControl.IoControlCode;	
		switch (control_code) {
		case codes::attach:
			// get the process id of the target process
			status = PsLookupProcessByProcessId(request->process_id, &target_process);	
			break;
		case codes::read:
			if (target_process != nullptr)
				status = MmCopyVirtualMemory(target_process, request->Target, PsGetCurrentProcess(), request->buffer, request->size, KernelMode, &request->return_size);	 
			
			break;

		case codes::write:
			if (target_process != nullptr)
				status = MmCopyVirtualMemory(PsGetCurrentProcess(), request->buffer, target_process, request->Target, request->size, KernelMode, &request->return_size);	
			break;
		}


		irp->IoStatus.Status = status;	
		irp->IoStatus.Information = sizeof(Request);


		IoCompleteRequest(irp, IO_NO_INCREMENT);
		return status;

	}


}





NTSTATUS driver_main(PDRIVER_OBJECT driver_object, PUNICODE_STRING registry_path) {
	UNREFERENCED_PARAMETER(registry_path);

	UNICODE_STRING device_name = {};
	RtlInitUnicodeString(&device_name, L"\\Device\\Cexy-Driver");


	PDEVICE_OBJECT device_object = nullptr;
	NTSTATUS status = IoCreateDevice(driver_object, 0, &device_name, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &device_object);	

	if (!NT_SUCCESS(status)) {
		debug_print("[-] Failed to create device object. \n");
		return status;
	}

	debug_print("[+] Device object created successfully. \n ");



	UNICODE_STRING sym_link = {};
	RtlInitUnicodeString(&sym_link, L"\\DosDevices\\Cexy-Driver");

	status = IoCreateSymbolicLink(&sym_link, &device_name);
	if (!NT_SUCCESS(status)) {
		debug_print("[-] Failed to create symbolic link. \n");
		//IoDeleteDevice(device_object);
		return status;
	}

	debug_print("[+] Symbolic link created successfully. \n");




	// setup driver


	// Allow us to send small amount of data between user and kernel space

	SetFlag(device_object->Flags, DO_BUFFERED_IO);

	// set the driver handler to our functions with our logic


	driver_object->MajorFunction[IRP_MJ_CREATE] = driver::create;
	driver_object->MajorFunction[IRP_MJ_CLOSE] = driver::close;
	driver_object->MajorFunction[IRP_MJ_DEVICE_CONTROL] = driver::device_control;


	ClearFlag(device_object->Flags, DO_DEVICE_INITIALIZING);

	// we have initialized device 


	debug_print("[+] Driver initialized successfully. \n");

	return status;

}






// DriverEntry is the entry point for the driver
NTSTATUS DriverEntry() {
	debug_print("[+] hi from kernal\n");
	

	UNICODE_STRING driver_name = {};
	RtlInitUnicodeString(&driver_name, L"\\Device\\Cexy-Driver");
	return IoCreateDriver(&driver_name , &driver_main);
}


