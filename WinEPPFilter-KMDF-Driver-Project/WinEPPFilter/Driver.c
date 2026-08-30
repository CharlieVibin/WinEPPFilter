/*++

Module Name:

    Driver.c

Abstract:

    Implementation of KMDF Mouse Class UpperFilter.
    Directly attaches to Mouse Class Stack and executes zero-delay Windows EPP
    mathematical ballistics inside the interrupt service DPC chain.

--*/

#include "Driver.h"

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;

    KdPrint(("WinEPPFilter: DriverEntry initializing\n"));

    WDF_DRIVER_CONFIG_INIT(&config, WinEppFilterEvtDeviceAdd);
    config.EvtDriverUnload = NULL;

    status = WdfDriverCreate(
        DriverObject,
        RegistryPath,
        WDF_NO_OBJECT_ATTRIBUTES,
        &config,
        WDF_NO_HANDLE
    );

    if (!NT_SUCCESS(status)) {
        KdPrint(("WinEPPFilter: WdfDriverCreate failed with status 0x%08x\n", status));
    }

    return status;
}

NTSTATUS
WinEppFilterEvtDeviceAdd(
    _In_    WDFDRIVER       Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
)
{
    NTSTATUS status;
    WDFDEVICE device;
    WDF_OBJECT_ATTRIBUTES attributes;
    PDEVICE_EXTENSION devExt;
    WDF_IO_QUEUE_CONFIG queueConfig;

    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    // Mark driver as a generic UpperFilter
    WdfFdoInitSetFilter(DeviceInit);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DEVICE_EXTENSION);

    status = WdfDeviceCreate(&DeviceInit, &attributes, &device);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    devExt = FilterGetData(device);
    RtlZeroMemory(devExt, sizeof(DEVICE_EXTENSION));
    devExt->WdfDevice = device;
    devExt->IoTarget = WdfDeviceGetIoTarget(device);

    KeInitializeSpinLock(&devExt->FilterSpinLock);

    // Initialize Default EPP Engine State with standard Windows 6/11 curves (800 DPI default)
    EppEngine_Initialize(&devExt->EppEngine);
    devExt->Config.Dpi = 800;
    devExt->Config.WindowsSensitivity = 10; // 6/11 default
    devExt->Config.EppEnabled = TRUE;
    devExt->Config.SubpixelRemainderEnabled = TRUE;
    devExt->Config.PassThroughToCursor = FALSE; // Direct exclusively to Virtual Raw Input

    // Configure Default I/O Queue for IOCTLs and Internal Device Controls
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel);
    queueConfig.EvtIoDeviceControl = WinEppFilterEvtIoDeviceControl;
    queueConfig.EvtIoInternalDeviceControl = WinEppFilterEvtIoInternalDeviceControl;

    status = WdfIoQueueCreate(device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    KdPrint(("WinEPPFilter: Device 0x%p attached to mouse stack successfully\n", device));
    return STATUS_SUCCESS;
}

VOID
WinEppFilterEvtIoInternalDeviceControl(
    _In_ WDFQUEUE   Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t     OutputBufferLength,
    _In_ size_t     InputBufferLength,
    _In_ ULONG      IoControlCode
)
{
    WDFDEVICE device = WdfIoQueueGetDevice(Queue);
    PDEVICE_EXTENSION devExt = FilterGetData(device);
    NTSTATUS status = STATUS_SUCCESS;
    WDF_REQUEST_SEND_OPTIONS sendOptions;
    CONNECT_DATA* connectData = NULL;

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    switch (IoControlCode) {
    case IOCTL_INTERNAL_MOUSE_CONNECT:
        // Intercept connection request between mouhid.sys and mouclass.sys
        status = WdfRequestRetrieveInputBuffer(Request, sizeof(CONNECT_DATA), (PVOID*)&connectData, NULL);
        if (NT_SUCCESS(status)) {
            // Save original class service callback
            devExt->OriginalServiceCallbackTarget = connectData->ClassDeviceObject;
            devExt->OriginalServiceCallback = (PMOUSE_CLASS_SERVICE_CALLBACK)connectData->ClassService;

            // Hook with our high-speed EPP transformation callback
            connectData->ClassDeviceObject = WdfDeviceWdmGetDeviceObject(device);
            connectData->ClassService = (PVOID)WinEppMouseClassServiceCallback;

            KdPrint(("WinEPPFilter: Hooked MouseClassServiceCallback successfully\n"));
        }
        break;

    default:
        break;
    }

    // Forward request down the mouse driver stack
    WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);
    if (!WdfRequestSend(Request, devExt->IoTarget, &sendOptions)) {
        status = WdfRequestGetStatus(Request);
        WdfRequestComplete(Request, status);
    }
}

VOID
WinEppMouseClassServiceCallback(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PMOUSE_INPUT_DATA InputDataStart,
    _In_ PMOUSE_INPUT_DATA InputDataEnd,
    _Inout_ PULONG InputDataConsumed
)
{
    PDEVICE_EXTENSION devExt;
    PMOUSE_INPUT_DATA current;
    KLOCK_QUEUE_HANDLE lockHandle;
    LARGE_INTEGER qpcStart, qpcEnd, frequency;

    devExt = FilterGetData(WdfGetDriverGlobals(), DeviceObject);
    if (!devExt || !devExt->OriginalServiceCallback) {
        return;
    }

    KeQueryPerformanceCounter(&frequency);
    qpcStart = KeQueryPerformanceCounter(NULL);

    KeAcquireInStackQueuedSpinLock(&devExt->FilterSpinLock, &lockHandle);

    // Process every incoming packet in the batch (typically 1 packet @ 2000Hz)
    for (current = InputDataStart; current < InputDataEnd; current++) {
        if (current->Flags & MOUSE_MOVE_RELATIVE) {
            LONG originalX = current->LastX;
            LONG originalY = current->LastY;
            LONG processedX = 0;
            LONG processedY = 0;

            devExt->Stats.TotalInputPackets++;

            // Execute in-kernel Windows EPP calculation with sub-pixel remainder preservation
            EppEngine_ProcessPacket(
                &devExt->EppEngine,
                &devExt->Config,
                originalX,
                originalY,
                &processedX,
                &processedY
            );

            devExt->Stats.TotalOutputPackets++;
            devExt->Stats.LastCalculatedDeltaX = processedX;
            devExt->Stats.LastCalculatedDeltaY = processedY;

            // Update packet for delivery
            current->LastX = processedX;
            current->LastY = processedY;
        }
    }

    KeReleaseInStackQueuedSpinLock(&lockHandle);

    qpcEnd = KeQueryPerformanceCounter(NULL);
    devExt->Stats.LastProcessingTimeUs = (ULONG)(((qpcEnd.QuadPart - qpcStart.QuadPart) * 1000000) / frequency.QuadPart);

    // Invoke original class callback to dispatch transformed packets to Raw Input queue
    (*(PMOUSE_CLASS_SERVICE_CALLBACK)devExt->OriginalServiceCallback)(
        devExt->OriginalServiceCallbackTarget,
        InputDataStart,
        InputDataEnd,
        InputDataConsumed
    );
}

VOID
WinEppFilterEvtIoDeviceControl(
    _In_ WDFQUEUE   Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t     OutputBufferLength,
    _In_ size_t     InputBufferLength,
    _In_ ULONG      IoControlCode
)
{
    WDFDEVICE device = WdfIoQueueGetDevice(Queue);
    PDEVICE_EXTENSION devExt = FilterGetData(device);
    NTSTATUS status = STATUS_SUCCESS;
    size_t bytesReturned = 0;

    switch (IoControlCode) {
    case IOCTL_EPP_GET_DRIVER_VERSION:
        if (OutputBufferLength >= sizeof(ULONG)) {
            PULONG version;
            status = WdfRequestRetrieveOutputBuffer(Request, sizeof(ULONG), (PVOID*)&version, NULL);
            if (NT_SUCCESS(status)) {
                *version = WIN_EPP_DRIVER_VERSION;
                bytesReturned = sizeof(ULONG);
            }
        } else {
            status = STATUS_BUFFER_TOO_SMALL;
        }
        break;

    case IOCTL_EPP_SET_CONFIG:
        if (InputBufferLength >= sizeof(EPP_DRIVER_CONFIG)) {
            PEPP_DRIVER_CONFIG config;
            status = WdfRequestRetrieveInputBuffer(Request, sizeof(EPP_DRIVER_CONFIG), (PVOID*)&config, NULL);
            if (NT_SUCCESS(status)) {
                KLOCK_QUEUE_HANDLE lockHandle;
                KeAcquireInStackQueuedSpinLock(&devExt->FilterSpinLock, &lockHandle);
                RtlCopyMemory(&devExt->Config, config, sizeof(EPP_DRIVER_CONFIG));
                KeReleaseInStackQueuedSpinLock(&lockHandle);
                bytesReturned = sizeof(EPP_DRIVER_CONFIG);
            }
        } else {
            status = STATUS_INVALID_PARAMETER;
        }
        break;

    case IOCTL_EPP_GET_STATS:
        if (OutputBufferLength >= sizeof(EPP_DRIVER_STATS)) {
            PEPP_DRIVER_STATS stats;
            status = WdfRequestRetrieveOutputBuffer(Request, sizeof(EPP_DRIVER_STATS), (PVOID*)&stats, NULL);
            if (NT_SUCCESS(status)) {
                KLOCK_QUEUE_HANDLE lockHandle;
                KeAcquireInStackQueuedSpinLock(&devExt->FilterSpinLock, &lockHandle);
                RtlCopyMemory(stats, &devExt->Stats, sizeof(EPP_DRIVER_STATS));
                KeReleaseInStackQueuedSpinLock(&lockHandle);
                bytesReturned = sizeof(EPP_DRIVER_STATS);
            }
        } else {
            status = STATUS_BUFFER_TOO_SMALL;
        }
        break;

    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    WdfRequestCompleteWithInformation(Request, status, bytesReturned);
}
