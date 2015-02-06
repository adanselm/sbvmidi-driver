
#include "adapter.h"

#define kUseDMusicMiniport 1

#define NUM_CHANNEL_REFERENCE_NAMES 8

PWSTR port_reference_names[8] = {
	L"SbVMidi1",
	L"SbVMidi2",
	L"SbVMidi3",
	L"SbVMidi4",
	L"SbVMidi5",
	L"SbVMidi6",
	L"SbVMidi7",
	L"SbVMidi8" 
};

PWSTR get_port_reference_name(ULONG i) {
	return port_reference_names[i-1];
}


#pragma warning (disable : 4100 4127)

extern "C"
NTSTATUS
DriverEntry
(
IN      PVOID   Context1,   // Context for the class driver.
IN      PVOID   Context2    // Context for the class driver.
)
{
	VLOG("DriverEntry");
	//    _DbgPrintF(DEBUGLVL_ERROR, ("Starting breakpoint for debugging"));

	//
	// Tell the class driver to initialize the driver.
	//
	return PcInitializeAdapterDriver((PDRIVER_OBJECT)Context1,
		(PUNICODE_STRING)Context2,
		(PDRIVER_ADD_DEVICE)AddDevice);
}

NTSTATUS AddDevice
(
IN PDRIVER_OBJECT   DriverObject,
IN PDEVICE_OBJECT   PhysicalDeviceObject
)
{
	//PAGED_CODE();
	VLOG("AddDevice");

	//
	// Tell the class driver to add the device.
	//
	//http://msdn.microsoft.com/en-us/library/windows/hardware/ff537683(v=vs.85).aspx
	//NUM_CHANNEL_REFERENCE_NAMES gives the maximun number of subdevices. Adjust above. 
	return PcAddAdapterDevice(DriverObject, PhysicalDeviceObject, StartDevice, NUM_CHANNEL_REFERENCE_NAMES, 0);
}

NTSTATUS
StartDevice
(
IN      PDEVICE_OBJECT  pDeviceObject,  // Context for the class driver.
IN      PIRP            pIrp,           // Context for the class driver.
IN      PRESOURCELIST   ResourceList    // List of hardware resources.
) {
	ASSERT(pDeviceObject);
	ASSERT(pIrp);
	ASSERT(ResourceList);
	VLOG("StartDevice");
	ULONG i;
	if (!ResourceList)
	{
		VLOG("StartDevice: NULL resource list");
		return STATUS_INVALID_PARAMETER;
	}

	NTSTATUS ntStatus = STATUS_INSUFFICIENT_RESOURCES;
	ULONG nresources = ResourceList->NumberOfEntries();
	VLOG("Available resources: %d.", nresources);

		/*ntStatus = InstallSubdevice(
			pDeviceObject,
			pIrp,
			L"Uart",
			CLSID_PortDMus,
			CLSID_MiniportDriverDMusUART,
			NULL,
			ResourceList,
			IID_IPortDMus,
			NULL,
			NULL    // Not physically connected to anything.
			);*/
	for (i = 1; i <= NUM_CHANNEL_REFERENCE_NAMES; i++) {
#if (kUseDMusicMiniport)
		ntStatus = InstallSubdeviceVirtual(
			pDeviceObject,
			pIrp,
			get_port_reference_name(i),
			CLSID_PortDMus,
			CLSID_MiniportDriverDMusUART,
			ResourceList
			);
#else
		ntStatus = InstallSubdeviceVirtual(
			pDeviceObject,
			pIrp,
			get_port_reference_name(i),
			IID_IPortMidi,
			CLSID_MiniportDriverUart,
			ResourceList
			);
#endif
	}
	return ntStatus;
}

/*
ntStatus = InstallSubdevice(
pDeviceObject,
pIrp,
L"Uart",
CLSID_PortDMus,
CLSID_MiniportDriverDMusUART,
NULL,
ResourceList,
IID_IPortDMus,
NULL,
NULL    // Not physically connected to anything.
);
*/

NTSTATUS InstallSubdeviceVirtual(
	_In_        PVOID               Context1,
	_In_        PVOID               Context2,
	_In_        PWSTR               Name,
	_In_        REFGUID             PortClassId, //DirectMusic or MIDI
	_In_        REFGUID             MiniportClassId, //DirectMusic or MIDI
	_In_        PRESOURCELIST       ResourceList
	) {
	VLOG("InstallSubdevice");
	ASSERT(Context1);
	ASSERT(Context2);
	ASSERT(Name);
	ASSERT(ResourceList);
	PPORT       port;
	VLOG("Creating port driver...");
	NTSTATUS    ntStatus = PcNewPort(&port, PortClassId); //Create a directmusic or midi port driver

	if (NT_SUCCESS(ntStatus)) {
		//Manually create minport driver
		PMINIPORT miniport;
		VLOG("Creating miniport driver...");
		ntStatus = CreateMiniportDMusUART((PUNKNOWN*)&miniport, MiniportClassId, NULL, NonPagedPool); //Temporal fix, we need a new one!
		if (NT_SUCCESS(ntStatus))
		{
			//
			// Init the port driver and miniport in one go.
			//
			ntStatus = port->Init((PDEVICE_OBJECT)Context1,
				(PIRP)Context2,
				miniport,
				NULL,   // interruptsync created in miniport.
				ResourceList);

			if (NT_SUCCESS(ntStatus))
			{
				VLOG("Miniport-init: success. Registering sub device...")
				//
				// Register the subdevice (port/miniport combination).
				//
				ntStatus = PcRegisterSubdevice((PDEVICE_OBJECT)Context1,
					Name,
					port);
				if (!(NT_SUCCESS(ntStatus)))
				{
						VLOG("StartDevice: PcRegisterSubdevice failed");
				}
				VLOG("PcRegisterSubdevice: success.");
			}
			else
			{
				VLOG("InstallSubdevice: port->Init failed");
			}

			//
			// We don't need the miniport any more.  Either the port has it,
			// or we've failed, and it should be deleted.
			//
			VLOG("Miniport successfully created!");
			miniport->Release();
		}
		//Release the port in any case if it was created
		port->Release();
	}
	else {
		VLOG("Port driver creation failed!");
	}
	return STATUS_SUCCESS;

}

NTSTATUS
InstallSubdevice
(
_In_        PVOID               Context1,
_In_        PVOID               Context2,
_In_        PWSTR               Name,
_In_        REFGUID             PortClassId, //DirectMusic or MIDI
_In_        REFGUID             MiniportClassId, //Our .lib I guess
_In_opt_    PUNKNOWN            UnknownAdapter,     //not used - null
_In_        PRESOURCELIST       ResourceList,       //not optional, but can be EMPTY!
_In_        REFGUID             PortInterfaceId,
_Out_opt_   PUNKNOWN *          OutPortInterface,   //not used - null
_Out_opt_   PUNKNOWN *          OutPortUnknown      //not used - null
)
{
	VLOG("InstallSubdevice");
	ASSERT(Context1);
	ASSERT(Context2);
	ASSERT(Name);
	ASSERT(ResourceList);

	//
	// Create the port driver object
	//
	PPORT       port;
	NTSTATUS    ntStatus = PcNewPort(&port, PortClassId); //Create a directmusic or midi port driver

	if (NT_SUCCESS(ntStatus))
	{
		//
		// Deposit the port somewhere if it's needed.
		//
		if (OutPortInterface)
		{
			//
			//  Failure here doesn't cause the entire routine to fail.
			//
			(void)port->QueryInterface
				(
				PortInterfaceId,
				(PVOID *)OutPortInterface
				);
		}

		PMINIPORT miniport;
		//
		// Create the miniport object
		//
		ntStatus = PcNewMiniport(&miniport, MiniportClassId);

		if (NT_SUCCESS(ntStatus))
		{
			//
			// Init the port driver and miniport in one go.
			//
			ntStatus = port->Init((PDEVICE_OBJECT)Context1,
				(PIRP)Context2,
				miniport,
				NULL,   // interruptsync created in miniport.
				ResourceList);

			if (NT_SUCCESS(ntStatus))
			{
				//
				// Register the subdevice (port/miniport combination).
				//
				ntStatus = PcRegisterSubdevice((PDEVICE_OBJECT)Context1,
					Name,
					port);
				if (!(NT_SUCCESS(ntStatus)))
				{
					VLOG("StartDevice: PcRegisterSubdevice failed");
				}
			}
			else
			{
				VLOG("InstallSubdevice: port->Init failed");
			}

			//
			// We don't need the miniport any more.  Either the port has it,
			// or we've failed, and it should be deleted.
			//
			miniport->Release();
		}
		else
		{
			VLOG("InstallSubdevice: PcNewMiniport failed");
		}

		if (NT_SUCCESS(ntStatus))
		{
			//
			// Deposit the port as an unknown if it's needed.
			//
			if (OutPortUnknown)
			{
				//
				//  Failure here doesn't cause the entire routine to fail.
				//
				/*(void)port->QueryInterface
					(
					IID_IUnknown,
					(PVOID *)OutPortUnknown
					);*/
			}
		}
		else
		{
			//
			// Retract previously delivered port interface.
			//
			if (OutPortInterface && (*OutPortInterface))
			{
				(*OutPortInterface)->Release();
				*OutPortInterface = NULL;
			}
		}

		//
		// Release the reference which existed when PcNewPort() gave us the
		// pointer in the first place.  This is the right thing to do
		// regardless of the outcome.
		//
		port->Release();
	}
	else
	{
		VLOG("InstallSubdevice: PcNewPort failed");
	}

	return ntStatus;
}
