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
#include "Public.h"
#include "EppEngine.h"

#define DRIVER_TAG 'PPEW' // 'WEPP' in little-endian

//
// Connect data structure for IOCTL_INTERNAL_MOUSE_CONNECT between mouhid and mouclass
//
#ifndef IOCTL_INTERNAL_MOUSE_CONNECT
#define IOCTL_INTERNAL_MOUSE_CONNECT CTL_CODE(FILE_DEVICE_MOUSE, 0x0080, METHOD_NEITHER, FILE_ANY_ACCESS)
#endif

#ifndef _CONNECT_DATA_DEFINED
#define _CONNECT_DATA_DEFINED

typedef VOID
(*PMOUSE_CLASS_SERVICE_CALLBACK)(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PMOUSE_INPUT_DATA InputDataStart,
    _In_ PMOUSE_INPUT_DATA InputDataEnd,
    _Inout_ PULONG InputDataConsumed
);

typedef struct _CONNECT_DATA {
    IN PDEVICE_OBJECT ClassDeviceObject;
    IN PMOUSE_CLASS_SERVICE_CALLBACK ClassService;
} CONNECT_DATA, *PCONNECT_DATA;

#endif

typedef struct _DEVICE_EXTENSION {
    WDFDEVICE               WdfDevice;
    WDFIOTARGET             IoTarget;
    
    // Original MouseClassServiceCallback saved during internal IOCTL connect
    PDEVICE_OBJECT          OriginalServiceCallbackTarget;
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
