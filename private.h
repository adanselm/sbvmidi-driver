/*****************************************************************************
 * private.h - MPU-401 miniport private definitions
 *****************************************************************************
 * Copyright (c) 1997-2000 Microsoft Corporation.  All Rights Reserved.
 */

#ifndef _DMUSUART_PRIVATE_H_
#define _DMUSUART_PRIVATE_H_

#define INITGUID 1
#include "portcls.h"
#include <ntstrsafe.h>
#include "stdunk.h"
#include "dmusicks.h"

#define MLOG(...) { DbgPrint(__VA_ARGS__); DbgPrint("\n");}


//  + for absolute / - for relative
#define kOneMillisec (10 * 1000)



/*****************************************************************************
 * References forward
 */


/*****************************************************************************
 * Prototypes
 */

NTSTATUS ValidatePropertyRequest(IN PPCPROPERTY_REQUEST pRequest, IN ULONG ulValueSize, IN BOOLEAN fValueRequired);


/*****************************************************************************
 * Globals
 */


/*****************************************************************************
 * Classes
 */

/*****************************************************************************
 * CMiniportDMusUART
 *****************************************************************************
 * MPU-401 miniport.  This object is associated with the device and is
 * created when the device is started.  The class inherits IMiniportDMus
 * so it can expose this interface and CUnknown so it automatically gets
 * reference counting and aggregation support.
 */
class CMiniportDMusUART
:   public IMiniportDMus,
    public IMusicTechnology,
    public IPowerNotify,
    public CUnknown
{
private:
    KSSTATE         m_KSStateInput;         // Miniport state (RUN/PAUSE/ACQUIRE/STOP)
    PPORTDMUS       m_pPort;                // Callback interface.
    PUCHAR          m_pPortBase;            // Base port address.
    PSERVICEGROUP   m_pServiceGroup;        // Service group for capture.
    PMASTERCLOCK    m_MasterClock;          // for input data
    REFERENCE_TIME  m_InputTimeStamp;       // capture data timestamp
    USHORT          m_NumRenderStreams;     // Num active render streams.
    USHORT          m_NumCaptureStreams;    // Num active capture streams.
    GUID            m_MusicFormatTechnology;
    POWER_STATE     m_PowerState;           // Saved power state (D0 = full power, D3 = off)
	PVOID			m_inputStream;
	PVOID			m_outputStream;
    /*************************************************************************
     * CMiniportDMusUART methods
     *
     * These are private member functions used internally by the object.
     * See MINIPORT.CPP for specific descriptions.
     */
    NTSTATUS ProcessResources
    (
        IN      PRESOURCELIST   ResourceList
    );

public:
	VOID ForwardOutputFromSource(IN PVOID source, IN PVOID BufferAddress, IN ULONG Length);
	VOID StreamDestroying(IN PVOID source, IN BOOL capture);
    /*************************************************************************
     * The following two macros are from STDUNK.H.  DECLARE_STD_UNKNOWN()
     * defines inline IUnknown implementations that use CUnknown's aggregation
     * support.  NonDelegatingQueryInterface() is declared, but it cannot be
     * implemented generically.  Its definition appears in MINIPORT.CPP.
     * DEFINE_STD_CONSTRUCTOR() defines inline a constructor which accepts
     * only the outer unknown, which is used for aggregation.  The standard
     * create macro (in MINIPORT.CPP) uses this constructor.
     */
    DECLARE_STD_UNKNOWN();
    DEFINE_STD_CONSTRUCTOR(CMiniportDMusUART);

    ~CMiniportDMusUART();

    /*************************************************************************
     * IMiniport methods
     */
    STDMETHODIMP_(NTSTATUS)
    GetDescription
    (   OUT     PPCFILTER_DESCRIPTOR *  OutFilterDescriptor
    );
    STDMETHODIMP_(NTSTATUS)
    DataRangeIntersection
    (   IN      ULONG           PinId
    ,   IN      PKSDATARANGE    DataRange
    ,   IN      PKSDATARANGE    MatchingDataRange
    ,   IN      ULONG           OutputBufferLength
    ,   OUT     PVOID           ResultantFormat
    ,   OUT     PULONG          ResultantFormatLength
    )
    {
        UNREFERENCED_PARAMETER(PinId);
        UNREFERENCED_PARAMETER(DataRange);
        UNREFERENCED_PARAMETER(MatchingDataRange);
        UNREFERENCED_PARAMETER(OutputBufferLength);
        UNREFERENCED_PARAMETER(ResultantFormat);
        UNREFERENCED_PARAMETER(ResultantFormatLength);

        return STATUS_NOT_IMPLEMENTED;
    }

    /*************************************************************************
     * IMiniportDMus methods
     */
    STDMETHODIMP_(NTSTATUS) Init
    (
        IN      PUNKNOWN        UnknownAdapter,
        IN      PRESOURCELIST   ResourceList,
        IN      PPORTDMUS       Port,
        OUT     PSERVICEGROUP * ServiceGroup
    );
    STDMETHODIMP_(NTSTATUS) NewStream
    (
        OUT     PMXF                  * Stream,
        IN      PUNKNOWN                OuterUnknown    OPTIONAL,
        IN      POOL_TYPE               PoolType,
        IN      ULONG                   PinID,
        IN      DMUS_STREAM_TYPE        StreamType,
        IN      PKSDATAFORMAT           DataFormat,
        OUT     PSERVICEGROUP         * ServiceGroup,
        IN      PAllocatorMXF           AllocatorMXF,
        IN      PMASTERCLOCK            MasterClock,
        OUT     PULONGLONG              SchedulePreFetch
    );
    STDMETHODIMP_(void) Service
    (   void
    );

    /*************************************************************************
     * IMusicTechnology methods
     */
    IMP_IMusicTechnology;

    /*************************************************************************
     * IPowerNotify methods
     */
    IMP_IPowerNotify;

    /*************************************************************************
     * Friends
     */
    friend class CMiniportDMusUARTStream;
    friend NTSTATUS
        DMusMPUInterruptServiceRoutine(PINTERRUPTSYNC InterruptSync,PVOID DynamicContext);
    friend NTSTATUS
        SynchronizedDMusMPUWrite(PINTERRUPTSYNC InterruptSync,PVOID syncWriteContext);
    friend KDEFERRED_ROUTINE DMusUARTTimerDPC;
    friend NTSTATUS PropertyHandler_Synth(IN PPCPROPERTY_REQUEST);
    friend STDMETHODIMP_(NTSTATUS) SnapTimeStamp(PINTERRUPTSYNC InterruptSync,PVOID pStream);
};

/*****************************************************************************
 * CMiniportDMusUARTStream
 *****************************************************************************
 * MPU-401 miniport stream.  This object is associated with the pin and is
 * created when the pin is instantiated.  It inherits IMXF
 * so it can expose this interface and CUnknown so it automatically gets
 * reference counting and aggregation support.
 */
class CMiniportDMusUARTStream
:   public IMXF,
    public CUnknown
{
private:
    CMiniportDMusUART * m_pMiniport;            // Parent.
    REFERENCE_TIME      m_SnapshotTimeStamp;    // Current snapshot of miniport's input timestamp.
    PUCHAR              m_pPortBase;            // Base port address.
    BOOLEAN             m_fCapture;             // Whether this is capture.
    PAllocatorMXF       m_AllocatorMXF;         // source/sink for DMus structs
    PMXF                m_sinkMXF;              // sink for DMus capture

public:
    /*************************************************************************
     * The following two macros are from STDUNK.H.  DECLARE_STD_UNKNOWN()
     * defines inline IUnknown implementations that use CUnknown's aggregation
     * support.  NonDelegatingQueryInterface() is declared, but it cannot be
     * implemented generically.  Its definition appears in MINIPORT.CPP.
     * DEFINE_STD_CONSTRUCTOR() defines inline a constructor which accepts
     * only the outer unknown, which is used for aggregation.  The standard
     * create macro (in MINIPORT.CPP) uses this constructor.
     */
    DECLARE_STD_UNKNOWN();
    DEFINE_STD_CONSTRUCTOR(CMiniportDMusUARTStream);

    ~CMiniportDMusUARTStream();
	VOID ForwardEventsToPort(IN PVOID inputBuffer, IN ULONG Length);
    STDMETHODIMP_(NTSTATUS) Init
    (
        IN      CMiniportDMusUART * pMiniport,
        IN      PUCHAR              pPortBase,
        IN      BOOLEAN             fCapture,
        IN      PAllocatorMXF       allocatorMXF,
        IN      PMASTERCLOCK        masterClock
    );

    NTSTATUS HandlePortParams
    (
        IN      PPCPROPERTY_REQUEST     Request
    );

    /*************************************************************************
     * IMiniportStreamDMusUART methods
     */
    IMP_IMXF;

    STDMETHODIMP_(NTSTATUS) Write
    (
        IN      PVOID       BufferAddress,
        IN      ULONG       BytesToWrite,
        OUT     PULONG      BytesWritten
    );

    friend NTSTATUS PropertyHandler_Synth(IN PPCPROPERTY_REQUEST);
    friend STDMETHODIMP_(NTSTATUS) SnapTimeStamp(PINTERRUPTSYNC InterruptSync,PVOID pStream);
};
#endif  //  _DMusUART_PRIVATE_H_
