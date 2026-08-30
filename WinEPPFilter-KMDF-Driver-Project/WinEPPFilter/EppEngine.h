/*++

Module Name:

    EppEngine.h

Abstract:

    Deterministic Windows EPP mathematical engine running at IRQL DISPATCH_LEVEL.
    Implements exact fixed-point Q16.16 polynomial spline evaluation and
    remainder accumulator to guarantee zero sub-pixel coordinate drift.

--*/

#pragma once

#include <ntddk.h>
#include "Public.h"

// Fixed Point Q16.16 Representation
typedef LONG FIXED16_16;
#define TO_FIXED(x)       ((FIXED16_16)((x) * 65536.0f))
#define FROM_FIXED(x)     ((float)(x) / 65536.0f)
#define FIXED_MUL(a, b)   ((FIXED16_16)(((LONGLONG)(a) * (b)) >> 16))
#define FIXED_DIV(a, b)   ((FIXED16_16)(((LONGLONG)(a) << 16) / (b)))

typedef struct _EPP_ENGINE_STATE {
    LARGE_INTEGER   LastTimestampQpc;
    LARGE_INTEGER   QpcFrequency;
    
    // Sub-pixel Remainders in Q16.16 format
    FIXED16_16      RemainderX;
    FIXED16_16      RemainderY;
    
    // Precalculated sensitivity multiplier
    FIXED16_16      SensitivityMultiplier;

} EPP_ENGINE_STATE, *PEPP_ENGINE_STATE;

VOID
EppEngine_Initialize(
    _Out_ PEPP_ENGINE_STATE State
);

VOID
EppEngine_ProcessPacket(
    _Inout_ PEPP_ENGINE_STATE State,
    _In_    PEPP_DRIVER_CONFIG Config,
    _In_    LONG InX,
    _In_    LONG InY,
    _Out_   PLONG OutX,
    _Out_   PLONG OutY
);

FIXED16_16
EppEngine_CalculateMultiplier(
    _In_ FIXED16_16 VelocityCountsPerMs,
    _In_ FIXED16_16 Sensitivity
);
