#pragma once

#include <cstddef>
#include <cstdint>

namespace RuffnecKk::MeleeSplash::BKVCombatInterop {

inline constexpr wchar_t ModuleName[] = L"ReimaginedCombat.dll";
inline constexpr wchar_t LegacyModuleName[] = L"BKVCombat.dll";
inline constexpr char GetApiExportName[] = "BKVCombat_GetApi";
inline constexpr std::uint32_t ApiVersion = 1;
inline constexpr std::uint64_t CriticalDeadlyResolverCapability =
    UINT64_C(1) << 0U;
inline constexpr std::uint32_t SyntheticSecondaryFlag = 1U << 0U;

enum class CriticalDeadlyOutcome : std::uint32_t {
    Unavailable = 0,
    None = 1,
    Critical = 2,
    Deadly = 3,
    AlreadyResolved = 4,
};

using ResolveCriticalDeadlyFn = CriticalDeadlyOutcome(__fastcall*)(
    void* game,
    void* attacker,
    void* target,
    void* damage,
    std::int32_t damageMode,
    std::uint32_t flags) noexcept;
using GetActiveCapabilitiesFn = std::uint64_t(__cdecl*)() noexcept;

struct ApiV1 {
    std::uint32_t size;
    std::uint32_t version;
    std::uint64_t compiledCapabilities;
    ResolveCriticalDeadlyFn resolveCriticalDeadly;
    GetActiveCapabilitiesFn getActiveCapabilities;
};

using GetApiFn = const ApiV1*(__cdecl*)(std::uint32_t requestedVersion) noexcept;

inline bool IsCompatibleApiV1(const ApiV1* api) noexcept {
    return api
        && api->size >= sizeof(ApiV1)
        && api->version == ApiVersion
        && (api->compiledCapabilities & CriticalDeadlyResolverCapability) != 0
        && api->resolveCriticalDeadly
        && api->getActiveCapabilities;
}

inline bool HasActiveCriticalDeadlyResolver(const ApiV1* api) noexcept {
    return IsCompatibleApiV1(api)
        && (api->getActiveCapabilities()
            & CriticalDeadlyResolverCapability) != 0;
}

static_assert(offsetof(ApiV1, compiledCapabilities) == 8);
static_assert(offsetof(ApiV1, resolveCriticalDeadly) == 16);
static_assert(offsetof(ApiV1, getActiveCapabilities) == 24);
static_assert(sizeof(ApiV1) == 32);

} // namespace RuffnecKk::MeleeSplash::BKVCombatInterop
