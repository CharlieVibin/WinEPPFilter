/*++

Module Name:

    Driver.h

Abstract:

    KMDF Mouse Class UpperFilter Driver for Windows EPP to Raw Input Bridge.
    Intercepts MouseClassServiceCallback to perform deterministic sub-pixel
    Windows Enhance Pointer Precision transformation at DISPATCH_LEVEL.

Environment:

    Kernel mode only.

--*/

#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <ntddmou.h>
#include <hidport.h>
#include "Public.h"
#include "EppEngine.h"

#define DRIVER_TAG 'PPEW' // 'WEPP' in little-endian

typedef struct _DEVICE_EXTENSION {
    WDFDEVICE               WdfDevice;
    WDFIOTARGET             IoTarget;
    
    // Original MouseClassServiceCallback saved during internal IOCTL connect
    PVOID                   OriginalServiceCallbackTarget;
    PMOUSE_CLASS_SERVICE_CALLBACK OriginalServiceCallback;
    
    // Synchronization spinlock for high-frequency IRQL DISPATCH_LEVEL operations
    KSPIN_LOCK              FilterSpinLock;
    
    // Live Windows EPP Engine State & Sub-pixel Accumulator
    EPP_ENGINE_STATE        EppEngine;
    
    // Active driver configuration (DPI, Sensitivity, Remainder, PassThrough)
    EPP_DRIVER_CONFIG       Config;
    
    // Real-time packet and performance statistics
    EPP_DRIVER_STATS        Stats;
    
    // Virtual HID communication target / queue
    WDFQUEUE                VirtualHidQueue;
    BOOLEAN                 VirtualHidAttached;

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_EXTENSION, FilterGetData)

//
// Function Prototypes
//

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD WinEppFilterEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP WinEppFilterEvtDriverContextCleanup;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL WinEppFilterEvtIoDeviceControl;
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL WinEppFilterEvtIoInternalDeviceControl;

VOID
WinEppMouseClassServiceCallback(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PMOUSE_INPUT_DATA InputDataStart,
    _In_ PMOUSE_INPUT_DATA InputDataEnd,
    _Inout_ PULONG InputDataConsumed
);
