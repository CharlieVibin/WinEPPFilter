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

    EppEngine_Initialize(&devExt->EppEngine);
    devExt->Config.Dpi = 800;
    devExt->Config.WindowsSensitivity = 10;
    devExt->Config.EppEnabled = TRUE;
    devExt->Config.SubpixelRemainderEnabled = TRUE;
    devExt->Config.PassThroughToCursor = FALSE;

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
        status = WdfRequestRetrieveInputBuffer(Request, sizeof(CONNECT_DATA), (PVOID*)&connectData, NULL);
        if (NT_SUCCESS(status)) {
            devExt->OriginalServiceCallbackTarget = connectData->ClassDeviceObject;
            devExt->OriginalServiceCallback = (PMOUSE_CLASS_SERVICE_CALLBACK)connectData->ClassService;

            connectData->ClassDeviceObject = WdfDeviceWdmGetDeviceObject(device);
            connectData->ClassService = (PVOID)WinEppMouseClassServiceCallback;

            KdPrint(("WinEPPFilter: Hooked MouseClassServiceCallback successfully\n"));
        }
        break;

    default:
        break;
    }

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
    KIRQL oldIrql;

    devExt = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    if (!devExt || !devExt->OriginalServiceCallback) {
        return;
    }

    KeAcquireSpinLock(&devExt->FilterSpinLock, &oldIrql);

    for (current = InputDataStart; current < InputDataEnd; current++) {
        if (current->Flags & MOUSE_MOVE_RELATIVE) {
            LONG originalX = current->LastX;
            LONG originalY = current->LastY;
            LONG processedX = 0;
            LONG processedY = 0;

            devExt->Stats.TotalInputPackets++;

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

            current->LastX = processedX;
            current->LastY = processedY;
        }
    }

    KeReleaseSpinLock(&devExt->FilterSpinLock, oldIrql);

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
            KIRQL oldIrql;
            status = WdfRequestRetrieveInputBuffer(Request, sizeof(EPP_DRIVER_CONFIG), (PVOID*)&config, NULL);
            if (NT_SUCCESS(status)) {
                KeAcquireSpinLock(&devExt->FilterSpinLock, &oldIrql);
                RtlCopyMemory(&devExt->Config, config, sizeof(EPP_DRIVER_CONFIG));
                KeReleaseSpinLock(&devExt->FilterSpinLock, oldIrql);
                bytesReturned = sizeof(EPP_DRIVER_CONFIG);
            }
        } else {
            status = STATUS_INVALID_PARAMETER;
        }
        break;

    case IOCTL_EPP_GET_STATS:
        if (OutputBufferLength >= sizeof(EPP_DRIVER_STATS)) {
            PEPP_DRIVER_STATS stats;
            KIRQL oldIrql;
            status = WdfRequestRetrieveOutputBuffer(Request, sizeof(EPP_DRIVER_STATS), (PVOID*)&stats, NULL);
            if (NT_SUCCESS(status)) {
                KeAcquireSpinLock(&devExt->FilterSpinLock, &oldIrql);
                RtlCopyMemory(stats, &devExt->Stats, sizeof(EPP_DRIVER_STATS));
                KeReleaseSpinLock(&devExt->FilterSpinLock, oldIrql);
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
