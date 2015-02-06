/*****************************************************************************
 * miniport.cpp - UART miniport implementation
 *****************************************************************************
 * Copyright (c) 1997-2000 Microsoft Corporation.  All Rights Reserved.
 *
 */


#include "private.h"
#include "ksdebug.h"
#include "stdio.h"

static CMiniportDMusUARTStream *inpuStream;
static CMiniportDMusUARTStream *outputStream;


#pragma warning (disable : 4100 4127)


#define STR_MODULENAME "DMusUART:Miniport: "
 
#pragma code_seg("PAGE")

/*****************************************************************************
 * PinDataRangesStreamLegacy
 * PinDataRangesStreamDMusic
 *****************************************************************************
 * Structures indicating range of valid format values for live pins.
 */
static
KSDATARANGE_MUSIC PinDataRangesStreamLegacy =
{
    {
        sizeof(KSDATARANGE_MUSIC),
        0,
        0,
        0,
        STATICGUIDOF(KSDATAFORMAT_TYPE_MUSIC),
        STATICGUIDOF(KSDATAFORMAT_SUBTYPE_MIDI),
        STATICGUIDOF(KSDATAFORMAT_SPECIFIER_NONE)
    },
    STATICGUIDOF(KSMUSIC_TECHNOLOGY_PORT),
    0,
    0,
    0xFFFF
};
static
KSDATARANGE_MUSIC PinDataRangesStreamDMusic =
{
    {
        sizeof(KSDATARANGE_MUSIC),
        0,
        0,
        0,
        STATICGUIDOF(KSDATAFORMAT_TYPE_MUSIC),
        STATICGUIDOF(KSDATAFORMAT_SUBTYPE_DIRECTMUSIC),
        STATICGUIDOF(KSDATAFORMAT_SPECIFIER_NONE)
    },
    STATICGUIDOF(KSMUSIC_TECHNOLOGY_PORT),
    0,
    0,
    0xFFFF
};

/*****************************************************************************
 * PinDataRangePointersStreamLegacy
 * PinDataRangePointersStreamDMusic
 * PinDataRangePointersStreamCombined
 *****************************************************************************
 * List of pointers to structures indicating range of valid format values
 * for live pins.
 */
static
PKSDATARANGE PinDataRangePointersStreamLegacy[] =
{
    PKSDATARANGE(&PinDataRangesStreamLegacy)
};
static
PKSDATARANGE PinDataRangePointersStreamDMusic[] =
{
    PKSDATARANGE(&PinDataRangesStreamDMusic)
};
static
PKSDATARANGE PinDataRangePointersStreamCombined[] =
{
    PKSDATARANGE(&PinDataRangesStreamLegacy)
   ,PKSDATARANGE(&PinDataRangesStreamDMusic)
};

/*****************************************************************************
 * PinDataRangesBridge
 *****************************************************************************
 * Structures indicating range of valid format values for bridge pins.
 */
static
KSDATARANGE PinDataRangesBridge[] =
{
   {
      sizeof(KSDATARANGE),
      0,
      0,
      0,
      STATICGUIDOF(KSDATAFORMAT_TYPE_MUSIC),
      STATICGUIDOF(KSDATAFORMAT_SUBTYPE_MIDI_BUS),
      STATICGUIDOF(KSDATAFORMAT_SPECIFIER_NONE)
   }
};

/*****************************************************************************
 * PinDataRangePointersBridge
 *****************************************************************************
 * List of pointers to structures indicating range of valid format values
 * for bridge pins.
 */
static
PKSDATARANGE PinDataRangePointersBridge[] =
{
    &PinDataRangesBridge[0]
};

/*****************************************************************************
 * SynthProperties
 *****************************************************************************
 * List of properties in the Synth set.
 */
static
PCPROPERTY_ITEM
SynthProperties[] =
{
    // Global: S/Get synthesizer caps
    {
        &KSPROPSETID_Synth,
        KSPROPERTY_SYNTH_CAPS,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_Synth
    },
    // Global: S/Get port parameters
    {
        &KSPROPSETID_Synth,
        KSPROPERTY_SYNTH_PORTPARAMETERS,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_Synth
    },
    // Per stream: S/Get channel groups
    {
        &KSPROPSETID_Synth,
        KSPROPERTY_SYNTH_CHANNELGROUPS,
        KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_Synth
    },
    // Per stream: Get current latency time
    {
        &KSPROPSETID_Synth,
        KSPROPERTY_SYNTH_LATENCYCLOCK,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_Synth
    }
};
DEFINE_PCAUTOMATION_TABLE_PROP(AutomationSynth,  SynthProperties);
DEFINE_PCAUTOMATION_TABLE_PROP(AutomationSynth2, SynthProperties);

#define kMaxNumCaptureStreams       1
#define kMaxNumLegacyRenderStreams  1
#define kMaxNumDMusicRenderStreams  1

/*****************************************************************************
 * MiniportPins
 *****************************************************************************
 * List of pins.
 */
static
PCPIN_DESCRIPTOR MiniportPins[] =
{
    {
        kMaxNumLegacyRenderStreams,kMaxNumLegacyRenderStreams,0,    // InstanceCount
        NULL,                                                       // AutomationTable
        {                                                           // KsPinDescriptor
            0,                                              // InterfacesCount
            NULL,                                           // Interfaces
            0,                                              // MediumsCount
            NULL,                                           // Mediums
            SIZEOF_ARRAY(PinDataRangePointersStreamLegacy), // DataRangesCount
            PinDataRangePointersStreamLegacy,               // DataRanges
            KSPIN_DATAFLOW_IN,                              // DataFlow
            KSPIN_COMMUNICATION_SINK,                       // Communication
            (GUID *) &KSCATEGORY_AUDIO,                     // Category
            &KSAUDFNAME_MIDI,                               // Name
            0                                               // Reserved
        }
    },
    {
        kMaxNumDMusicRenderStreams,kMaxNumDMusicRenderStreams,0,    // InstanceCount
        NULL,                                                       // AutomationTable
        {                                                           // KsPinDescriptor
            0,                                              // InterfacesCount
            NULL,                                           // Interfaces
            0,                                              // MediumsCount
            NULL,                                           // Mediums
            SIZEOF_ARRAY(PinDataRangePointersStreamDMusic), // DataRangesCount
            PinDataRangePointersStreamDMusic,               // DataRanges
            KSPIN_DATAFLOW_IN,                              // DataFlow
            KSPIN_COMMUNICATION_SINK,                       // Communication
            (GUID *) &KSCATEGORY_AUDIO,                     // Category
            &KSAUDFNAME_DMUSIC_MPU_OUT,                     // Name
            0                                               // Reserved
        }
    },
    {
        0,0,0,                                      // InstanceCount
        NULL,                                       // AutomationTable
        {                                           // KsPinDescriptor
            0,                                          // InterfacesCount
            NULL,                                       // Interfaces
            0,                                          // MediumsCount
            NULL,                                       // Mediums
            SIZEOF_ARRAY(PinDataRangePointersBridge),   // DataRangesCount
            PinDataRangePointersBridge,                 // DataRanges
            KSPIN_DATAFLOW_OUT,                         // DataFlow
            KSPIN_COMMUNICATION_NONE,                   // Communication
            (GUID *) &KSCATEGORY_AUDIO,                 // Category
            NULL,                                       // Name
            0                                           // Reserved
        }
    },
    {
        0,0,0,                                      // InstanceCount
        NULL,                                       // AutomationTable
        {                                           // KsPinDescriptor
            0,                                          // InterfacesCount
            NULL,                                       // Interfaces
            0,                                          // MediumsCount
            NULL,                                       // Mediums
            SIZEOF_ARRAY(PinDataRangePointersBridge),   // DataRangesCount
            PinDataRangePointersBridge,                 // DataRanges
            KSPIN_DATAFLOW_IN,                          // DataFlow
            KSPIN_COMMUNICATION_NONE,                   // Communication
            (GUID *) &KSCATEGORY_AUDIO,                 // Category
            NULL,                                       // Name
            0                                           // Reserved
        }
    },
    {
        kMaxNumCaptureStreams,kMaxNumCaptureStreams,0,      // InstanceCount
        NULL,                                               // AutomationTable
        {                                                   // KsPinDescriptor
            0,                                                // InterfacesCount
            NULL,                                             // Interfaces
            0,                                                // MediumsCount
            NULL,                                             // Mediums
            SIZEOF_ARRAY(PinDataRangePointersStreamCombined), // DataRangesCount
            PinDataRangePointersStreamCombined,               // DataRanges
            KSPIN_DATAFLOW_OUT,                               // DataFlow
            KSPIN_COMMUNICATION_SINK,                         // Communication
            (GUID *) &KSCATEGORY_AUDIO,                       // Category
            &KSAUDFNAME_DMUSIC_MPU_IN,                        // Name
            0                                                 // Reserved
        }
    }
};

/*****************************************************************************
 * MiniportNodes
 *****************************************************************************
 * List of nodes.
 */
#define CONST_PCNODE_DESCRIPTOR(n)          { 0, NULL, &n, NULL }
#define CONST_PCNODE_DESCRIPTOR_AUTO(n,a)   { 0, &a, &n, NULL }
static
PCNODE_DESCRIPTOR MiniportNodes[] =
{
      CONST_PCNODE_DESCRIPTOR_AUTO(KSNODETYPE_SYNTHESIZER, AutomationSynth)
    , CONST_PCNODE_DESCRIPTOR_AUTO(KSNODETYPE_SYNTHESIZER, AutomationSynth2)
};

/*****************************************************************************
 * MiniportConnections
 *****************************************************************************
 * List of connections.
 */
enum {
      eSynthNode  = 0
    , eInputNode
};

enum {
      eFilterInputPinLeg = 0,
      eFilterInputPinDM,
      eBridgeOutputPin,
      eBridgeInputPin,
      eFilterOutputPin
};

static
PCCONNECTION_DESCRIPTOR MiniportConnections[] =
{  // From                                   To
   // Node           pin                     Node           pin
    { PCFILTER_NODE, eFilterInputPinLeg,     PCFILTER_NODE, eBridgeOutputPin }      // Legacy Stream in to synth.
  , { PCFILTER_NODE, eFilterInputPinDM,      eSynthNode,    KSNODEPIN_STANDARD_IN } // DM Stream in to synth.
  , { eSynthNode,    KSNODEPIN_STANDARD_OUT, PCFILTER_NODE, eBridgeOutputPin }      // Synth to bridge out.
  , { PCFILTER_NODE, eBridgeInputPin,        eInputNode,    KSNODEPIN_STANDARD_IN } // Bridge in to input.
  , { eInputNode,    KSNODEPIN_STANDARD_OUT, PCFILTER_NODE, eFilterOutputPin }      // Input to DM/Legacy Stream out.
};

/*****************************************************************************
 * MiniportCategories
 *****************************************************************************
 * List of categories.
 */
static
GUID MiniportCategories[] =
{
    STATICGUIDOF(KSCATEGORY_AUDIO),
    STATICGUIDOF(KSCATEGORY_RENDER),
    STATICGUIDOF(KSCATEGORY_CAPTURE)
};

/*****************************************************************************
 * MiniportFilterDescriptor
 *****************************************************************************
 * Complete miniport filter description.
 */
static
PCFILTER_DESCRIPTOR MiniportFilterDescriptor =
{
    0,                                  // Version
    NULL,                               // AutomationTable
    sizeof(PCPIN_DESCRIPTOR),           // PinSize
    SIZEOF_ARRAY(MiniportPins),         // PinCount
    MiniportPins,                       // Pins
    sizeof(PCNODE_DESCRIPTOR),          // NodeSize
    SIZEOF_ARRAY(MiniportNodes),        // NodeCount
    MiniportNodes,                      // Nodes
    SIZEOF_ARRAY(MiniportConnections),  // ConnectionCount
    MiniportConnections,                // Connections
    SIZEOF_ARRAY(MiniportCategories),   // CategoryCount
    MiniportCategories                  // Categories
};

#pragma code_seg("PAGE")
/*****************************************************************************
 * CMiniportDMusUART::GetDescription()
 *****************************************************************************
 * Gets the topology.
 */
STDMETHODIMP_(NTSTATUS)
CMiniportDMusUART::
GetDescription
(
    OUT     PPCFILTER_DESCRIPTOR *  OutFilterDescriptor
)
{
    PAGED_CODE();

    ASSERT(OutFilterDescriptor);

    MLOG("GetDescription");

    *OutFilterDescriptor = &MiniportFilterDescriptor;

    return STATUS_SUCCESS;
}

#pragma code_seg("PAGE")
/*****************************************************************************
 * CreateMiniportDMusUART()
 *****************************************************************************
 * Creates a MPU-401 miniport driver for the adapter.  This uses a
 * macro from STDUNK.H to do all the work.
 */

 

NTSTATUS
CreateMiniportDMusUART
(
    OUT     PUNKNOWN *  Unknown,
    IN      REFCLSID,
    IN      PUNKNOWN    UnknownOuter    OPTIONAL,
    IN      POOL_TYPE   PoolType
)
{
    PAGED_CODE();

    MLOG("CreateMiniportDMusUART");
    ASSERT(Unknown);

    STD_CREATE_BODY_(   CMiniportDMusUART,
                        Unknown,
                        UnknownOuter,
                        PoolType,
                        PMINIPORTDMUS);
}
VOID CMiniportDMusUART::StreamDestroying(IN PVOID source, IN BOOL capture) {
	MLOG("Destroying stream...");
	if (capture) {
		MLOG("Destroying input stream...");
		m_inputStream = NULL;
	}
	else {
		MLOG("Destroying output stream...");
		m_outputStream = NULL;
	}
}
VOID CMiniportDMusUART::ForwardOutputFromSource(IN PVOID source, IN PVOID BufferAddress, IN ULONG Length) {
	MLOG("Forwarding event to capture stream, if available")
	if (source == m_outputStream) {
		MLOG("Source is correct: output stream");
	}
	if (m_inputStream != NULL) {
		MLOG("Input stream is not null, forwarding event...");
		CMiniportDMusUARTStream *delegate=static_cast<CMiniportDMusUARTStream *>(m_inputStream);
		delegate->ForwardEventsToPort(BufferAddress, Length);
	}
}

#pragma code_seg("PAGE")
/*****************************************************************************
 * CMiniportDMusUART::ProcessResources()
 *****************************************************************************
 * Processes the resource list, setting up helper objects accordingly.
 */
NTSTATUS
CMiniportDMusUART::
ProcessResources
(
    IN      PRESOURCELIST   ResourceList
)
{
	PAGED_CODE();
	UNREFERENCED_PARAMETER(ResourceList);
	MLOG("ProcessResources");
	//Here we are supposed to call: initmpu, resethardware
	//We do not use any physical resouces
	return STATUS_SUCCESS;
}

#pragma code_seg("PAGE")
/*****************************************************************************
 * CMiniportDMusUART::NonDelegatingQueryInterface()
 *********************************p********************************************
 * Obtains an interface.  This function works just like a COM QueryInterface
 * call and is used if the object is not being aggregated.
 */
NTSTATUS CMiniportDMusUARTStream::PutMessage(_In_   PDMUS_KERNEL_EVENT pDMKEvt)
{
	ULONG bytesRemaining = 0;
	ULONG bytesWritten = 0;
	NTSTATUS ntStatus;
	MLOG("PutMessage with kernel event %p",pDMKEvt);
	if (pDMKEvt == NULL) {
		MLOG("Event is null, do nothing");
		return STATUS_SUCCESS;
	}
	//Basically just write and free the allocator
	bytesRemaining = pDMKEvt->cbEvent;
	if (bytesRemaining > sizeof(PBYTE)) {
		MLOG("Message is bigger than size of PBYTE, using pbData for writing...")
		ntStatus = Write(pDMKEvt->uData.pbData, bytesRemaining, &bytesWritten);
	}
	else {
		ntStatus = Write(pDMKEvt->uData.abData, bytesRemaining, &bytesWritten);
	}
	pDMKEvt->uData.pPackageEvt = NULL;
	pDMKEvt->cbEvent = 0;
	m_AllocatorMXF->PutMessage(pDMKEvt);    //  throw back in free pool
	return STATUS_SUCCESS;
}

#pragma code_seg("PAGE")
STDMETHODIMP_(void)
CMiniportDMusUART::
PowerChangeNotify
(
_In_    POWER_STATE             PowerState
)
{
	PAGED_CODE();

	MLOG("CMiniportDMusUART::PoweChangeNotify D%d", PowerState.DeviceState);
} 

STDMETHODIMP_(NTSTATUS)
CMiniportDMusUART::
NonDelegatingQueryInterface
(
    _In_           REFIID  Interface,
    _COM_Outptr_   PVOID * Object
)
{
    PAGED_CODE();
	/*
	typedef struct _GUID {
  DWORD Data1;
  WORD  Data2;
  WORD  Data3;
  BYTE  Data4[8];
} GUID;
*/

    MLOG("Miniport::NonDelegatingQueryInterface");
    ASSERT(Object);

	GUID input = (GUID)Interface;
	MLOG("Requested GUI: %x-%x-%x", input.Data1, input.Data2, input.Data3);

    if (IsEqualGUIDAligned(Interface,IID_IUnknown))
    {
		MLOG("Requested IID_IUnknown interface...");
        *Object = PVOID(PUNKNOWN(PMINIPORTDMUS(this)));
    }
    else
    if (IsEqualGUIDAligned(Interface,IID_IMiniport))
    {
		MLOG("Requested IID_IMiniport interface...");
        *Object = PVOID(PMINIPORT(this));
    }
    else
    if (IsEqualGUIDAligned(Interface,IID_IMiniportDMus))
    {
		MLOG("Requested IID_IMiniportDMus interface...");

        *Object = PVOID(PMINIPORTDMUS(this));
    }
    else
    if (IsEqualGUIDAligned(Interface,IID_IMusicTechnology))
    {
		MLOG("Requested IID_IMusicTechnology interface...");

        *Object = PVOID(PMUSICTECHNOLOGY(this));
    }
    else
	if (IsEqualGUIDAligned(Interface, IID_IPowerNotify))
    {
		MLOG("Requested IID_IPowerNotify interface...");

        *Object = PVOID(PPOWERNOTIFY(this));
    }
    else
    {
		MLOG("Unknown interface, null...");

        *Object = NULL;
    }

    if (*Object)
    {
        //
        // We reference the interface for the caller.
        //
		MLOG("Success in nondelegatingqueryinterface")
        PUNKNOWN(*Object)->AddRef();
        return STATUS_SUCCESS;
    }

    return STATUS_INVALID_PARAMETER;
}

#pragma code_seg("PAGE")
/*****************************************************************************
 * CMiniportDMusUART::~CMiniportDMusUART()
 *****************************************************************************
 * Destructor.
 */
CMiniportDMusUART::~CMiniportDMusUART(void)
{
    PAGED_CODE();

    _DbgPrintF(DEBUGLVL_BLAB,("~CMiniportDMusUART"));

    ASSERT(0 == m_NumCaptureStreams);
    ASSERT(0 == m_NumRenderStreams);

    //  reset the HW so we don't get anymore interrupts
  
    if (m_pServiceGroup)
    {
        m_pServiceGroup->Release();
        m_pServiceGroup = NULL;
    }
    if (m_pPort)
    {
        m_pPort->Release();
        m_pPort = NULL;
    }
}

#pragma code_seg("PAGE")
/*****************************************************************************
 * CMiniportDMusUART::Init()
 ********************************************************ii*********************
 * Initializes a the miniport.
 */
STDMETHODIMP_(NTSTATUS)
CMiniportDMusUART::
Init
(
    IN      PUNKNOWN        UnknownInterruptSync    OPTIONAL,
    IN      PRESOURCELIST   ResourceList,
    IN      PPORTDMUS       Port_,
    OUT     PSERVICEGROUP * ServiceGroup
)
{
    PAGED_CODE();
	MLOG("Initializing the miniport...");
    ASSERT(ResourceList);
    if (!ResourceList)
    {
		MLOG("ResourceList is null, configuration error...");
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    ASSERT(Port_);
    ASSERT(ServiceGroup);

    *ServiceGroup = NULL;
    m_pPortBase = 0;

    // This will remain unspecified if the miniport does not get any power
    // messages.
    //
	//Possible problem here.
    m_PowerState.DeviceState = PowerDeviceUnspecified;

    //
    // AddRef() is required because we are keeping this pointer.
    //
    m_pPort = Port_;
    m_pPort->AddRef();

    // Set dataformat.
    //
    if (IsEqualGUIDAligned(m_MusicFormatTechnology, GUID_NULL))
    {
		MLOG("MusicFormatTechnology is null, using technology_port");
        RtlCopyMemory(  &m_MusicFormatTechnology,
                        &KSMUSIC_TECHNOLOGY_PORT,
                        sizeof(GUID));
    }
    RtlCopyMemory(  &PinDataRangesStreamLegacy.Technology,
                    &m_MusicFormatTechnology,
                    sizeof(GUID));
    RtlCopyMemory(  &PinDataRangesStreamDMusic.Technology,
                    &m_MusicFormatTechnology,
                    sizeof(GUID));
    m_InputTimeStamp = 0;
    m_KSStateInput = KSSTATE_STOP;

    NTSTATUS ntStatus = STATUS_SUCCESS;

    m_NumRenderStreams = 0;
    m_NumCaptureStreams = 0;

	MLOG("New service group...");
    ntStatus = PcNewServiceGroup(&m_pServiceGroup,NULL);
    if (NT_SUCCESS(ntStatus) && !m_pServiceGroup)   //  keep any error
    {
		MLOG("Could not allocate service group...");
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }

    if (NT_SUCCESS(ntStatus))
    {
        *ServiceGroup = m_pServiceGroup;
        m_pServiceGroup->AddRef();

        //
        // Register the service group with the port early so the port is
        // prepared to handle interrupts.
        //
		MLOG("Registering service group (necessary)?");
        m_pPort->RegisterServiceGroup(m_pServiceGroup);
    }

    if (NT_SUCCESS(ntStatus))
    {
		MLOG("Process resources");
        ntStatus = ProcessResources(ResourceList);
    }
	MLOG("Ending init function...");
    if (!NT_SUCCESS(ntStatus))
    {
		MLOG("Ending init function with error.");
        //
        // clean up our mess
        //

        // clean up the service group
        if( m_pServiceGroup )
        {
            m_pServiceGroup->Release();
            m_pServiceGroup = NULL;
        }

        // clean up the out param service group.
        if (*ServiceGroup)
        {
            (*ServiceGroup)->Release();
            (*ServiceGroup) = NULL;
        }

        // release the port
        m_pPort->Release();
        m_pPort = NULL;
    }

    return ntStatus;
}

#pragma code_seg("PAGE")
/*****************************************************************************
 * CMiniportDMusUART::NewStream()
 *****************************************************************************
 * Gets the topology.
 */
STDMETHODIMP_(NTSTATUS)
CMiniportDMusUART::
NewStream
(
    OUT     PMXF                  * MXF,
    IN      PUNKNOWN                OuterUnknown    OPTIONAL,
    IN      POOL_TYPE               PoolType,
    IN      ULONG                   PinID,
    IN      DMUS_STREAM_TYPE        StreamType,
    IN      PKSDATAFORMAT           DataFormat,
    OUT     PSERVICEGROUP         * ServiceGroup,
    IN      PAllocatorMXF           AllocatorMXF,
    IN      PMASTERCLOCK            MasterClock,
    OUT     PULONGLONG              SchedulePreFetch
)
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(DataFormat);
    UNREFERENCED_PARAMETER(PinID);

	MLOG("NewStream...");
	NTSTATUS ntStatus = STATUS_SUCCESS;

    // In 100 ns, we want stuff as soon as it comes in
    //
    *SchedulePreFetch = 0;
    if  (   ((m_NumCaptureStreams < kMaxNumCaptureStreams)
            && (StreamType == DMUS_STREAM_MIDI_CAPTURE))
        ||  ((m_NumRenderStreams < kMaxNumLegacyRenderStreams + kMaxNumDMusicRenderStreams)
            && (StreamType == DMUS_STREAM_MIDI_RENDER))
        )
    {
		MLOG("Creating new stream...");
        CMiniportDMusUARTStream *pStream =
            new(PoolType) CMiniportDMusUARTStream(OuterUnknown);

        if (pStream)
        {
			MLOG("Stream allocated., initializing");
            pStream->AddRef();

            ntStatus =
                pStream->Init(this,m_pPortBase,(StreamType == DMUS_STREAM_MIDI_CAPTURE),AllocatorMXF,MasterClock);

            if (NT_SUCCESS(ntStatus))
            {
				MLOG("Stream correctly initialized.");
                *MXF = PMXF(pStream);
                (*MXF)->AddRef();

                if (StreamType == DMUS_STREAM_MIDI_CAPTURE)
                {
					m_inputStream = pStream;
					MLOG("Stream type is DMUS capture, adding service group.");
                    m_NumCaptureStreams++;
                    *ServiceGroup = m_pServiceGroup;
                    (*ServiceGroup)->AddRef();
                }
                else
                {
					m_outputStream = pStream;
                    m_NumRenderStreams++;
                    *ServiceGroup = NULL;
                }
            }

            pStream->Release();
        }
        else
        {
			MLOG("Insufficient resources...");
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }
    else
    {
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        if (StreamType == DMUS_STREAM_MIDI_CAPTURE)
        {
           MLOG("NewStream failed, too many capture streams");
        }
        else if (StreamType == DMUS_STREAM_MIDI_RENDER)
        {
			MLOG("NewStream failed, too many render streams");
        }
        else
        {
			MLOG("NewStream invalid stream type");
        }
    }

    return ntStatus;
}

#pragma code_seg("PAGE")
/*****************************************************************************
 * CMiniportDMusUART::SetTechnology()
 *****************************************************************************
 * Sets pindatarange technology.
 */
STDMETHODIMP_(NTSTATUS)
CMiniportDMusUART::
SetTechnology
(
    _In_    const GUID *            Technology
)
{
    PAGED_CODE();

	MLOG("SetTechnology");
    NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;

    // Fail if miniport has already been initialized.
    //
    if (NULL == m_pPort)
    {
        RtlCopyMemory(&m_MusicFormatTechnology, Technology, sizeof(GUID));
        ntStatus = STATUS_SUCCESS;
    }

    return ntStatus;
} // SetTechnology

#pragma code_seg("PAGE")
/*****************************************************************************
 * CMiniportDMusUARTStream::NonDelegatingQueryInterface()
 *****************************************************************************
 * Obtains an interface.  This function works just like a COM QueryInterface
 * call and is used if the object is not being aggregated.
 */
STDMETHODIMP_(NTSTATUS)
CMiniportDMusUARTStream::
NonDelegatingQueryInterface
(
    _In_           REFIID  Interface,
    _COM_Outptr_   PVOID * Object
)
{
    PAGED_CODE();

    MLOG("Stream::NonDelegatingQueryInterface");
    ASSERT(Object);

    if (IsEqualGUIDAligned(Interface,IID_IUnknown))
    {
        *Object = PVOID(PUNKNOWN(this));
    }
    else
    if (IsEqualGUIDAligned(Interface,IID_IMXF))
    {
        *Object = PVOID(PMXF(this));
    }
    else
    {
        *Object = NULL;
    }

    if (*Object)
    {
        //
        // We reference the interface for the caller.
        //
        PUNKNOWN(*Object)->AddRef();
        return STATUS_SUCCESS;
    }

    return STATUS_INVALID_PARAMETER;
}

#pragma code_seg("PAGE")
/*****************************************************************************
 * CMiniportDMusUARTStream::~CMiniportDMusUARTStream()
 *****************************************************************************
 * Destructs a stream.
 */
CMiniportDMusUARTStream::~CMiniportDMusUARTStream(void)
{
    PAGED_CODE();

    _DbgPrintF(DEBUGLVL_BLAB,("~CMiniportDMusUARTStream"));

    if (m_AllocatorMXF)
    {
        m_AllocatorMXF->Release();
        m_AllocatorMXF = NULL;
    }

    if (m_pMiniport)
    {
        if (m_fCapture)
        {
			m_pMiniport->StreamDestroying(this, TRUE);
            m_pMiniport->m_NumCaptureStreams--;
        }
        else
        {
			m_pMiniport->StreamDestroying(this, FALSE);
            m_pMiniport->m_NumRenderStreams--;
        }

        m_pMiniport->Release();
    }
}

#pragma code_seg("PAGE")
/*****************************************************************************
 * CMiniportDMusUARTStream::Init()
 *****************************************************************************
 * Initializes a stream.
 */
STDMETHODIMP_(NTSTATUS)
CMiniportDMusUARTStream::
Init
(
    IN      CMiniportDMusUART * pMiniport,
    IN      PUCHAR              pPortBase,
    IN      BOOLEAN             fCapture,
    IN      PAllocatorMXF       allocatorMXF,
    IN      PMASTERCLOCK        masterClock
)
{
    PAGED_CODE();

    ASSERT(pMiniport);
   // ASSERT(pPortBase);

    _DbgPrintF(DEBUGLVL_BLAB,("Init"));

    m_pMiniport = pMiniport;
    m_pMiniport->AddRef();

    pMiniport->m_MasterClock = masterClock;

    m_pPortBase = pPortBase;
    m_fCapture = fCapture;

    m_SnapshotTimeStamp = 0;

    if (allocatorMXF)
    {
        allocatorMXF->AddRef();
        m_AllocatorMXF = allocatorMXF;
        m_sinkMXF = m_AllocatorMXF;
    }
    else
    {
        return STATUS_INVALID_PARAMETER;
    }

    return STATUS_SUCCESS;
}

#pragma code_seg("PAGE")
/*****************************************************************************
 * CMiniportDMusUARTStream::SetState()
 *****************************************************************************
 * Sets the state of the channel.
 */
STDMETHODIMP_(NTSTATUS)
CMiniportDMusUARTStream::
SetState
(
    _In_    KSSTATE     NewState
)
{
    PAGED_CODE();

    MLOG("SetState %d",NewState);

    if (NewState == KSSTATE_RUN)
    {
		MLOG("KSSTATE_RUN for stream");
    }

    if (m_fCapture)
    {
        m_pMiniport->m_KSStateInput = NewState;

    }
    return STATUS_SUCCESS;
}

#pragma code_seg()
/*****************************************************************************
 * CMiniportDMusUART::Service()
 *****************************************************************************
 * DPC-mode service call from the port.
 */
STDMETHODIMP_(void)
CMiniportDMusUART::
Service
(void
)
{
	MLOG("Service");
}

#pragma code_seg("PAGE")
/*****************************************************************************
 * CMiniportDMusUARTStream::ConnectOutput()
 *****************************************************************************
 * Writes outgoing MIDI data.
 */
NTSTATUS
CMiniportDMusUARTStream::
ConnectOutput(_In_  PMXF sinkMXF)
{
    PAGED_CODE();
	MLOG("ConnectOutput");
    if (m_fCapture)
    {
        if ((sinkMXF) && (m_sinkMXF == m_AllocatorMXF))
        {
			MLOG("Assigning m_sinKMXF");
			m_sinkMXF = sinkMXF;
            return STATUS_SUCCESS;
        }
        else
        {
			MLOG("ConnectOutput Failed")
		}
    }
    else
    {
       MLOG("ConnectOutput called on renderer; failed");
    }
    return STATUS_UNSUCCESSFUL;
}

#pragma code_seg("PAGE")
/*****************************************************************************
 * CMiniportDMusUARTStream::DisconnectOutput()
 *****************************************************************************
 * Writes outgoing MIDI data.
 */
NTSTATUS
CMiniportDMusUARTStream::
DisconnectOutput(_In_   PMXF sinkMXF)
{
    PAGED_CODE();

	MLOG("DisconnectOutput");
    if (m_fCapture)
    {
        if ((m_sinkMXF == sinkMXF) || (!sinkMXF))
        {
			MLOG("Assigning allocator to sink")
            m_sinkMXF = m_AllocatorMXF;
            return STATUS_SUCCESS;
        }
        else
        {
           MLOG("DisconnectOutput failed");
        }
    }
    else
    {
       MLOG("DisconnectOutput called on renderer; failed");
    }
    return STATUS_UNSUCCESSFUL;
}

#pragma code_seg("PAGE")
/*****************************************************************************
 * CMiniportDMusUARTStream::HandlePortParams()
 *****************************************************************************
 * Writes an outgoing MIDI message.
 */
NTSTATUS
CMiniportDMusUARTStream::
HandlePortParams
(
    IN      PPCPROPERTY_REQUEST     pRequest
)
{
    PAGED_CODE();

	MLOG("HandlePortParams");
    NTSTATUS ntStatus;

    if (pRequest->Verb & KSPROPERTY_TYPE_SET)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    ntStatus = ValidatePropertyRequest(pRequest, sizeof(SYNTH_PORTPARAMS), TRUE);
    if (NT_SUCCESS(ntStatus))
    {
        RtlCopyMemory(pRequest->Value, pRequest->Instance, sizeof(SYNTH_PORTPARAMS));

        PSYNTH_PORTPARAMS Params = (PSYNTH_PORTPARAMS)pRequest->Value;

        if (Params->ValidParams & ~SYNTH_PORTPARAMS_CHANNELGROUPS)
        {
            Params->ValidParams &= SYNTH_PORTPARAMS_CHANNELGROUPS;
        }

        if (!(Params->ValidParams & SYNTH_PORTPARAMS_CHANNELGROUPS))
        {
            Params->ChannelGroups = 1;
        }
        else if (Params->ChannelGroups != 1)
        {
            Params->ChannelGroups = 1;
        }

        pRequest->ValueSize = sizeof(SYNTH_PORTPARAMS);
    }

    return ntStatus;
}

/*****************************************************************************
 * DirectMusic properties
 ****************************************************************************/

#pragma code_seg("PAGE")
/*
 *  Properties concerning synthesizer functions.
 */
const WCHAR wszDescOut[] = L"DMusic MPU-401 Out ";
const WCHAR wszDescIn[] = L"DMusic MPU-401 In ";

NTSTATUS PropertyHandler_Synth
(
    IN      PPCPROPERTY_REQUEST     pRequest
)
{
    NTSTATUS    ntStatus;

    PAGED_CODE();

	MLOG("PropertyHandler synth");

    if (pRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
    {
        ntStatus = ValidatePropertyRequest(pRequest, sizeof(ULONG), TRUE);
        if (NT_SUCCESS(ntStatus))
        {
            // if return buffer can hold a ULONG, return the access flags
            PULONG AccessFlags = PULONG(pRequest->Value);

            *AccessFlags = KSPROPERTY_TYPE_BASICSUPPORT;
            switch (pRequest->PropertyItem->Id)
            {
                case KSPROPERTY_SYNTH_CAPS:
                case KSPROPERTY_SYNTH_CHANNELGROUPS:
                    *AccessFlags |= KSPROPERTY_TYPE_GET;
            }
            switch (pRequest->PropertyItem->Id)
            {
                case KSPROPERTY_SYNTH_CHANNELGROUPS:
                    *AccessFlags |= KSPROPERTY_TYPE_SET;
            }
            ntStatus = STATUS_SUCCESS;
            pRequest->ValueSize = sizeof(ULONG);

            switch (pRequest->PropertyItem->Id)
            {
                case KSPROPERTY_SYNTH_PORTPARAMETERS:
                    if (pRequest->MinorTarget)
                    {
                        *AccessFlags |= KSPROPERTY_TYPE_GET;
                    }
                    else
                    {
                        pRequest->ValueSize = 0;
                        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
                    }
            }
        }
    }
    else
    {
        ntStatus = STATUS_SUCCESS;
        switch(pRequest->PropertyItem->Id)
        {
            case KSPROPERTY_SYNTH_CAPS:
                _DbgPrintF(DEBUGLVL_VERBOSE,("PropertyHandler_Synth:KSPROPERTY_SYNTH_CAPS"));

                if (pRequest->Verb & KSPROPERTY_TYPE_SET)
                {
                    ntStatus = STATUS_INVALID_DEVICE_REQUEST;
                }

                if (NT_SUCCESS(ntStatus))
                {
                    ntStatus = ValidatePropertyRequest(pRequest, sizeof(SYNTHCAPS), TRUE);

                    if (NT_SUCCESS(ntStatus))
                    {
                        SYNTHCAPS *caps = (SYNTHCAPS*)pRequest->Value;
                        int increment;
                        RtlZeroMemory(caps, sizeof(SYNTHCAPS));
                        // XXX Different guids for different instances!
                        //
                        if (pRequest->Node == eSynthNode)
                        {
                            increment = sizeof(wszDescOut) - 2;
                            RtlCopyMemory( caps->Description,wszDescOut,increment);
                            caps->Guid           = CLSID_MiniportDriverDMusUART;
                        }
                        else
                        {
                            increment = sizeof(wszDescIn) - 2;
                            RtlCopyMemory( caps->Description,wszDescIn,increment);
                            caps->Guid           = CLSID_MiniportDriverDMusUARTCapture;
                        }

                        caps->Flags              = SYNTH_PC_EXTERNAL;
                        caps->MemorySize         = 0;
                        caps->MaxChannelGroups   = 1;
                        caps->MaxVoices          = 0xFFFFFFFF;
                        caps->MaxAudioChannels   = 0xFFFFFFFF;

                        caps->EffectFlags        = 0;

                        CMiniportDMusUART *aMiniport;
                        ASSERT(pRequest->MajorTarget);
                        aMiniport = (CMiniportDMusUART *)(PMINIPORTDMUS)(pRequest->MajorTarget);
                        WCHAR wszDesc2[16];
                        size_t cLen;
                        RtlStringCchPrintfW (wszDesc2, sizeof(wszDesc2)/sizeof(wszDesc2[0]), L"[%03X]\0", PtrToUlong(aMiniport->m_pPortBase));
                        ntStatus = RtlStringCchLengthW (wszDesc2, sizeof(wszDesc2)/sizeof(wszDesc2[0]), &cLen);
                        if (NT_SUCCESS(ntStatus))
                        {
#if _PREFAST_
                            __assume(cLen <= sizeof(wszDesc2));
#endif
                            RtlCopyMemory((WCHAR *)((DWORD_PTR)(caps->Description) + increment),
                                           wszDesc2,
                                           cLen);
                        }

                        pRequest->ValueSize = sizeof(SYNTHCAPS);
                    }
                }

                break;

             case KSPROPERTY_SYNTH_PORTPARAMETERS:
                _DbgPrintF(DEBUGLVL_VERBOSE,("PropertyHandler_Synth:KSPROPERTY_SYNTH_PORTPARAMETERS"));
    {
                CMiniportDMusUARTStream *aStream;

                aStream = (CMiniportDMusUARTStream*)(pRequest->MinorTarget);
                if (aStream)
                {
                    ntStatus = aStream->HandlePortParams(pRequest);
                }
                else
                {
                    ntStatus = STATUS_INVALID_DEVICE_REQUEST;
                }
               }
               break;

            case KSPROPERTY_SYNTH_CHANNELGROUPS:
                _DbgPrintF(DEBUGLVL_VERBOSE,("PropertyHandler_Synth:KSPROPERTY_SYNTH_CHANNELGROUPS"));

                ntStatus = ValidatePropertyRequest(pRequest, sizeof(ULONG), TRUE);
                if (NT_SUCCESS(ntStatus))
                {
                    *(PULONG)(pRequest->Value) = 1;
                    pRequest->ValueSize = sizeof(ULONG);
                }
                break;

            case KSPROPERTY_SYNTH_LATENCYCLOCK:
                _DbgPrintF(DEBUGLVL_VERBOSE,("PropertyHandler_Synth:KSPROPERTY_SYNTH_LATENCYCLOCK"));

                if(pRequest->Verb & KSPROPERTY_TYPE_SET)
                {
                    ntStatus = STATUS_INVALID_DEVICE_REQUEST;
                }
                else
                {
                    ntStatus = ValidatePropertyRequest(pRequest, sizeof(ULONGLONG), TRUE);
                    if(NT_SUCCESS(ntStatus))
                    {
                        REFERENCE_TIME rtLatency;
                        CMiniportDMusUARTStream *aStream;

                        aStream = (CMiniportDMusUARTStream*)(pRequest->MinorTarget);
                        if(aStream == NULL)
                        {
                            ntStatus = STATUS_INVALID_DEVICE_REQUEST;
                        }
                        else
                        {
                            aStream->m_pMiniport->m_MasterClock->GetTime(&rtLatency);
                            *((PULONGLONG)pRequest->Value) = rtLatency;
                            pRequest->ValueSize = sizeof(ULONGLONG);
                        }
                    }
                }
                break;

            default:
                _DbgPrintF(DEBUGLVL_TERSE,("Unhandled property in PropertyHandler_Synth"));
                break;
        }
    }
    return ntStatus;
}

/*****************************************************************************
 * ValidatePropertyRequest()
 *****************************************************************************
 * Validates pRequest.
 *  Checks if the ValueSize is valid
 *  Checks if the Value is valid
 *
 *  This does not update pRequest->ValueSize if it returns NT_SUCCESS.
 *  Caller must set pRequest->ValueSize in case of NT_SUCCESS.
 */
NTSTATUS ValidatePropertyRequest
(
    IN      PPCPROPERTY_REQUEST     pRequest,
    IN      ULONG                   ulValueSize,
    IN      BOOLEAN                 fValueRequired
)
{
    PAGED_CODE();

    NTSTATUS    ntStatus;

    if (pRequest->ValueSize >= ulValueSize)
    {
        if (fValueRequired && NULL == pRequest->Value)
        {
            ntStatus = STATUS_INVALID_PARAMETER;
        }
        else
        {
            ntStatus = STATUS_SUCCESS;
        }
    }
    else  if (0 == pRequest->ValueSize)
    {
        ntStatus = STATUS_BUFFER_OVERFLOW;
    }
    else
    {
        ntStatus = STATUS_BUFFER_TOO_SMALL;
    }

    if (STATUS_BUFFER_OVERFLOW == ntStatus)
    {
        pRequest->ValueSize = ulValueSize;
    }
    else
    {
        pRequest->ValueSize = 0;
    }

    return ntStatus;
} // ValidatePropertyRequest

#pragma code_seg()
