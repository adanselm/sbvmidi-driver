/*****************************************************************************
 * MPU.cpp - UART miniport implementation
 *****************************************************************************
 * Copyright (c) 1998-2000 Microsoft Corporation.  All rights reserved.
 *
 */

#pragma warning (disable : 4127)

#include "private.h"
#include "ksdebug.h"


#pragma code_seg()

/*****************************************************************************
 * CMiniportDMusUARTStream::Write()
 *****************************************************************************
 * Writes outgoing MIDI data by forwarding it to the miniport driver,
 * which will delegate to the input stream
 */
STDMETHODIMP_(NTSTATUS)
CMiniportDMusUARTStream::
Write
(
    IN      PVOID       BufferAddress,
    IN      ULONG       Length,
    OUT     PULONG      BytesWritten
)
{
	MLOG("Stream write. Length: %d",Length);
	ULONG i;
	PUCHAR cbuffer = (PUCHAR) BufferAddress;
	for (i = 0; i < Length; i++) {
		MLOG("Byte %d: %x", i, cbuffer[i]);
	}
    ASSERT(BytesWritten);
    if (!BufferAddress)
    {
        Length = 0;
    }
    if (!m_fCapture)
    {
		MLOG("Notifying miniport driver from stream...");
		m_pMiniport->ForwardOutputFromSource(this, BufferAddress, Length);
		*BytesWritten = Length;
		return STATUS_SUCCESS;
    }
    else   
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
}

#pragma code_seg()
/*****************************************************************************
 * SnapTimeStamp()
 *****************************************************************************
 *
 * At synchronized execution to ISR, copy miniport's volatile m_InputTimeStamp
 * to stream's m_SnapshotTimeStamp and zero m_InputTimeStamp.
 *
 */
STDMETHODIMP_(NTSTATUS)
SnapTimeStamp(PINTERRUPTSYNC InterruptSync,PVOID pStream)
{
    UNREFERENCED_PARAMETER(InterruptSync);

    CMiniportDMusUARTStream *pMPStream = (CMiniportDMusUARTStream *)pStream;

    //  cache the timestamp
    pMPStream->m_SnapshotTimeStamp = pMPStream->m_pMiniport->m_InputTimeStamp;

    return STATUS_SUCCESS;
}

VOID CMiniportDMusUARTStream::ForwardEventsToPort(IN PVOID inputBuffer, IN ULONG Length) {
	MLOG("Input stream, forward events to port (%p,%d)...", inputBuffer, Length);
	PDMUS_KERNEL_EVENT  aDMKEvt;
	PBYTE bbuffer = (PBYTE)inputBuffer;
	NTSTATUS ntStatus = STATUS_SUCCESS;
	if (m_AllocatorMXF == NULL) return;
	m_AllocatorMXF->GetMessage(&aDMKEvt);
	if (Length > sizeof(PBYTE)) {
		aDMKEvt->cbEvent = (USHORT)Length;
		MLOG("Message is too long for standard. Using pbdata. Max: %d. Current: %d", sizeof(PBYTE), Length);
		aDMKEvt->usFlags = DMUS_KEF_EVENT_COMPLETE;
		aDMKEvt->uData.pbData = (PBYTE)inputBuffer;
	}
	else {
		MLOG("Copying data to kernel event...");
		for (aDMKEvt->cbEvent = 0; aDMKEvt->cbEvent < Length; aDMKEvt->cbEvent++) {
			aDMKEvt->uData.abData[aDMKEvt->cbEvent] = bbuffer[aDMKEvt->cbEvent];
		}
		aDMKEvt->usFlags = DMUS_KEF_EVENT_INCOMPLETE;
	}
	MLOG("Snap timestamp...");
	ntStatus = SnapTimeStamp(NULL, PVOID(this));
	aDMKEvt->ullPresTime100ns = m_SnapshotTimeStamp;
	aDMKEvt->usChannelGroup = 1;
	MLOG("Fowarding message to port driver...")
	(void)m_sinkMXF->PutMessage(aDMKEvt);
}
