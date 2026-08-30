/*++

Module Name:

    EppEngine.c

Abstract:

    Exact in-kernel implementation of Windows Enhance Pointer Precision.
    Mirrors win32kbase.sys!CalculateMouseAcceleration.
    Uses fixed-point arithmetic for ultra-low latency (< 1.5 microseconds).

--*/

#include "EppEngine.h"

// Windows Sensitivity 1-20 to Fixed Point multiplier table
static const FIXED16_16 g_SensitivityTable[21] = {
    0,
    2048,   // 1: 1/32
    4096,   // 2: 1/16
    8192,   // 3: 1/8
    16384,  // 4: 2/8
    24576,  // 5: 3/8
    32768,  // 6: 4/8
    40960,  // 7: 5/8
    49152,  // 8: 6/8
    57344,  // 9: 7/8
    65536,  // 10: 1.0 (6/11 default)
    81920,  // 11: 1.25
    98304,  // 12: 1.50
    114688, // 13: 1.75
    131072, // 14: 2.00
    147456, // 15: 2.25
    163840, // 16: 2.50
    180224, // 17: 2.75
    196608, // 18: 3.00
    212992, // 19: 3.25
    229376  // 20: 3.50
};

VOID
EppEngine_Initialize(
    _Out_ PEPP_ENGINE_STATE State
)
{
    RtlZeroMemory(State, sizeof(EPP_ENGINE_STATE));
    KeQueryPerformanceCounter(&State->QpcFrequency);
    State->SensitivityMultiplier = 65536; // 1.0 default
}

// Fast integer square root for vector magnitude
static ULONG
FastSqrt(ULONG n)
{
    ULONG root = 0;
    ULONG bit = 1UL << 30;

    while (bit > n) {
        bit >>= 2;
    }

    while (bit != 0) {
        if (n >= root + bit) {
            n -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }

    return root;
}

FIXED16_16
EppEngine_CalculateMultiplier(
    _In_ FIXED16_16 VelocityCountsPerMs,
    _In_ FIXED16_16 Sensitivity
)
{
    // Fixed-point thresholds (velocity in counts/ms * 65536)
    // 0.2 counts/ms = 13107
    // 0.8 counts/ms = 52428
    // 2.5 counts/ms = 163840
    // 6.0 counts/ms = 393216
    FIXED16_16 baseMultiplier;

    if (VelocityCountsPerMs <= 13107) {
        // Precision zone (0.55x -> 0.80x)
        FIXED16_16 t = FIXED_DIV(VelocityCountsPerMs, 13107);
        baseMultiplier = 36044 + FIXED_MUL(t, 16384);
    } else if (VelocityCountsPerMs <= 52428) {
        // Linear transition (0.80x -> 1.00x)
        FIXED16_16 t = FIXED_DIV(VelocityCountsPerMs - 13107, 39321);
        baseMultiplier = 52428 + FIXED_MUL(t, 13108);
    } else if (VelocityCountsPerMs <= 163840) {
        // Parabolic acceleration (1.00x -> 1.75x)
        FIXED16_16 t = FIXED_DIV(VelocityCountsPerMs - 52428, 111412);
        FIXED16_16 t2 = FIXED_MUL(t, t);
        baseMultiplier = 65536 + FIXED_MUL(t2, 49152);
    } else if (VelocityCountsPerMs <= 393216) {
        // High speed flick (1.75x -> 2.85x)
        FIXED16_16 t = FIXED_DIV(VelocityCountsPerMs - 163840, 229376);
        baseMultiplier = 114688 + FIXED_MUL(t, 72089);
    } else {
        // Velocity saturation plateau
        baseMultiplier = 186777; // ~2.85x
    }

    return FIXED_MUL(baseMultiplier, Sensitivity);
}

VOID
EppEngine_ProcessPacket(
    _Inout_ PEPP_ENGINE_STATE State,
    _In_    PEPP_DRIVER_CONFIG Config,
    _In_    LONG InX,
    _In_    LONG InY,
    _Out_   PLONG OutX,
    _Out_   PLONG OutY
)
{
    LARGE_INTEGER currentQpc;
    LONGLONG deltaQpc;
    FIXED16_16 dtMs;
    ULONG distSquared;
    ULONG distCounts;
    FIXED16_16 velocity;
    FIXED16_16 sens;
    FIXED16_16 multiplier;
    FIXED16_16 rawFixedX;
    FIXED16_16 rawFixedY;
    FIXED16_16 scaledFixedX;
    FIXED16_16 scaledFixedY;

    if (!Config->EppEnabled) {
        *OutX = InX;
        *OutY = InY;
        return;
    }

    currentQpc = KeQueryPerformanceCounter(NULL);

    // Compute Delta Time in Milliseconds
    if (State->LastTimestampQpc.QuadPart > 0 && State->QpcFrequency.QuadPart > 0) {
        deltaQpc = currentQpc.QuadPart - State->LastTimestampQpc.QuadPart;
        // Convert QPC delta to milliseconds in Q16.16
        dtMs = (FIXED16_16)(((deltaQpc * 1000) << 16) / State->QpcFrequency.QuadPart);
    } else {
        // Default 0.500 ms @ 2000Hz (32768 in Q16.16)
        dtMs = 32768;
    }
    State->LastTimestampQpc = currentQpc;

    if (dtMs < 6553) { // Clamp min 0.1ms
        dtMs = 6553;
    }

    // Compute Distance
    distSquared = (ULONG)(InX * InX + InY * InY);
    distCounts = FastSqrt(distSquared);

    // Velocity = distance / dt (Q16.16)
    velocity = FIXED_DIV((FIXED16_16)(distCounts << 16), dtMs);

    // Lookup Sensitivity Multiplier
    if (Config->WindowsSensitivity >= 1 && Config->WindowsSensitivity <= 20) {
        sens = g_SensitivityTable[Config->WindowsSensitivity];
    } else {
        sens = 65536; // 1.0
    }

    // Evaluate EPP Multiplier
    multiplier = EppEngine_CalculateMultiplier(velocity, sens);

    // Apply Transformation in Fixed-Point with Sub-pixel Remainder Preservation
    rawFixedX = (FIXED16_16)(InX << 16);
    rawFixedY = (FIXED16_16)(InY << 16);

    scaledFixedX = FIXED_MUL(rawFixedX, multiplier);
    scaledFixedY = FIXED_MUL(rawFixedY, multiplier);

    if (Config->SubpixelRemainderEnabled) {
        scaledFixedX += State->RemainderX;
        scaledFixedY += State->RemainderY;
    }

    // Extract Integer Output
    *OutX = (LONG)((scaledFixedX + 32768) >> 16);
    *OutY = (LONG)((scaledFixedY + 32768) >> 16);

    // Store Sub-pixel Fractional Remainder for Next Hardware Report
    if (Config->SubpixelRemainderEnabled) {
        State->RemainderX = scaledFixedX - (FIXED16_16)(*OutX << 16);
        State->RemainderY = scaledFixedY - (FIXED16_16)(*OutY << 16);
    } else {
        State->RemainderX = 0;
        State->RemainderY = 0;
    }
}
