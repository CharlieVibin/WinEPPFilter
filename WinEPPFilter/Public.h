/*++

Module Name:

    Public.h

Abstract:

    Shared IOCTL definitions and structures between WinEPPFilter.sys kernel driver
    and DiagnosticApp user-mode testing tool.

--*/

#pragma once

#define WIN_EPP_DRIVER_VERSION 0x00010000 // 1.0.0

// {B7D6B92C-38A2-4B27-992C-826315A5A1B8}
DEFINE_GUID(GUID_DEVINTERFACE_WINEPPFILTER,
    0xb7d6b92c, 0x38a2, 0x4b27, 0x99, 0x2c, 0x82, 0x63, 0x15, 0xa5, 0xa1, 0xb8);

#define FILE_DEVICE_WINEPP 0x8000

#define IOCTL_EPP_GET_DRIVER_VERSION \
    CTL_CODE(FILE_DEVICE_WINEPP, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS)

#define IOCTL_EPP_SET_CONFIG \
    CTL_CODE(FILE_DEVICE_WINEPP, 0x801, METHOD_BUFFERED, FILE_WRITE_ACCESS)

#define IOCTL_EPP_GET_STATS \
    CTL_CODE(FILE_DEVICE_WINEPP, 0x802, METHOD_BUFFERED, FILE_READ_ACCESS)

#define IOCTL_EPP_RESET_REMAINDER \
    CTL_CODE(FILE_DEVICE_WINEPP, 0x803, METHOD_BUFFERED, FILE_WRITE_ACCESS)

typedef struct _EPP_DRIVER_CONFIG {
    ULONG       Dpi;                        // e.g. 800
    ULONG       WindowsSensitivity;         // 1 - 20 (10 = 6/11)
    BOOLEAN     EppEnabled;                 // TRUE = Active EPP ballistics
    BOOLEAN     SubpixelRemainderEnabled;   // TRUE = High precision accumulation
    BOOLEAN     PassThroughToCursor;        // TRUE = Feed cursor as well
} EPP_DRIVER_CONFIG, *PEPP_DRIVER_CONFIG;

typedef struct _EPP_DRIVER_STATS {
    ULONG64     TotalInputPackets;
    ULONG64     TotalOutputPackets;
    LONG        LastCalculatedDeltaX;
    LONG        LastCalculatedDeltaY;
    ULONG       LastProcessingTimeUs;       // Microseconds
    ULONG       PacketDroppedCount;
} EPP_DRIVER_STATS, *PEPP_DRIVER_STATS;
