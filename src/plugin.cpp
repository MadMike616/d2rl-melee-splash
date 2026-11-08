#include <D2RLPlugin/api.h>

#include "melee_splash_config.hpp"
#include "melee_splash_bkvcombat_interop.hpp"
#include "melee_splash_gameplay.hpp"
#include "melee_splash_installer.h"

#include <Windows.h>
#include <intrin.h>
#include <malloc.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <utility>
#include <vector>

#pragma intrinsic(_ReturnAddress)

namespace RuffnecKk::MeleeSplash {
namespace {

constexpr std::int32_t UnitPlayer = 0;
constexpr std::int32_t UnitMonster = 1;
constexpr std::int32_t HitPointsStatId = 6;
constexpr std::int32_t PassiveCriticalStatId = 337;
constexpr std::int32_t DeadlyStrikeStatId = 141;
constexpr std::int32_t UninterruptibleStateId = 0x36;
constexpr std::uint8_t ReactiveMeleeEvent = 3;
constexpr std::uint32_t StructuralAreaMask = 0x0583;
constexpr std::size_t MaximumSidecars = 256;
constexpr std::size_t MaximumPendingCaptures = 32;
constexpr std::size_t MaximumCombatNodes = 1024;
constexpr std::size_t MaximumPoisonEntries = 4096;

constexpr std::uint32_t SyntheticHitFlags = 0x00000020;
constexpr std::uint16_t ResultSuccess = 0x0001;
constexpr std::uint16_t ResultWillDie = 0x0002;
constexpr std::uint16_t ResultGetHit = 0x0004;

constexpr std::size_t DamageSize = 0x180;
constexpr std::size_t DamageHitFlagsOffset = 0x000;
constexpr std::size_t DamageResultFlagsOffset = 0x004;
constexpr std::size_t DamagePhysicalOffset = 0x018;
constexpr std::size_t DamageFireOffset = 0x020;
constexpr std::size_t DamageBurnOffset = 0x024;
constexpr std::size_t DamageLightningOffset = 0x02C;
constexpr std::size_t DamageMagicOffset = 0x030;
constexpr std::size_t DamageColdOffset = 0x034;
constexpr std::size_t DamagePoisonOffset = 0x038;
constexpr std::size_t DamagePoisonLengthOffset = 0x03C;
constexpr std::size_t DamagePoisonVectorOffset = 0x040;
constexpr std::size_t DamagePoisonVectorCountOffset = 0x048;
constexpr std::size_t DamagePoisonVectorCapacityOffset = 0x050;
constexpr std::size_t DamagePoisonInlineStorageOffset = 0x058;
constexpr std::size_t DamageColdLengthOffset = 0x118;
constexpr std::size_t DamageFreezeLengthOffset = 0x11C;
constexpr std::size_t DamageLifeLeechOffset = 0x120;
constexpr std::size_t DamageManaLeechOffset = 0x124;
constexpr std::size_t DamageStaminaLeechOffset = 0x128;
constexpr std::size_t DamageStunLengthOffset = 0x12C;
constexpr std::size_t DamageAbsorbedLifeOffset = 0x130;
constexpr std::size_t DamageTotalOffset = 0x134;
constexpr std::size_t DamageHitClassOffset = 0x144;
constexpr std::size_t DamageActiveHitClassOffset = 0x148;
constexpr std::size_t DamageConversionTypeOffset = 0x149;
constexpr std::size_t DamageConversionPercentOffset = 0x14C;
constexpr std::size_t DamageOverlayOffset = 0x150;

constexpr std::size_t UnitTypeOffset = 0x000;
constexpr std::size_t UnitClassIdOffset = 0x004;
constexpr std::size_t UnitGuidOffset = 0x008;
constexpr std::size_t UnitPathOffset = 0x038;
constexpr std::size_t UnitCombatListOffset = 0x108;

constexpr std::size_t SkillRecordOffset = 0x000;
constexpr std::size_t SkillsIdOffset = 0x000;
constexpr std::size_t ItemsCodeOffset = 0x080;
constexpr std::size_t ItemsNormalCodeOffset = 0x084;

constexpr std::size_t CombatAttackerTypeOffset = 0x008;
constexpr std::size_t CombatAttackerGuidOffset = 0x00C;
constexpr std::size_t CombatDefenderTypeOffset = 0x010;
constexpr std::size_t CombatDefenderGuidOffset = 0x014;
constexpr std::size_t CombatNextOffset = 0x198;

constexpr std::uintptr_t FillDamageValuesRva = 0x44C030;
constexpr std::uintptr_t AllocateCombatRecordRva = 0x4507B0;
constexpr std::uintptr_t ConsumeCombatRecordRva = 0x44B2B0;
constexpr std::uintptr_t ExecuteEventsRva = 0x44CE80;
constexpr std::uintptr_t EventFunc7Rva = 0x583E00;
constexpr std::uintptr_t EventFunc9Rva = 0x583400;
constexpr std::uintptr_t EventFunc14Rva = 0x583580;
constexpr std::uintptr_t EventFunc15Rva = 0x584170;
constexpr std::uintptr_t EventFunc16Rva = 0x583150;
constexpr std::uintptr_t EventFunc19Rva = 0x5849D0;
constexpr std::uintptr_t EventFunc20Rva = 0x583B30;
constexpr std::uintptr_t EventFunc21Rva = 0x5837F0;

constexpr std::uintptr_t ApplyDamageBonusesRva = 0x44D710;
constexpr std::uintptr_t ApplyDamageEventRva = 0x44D570;
constexpr std::uintptr_t PreCriticalCallRva = 0x44C2AC;
constexpr std::uintptr_t PreEvent3CallRva = 0x44CB9E;
constexpr std::uintptr_t PreConversionRva = 0x44CBA3;
constexpr std::uintptr_t SourceRollRva = 0x4501E0;
constexpr std::array<std::uintptr_t, 5> SourceRollCalls{
    0x44C611, 0x44C679, 0x44C6E1, 0x44C749, 0x44CA47};
constexpr std::array<std::size_t, 5> SourceComponentOffsets{
    DamageFireOffset, DamageLightningOffset, DamageColdOffset,
    DamageMagicOffset, DamagePoisonOffset};
constexpr std::uintptr_t CopyDamageRva = 0x4494B0;
constexpr std::uintptr_t DestroyDamageRva = 0x4496E0;
constexpr std::uintptr_t EnumerateUnitsRva = 0x4398B0;
constexpr std::uintptr_t CanDamageTargetRva = 0x48E060;
constexpr std::uintptr_t GetRoomRva = 0x34B440;
constexpr std::uintptr_t GetUsedSkillRva = 0x34B720;
constexpr std::uintptr_t ResolveActiveWeaponRva = 0x4242B0;
constexpr std::uintptr_t GetDataContextFromUnitRva = 0x34A0E0;
constexpr std::uintptr_t GetItemsRecordRva = 0x314110;
constexpr std::uintptr_t GetServerUnitRva = 0x48FE80;
constexpr std::uintptr_t GetLayeredStatRva = 0x2F5C60;
constexpr std::uintptr_t GetUnitStatRva = 0x2F5020;
constexpr std::uintptr_t CheckStateRva = 0x3351B0;
constexpr std::uintptr_t CalculateTotalDamageRva = 0x44DF10;
constexpr std::uintptr_t FinalizeDamageRva = 0x44A9B0;
constexpr std::uintptr_t GetSeedRva = 0x34A1E0;
constexpr std::uintptr_t RollLimitedRandomRva = 0x153B00;
constexpr std::uintptr_t GetWeaponMasteryChanceRva = 0x33D4F0;
constexpr std::uintptr_t GetPathXRva = 0x341A20;
constexpr std::uintptr_t GetPathYRva = 0x341A30;
constexpr std::uintptr_t PlayerSkillPositionInputRva = 0x4FDB40;
constexpr std::uintptr_t PlayerSkillUnitInputRva = 0x4F8DE0;
constexpr std::uintptr_t ActiveSkillLayoutWitnessRva = 0x33DBA0;
constexpr std::uint64_t RecentSkillInputLifetimeMs = 5000;
constexpr std::size_t MaximumRecentSkillInputs = 128;

constexpr std::uintptr_t DirectMeleeFillContextRva = 0x43009E;
constexpr std::uintptr_t DirectMeleeFillReturnRva = 0x4300BB;
constexpr std::uintptr_t QueuedMeleeFillReturnRva = 0x44B6A0;
constexpr std::uintptr_t AllocateMeleeReturnRva = 0x44B708;
constexpr std::uintptr_t ExecuteMeleeReturnRva = 0x44B3FF;
constexpr std::uintptr_t PreEvent3ReturnRva = 0x44CBA3;

constexpr auto FillExpected = std::to_array<std::uint8_t>({
    0x40,0x56,0x57,0x41,0x54,0x48,0x81,0xEC,0x10,0x05,0x00,0x00,0x48,0x8B,0x05,
    0x85,0xF2,0x57,0x02,0x48,0x33,0xC4,0x48,0x89,0x84,0x24,0xD0,0x04,0x00,0x00,
});
constexpr auto PlayerSkillPositionInputExpected = std::to_array<std::uint8_t>({
    0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x6C,0x24,0x10,0x48,0x89,0x74,0x24,0x18,
    0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x83,0xEC,0x50,0x48,0x8B,
    0xB4,0x24,0xA8,0x00,0x00,0x00,
});
constexpr auto PlayerSkillUnitInputExpected = std::to_array<std::uint8_t>({
    0x40,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,
    0xAC,0x24,0x28,0xFE,0xFF,0xFF,0x48,0x81,0xEC,0xD8,0x02,0x00,0x00,
});
constexpr auto ActiveSkillLayoutWitnessExpected = std::to_array<std::uint8_t>({
    0x48,0x85,0xC9,0x74,0x0E,0x48,0x8B,0x01,0x66,0x83,0x38,0x00,0xB8,0xFF,0xFF,
    0xFF,0xFF,0x75,0x03,0x8B,0x41,0x30,0xC3,0xCC,
});
constexpr auto DirectMeleeFillContextExpected = std::to_array<std::uint8_t>({
    0x4C,0x8D,0x4C,0x24,0x70,0xC6,0x44,0x24,0x28,0x80,0x4C,0x8B,0xC3,0x44,0x89,
    0x7C,0x24,0x20,0x48,0x8B,0xD6,0x49,0x8B,0xCE,0xE8,0x75,0xBF,0x01,0x00,
});
constexpr auto AllocateExpected = std::to_array<std::uint8_t>({
    0x40,0x55,0x57,0x41,0x56,0x41,0x57,0x48,0x81,0xEC,0xC8,0x01,0x00,0x00,0x48,
    0x8B,0x05,0x03,0xAB,0x57,0x02,0x48,0x33,0xC4,0x48,0x89,0x84,0x24,0xB0,0x01,
    0x00,0x00,
});
constexpr auto ConsumeExpected = std::to_array<std::uint8_t>({
    0x40,0x53,0x55,0x56,0x57,0x41,0x54,0x41,0x56,0x48,0x81,0xEC,0xC8,0x01,0x00,
    0x00,0x48,0x8B,0x05,0x01,0x00,0x58,0x02,0x48,0x33,0xC4,0x48,0x89,0x84,0x24,
    0xB0,0x01,0x00,0x00,
});
constexpr auto ExecuteExpected = std::to_array<std::uint8_t>({
    0x40,0x55,0x53,0x56,0x57,0x41,0x56,0x41,0x57,0x48,0x8D,0xAC,0x24,0xE8,0xFE,
    0xFF,0xFF,0x48,0x81,0xEC,0x18,0x02,0x00,0x00,0x48,0x8B,0x05,0x29,0xE4,0x57,
    0x02,
});
constexpr auto Event7Expected = std::to_array<std::uint8_t>({
    0x48,0x89,0x6C,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x48,0x89,0x7C,0x24,0x20,
    0x41,0x56,0x48,0x83,0xEC,0x20,0x49,0x8B,0xF1,0x49,0x8B,0xE8,0x4C,0x8B,0xF1,
});
constexpr auto Event9Expected = std::to_array<std::uint8_t>({
    0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x6C,0x24,0x10,0x48,0x89,0x74,0x24,0x18,
    0x57,0x41,0x56,0x41,0x57,0x48,0x83,0xEC,0x20,0x49,0x8B,0xE9,0x49,0x8B,0xF0,
});
constexpr auto Event14Expected = std::to_array<std::uint8_t>({
    0x40,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x56,0x41,0x57,0x48,0x8D,0xAC,0x24,
    0x40,0xFF,0xFF,0xFF,0x48,0x81,0xEC,0xC0,0x01,0x00,0x00,0x48,0x8B,0x05,0x27,
    0x7D,0x44,0x02,
});
constexpr auto Event15Expected = std::to_array<std::uint8_t>({
    0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x6C,0x24,0x10,0x48,0x89,0x74,0x24,0x18,
    0x57,0x48,0x83,0xEC,0x50,0x49,0x8B,0xF9,0x49,
});
constexpr auto Event16Expected = std::to_array<std::uint8_t>({
    0x48,0x89,0x6C,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x41,0x56,0x41,0x57,
    0x48,0x83,0xEC,0x60,0x49,0x8B,0xF1,0x49,0x8B,0xF8,0x44,0x8B,0xFA,0x4C,0x8B,
    0xF1,
});
constexpr auto Event19Expected = std::to_array<std::uint8_t>({
    0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x6C,0x24,0x10,0x48,0x89,0x74,0x24,0x18,
    0x57,0x41,0x56,0x41,0x57,0x48,0x83,0xEC,0x60,0x49,0x8B,0xF1,0x49,0x8B,0xE8,
    0x4C,0x8B,0xF1,
});
constexpr auto Event20Expected = std::to_array<std::uint8_t>({
    0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x6C,0x24,0x10,0x48,0x89,0x74,0x24,0x18,
    0x48,0x89,0x7C,0x24,0x20,0x41,0x54,0x41,0x56,0x41,0x57,0x48,0x83,0xEC,0x40,
    0x49,0x8B,0xF1,
});
constexpr auto Event21Expected = std::to_array<std::uint8_t>({
    0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x6C,0x24,0x10,0x56,0x57,0x41,0x54,0x41,
    0x56,0x41,0x57,0x48,0x83,0xEC,0x40,0x4D,0x8B,0xF1,0x49,0x8B,0xF8,0x4C,0x8B,
    0xF9,
});
constexpr auto PreCriticalContextExpected = std::to_array<std::uint8_t>({
    0xE8,0x5F,0x14,0x00,0x00,0x89,0x47,0x18,0x45,0x85,0xF6,0x75,0x67,0x48,0x8B,
    0xCE,0xE8,0xEF,0x7F,0xFD,0xFF,
});
constexpr auto PreEvent3ContextExpected = std::to_array<std::uint8_t>({
    0x48,0x8B,0x4C,0x24,0x68,0x4C,0x8B,0xCE,0x4D,0x8B,0xC4,0x48,0x89,0x7C,0x24,
    0x20,0xBA,0x03,0x00,0x00,0x00,0xE8,0xCD,0x09,0x00,0x00,0x44,0x0F,0xB6,0x97,
    0x49,0x01,0x00,0x00,
});
constexpr auto PreCriticalCallExpected = std::to_array<std::uint8_t>({
    0xE8,0x5F,0x14,0x00,0x00,
});
constexpr auto PreEvent3CallExpected = std::to_array<std::uint8_t>({
    0xE8,0xCD,0x09,0x00,0x00,
});
constexpr auto PreConversionExpected = std::to_array<std::uint8_t>({
    0x44,0x0F,0xB6,0x97,0x49,0x01,0x00,0x00,
    0x4C,0x8B,0xBC,0x24,0xE8,0x04,0x00,0x00,
    0x45,0x84,0xD2,0x0F,0x84,0xC3,0x01,0x00,0x00,
    0x4C,0x63,0x47,0x18,0x4C,0x63,0x8F});
constexpr std::array<std::array<std::uint8_t, 32>, 4> SourceRollContexts{{
    {0x44,0x8B,0xC8,0x89,0x6C,0x24,0x28,0x45,0x8B,0xC6,0x89,0x44,0x24,0x20,0x8B,0xD3,0x48,0x8B,0xCE,0xE8,0xCA,0x3B,0x00,0x00,0x8B,0xE8,0x45,0x33,0xC0,0x89,0x6F,0x20},
    {0x44,0x8B,0xC8,0x89,0x6C,0x24,0x28,0x45,0x8B,0xC6,0x89,0x44,0x24,0x20,0x8B,0xD3,0x48,0x8B,0xCE,0xE8,0x62,0x3B,0x00,0x00,0x8B,0xE8,0x45,0x33,0xC0,0x89,0x6F,0x2C},
    {0x44,0x8B,0xC8,0x89,0x6C,0x24,0x28,0x45,0x8B,0xC6,0x89,0x44,0x24,0x20,0x8B,0xD3,0x48,0x8B,0xCE,0xE8,0xFA,0x3A,0x00,0x00,0x8B,0xE8,0x45,0x33,0xC0,0x89,0x6F,0x34},
    {0x44,0x8B,0xC8,0x89,0x6C,0x24,0x28,0x45,0x8B,0xC6,0x89,0x44,0x24,0x20,0x8B,0xD3,0x48,0x8B,0xCE,0xE8,0x92,0x3A,0x00,0x00,0x8B,0xE8,0x48,0x8B,0xCE,0x89,0x6F,0x30},
}};
constexpr auto PoisonRollContext = std::to_array<std::uint8_t>({
    0x8B,0x4F,0x38,0x44,0x8B,0xC8,0x89,0x4C,0x24,0x28,0x44,0x8B,0xC3,0x48,0x8B,0xCE,0x41,0x0F,0xB6,0xED,0x41,0x8B,0xD6,0x89,0x44,0x24,0x20,0xE8,0x94,0x37,0x00,0x00,0x4C,0x8B,0xAC,0x24,0xF8,0x04,0x00,0x00,0x0F,0xAF,0xC5,0x99,0x83,0xE2,0x7F,0x03,0xC2,0xC1,0xF8,0x07,0x89,0x47,0x38});
extern "C" void MeleeSplashPreConversionRelay();
using SourceRollFn = std::int32_t(__fastcall*)(
    void*, std::int32_t, std::int32_t, std::int32_t,
    std::int32_t, std::int32_t) noexcept;

using FillDamageFn = void(__fastcall*)(
    void*, void*, void*, void*, std::int32_t, std::uint8_t) noexcept;
using AllocateCombatRecordFn = void(__fastcall*)(
    void*, void*, void*, const void*) noexcept;
using ConsumeCombatRecordFn = std::uint8_t(__fastcall*)(
    void*, void*, void*, std::int32_t) noexcept;
using ExecuteEventsFn = void(__fastcall*)(
    void*, void*, void*, std::int32_t, void*) noexcept;
using EventFunctionFn = std::int32_t(__fastcall*)(
    void*, std::int32_t, void*, void*, void*, std::int32_t, std::int32_t,
    std::int32_t, void*) noexcept;
using ApplyDamageBonusesFn = std::int32_t(__fastcall*)(
    void*, std::int32_t, void*, std::int32_t, std::int32_t, std::int32_t,
    std::int32_t, std::uint8_t) noexcept;
using ApplyDamageEventFn = std::int32_t(__fastcall*)(
    void*, std::int32_t, void*, void*, void*) noexcept;
using DamageStageFn = void(__fastcall*)(void*, void*, void*, void*) noexcept;
using CopyDamageFn = void*(__fastcall*)(void*, const void*) noexcept;
using DestroyDamageFn = void(__fastcall*)(void*) noexcept;
using AreaCallbackFn = std::int32_t(__fastcall*)(void*, void*) noexcept;
using EnumerateUnitsFn = void(__fastcall*)(
    void*, void*, void*, std::int32_t, std::uint32_t, AreaCallbackFn, void*,
    std::int32_t, const char*, std::int32_t) noexcept;
using CanDamageTargetFn = std::int32_t(__fastcall*)(void*, void*, void*) noexcept;
using GetUnitPointerFn = void*(__fastcall*)(void*) noexcept;
using GetDataContextFromUnitFn = std::uint8_t(__fastcall*)(void*) noexcept;
using GetItemsRecordFn = void*(__fastcall*)(std::uint8_t, std::uint32_t) noexcept;
using GetServerUnitFn = void*(__fastcall*)(
    void*, std::int32_t, std::uint32_t) noexcept;
using GetStatFn = std::int32_t(__fastcall*)(
    void*, std::int32_t, std::uint16_t) noexcept;
using CheckStateFn = std::int32_t(__fastcall*)(void*, std::int32_t) noexcept;
using GetSeedFn = NativeSeedPair*(__fastcall*)(void*) noexcept;
using RollLimitedRandomFn = std::uint32_t(__fastcall*)(
    NativeSeedPair*, std::int32_t) noexcept;
using GetWeaponMasteryChanceFn = std::int32_t(__fastcall*)(
    void*, void*, std::int32_t, std::int32_t) noexcept;
using GetPathCoordinateFn = std::uint16_t(__fastcall*)(void*) noexcept;
using PlayerSkillPositionInputFn = std::int32_t(__fastcall*)(
    void*, void*, std::int32_t, std::int32_t, std::int32_t, void*) noexcept;
using PlayerSkillUnitInputFn = std::int32_t(__fastcall*)(
    void*, void*, std::int32_t, std::int32_t, void*, std::int32_t,
    std::int32_t) noexcept;

const D2RL::PluginContext* Context{};
std::uint8_t* Base{};
Config ActiveConfig{};
std::filesystem::path ActiveConfigPath;

FillDamageFn OriginalFill{};
AllocateCombatRecordFn OriginalAllocate{};
ConsumeCombatRecordFn OriginalConsume{};
ExecuteEventsFn OriginalExecute{};
EventFunctionFn OriginalEvent7{};
EventFunctionFn OriginalEvent9{};
EventFunctionFn OriginalEvent14{};
EventFunctionFn OriginalEvent15{};
EventFunctionFn OriginalEvent16{};
EventFunctionFn OriginalEvent19{};
EventFunctionFn OriginalEvent20{};
EventFunctionFn OriginalEvent21{};
PlayerSkillPositionInputFn OriginalPlayerSkillPositionInput{};
PlayerSkillUnitInputFn OriginalPlayerSkillUnitInput{};

ApplyDamageBonusesFn ApplyDamageBonuses{};
ApplyDamageEventFn ApplyDamageEvent{};
DamageStageFn CalculateTotalDamage{};
DamageStageFn FinalizeDamage{};
CopyDamageFn CopyDamage{};
DestroyDamageFn DestroyDamage{};
EnumerateUnitsFn EnumerateUnits{};
CanDamageTargetFn CanDamageTarget{};
GetUnitPointerFn GetRoom{};
GetUnitPointerFn GetUsedSkill{};
GetUnitPointerFn ResolveActiveWeapon{};
GetDataContextFromUnitFn GetDataContextFromUnit{};
GetItemsRecordFn GetItemsRecord{};
GetServerUnitFn GetServerUnit{};
GetStatFn GetLayeredStat{};
GetStatFn GetUnitStat{};
CheckStateFn CheckState{};
GetSeedFn GetSeed{};
RollLimitedRandomFn RollLimitedRandom{};
GetWeaponMasteryChanceFn GetWeaponMasteryChance{};
GetPathCoordinateFn GetPathX{};
GetPathCoordinateFn GetPathY{};

std::atomic<bool> Operational{};
std::atomic<bool> AnyMutationInstalled{};
std::atomic<std::uint64_t> Sequence{};
std::atomic<std::uint64_t> Captures{};
std::atomic<std::uint64_t> Bursts{};
std::atomic<std::uint64_t> SecondaryHits{};
std::atomic<std::uint64_t> RejectedHits{};
std::atomic<std::uint64_t> RejectedTargets{};
std::atomic<std::uint64_t> DroppedSidecars{};
std::atomic<std::uint32_t> FillDiagnosticLines{};
std::atomic<std::uint32_t> SkillPolicyDiagnosticLines{};
std::atomic<std::uint32_t> SkillInputDiagnosticLines{};
std::atomic<const BKVCombatInterop::ApiV1*> BKVCombatApi{};
std::atomic<bool> BKVCombatIncompatibleLogged{};

constexpr std::uint32_t MaximumFillDiagnosticLines = 64;

struct RecentSkillInput {
    void* game{};
    UnitIdentity attacker{};
    UnitIdentity target{};
    std::int32_t skillId{};
    std::uint64_t observedAtMs{};
    bool hasTarget{};
};

std::mutex RecentSkillInputsMutex;
std::vector<RecentSkillInput> RecentSkillInputs;

void* RelayPage{};
constexpr std::size_t RelayStride = 16;
constexpr std::size_t RelayCount = 8;
constexpr std::size_t RelayBytes = RelayStride * RelayCount;

template <typename Fn>
Fn At(std::uintptr_t rva) noexcept {
    return reinterpret_cast<Fn>(Base + rva);
}

template <typename T>
T Read(const void* object, std::size_t offset) noexcept {
    T value{};
    std::memcpy(
        &value,
        static_cast<const std::byte*>(object) + offset,
        sizeof(value));
    return value;
}

template <typename T>
void Write(void* object, std::size_t offset, T value) noexcept {
    std::memcpy(
        static_cast<std::byte*>(object) + offset,
        &value,
        sizeof(value));
}

auto Identity(const void* unit) noexcept -> UnitIdentity {
    if (!unit) return {};
    return {
        .type = Read<std::int32_t>(unit, UnitTypeOffset),
        .guid = Read<std::uint32_t>(unit, UnitGuidOffset),
    };
}

bool IsSupportedMeleeFillReturn(std::uintptr_t returnRva) noexcept {
    return returnRva == DirectMeleeFillReturnRva
        || returnRva == QueuedMeleeFillReturnRva;
}

void DiagnosticLog(const char* format, ...) noexcept {
    if (!ActiveConfig.diagnosticLogging || !Context) return;
    char message[1024]{};
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    Context->LogInfo(message);
}

const char* CriticalOutcomeName(
        Native92777CriticalOutcome outcome) noexcept {
    switch (outcome) {
    case Native92777CriticalOutcome::WeaponMastery: return "weaponMastery";
    case Native92777CriticalOutcome::PassiveCritical: return "critical";
    case Native92777CriticalOutcome::DeadlyStrike: return "deadly";
    case Native92777CriticalOutcome::None: return "none";
    }
    return "none";
}

class OwnedDamage final {
public:
    OwnedDamage() = default;
    OwnedDamage(const OwnedDamage&) = delete;
    auto operator=(const OwnedDamage&) -> OwnedDamage& = delete;
    ~OwnedDamage() noexcept { Reset(); }

    static auto Copy(const void* source) -> std::unique_ptr<OwnedDamage> {
        if (!source || !CopyDamage) return {};
        auto result = std::make_unique<OwnedDamage>();
        result->bytes_ = static_cast<std::byte*>(
            _aligned_malloc(DamageSize, 16));
        if (!result->bytes_) throw std::bad_alloc{};
        CopyDamage(result->bytes_, source);
        result->constructed_ = true;
        return result;
    }

    auto Get() noexcept -> void* { return bytes_; }
    auto Get() const noexcept -> const void* { return bytes_; }

private:
    void Reset() noexcept {
        if (constructed_ && DestroyDamage) DestroyDamage(bytes_);
        if (bytes_) _aligned_free(bytes_);
        bytes_ = nullptr;
        constructed_ = false;
    }

    std::byte* bytes_{};
    bool constructed_{};
};

struct AreaDescriptor {
    void* room{};
    std::int32_t x{};
    std::int32_t y{};
};
static_assert(sizeof(AreaDescriptor) == 0x10);

using PoisonEntry = PoisonSourceEntry;
static_assert(sizeof(PoisonEntry) == 12);

struct CaptureMetadata {
    UnitIdentity attacker{};
    UnitIdentity primary{};
    std::int32_t skillId{};
    std::int32_t damageMode{};
    std::uint32_t weaponClassId{UINT32_MAX};
    std::int32_t baseRadiusTiles{};
    std::int32_t radiusBonusTiles{};
    std::int32_t radiusTiles{};
    std::int32_t splashPercent{};
    bool exceptionalOrEliteWeapon{};
    bool gateRequired{};
    bool gateSatisfied{};
    bool primaryCriticalDeadly{};
};

struct FillFrame {
    FillFrame* previous{};
    void* game{};
    void* attacker{};
    void* defender{};
    void* damage{};
    bool candidate{};
    bool preCriticalSeen{};
    bool event3Seen{};
    bool gateSeen{};
    bool primaryCriticalDeadly{};
    bool preConversionSeen{};
    bool weaponSourceOnly{};
    bool sourceCaptureValid{true};
    std::uint8_t sourceDamage{128};
    std::uint8_t sourceRollSeen{};
    std::array<std::int32_t, 5> sourceComponents{};
    std::uint64_t poisonPrefixCount{};
    std::int32_t sourcePoisonLength{};
    std::int32_t sourceColdLength{};
    std::int32_t preCriticalPhysical{};
    std::uint16_t preCriticalFlags{};
    std::unique_ptr<OwnedDamage> normalizedDamage;
};
bool MaterializeWeaponSource(const FillFrame& frame, void* damage) noexcept;

struct PendingCapture {
    void* gameToken{};
    const void* sourceDamage{};
    std::uint32_t threadId{};
    std::uint64_t sequence{};
    CaptureMetadata metadata{};
    std::unique_ptr<OwnedDamage> damage;
};

struct CombatSidecar {
    void* gameToken{};
    void* node{};
    std::uint32_t threadId{};
    std::uint64_t sequence{};
    CaptureMetadata metadata{};
    std::unique_ptr<OwnedDamage> damage;
};

struct PrimaryFrame {
    CombatSidecar* sidecar{};
    void* game{};
    void* attacker{};
    void* defender{};
    void* damage{};
    bool gateSeen{};
    bool primaryHitSucceeded{};
};

struct SecondaryDiagnostics {
    std::uint32_t openWoundsCalls{};
    std::uint32_t crushingBlowCalls{};
    std::int32_t lastOpenWoundsResult{};
    std::int32_t lastCrushingBlowResult{};
};

struct CollectionContext {
    UnitIdentity attacker{};
    UnitIdentity primary{};
    std::vector<UnitIdentity>* targets{};
    bool failed{};
};

std::mutex SidecarMutex;
std::vector<std::unique_ptr<CombatSidecar>> Sidecars;

thread_local FillFrame* ActiveFillFrame{};
thread_local PrimaryFrame* ActivePrimaryFrame{};
thread_local SecondaryDiagnostics* ActiveSecondaryDiagnostics{};
thread_local std::vector<PendingCapture> PendingCaptures;
thread_local std::uint32_t SecondaryDepth{};

class FillFrameScope final {
public:
    explicit FillFrameScope(FillFrame* frame) noexcept
        : frame_(frame) {
        frame_->previous = ActiveFillFrame;
        ActiveFillFrame = frame_;
    }
    ~FillFrameScope() noexcept { ActiveFillFrame = frame_->previous; }
    FillFrameScope(const FillFrameScope&) = delete;
    auto operator=(const FillFrameScope&) -> FillFrameScope& = delete;
private:
    FillFrame* frame_{};
};

class PrimaryFrameScope final {
public:
    explicit PrimaryFrameScope(PrimaryFrame* frame) noexcept
        : previous_(ActivePrimaryFrame) {
        ActivePrimaryFrame = frame;
    }
    ~PrimaryFrameScope() noexcept { ActivePrimaryFrame = previous_; }
    PrimaryFrameScope(const PrimaryFrameScope&) = delete;
    auto operator=(const PrimaryFrameScope&) -> PrimaryFrameScope& = delete;
private:
    PrimaryFrame* previous_{};
};

class SecondaryScope final {
public:
    SecondaryScope() noexcept { ++SecondaryDepth; }
    ~SecondaryScope() noexcept { --SecondaryDepth; }
    SecondaryScope(const SecondaryScope&) = delete;
    auto operator=(const SecondaryScope&) -> SecondaryScope& = delete;
};

class SecondaryDiagnosticsScope final {
public:
    explicit SecondaryDiagnosticsScope(
            SecondaryDiagnostics* diagnostics) noexcept
        : previous_(ActiveSecondaryDiagnostics) {
        ActiveSecondaryDiagnostics = diagnostics;
    }
    ~SecondaryDiagnosticsScope() noexcept {
        ActiveSecondaryDiagnostics = previous_;
    }
    SecondaryDiagnosticsScope(const SecondaryDiagnosticsScope&) = delete;
    auto operator=(const SecondaryDiagnosticsScope&)
        -> SecondaryDiagnosticsScope& = delete;
private:
    SecondaryDiagnostics* previous_{};
};

auto SkillIdFromActualHit(void* attacker) noexcept -> std::int32_t {
    if (!attacker || !GetUsedSkill) return NormalAttackSkillId;
    const auto* skill = GetUsedSkill(attacker);
    if (!skill) return NormalAttackSkillId;
    const auto* record = Read<void*>(skill, SkillRecordOffset);
    if (!record) return NormalAttackSkillId;
    return static_cast<std::int32_t>(
        Read<std::int16_t>(record, SkillsIdOffset));
}

auto SkillIdFromActiveInput(void* activeSkill) noexcept -> std::int32_t {
    if (!activeSkill) return -1;
    const auto* skill = Read<void*>(activeSkill, SkillRecordOffset);
    if (!skill) return -1;
    return static_cast<std::int32_t>(Read<std::uint16_t>(skill, SkillsIdOffset));
}

void RememberSkillInput(
        void* game,
        void* attacker,
        std::int32_t skillId,
        UnitIdentity target,
        bool hasTarget) noexcept {
    if (!game || !attacker || Identity(attacker).type != UnitPlayer) return;
    try {
        const auto attackerId = Identity(attacker);
        const auto now = static_cast<std::uint64_t>(GetTickCount64());
        std::scoped_lock lock(RecentSkillInputsMutex);
        std::erase_if(RecentSkillInputs, [game, attackerId](const auto& entry) {
            return entry.game == game && entry.attacker == attackerId;
        });
        if (RecentSkillInputs.size() >= MaximumRecentSkillInputs) {
            RecentSkillInputs.erase(RecentSkillInputs.begin());
        }
        RecentSkillInputs.push_back({
            .game = game,
            .attacker = attackerId,
            .target = target,
            .skillId = skillId < 0 ? NormalAttackSkillId : skillId,
            .observedAtMs = now,
            .hasTarget = hasTarget,
        });
        if (SkillInputDiagnosticLines.fetch_add(
                    1, std::memory_order_relaxed) < MaximumFillDiagnosticLines) {
            DiagnosticLog(
                "MeleeSplash skill input skill=%d target=%s%d/%u.",
                skillId < 0 ? NormalAttackSkillId : skillId,
                hasTarget ? "unit:" : "position:",
                hasTarget ? target.type : 0,
                hasTarget ? target.guid : 0);
        }
    } catch (...) {
        DiagnosticLog("MeleeSplash skill input context could not be stored.");
    }
}

auto RecentSkillIdForHit(
        void* game, UnitIdentity attacker, UnitIdentity primary)
        noexcept -> std::optional<std::int32_t> {
    const auto now = static_cast<std::uint64_t>(GetTickCount64());
    try {
        std::scoped_lock lock(RecentSkillInputsMutex);
        std::erase_if(RecentSkillInputs, [now](const auto& entry) {
            return now < entry.observedAtMs
                || now - entry.observedAtMs > RecentSkillInputLifetimeMs;
        });
        for (auto entry = RecentSkillInputs.rbegin();
                entry != RecentSkillInputs.rend(); ++entry) {
            if (entry->game != game || entry->attacker != attacker
                    || (entry->hasTarget && entry->target != primary)) {
                continue;
            }
            return entry->skillId;
        }
    } catch (...) {
    }
    return std::nullopt;
}

struct SkillIdResolution {
    std::int32_t id{};
    const char* source{};
};

auto ResolveSkillIdFromActualHit(
        void* game, void* attacker, void* primary) noexcept -> SkillIdResolution {
    const auto nativeSkillId = SkillIdFromActualHit(attacker);
    if (nativeSkillId != NormalAttackSkillId) {
        return {.id = nativeSkillId, .source = "native"};
    }
    if (const auto inputSkillId = RecentSkillIdForHit(
            game, Identity(attacker), Identity(primary))) {
        return {.id = *inputSkillId, .source = "input"};
    }
    return {.id = nativeSkillId, .source = "native"};
}

auto IsExceptionalOrEliteWeapon(void* weapon) noexcept -> bool {
    if (!weapon || !GetDataContextFromUnit || !GetItemsRecord) {
        return false;
    }
    const auto classId = Read<std::uint32_t>(weapon, UnitClassIdOffset);
    auto* record = GetItemsRecord(GetDataContextFromUnit(weapon), classId);
    if (!record) return false;
    return Read<std::uint32_t>(record, ItemsCodeOffset)
        != Read<std::uint32_t>(record, ItemsNormalCodeOffset);
}

auto BuildMetadata(
        void* game,
        void* attacker,
        void* primary,
        std::int32_t damageMode)
        -> std::unique_ptr<CaptureMetadata> {
    if (!attacker || !primary || Identity(attacker).type != UnitPlayer
            || Identity(primary).type != UnitMonster) {
        return {};
    }
    const auto skillResolution = ResolveSkillIdFromActualHit(
        game, attacker, primary);
    const auto skillId = skillResolution.id;
    const auto skillEnabled = IsSkillEnabled(ActiveConfig, skillId);
    const auto logSkillPolicy = SkillPolicyDiagnosticLines.fetch_add(
        1, std::memory_order_relaxed) < MaximumFillDiagnosticLines;
    if (logSkillPolicy) {
        DiagnosticLog(
            "MeleeSplash skill policy skill=%d source=%s enabled=%s mode=%s.",
            skillId,
            skillResolution.source,
            skillEnabled ? "true" : "false",
            ActivationModeName(ActiveConfig.activationMode));
    }
    if (!skillEnabled) return {};

    auto metadata = std::make_unique<CaptureMetadata>();
    metadata->attacker = Identity(attacker);
    metadata->primary = Identity(primary);
    metadata->skillId = skillId;
    metadata->damageMode = damageMode;
    auto* weapon = ResolveActiveWeapon ? ResolveActiveWeapon(attacker) : nullptr;
    if (weapon) {
        metadata->weaponClassId = Read<std::uint32_t>(
            weapon, UnitClassIdOffset);
    }
    metadata->exceptionalOrEliteWeapon = IsExceptionalOrEliteWeapon(weapon);
    metadata->gateRequired = RequiresGateStat(ActiveConfig, skillId);
    metadata->gateSatisfied = !metadata->gateRequired;
    if (metadata->gateRequired && ActiveConfig.gateStatId >= 0) {
        metadata->gateSatisfied =
            GetUnitStat(attacker, ActiveConfig.gateStatId, 0) > 0;
    }

    const auto radiusPercent = ActiveConfig.increasedRadiusStatId >= 0
        ? GetUnitStat(attacker, ActiveConfig.increasedRadiusStatId, 0)
        : 0;
    const auto damagePercent = ActiveConfig.splashDamagePercentStatId >= 0
        ? GetUnitStat(attacker, ActiveConfig.splashDamagePercentStatId, 0)
        : 0;
    metadata->baseRadiusTiles = metadata->exceptionalOrEliteWeapon
        ? ActiveConfig.baseRadiusExceptionalEliteWeapon
        : ActiveConfig.baseRadiusNormalWeapon;
    if (const auto* override = FindSkillOverride(ActiveConfig, skillId);
            override && override->baseRadiusTiles.has_value()) {
        metadata->baseRadiusTiles = *override->baseRadiusTiles;
    }
    metadata->radiusBonusTiles = std::max(radiusPercent, 0)
        / std::max(ActiveConfig.radiusPercentPerTile, 1);
    metadata->radiusTiles = ClampNativeRadius(ResolveRadiusTiles(
        ActiveConfig,
        skillId,
        metadata->exceptionalOrEliteWeapon,
        radiusPercent));
    metadata->splashPercent = ResolveSplashDamagePercent(
        ActiveConfig, skillId, damagePercent);
    const auto* charmTier = ResolveCharmTier(ActiveConfig,
        [attacker](std::int32_t statId, std::uint16_t layer) {
            return GetLayeredStat(attacker, statId, layer);
        });
    if (ActiveConfig.requireCharmTier && !charmTier) return {};
    if (charmTier) {
        metadata->baseRadiusTiles = charmTier->radiusTiles;
        metadata->radiusBonusTiles = 0;
        metadata->radiusTiles = charmTier->radiusTiles;
        metadata->splashPercent = charmTier->damagePercent;
    }
    if (logSkillPolicy) {
        DiagnosticLog(
            "MeleeSplash skill settings skill=%d radius=%d splashPercent=%d "
            "gateRequired=%s gateSatisfied=%s.",
            skillId,
            metadata->radiusTiles,
            metadata->splashPercent,
            metadata->gateRequired ? "true" : "false",
            metadata->gateSatisfied ? "true" : "false");
    }
    if (metadata->radiusTiles <= 0 || metadata->splashPercent <= 0) return {};
    return metadata;
}

bool CaptureNormalizedPacket(FillFrame& frame, void* damage) {
    if (!frame.candidate) {
        DiagnosticLog("MeleeSplash capture reject reason=frame-not-candidate.");
        return false;
    }
    if (frame.normalizedDamage) {
        DiagnosticLog("MeleeSplash capture skip reason=already-normalized.");
        return true;
    }
    if (!frame.preCriticalSeen) {
        DiagnosticLog(
            "MeleeSplash capture reject reason=precritical-call-not-observed "
            "event3=%s.",
            frame.event3Seen ? "true" : "false");
        return false;
    }
    if (frame.damage != damage || !damage) {
        DiagnosticLog(
            "MeleeSplash capture reject reason=damage-identity-mismatch "
            "expected=%p actual=%p.",
            frame.damage,
            damage);
        return false;
    }
    const auto conversionType = Read<std::uint8_t>(
        damage, DamageConversionTypeOffset);
    if (conversionType != 0 && (!ActiveConfig.enablePhysicalConversion
            || !(frame.event3Seen || frame.preConversionSeen)
            || !SupportsConversion(conversionType, Read<std::int32_t>(
                damage, DamageConversionPercentOffset)))) {
        DiagnosticLog(
            "MeleeSplash capture reject reason=damage-conversion type=%u.",
            static_cast<unsigned>(conversionType));
        return false;
    }
    if ((frame.preCriticalFlags & CriticalDeadlyResultFlag) != 0) {
        DiagnosticLog(
            "MeleeSplash capture reject reason=precritical-flag-already-set "
            "flags=0x%04X.",
            static_cast<unsigned>(frame.preCriticalFlags));
        return false;
    }
    const auto lateFlags = Read<std::uint16_t>(
        damage, DamageResultFlagsOffset);
    const auto allowedLateFlags = static_cast<std::uint16_t>(
        frame.preCriticalFlags | CriticalDeadlyResultFlag);
    if (lateFlags != frame.preCriticalFlags && lateFlags != allowedLateFlags) {
        DiagnosticLog(
            "MeleeSplash capture reject reason=unexpected-late-flags "
            "pre=0x%04X late=0x%04X allowed=0x%04X.",
            static_cast<unsigned>(frame.preCriticalFlags),
            static_cast<unsigned>(lateFlags),
            static_cast<unsigned>(allowedLateFlags));
        return false;
    }
    frame.primaryCriticalDeadly =
        (lateFlags & CriticalDeadlyResultFlag) != 0;
    auto copy = OwnedDamage::Copy(damage);
    if (!copy) {
        DiagnosticLog("MeleeSplash capture reject reason=damage-copy-failed.");
        return false;
    }
    Write(copy->Get(), DamagePhysicalOffset, frame.preCriticalPhysical);
    Write(copy->Get(), DamageResultFlagsOffset, frame.preCriticalFlags);
    if (frame.weaponSourceOnly && !MaterializeWeaponSource(frame, copy->Get())) {
        DiagnosticLog("MeleeSplash capture reject reason=unqualified-source-packet.");
        return false;
    }
    frame.normalizedDamage = std::move(copy);
    return true;
}

void PushPendingCapture(
        FillFrame& frame, std::unique_ptr<CaptureMetadata> metadata) {
    if (!frame.normalizedDamage || !metadata) return;
    metadata->gateSatisfied = metadata->gateSatisfied || frame.gateSeen;
    metadata->primaryCriticalDeadly = frame.primaryCriticalDeadly;
    std::erase_if(PendingCaptures, [&frame](const auto& pending) {
        return pending.gameToken != frame.game;
    });
    if (PendingCaptures.size() >= MaximumPendingCaptures) {
        PendingCaptures.erase(PendingCaptures.begin());
        DroppedSidecars.fetch_add(1, std::memory_order_relaxed);
    }
    const auto sequence = Sequence.fetch_add(1, std::memory_order_relaxed) + 1;
    const auto* packet = frame.normalizedDamage->Get();
    PendingCaptures.push_back({
        .gameToken = frame.game,
        .sourceDamage = frame.damage,
        .threadId = GetCurrentThreadId(),
        .sequence = sequence,
        .metadata = *metadata,
        .damage = std::move(frame.normalizedDamage),
    });
    Captures.fetch_add(1, std::memory_order_relaxed);
    DiagnosticLog(
        "MeleeSplash capture seq=%llu attacker=%d/%u primary=%d/%u skill=%d mode=%d "
        "weaponClass=%u tier=%s baseRadius=%d radiusBonus=%d radiusFinal=%d "
        "splashPercent=%d sharedPhys=%d sharedFire=%d sharedBurn=%d "
        "sharedLightning=%d sharedMagic=%d sharedCold=%d sharedPoison=%d "
        "primaryCrit=%s.",
        static_cast<unsigned long long>(sequence),
        metadata->attacker.type,
        metadata->attacker.guid,
        metadata->primary.type,
        metadata->primary.guid,
        metadata->skillId,
        metadata->damageMode,
        metadata->weaponClassId,
        metadata->exceptionalOrEliteWeapon ? "exceptionalOrElite" : "normal",
        metadata->baseRadiusTiles,
        metadata->radiusBonusTiles,
        metadata->radiusTiles,
        metadata->splashPercent,
        Read<std::int32_t>(packet, DamagePhysicalOffset),
        Read<std::int32_t>(packet, DamageFireOffset),
        Read<std::int32_t>(packet, DamageBurnOffset),
        Read<std::int32_t>(packet, DamageLightningOffset),
        Read<std::int32_t>(packet, DamageMagicOffset),
        Read<std::int32_t>(packet, DamageColdOffset),
        Read<std::int32_t>(packet, DamagePoisonOffset),
        metadata->primaryCriticalDeadly ? "yes" : "no");
}

auto TakePendingCapture(
        const void* sourceDamage,
        void* gameToken,
        UnitIdentity attacker,
        UnitIdentity defender) -> std::unique_ptr<PendingCapture> {
    for (auto iterator = PendingCaptures.rbegin();
            iterator != PendingCaptures.rend(); ++iterator) {
        if (iterator->sourceDamage != sourceDamage
                || iterator->gameToken != gameToken
                || iterator->metadata.attacker != attacker
                || iterator->metadata.primary != defender) {
            continue;
        }
        const auto index = static_cast<std::size_t>(
            std::distance(iterator, PendingCaptures.rend()) - 1);
        auto result = std::make_unique<PendingCapture>(
            std::move(PendingCaptures[index]));
        PendingCaptures.erase(PendingCaptures.begin()
            + static_cast<std::ptrdiff_t>(index));
        return result;
    }
    return {};
}

bool CombatNodeMatches(
        const void* node,
        UnitIdentity attacker,
        UnitIdentity defender) noexcept {
    return node
        && Read<std::int32_t>(node, CombatAttackerTypeOffset) == attacker.type
        && Read<std::uint32_t>(node, CombatAttackerGuidOffset) == attacker.guid
        && Read<std::int32_t>(node, CombatDefenderTypeOffset) == defender.type
        && Read<std::uint32_t>(node, CombatDefenderGuidOffset) == defender.guid;
}

auto FindCombatNode(
        void* attacker,
        UnitIdentity attackerId,
        UnitIdentity defenderId,
        const void* stopBefore = nullptr) noexcept -> void* {
    auto* node = attacker
        ? Read<void*>(attacker, UnitCombatListOffset)
        : nullptr;
    for (std::size_t index = 0;
            node && index < MaximumCombatNodes; ++index) {
        if (node == stopBefore) break;
        if (CombatNodeMatches(node, attackerId, defenderId)) return node;
        node = Read<void*>(node, CombatNextOffset);
    }
    return nullptr;
}

void StoreSidecar(
        void* node, std::unique_ptr<PendingCapture> pending) {
    if (!node || !pending || !pending->damage) return;
    auto sidecar = std::make_unique<CombatSidecar>();
    sidecar->gameToken = pending->gameToken;
    sidecar->node = node;
    sidecar->threadId = pending->threadId;
    sidecar->sequence = pending->sequence;
    sidecar->metadata = pending->metadata;
    sidecar->damage = std::move(pending->damage);
    std::scoped_lock lock(SidecarMutex);
    const auto gameToken = sidecar->gameToken;
    std::erase_if(Sidecars, [node, gameToken](const auto& existing) {
        return existing
            && (existing->node == node || existing->gameToken != gameToken);
    });
    if (Sidecars.size() >= MaximumSidecars) {
        Sidecars.erase(Sidecars.begin());
        DroppedSidecars.fetch_add(1, std::memory_order_relaxed);
    }
    Sidecars.push_back(std::move(sidecar));
}

auto TakeSidecar(
        void* node,
        void* gameToken,
        UnitIdentity attacker,
        UnitIdentity defender) noexcept -> std::unique_ptr<CombatSidecar> {
    if (!node || !gameToken) return {};
    try {
        std::scoped_lock lock(SidecarMutex);
        const auto iterator = std::find_if(
            Sidecars.begin(), Sidecars.end(),
            [node, gameToken, attacker, defender](const auto& sidecar) {
                return sidecar
                    && sidecar->node == node
                    && sidecar->gameToken == gameToken
                    && sidecar->metadata.attacker == attacker
                    && sidecar->metadata.primary == defender;
            });
        if (iterator == Sidecars.end()) return {};
        auto result = std::move(*iterator);
        Sidecars.erase(iterator);
        return result;
    } catch (...) {
        return {};
    }
}

void ReturnSidecar(std::unique_ptr<CombatSidecar> sidecar) noexcept {
    if (!sidecar) return;
    try {
        std::scoped_lock lock(SidecarMutex);
        const auto node = sidecar->node;
        const auto gameToken = sidecar->gameToken;
        std::erase_if(Sidecars, [node, gameToken](const auto& existing) {
            return existing
                && (existing->node == node || existing->gameToken != gameToken);
        });
        if (Sidecars.size() >= MaximumSidecars) {
            Sidecars.erase(Sidecars.begin());
            DroppedSidecars.fetch_add(1, std::memory_order_relaxed);
        }
        Sidecars.push_back(std::move(sidecar));
    } catch (...) {
        DroppedSidecars.fetch_add(1, std::memory_order_relaxed);
    }
}

bool IsCombatNodeLinked(void* attacker, const void* expected) noexcept {
    auto* node = attacker
        ? Read<void*>(attacker, UnitCombatListOffset)
        : nullptr;
    for (std::size_t index = 0;
            node && index < MaximumCombatNodes; ++index) {
        if (node == expected) return true;
        node = Read<void*>(node, CombatNextOffset);
    }
    return false;
}

bool ScalePoisonPacket(void* damage, std::int32_t percent) noexcept {
    const auto rawCapacity = Read<std::uint64_t>(
        damage, DamagePoisonVectorCapacityOffset);
    const auto capacity = rawCapacity & UINT64_C(0x7FFFFFFFFFFFFFFF);
    const auto count = Read<std::uint64_t>(
        damage, DamagePoisonVectorCountOffset);
    const auto inlineStorage = (rawCapacity >> 63U) != 0;
    auto* entries = Read<PoisonEntry*>(damage, DamagePoisonVectorOffset);
    if (count > capacity || count > MaximumPoisonEntries
            || capacity > MaximumPoisonEntries
            || (count > 0 && !entries)
            || (reinterpret_cast<std::uintptr_t>(entries)
                % alignof(std::uint32_t)) != 0) {
        return false;
    }
    if (inlineStorage) {
        const auto* expected = static_cast<std::byte*>(damage)
            + DamagePoisonInlineStorageOffset;
        if (capacity != 16 || entries != reinterpret_cast<const PoisonEntry*>(
                expected)) {
            return false;
        }
    }
    const auto scalar = Read<std::int32_t>(damage, DamagePoisonOffset);
    if (scalar < 0) return false;
    for (std::uint64_t index = 0; index < count; ++index) {
        if (entries[index].damage < 0 || entries[index].length < 0) return false;
    }
    const auto scaledScalar = ScaleFixedDamage(scalar, percent);
    // Native rolls aggregate and entries independently and ignores a
    // nonpositive aggregate remainder. Entry sums may exceed the scalar.
    Write(damage, DamagePoisonOffset, scaledScalar);
    for (std::uint64_t index = 0; index < count; ++index) {
        entries[index].damage = ScaleFixedDamage(entries[index].damage, percent);
    }
    return true;
}

bool MaterializeWeaponSource(const FillFrame& frame, void* damage) noexcept {
    if (!frame.sourceCaptureValid || frame.sourceDamage != 128
            || !ScalePoisonPacket(damage, 100)) return false;
    const auto count = Read<std::uint64_t>(damage, DamagePoisonVectorCountOffset);
    if (frame.poisonPrefixCount > count) return false;
    auto* entries = Read<PoisonEntry*>(damage, DamagePoisonVectorOffset);
    auto sourceCount = static_cast<std::size_t>(count);
    if (!RetainOwnerPoisonEntries(entries, sourceCount,
            static_cast<std::size_t>(frame.poisonPrefixCount))) return false;
    Write(damage, DamagePoisonVectorCountOffset, static_cast<std::uint64_t>(sourceCount));
    for (std::size_t index = 0; index < SourceComponentOffsets.size(); ++index) {
        Write(damage, SourceComponentOffsets[index], frame.sourceComponents[index]);
    }
    Write(damage, DamagePoisonLengthOffset, frame.sourcePoisonLength);
    Write(damage, DamageColdLengthOffset, frame.sourceColdLength);
    return true;
}

bool ScaleOffensivePacket(void* damage, std::int32_t percent) noexcept {
    constexpr std::array<std::size_t, 6> FixedComponents{
        DamagePhysicalOffset,
        DamageFireOffset,
        DamageBurnOffset,
        DamageLightningOffset,
        DamageMagicOffset,
        DamageColdOffset,
    };
    for (const auto offset : FixedComponents) {
        const auto value = Read<std::int32_t>(damage, offset);
        if (value < 0) return false;
    }
    if (!ScalePoisonPacket(damage, percent)) return false;
    for (const auto offset : FixedComponents) {
        Write(damage, offset, ScaleFixedDamage(
            Read<std::int32_t>(damage, offset), percent));
    }
    return true;
}

bool ApplySecondaryConversion(void* attacker, void* damage) noexcept {
    auto type = Read<std::uint8_t>(damage, DamageConversionTypeOffset);
    if (type == 0) return true;
    const auto percent = Read<std::int32_t>(damage, DamageConversionPercentOffset);
    if (!ActiveConfig.enablePhysicalConversion || !SupportsConversion(type, percent)) {
        return false;
    }
    if (type == 10) {
        auto* seed = GetSeed(attacker);
        if (!seed) return false;
        type = static_cast<std::uint8_t>(1 + RollNativePercent(*seed) % 5);
    }
    ConversionDamage values{
        .physical = Read<std::int32_t>(damage, DamagePhysicalOffset),
        .fire = Read<std::int32_t>(damage, DamageFireOffset),
        .lightning = Read<std::int32_t>(damage, DamageLightningOffset),
        .magic = Read<std::int32_t>(damage, DamageMagicOffset),
        .cold = Read<std::int32_t>(damage, DamageColdOffset),
        .poison = Read<std::int32_t>(damage, DamagePoisonOffset),
        .coldLength = Read<std::int32_t>(damage, DamageColdLengthOffset),
        .poisonLength = Read<std::int32_t>(damage, DamagePoisonLengthOffset),
        .burnLength = Read<std::int32_t>(damage, 0x028),
        .freezeLength = Read<std::int32_t>(damage, DamageFreezeLengthOffset),
    };
    if (!ConvertPhysicalDamage(values, type, percent)) return false;
    Write(damage, DamagePhysicalOffset, values.physical);
    Write(damage, DamageFireOffset, values.fire);
    Write(damage, DamageLightningOffset, values.lightning);
    Write(damage, DamageMagicOffset, values.magic);
    Write(damage, DamageColdOffset, values.cold);
    Write(damage, DamagePoisonOffset, values.poison);
    // Preserve the secondary crowd-control filter after conversion, too.
    Write(damage, DamageColdLengthOffset, std::int32_t{});
    Write(damage, DamagePoisonLengthOffset, values.poisonLength);
    Write(damage, 0x028, values.burnLength);
    Write(damage, DamageFreezeLengthOffset, std::int32_t{});
    Write(damage, DamageConversionTypeOffset, std::uint8_t{});
    Write(damage, DamageConversionPercentOffset, std::int32_t{});
    return true;
}

auto RollCriticalForTarget(
        void* attacker, void* damage, std::int32_t damageMode) noexcept
        -> Native92777CriticalOutcome {
    if (!attacker || !damage || !GetSeed || !RollLimitedRandom) {
        return Native92777CriticalOutcome::None;
    }
    auto* seed = GetSeed(attacker);
    if (!seed) return Native92777CriticalOutcome::None;
    const auto succeeds = [seed](std::int32_t chance) noexcept {
        return chance > 0
            && static_cast<std::int32_t>(RollLimitedRandom(seed, 100)) < chance;
    };

    auto* weapon = damageMode == 0 ? ResolveActiveWeapon(attacker) : nullptr;
    if (weapon && succeeds(GetWeaponMasteryChance(attacker, weapon, 0, 2))) {
        Write(damage, DamagePhysicalOffset, ApplyNative92777CriticalMultiplier(
            Read<std::int32_t>(damage, DamagePhysicalOffset),
            Native92777CriticalOutcome::WeaponMastery));
        Write(damage, DamageResultFlagsOffset, static_cast<std::uint16_t>(
            Read<std::uint16_t>(damage, DamageResultFlagsOffset)
            | CriticalDeadlyResultFlag));
        return Native92777CriticalOutcome::WeaponMastery;
    }
    if (succeeds(GetUnitStat(attacker, PassiveCriticalStatId, 0))) {
        Write(damage, DamagePhysicalOffset, ApplyNative92777CriticalMultiplier(
            Read<std::int32_t>(damage, DamagePhysicalOffset),
            Native92777CriticalOutcome::PassiveCritical));
        Write(damage, DamageResultFlagsOffset, static_cast<std::uint16_t>(
            Read<std::uint16_t>(damage, DamageResultFlagsOffset)
            | CriticalDeadlyResultFlag));
        return Native92777CriticalOutcome::PassiveCritical;
    }
    if (succeeds(GetLayeredStat(attacker, DeadlyStrikeStatId, 0))) {
        Write(damage, DamagePhysicalOffset, ApplyNative92777CriticalMultiplier(
            Read<std::int32_t>(damage, DamagePhysicalOffset),
            Native92777CriticalOutcome::DeadlyStrike));
        Write(damage, DamageResultFlagsOffset, static_cast<std::uint16_t>(
            Read<std::uint16_t>(damage, DamageResultFlagsOffset)
            | CriticalDeadlyResultFlag));
        return Native92777CriticalOutcome::DeadlyStrike;
    }
    return Native92777CriticalOutcome::None;
}

struct CriticalResolution {
    const char* outcome{"none"};
    const char* resolver{"native92777"};
};

auto ResolveBKVCombatApi() noexcept -> const BKVCombatInterop::ApiV1* {
    if (const auto* cached = BKVCombatApi.load(std::memory_order_acquire)) {
        return cached;
    }
    auto module = GetModuleHandleW(BKVCombatInterop::ModuleName);
    if (!module) module = GetModuleHandleW(BKVCombatInterop::LegacyModuleName);
    if (!module) return nullptr;
    const auto getApi = reinterpret_cast<BKVCombatInterop::GetApiFn>(
        GetProcAddress(module, BKVCombatInterop::GetApiExportName));
    const auto* api = getApi ? getApi(BKVCombatInterop::ApiVersion) : nullptr;
    if (!BKVCombatInterop::IsCompatibleApiV1(api)) {
        if (!BKVCombatIncompatibleLogged.exchange(
                true, std::memory_order_relaxed)) {
            DiagnosticLog(
                "MeleeSplash: Reimagined Combat API unavailable or incompatible; "
                "using the exact native 92777 Critical/Deadly resolver.");
        }
        return nullptr;
    }
    BKVCombatApi.store(api, std::memory_order_release);
    DiagnosticLog(
        "MeleeSplash: negotiated Reimagined Combat API v%u for optional "
        "Critical/Deadly resolution.",
        BKVCombatInterop::ApiVersion);
    return api;
}

auto ResolveCriticalForTarget(
        void* game,
        void* attacker,
        void* target,
        void* damage,
        std::int32_t damageMode) noexcept -> CriticalResolution {
    const auto* api = ResolveBKVCombatApi();
    if (BKVCombatInterop::HasActiveCriticalDeadlyResolver(api)) {
        const auto outcome = api->resolveCriticalDeadly(
            game,
            attacker,
            target,
            damage,
            damageMode,
            BKVCombatInterop::SyntheticSecondaryFlag);
        switch (outcome) {
        case BKVCombatInterop::CriticalDeadlyOutcome::None:
            return {.outcome = "none", .resolver = "Reimagined Combat"};
        case BKVCombatInterop::CriticalDeadlyOutcome::Critical:
            return {.outcome = "critical", .resolver = "Reimagined Combat"};
        case BKVCombatInterop::CriticalDeadlyOutcome::Deadly:
            return {.outcome = "deadly", .resolver = "Reimagined Combat"};
        case BKVCombatInterop::CriticalDeadlyOutcome::AlreadyResolved:
            return {.outcome = "alreadyResolved", .resolver = "Reimagined Combat"};
        case BKVCombatInterop::CriticalDeadlyOutcome::Unavailable:
            break;
        }
        DiagnosticLog(
            "MeleeSplash: Reimagined Combat Critical/Deadly resolver became "
            "unavailable; using the exact native 92777 fallback.");
    }

    const auto native = RollCriticalForTarget(attacker, damage, damageMode);
    return {
        .outcome = CriticalOutcomeName(native),
        .resolver = "native92777",
    };
}

void PrepareSyntheticRecord(void* target, void* damage) noexcept {
    Write(damage, DamageHitFlagsOffset, SyntheticHitFlags);
    // The synthetic target shares only the successful-hit decision.  Do not
    // inherit primary knockback, block/dodge/avoid/evade, weapon-block, event
    // suppression, lethal, or any other target-specific result bit.
    auto flags = ResultSuccess;
    if (!CheckState(target, UninterruptibleStateId)) flags |= ResultGetHit;
    Write(damage, DamageResultFlagsOffset, flags);
    Write(damage, DamageColdLengthOffset, std::int32_t{});
    Write(damage, DamageFreezeLengthOffset, std::int32_t{});
    Write(damage, DamageStaminaLeechOffset, std::int32_t{});
    Write(damage, DamageStunLengthOffset, std::int32_t{});
    Write(damage, DamageAbsorbedLifeOffset, std::int32_t{});
    Write(damage, DamageTotalOffset, std::int32_t{});
    Write(damage, DamageHitClassOffset, std::uint32_t{});
    Write(damage, DamageActiveHitClassOffset, std::uint8_t{});
    Write(damage, DamageOverlayOffset, std::uint32_t{});
}

void UpdateWillDie(void* target, void* damage) noexcept {
    constexpr std::array<std::size_t, 6> FixedComponents{
        DamagePhysicalOffset,
        DamageFireOffset,
        DamageLightningOffset,
        DamageMagicOffset,
        DamageColdOffset,
        DamagePoisonOffset,
    };
    std::uint32_t total{};
    for (const auto offset : FixedComponents) {
        total += static_cast<std::uint32_t>(
            Read<std::int32_t>(damage, offset));
    }
    const auto damageMasked = total & UINT32_C(0xFFFFFF00);
    const auto hitPointsMasked = static_cast<std::uint32_t>(
        GetUnitStat(target, HitPointsStatId, 0)) & UINT32_C(0xFFFFFF00);
    auto flags = Read<std::uint16_t>(damage, DamageResultFlagsOffset);
    if (static_cast<std::int32_t>(hitPointsMasked)
            < static_cast<std::int32_t>(damageMasked)) {
        flags |= ResultWillDie;
    }
    else flags = static_cast<std::uint16_t>(flags & ~ResultWillDie);
    Write(damage, DamageResultFlagsOffset, flags);
}

auto CollectAreaCandidate(void* nativeContext, void* candidate) noexcept
        -> std::int32_t {
    if (!nativeContext || !candidate) return 0;
    auto* collection = Read<CollectionContext*>(nativeContext, 0x18);
    if (!collection || !collection->targets || collection->failed) return 0;
    try {
        const auto candidateId = Identity(candidate);
        if (candidateId.type != UnitMonster) {
            RejectedTargets.fetch_add(1, std::memory_order_relaxed);
            DiagnosticLog(
                "MeleeSplash reject target=%d/%u reason=not-monster.",
                candidateId.type,
                candidateId.guid);
            return 0;
        }
        if (!AppendUniqueSecondaryTarget(
            *collection->targets,
            candidateId,
            collection->attacker,
            collection->primary)) {
            RejectedTargets.fetch_add(1, std::memory_order_relaxed);
            const auto* reason = candidateId == collection->attacker
                ? "attacker"
                : (candidateId == collection->primary
                    ? "primary"
                    : "duplicate");
            DiagnosticLog(
                "MeleeSplash reject target=%d/%u reason=%s.",
                candidateId.type,
                candidateId.guid,
                reason);
            return 0;
        }
        return 1;
    } catch (...) {
        collection->failed = true;
        return 0;
    }
}

auto CollectSecondaryTargets(
        void* game,
        void* attacker,
        void* primary,
        std::int32_t radius) -> std::vector<UnitIdentity> {
    std::vector<UnitIdentity> targets;
    if (!game || !attacker || !primary || radius <= 0) {
        DiagnosticLog("MeleeSplash reject area reason=invalid-origin-or-radius.");
        return targets;
    }
    auto* path = Read<void*>(primary, UnitPathOffset);
    auto* room = GetRoom(primary);
    if (!path || !room) {
        DiagnosticLog(
            "MeleeSplash reject area primary=%d/%u reason=missing-path-or-room.",
            Identity(primary).type,
            Identity(primary).guid);
        return targets;
    }
    AreaDescriptor descriptor{
        .room = room,
        .x = static_cast<std::int32_t>(GetPathX(path)),
        .y = static_cast<std::int32_t>(GetPathY(path)),
    };
    CollectionContext collection{
        .attacker = Identity(attacker),
        .primary = Identity(primary),
        .targets = &targets,
    };
    EnumerateUnits(
        game,
        primary,
        &descriptor,
        radius,
        StructuralAreaMask,
        CollectAreaCandidate,
        &collection,
        0,
        "MeleeSplash",
        __LINE__);
    if (collection.failed) throw std::bad_alloc{};
    std::sort(targets.begin(), targets.end(), [](const auto& left, const auto& right) {
        return left.type < right.type
            || (left.type == right.type && left.guid < right.guid);
    });
    return targets;
}

bool ApplySplashToTarget(
        void* game,
        void* attacker,
        const CaptureMetadata& metadata,
        const OwnedDamage& baseDamage,
        UnitIdentity targetId) {
    auto* target = GetServerUnit(game, targetId.type, targetId.guid);
    if (!target || Identity(target) != targetId
            || !CanDamageTarget(game, attacker, target)) {
        RejectedTargets.fetch_add(1, std::memory_order_relaxed);
        DiagnosticLog(
            "MeleeSplash reject target=%d/%u reason=unresolved-or-cannot-damage.",
            targetId.type,
            targetId.guid);
        return false;
    }
    auto damage = OwnedDamage::Copy(baseDamage.Get());
    if (!damage) return false;
    PrepareSyntheticRecord(target, damage->Get());
    if (!ScaleOffensivePacket(damage->Get(), metadata.splashPercent)) {
        RejectedTargets.fetch_add(1, std::memory_order_relaxed);
        DiagnosticLog(
            "MeleeSplash reject target=%d/%u reason=invalid-poison-packet.",
            targetId.type,
            targetId.guid);
        return false;
    }
    const auto critical = ResolveCriticalForTarget(
        game,
        attacker,
        target,
        damage->Get(),
        metadata.damageMode);
    if (!ApplySecondaryConversion(attacker, damage->Get())) {
        RejectedTargets.fetch_add(1, std::memory_order_relaxed);
        DiagnosticLog("MeleeSplash reject reason=unsupported-secondary-conversion.");
        return false;
    }

    SecondaryDiagnostics diagnostics{};
    std::int32_t normalLifeLeechPercent{};
    std::int32_t normalManaLeechPercent{};
    std::int32_t splashLifeLeechPercent{};
    std::int32_t splashManaLeechPercent{};
    {
        SecondaryScope secondaryScope;
        SecondaryDiagnosticsScope diagnosticsScope(&diagnostics);
        CalculateTotalDamage(game, attacker, target, damage->Get());
        normalLifeLeechPercent = Read<std::int32_t>(
            damage->Get(), DamageLifeLeechOffset);
        normalManaLeechPercent = Read<std::int32_t>(
            damage->Get(), DamageManaLeechOffset);
        splashLifeLeechPercent = normalLifeLeechPercent / 2;
        splashManaLeechPercent = normalManaLeechPercent / 2;
        Write(damage->Get(), DamageLifeLeechOffset, splashLifeLeechPercent);
        Write(damage->Get(), DamageManaLeechOffset, splashManaLeechPercent);
        UpdateWillDie(target, damage->Get());
        OriginalExecute(game, attacker, target, 0, damage->Get());
        FinalizeDamage(game, attacker, target, damage->Get());
    }

    SecondaryHits.fetch_add(1, std::memory_order_relaxed);
    DiagnosticLog(
        "MeleeSplash target=%d/%u crit=%s critResolver=%s phys=%d fire=%d "
        "burn=%d lightning=%d "
        "magic=%d cold=%d poison=%d postTotal=%d normalLifeLeechPct=%d "
        "splashLifeLeechPct=%d nativeAdjustedLifeLeechPct=%d normalManaLeechPct=%d "
        "splashManaLeechPct=%d nativeAdjustedManaLeechPct=%d "
        "CBcalls=%u CBresult=%d OWcalls=%u OWresult=%d.",
        targetId.type,
        targetId.guid,
        critical.outcome,
        critical.resolver,
        Read<std::int32_t>(damage->Get(), DamagePhysicalOffset),
        Read<std::int32_t>(damage->Get(), DamageFireOffset),
        Read<std::int32_t>(damage->Get(), DamageBurnOffset),
        Read<std::int32_t>(damage->Get(), DamageLightningOffset),
        Read<std::int32_t>(damage->Get(), DamageMagicOffset),
        Read<std::int32_t>(damage->Get(), DamageColdOffset),
        Read<std::int32_t>(damage->Get(), DamagePoisonOffset),
        Read<std::int32_t>(damage->Get(), DamageTotalOffset),
        normalLifeLeechPercent,
        splashLifeLeechPercent,
        Read<std::int32_t>(damage->Get(), DamageLifeLeechOffset),
        normalManaLeechPercent,
        splashManaLeechPercent,
        Read<std::int32_t>(damage->Get(), DamageManaLeechOffset),
        diagnostics.crushingBlowCalls,
        diagnostics.lastCrushingBlowResult,
        diagnostics.openWoundsCalls,
        diagnostics.lastOpenWoundsResult);
    return true;
}

void TriggerSplash(PrimaryFrame& frame) noexcept {
    try {
        if (!frame.sidecar || !frame.sidecar->damage) {
            DiagnosticLog("MeleeSplash reject hit reason=missing-sidecar-packet.");
            return;
        }
        if (!frame.primaryHitSucceeded) {
            DiagnosticLog(
                "MeleeSplash reject hit seq=%llu reason=primary-not-successful.",
                static_cast<unsigned long long>(frame.sidecar->sequence));
            return;
        }
        if (frame.sidecar->metadata.gateRequired
                && !frame.sidecar->metadata.gateSatisfied
                && !frame.gateSeen) {
            DiagnosticLog(
                "MeleeSplash reject hit seq=%llu reason=gate-stat-absent.",
                static_cast<unsigned long long>(frame.sidecar->sequence));
            return;
        }
        const auto& metadata = frame.sidecar->metadata;
        auto* attacker = GetServerUnit(
            frame.game, metadata.attacker.type, metadata.attacker.guid);
        auto* primary = GetServerUnit(
            frame.game, metadata.primary.type, metadata.primary.guid);
        if (!attacker || !primary
                || Identity(attacker) != metadata.attacker
                || Identity(primary) != metadata.primary) {
            RejectedHits.fetch_add(1, std::memory_order_relaxed);
            DiagnosticLog(
                "MeleeSplash reject hit seq=%llu reason=unit-reresolve-failed.",
                static_cast<unsigned long long>(frame.sidecar->sequence));
            return;
        }
        auto targets = CollectSecondaryTargets(
            frame.game, attacker, primary, metadata.radiusTiles);
        std::uint32_t applied{};
        for (const auto target : targets) {
            if (ApplySplashToTarget(
                    frame.game,
                    attacker,
                    metadata,
                    *frame.sidecar->damage,
                    target)) {
                ++applied;
            }
        }
        Bursts.fetch_add(1, std::memory_order_relaxed);
        DiagnosticLog(
            "MeleeSplash burst seq=%llu attacker=%d/%u primary=%d/%u "
            "radius=%d percent=%d candidates=%zu applied=%u recursionDepth=%u.",
            static_cast<unsigned long long>(frame.sidecar->sequence),
            metadata.attacker.type,
            metadata.attacker.guid,
            metadata.primary.type,
            metadata.primary.guid,
            metadata.radiusTiles,
            metadata.splashPercent,
            targets.size(),
            applied,
            SecondaryDepth);
    } catch (const std::exception& exception) {
        RejectedHits.fetch_add(1, std::memory_order_relaxed);
        if (Context) {
            char message[1024]{};
            std::snprintf(
                message,
                sizeof(message),
                "MeleeSplash: synthetic resolution rejected: %s",
                exception.what());
            Context->LogError(message);
        }
    } catch (...) {
        RejectedHits.fetch_add(1, std::memory_order_relaxed);
        if (Context) Context->LogError(
            "MeleeSplash: synthetic resolution rejected by an unknown error.");
    }
}

std::int32_t __fastcall HookPlayerSkillPositionInput(
        void* game,
        void* unit,
        std::int32_t x,
        std::int32_t y,
        std::int32_t argument,
        void* activeSkill) noexcept {
    if (!Operational.load(std::memory_order_acquire)) {
        return OriginalPlayerSkillPositionInput(
            game, unit, x, y, argument, activeSkill);
    }
    RememberSkillInput(
        game, unit, SkillIdFromActiveInput(activeSkill), {}, false);
    return OriginalPlayerSkillPositionInput(
        game, unit, x, y, argument, activeSkill);
}

std::int32_t __fastcall HookPlayerSkillUnitInput(
        void* game,
        void* unit,
        std::int32_t targetType,
        std::int32_t targetGuid,
        void* activeSkill,
        std::int32_t argument6,
        std::int32_t argument7) noexcept {
    if (!Operational.load(std::memory_order_acquire)) {
        return OriginalPlayerSkillUnitInput(
            game, unit, targetType, targetGuid, activeSkill,
            argument6, argument7);
    }
    RememberSkillInput(
        game,
        unit,
        SkillIdFromActiveInput(activeSkill),
        {
            .type = targetType,
            .guid = static_cast<std::uint32_t>(targetGuid),
        },
        true);
    return OriginalPlayerSkillUnitInput(
        game, unit, targetType, targetGuid, activeSkill,
        argument6, argument7);
}

void __fastcall HookFill(
        void* game,
        void* attacker,
        void* defender,
        void* damage,
        std::int32_t mode,
        std::uint8_t sourceDamage) noexcept {
    const auto returnRva = reinterpret_cast<std::uintptr_t>(_ReturnAddress())
        - reinterpret_cast<std::uintptr_t>(Base);
    const auto attackerId = Identity(attacker);
    const auto defenderId = Identity(defender);
    if (Operational.load(std::memory_order_acquire) && SecondaryDepth != 0) {
        DiagnosticLog(
            "MeleeSplash recursion guard pass-through at Fill depth=%u.",
            SecondaryDepth);
    }
    if (!Operational.load(std::memory_order_acquire)
            || SecondaryDepth != 0
            || !IsSupportedMeleeFillReturn(returnRva)
            || !attacker || !defender || !damage
            || Identity(attacker).type != UnitPlayer
            || Identity(defender).type != UnitMonster) {
        if (Operational.load(std::memory_order_acquire)
                && SecondaryDepth == 0
                && attacker && defender && damage
                && attackerId.type == UnitPlayer
                && defenderId.type == UnitMonster
                && FillDiagnosticLines.fetch_add(
                    1, std::memory_order_relaxed) < MaximumFillDiagnosticLines) {
            DiagnosticLog(
                "MeleeSplash Fill pass-through reason=%s returnRva=0x%llX "
                "expectedDirect=0x%llX expectedQueued=0x%llX "
                "attacker=%d/%u defender=%d/%u mode=%d.",
                IsSupportedMeleeFillReturn(returnRva)
                    ? "non-caller-guard" : "caller-mismatch",
                static_cast<unsigned long long>(returnRva),
                static_cast<unsigned long long>(DirectMeleeFillReturnRva),
                static_cast<unsigned long long>(QueuedMeleeFillReturnRva),
                attackerId.type,
                attackerId.guid,
                defenderId.type,
                defenderId.guid,
                mode);
        }
        OriginalFill(game, attacker, defender, damage, mode, sourceDamage);
        return;
    }

    if (FillDiagnosticLines.fetch_add(
            1, std::memory_order_relaxed) < MaximumFillDiagnosticLines) {
        DiagnosticLog(
            "MeleeSplash Fill candidate returnRva=0x%llX attacker=%d/%u "
            "defender=%d/%u mode=%d damage=%p.",
            static_cast<unsigned long long>(returnRva),
            attackerId.type,
            attackerId.guid,
            defenderId.type,
            defenderId.guid,
            mode,
            damage);
    }

    std::unique_ptr<CaptureMetadata> metadata;
    try {
        metadata = BuildMetadata(game, attacker, defender, mode);
    } catch (...) {
        OriginalFill(game, attacker, defender, damage, mode, sourceDamage);
        return;
    }
    if (!metadata) {
        OriginalFill(game, attacker, defender, damage, mode, sourceDamage);
        return;
    }
    FillFrame frame{
        .game = game,
        .attacker = attacker,
        .defender = defender,
        .damage = damage,
        .candidate = true,
    };
    frame.sourceDamage = sourceDamage == 0 ? 128 : sourceDamage;
    const auto* skillPolicy = FindSkillOverride(ActiveConfig, metadata->skillId);
    frame.weaponSourceOnly = skillPolicy && skillPolicy->weaponSourceOnly;
    frame.poisonPrefixCount = Read<std::uint64_t>(damage, DamagePoisonVectorCountOffset);
    frame.sourceCaptureValid = frame.poisonPrefixCount <= MaximumPoisonEntries;
    FillFrameScope scope(&frame);
    OriginalFill(game, attacker, defender, damage, mode, sourceDamage);
    try {
        const auto captured = (frame.event3Seen || frame.preConversionSeen)
            ? static_cast<bool>(frame.normalizedDamage)
            : CaptureNormalizedPacket(frame, damage);
        DiagnosticLog(
            "MeleeSplash Fill result captured=%s event3=%s precritical=%s "
            "gateSeen=%s preFlags=0x%04X lateFlags=0x%04X conversion=%u.",
            captured ? "true" : "false",
            frame.event3Seen ? "true" : "false",
            frame.preCriticalSeen ? "true" : "false",
            frame.gateSeen ? "true" : "false",
            static_cast<unsigned>(frame.preCriticalFlags),
            static_cast<unsigned>(Read<std::uint16_t>(
                damage, DamageResultFlagsOffset)),
            static_cast<unsigned>(Read<std::uint8_t>(
                damage, DamageConversionTypeOffset)));
        PushPendingCapture(frame, std::move(metadata));
    } catch (...) {
        RejectedHits.fetch_add(1, std::memory_order_relaxed);
        DiagnosticLog("MeleeSplash capture reject reason=exception.");
    }
}

void CaptureBeforeConversion() noexcept {
    auto* frame = ActiveFillFrame;
    if (!Operational.load(std::memory_order_acquire) || !frame || !frame->candidate) return;
    frame->preConversionSeen = true;
    try {
        CaptureNormalizedPacket(*frame, frame->damage);
    } catch (...) {
        frame->sourceCaptureValid = false;
        DiagnosticLog("MeleeSplash capture reject reason=preconversion-exception.");
    }
}

std::int32_t __fastcall HookSourceRoll(
        void* attacker, std::int32_t minimum, std::int32_t maximum,
        std::int32_t minimumMastery, std::int32_t maximumMastery,
        std::int32_t accumulator) noexcept {
    const auto returnRva = reinterpret_cast<std::uintptr_t>(_ReturnAddress())
        - reinterpret_cast<std::uintptr_t>(Base);
    const auto result = At<SourceRollFn>(SourceRollRva)(
        attacker, minimum, maximum, minimumMastery, maximumMastery, accumulator);
    auto* frame = ActiveFillFrame;
    if (!Operational.load(std::memory_order_acquire) || !frame
            || !frame->weaponSourceOnly || frame->attacker != attacker) return result;
    for (std::size_t index = 0; index < SourceRollCalls.size(); ++index) {
        if (returnRva != SourceRollCalls[index] + 5) continue;
        std::int32_t addition{};
        const auto bit = static_cast<std::uint8_t>(1U << index);
        if (frame->sourceDamage != 128
                || !CaptureSourceAddition(accumulator, result, addition)
                || (frame->sourceRollSeen & bit) != 0) {
            frame->sourceCaptureValid = false;
            return result;
        }
        frame->sourceRollSeen |= bit;
        frame->sourceComponents[index] = addition;
        if (index == 2) frame->sourceColdLength = GetUnitStat(attacker, 56, 0);
        if (index == 4 && addition != 0) {
            const auto overrideLength = GetUnitStat(attacker, 101, 0);
            const auto count = GetUnitStat(attacker, 326, 0);
            const auto length = GetUnitStat(attacker, 59, 0);
            frame->sourcePoisonLength = overrideLength > 0 ? overrideLength
                : (count > 1 ? length / count : length);
        }
        return result;
    }
    frame->sourceCaptureValid = false;
    return result;
}

std::int32_t __fastcall HookPreCriticalBonuses(
        void* attacker,
        std::int32_t getStats,
        void* item,
        std::int32_t minimumDamage,
        std::int32_t maximumDamage,
        std::int32_t enhancedDamage,
        std::int32_t flatDamage,
        std::uint8_t sourceDamage) noexcept {
    const auto result = ApplyDamageBonuses(
        attacker,
        getStats,
        item,
        minimumDamage,
        maximumDamage,
        enhancedDamage,
        flatDamage,
        sourceDamage);
    auto* frame = ActiveFillFrame;
    if (Operational.load(std::memory_order_acquire)
            && frame && frame->candidate && frame->attacker == attacker
            && frame->damage) {
        frame->preCriticalPhysical = result;
        frame->preCriticalFlags = Read<std::uint16_t>(
            frame->damage, DamageResultFlagsOffset);
        frame->preCriticalSeen = true;
    }
    return result;
}

std::int32_t __fastcall HookPreEvent3(
        void* game,
        std::int32_t event,
        void* defender,
        void* attacker,
        void* damage) noexcept {
    const auto returnRva = reinterpret_cast<std::uintptr_t>(_ReturnAddress())
        - reinterpret_cast<std::uintptr_t>(Base);
    auto* frame = ActiveFillFrame;
    if (Operational.load(std::memory_order_acquire)
            && frame && frame->candidate
            && returnRva == PreEvent3ReturnRva
            && event == ReactiveMeleeEvent
            && frame->game == game
            && frame->attacker == attacker
            && frame->defender == defender
            && frame->damage == damage) {
        frame->event3Seen = true;
        try {
            CaptureNormalizedPacket(*frame, damage);
        } catch (...) {
            frame->candidate = false;
        }
    }
    return ApplyDamageEvent(game, event, defender, attacker, damage);
}

void __fastcall HookAllocate(
        void* game,
        void* attacker,
        void* defender,
        const void* damage) noexcept {
    const auto returnRva = reinterpret_cast<std::uintptr_t>(_ReturnAddress())
        - reinterpret_cast<std::uintptr_t>(Base);
    const auto attackerId = Identity(attacker);
    const auto defenderId = Identity(defender);
    auto* oldHead = attacker
        ? Read<void*>(attacker, UnitCombatListOffset)
        : nullptr;
    OriginalAllocate(game, attacker, defender, damage);
    if (!Operational.load(std::memory_order_acquire)
            || SecondaryDepth != 0
            || returnRva != AllocateMeleeReturnRva) {
        return;
    }
    try {
        auto pending = TakePendingCapture(
            damage, game, attackerId, defenderId);
        if (!pending) return;
        auto* node = FindCombatNode(
            attacker, attackerId, defenderId, oldHead);
        if (!node) {
            DroppedSidecars.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        StoreSidecar(node, std::move(pending));
    } catch (...) {
        DroppedSidecars.fetch_add(1, std::memory_order_relaxed);
    }
}

std::uint8_t __fastcall HookConsume(
        void* game,
        void* attacker,
        void* defender,
        std::int32_t rangeBonus) noexcept {
    if (!Operational.load(std::memory_order_acquire)
            || SecondaryDepth != 0 || !attacker || !defender) {
        return OriginalConsume(game, attacker, defender, rangeBonus);
    }
    auto* node = FindCombatNode(
        attacker, Identity(attacker), Identity(defender));
    const auto attackerId = Identity(attacker);
    const auto defenderId = Identity(defender);
    auto sidecar = TakeSidecar(node, game, attackerId, defenderId);
    if (!sidecar) return OriginalConsume(game, attacker, defender, rangeBonus);
    if (sidecar->threadId != GetCurrentThreadId()
            || sidecar->gameToken != game
            || sidecar->metadata.attacker != attackerId
            || sidecar->metadata.primary != defenderId) {
        RejectedHits.fetch_add(1, std::memory_order_relaxed);
        return OriginalConsume(game, attacker, defender, rangeBonus);
    }

    PrimaryFrame frame{
        .sidecar = sidecar.get(),
        .game = game,
        .attacker = attacker,
        .defender = defender,
    };
    PrimaryFrameScope scope(&frame);
    const auto result = OriginalConsume(game, attacker, defender, rangeBonus);
    if (IsCombatNodeLinked(attacker, node)) {
        ReturnSidecar(std::move(sidecar));
        return result;
    }
    return result;
}

void __fastcall HookExecute(
        void* game,
        void* attacker,
        void* defender,
        std::int32_t missile,
        void* damage) noexcept {
    const auto returnRva = reinterpret_cast<std::uintptr_t>(_ReturnAddress())
        - reinterpret_cast<std::uintptr_t>(Base);
    auto* frame = ActivePrimaryFrame;
    if (!Operational.load(std::memory_order_acquire)
            || SecondaryDepth != 0 || !frame
            || returnRva != ExecuteMeleeReturnRva || missile != 0
            || frame->game != game || frame->attacker != attacker
            || frame->defender != defender) {
        OriginalExecute(game, attacker, defender, missile, damage);
        return;
    }
    OriginalExecute(game, attacker, defender, missile, damage);
    frame->damage = damage;
    frame->primaryHitSucceeded = damage
        && (Read<std::uint16_t>(damage, DamageResultFlagsOffset)
            & ResultSuccess) != 0;
    // Resolve the burst at the first authoritative post-hit seam.  The rest
    // of the native melee consumer performs durability, reactive melee
    // events, thorns, primary FinalizeDamage and combat-node destruction;
    // delaying until HookConsume returns would observe mutated item/RNG state
    // and can lose the primary's room/path on a killing blow.
    TriggerSplash(*frame);
}

std::int32_t __fastcall FilteredEvent(
        EventFunctionFn original,
        const char* name,
        void* game,
        std::int32_t event,
        void* attacker,
        void* target,
        void* damage,
        std::int32_t argument6,
        std::int32_t argument7,
        std::int32_t argument8,
        void* argument9) noexcept {
    if (Operational.load(std::memory_order_acquire) && SecondaryDepth != 0) {
        DiagnosticLog(
            "MeleeSplash filtered event=%s attacker=%d/%u target=%d/%u "
            "recursionDepth=%u.",
            name,
            Identity(attacker).type,
            Identity(attacker).guid,
            Identity(target).type,
            Identity(target).guid,
            SecondaryDepth);
        return 0;
    }
    return original(
        game,
        event,
        attacker,
        target,
        damage,
        argument6,
        argument7,
        argument8,
        argument9);
}

#define MELEE_SPLASH_FILTERED_EVENT(NUMBER) \
    std::int32_t __fastcall HookEvent##NUMBER( \
            void* game, std::int32_t event, void* attacker, void* target, \
            void* damage, std::int32_t argument6, std::int32_t argument7, \
            std::int32_t argument8, void* argument9) noexcept { \
        return FilteredEvent( \
            OriginalEvent##NUMBER, #NUMBER, game, event, attacker, target, \
            damage, argument6, argument7, argument8, argument9); \
    }

MELEE_SPLASH_FILTERED_EVENT(7)
MELEE_SPLASH_FILTERED_EVENT(9)
MELEE_SPLASH_FILTERED_EVENT(14)
MELEE_SPLASH_FILTERED_EVENT(19)
MELEE_SPLASH_FILTERED_EVENT(21)

#undef MELEE_SPLASH_FILTERED_EVENT

std::int32_t __fastcall HookEvent15(
        void* game,
        std::int32_t event,
        void* attacker,
        void* target,
        void* damage,
        std::int32_t argument6,
        std::int32_t argument7,
        std::int32_t argument8,
        void* argument9) noexcept {
    const auto result = OriginalEvent15(
        game, event, attacker, target, damage,
        argument6, argument7, argument8, argument9);
    if (Operational.load(std::memory_order_acquire)
            && SecondaryDepth != 0 && ActiveSecondaryDiagnostics) {
        ++ActiveSecondaryDiagnostics->openWoundsCalls;
        ActiveSecondaryDiagnostics->lastOpenWoundsResult = result;
    }
    return result;
}

std::int32_t __fastcall HookEvent16(
        void* game,
        std::int32_t event,
        void* attacker,
        void* target,
        void* damage,
        std::int32_t argument6,
        std::int32_t argument7,
        std::int32_t argument8,
        void* argument9) noexcept {
    const auto result = OriginalEvent16(
        game, event, attacker, target, damage,
        argument6, argument7, argument8, argument9);
    if (Operational.load(std::memory_order_acquire)
            && SecondaryDepth != 0 && ActiveSecondaryDiagnostics) {
        ++ActiveSecondaryDiagnostics->crushingBlowCalls;
        ActiveSecondaryDiagnostics->lastCrushingBlowResult = result;
    }
    return result;
}

std::int32_t __fastcall HookEvent20(
        void* game,
        std::int32_t event,
        void* attacker,
        void* target,
        void* damage,
        std::int32_t argument6,
        std::int32_t argument7,
        std::int32_t argument8,
        void* argument9) noexcept {
    if (!Operational.load(std::memory_order_acquire)) {
        return OriginalEvent20(
            game, event, attacker, target, damage,
            argument6, argument7, argument8, argument9);
    }
    if (SecondaryDepth != 0) {
        DiagnosticLog(
            "MeleeSplash filtered event=20 attacker=%d/%u target=%d/%u.",
            Identity(attacker).type,
            Identity(attacker).guid,
            Identity(target).type,
            Identity(target).guid);
        return 0;
    }
    const auto packed = static_cast<std::uint32_t>(argument6);
    const auto statId = static_cast<std::uint16_t>(packed >> 16U);
    const auto layer = static_cast<std::uint16_t>(packed);
    if (ActiveFillFrame && ActiveConfig.gateStatId >= 0
            && statId == static_cast<std::uint16_t>(ActiveConfig.gateStatId)
            && ActiveFillFrame->game == game
            && ActiveFillFrame->attacker == attacker
            && ActiveFillFrame->defender == target
            && GetLayeredStat(attacker, statId, layer) > 0) {
        ActiveFillFrame->gateSeen = true;
    }
    if (ActivePrimaryFrame && ActiveConfig.gateStatId >= 0
            && statId == static_cast<std::uint16_t>(ActiveConfig.gateStatId)
            && ActivePrimaryFrame->attacker == attacker
            && ActivePrimaryFrame->defender == target
            && GetLayeredStat(attacker, statId, layer) > 0) {
        ActivePrimaryFrame->gateSeen = true;
    }
    if (MatchesLegacyEvent20Suppression(
            ActiveConfig,
            packed,
            attacker && Identity(attacker).type == UnitPlayer)) {
        DiagnosticLog(
            "MeleeSplash suppressed configured legacy Event20 token=0x%08X.",
            packed);
        return 0;
    }
    return OriginalEvent20(
        game, event, attacker, target, damage,
        argument6, argument7, argument8, argument9);
}

bool CheckBytes(
        std::uintptr_t rva,
        const std::uint8_t* expected,
        std::size_t size,
        const char* label) noexcept {
    if (Context->CheckExpectedBytes(
            rva, expected, static_cast<std::uint32_t>(size))) {
        return true;
    }
    char message[512]{};
    std::snprintf(
        message,
        sizeof(message),
        "MeleeSplash: signature mismatch at %s; plugin refused before mutation.",
        label ? label : "<unknown>");
    Context->LogError(message);
    return false;
}

template <std::size_t Size>
bool CheckBytes(
        std::uintptr_t rva,
        const std::array<std::uint8_t, Size>& expected,
        const char* label) noexcept {
    return CheckBytes(rva, expected.data(), expected.size(), label);
}

bool NeedsSourceCapture() noexcept {
    return std::any_of(ActiveConfig.skillOverrides.begin(), ActiveConfig.skillOverrides.end(),
        [](const auto& entry) { return entry.second.weaponSourceOnly; });
}

bool ValidateExtensionSites() noexcept {
    if ((ActiveConfig.enablePhysicalConversion || NeedsSourceCapture())
            && !CheckBytes(PreConversionRva, PreConversionExpected, "pre-conversion capture")) {
        return false;
    }
    if (!NeedsSourceCapture()) return true;
    for (std::size_t index = 0; index < SourceRollContexts.size(); ++index) {
        if (!CheckBytes(SourceRollCalls[index] - 0x13,
                SourceRollContexts[index], "weapon source roll")) return false;
    }
    return CheckBytes(SourceRollCalls[4] - 0x1B, PoisonRollContext, "poison source roll");
}

bool ValidateOwnedSites() noexcept {
    return CheckBytes(FillDamageValuesRva, FillExpected, "FillDamageValues")
        && CheckBytes(PlayerSkillPositionInputRva,
            PlayerSkillPositionInputExpected, "player position-skill input")
        && CheckBytes(PlayerSkillUnitInputRva,
            PlayerSkillUnitInputExpected, "player unit-skill input")
        && CheckBytes(ActiveSkillLayoutWitnessRva,
            ActiveSkillLayoutWitnessExpected, "active-skill record layout")
        && CheckBytes(
            DirectMeleeFillContextRva,
            DirectMeleeFillContextExpected,
            "direct melee Fill call context")
        && CheckBytes(AllocateCombatRecordRva, AllocateExpected, "AllocateCombatRecord")
        && CheckBytes(ConsumeCombatRecordRva, ConsumeExpected, "ConsumeCombatRecord")
        && CheckBytes(ExecuteEventsRva, ExecuteExpected, "ExecuteEvents")
        && CheckBytes(EventFunc7Rva, Event7Expected, "EventFunc7")
        && CheckBytes(EventFunc9Rva, Event9Expected, "EventFunc9")
        && CheckBytes(EventFunc14Rva, Event14Expected, "EventFunc14")
        && CheckBytes(EventFunc15Rva, Event15Expected, "EventFunc15")
        && CheckBytes(EventFunc16Rva, Event16Expected, "EventFunc16")
        && CheckBytes(EventFunc19Rva, Event19Expected, "EventFunc19")
        && CheckBytes(EventFunc20Rva, Event20Expected, "EventFunc20")
        && CheckBytes(EventFunc21Rva, Event21Expected, "EventFunc21")
        && CheckBytes(PreCriticalCallRva, PreCriticalContextExpected, "pre-critical call")
        && CheckBytes(PreEvent3CallRva - 0x15, PreEvent3ContextExpected, "pre-Event3 call context")
        && ValidateExtensionSites();
}

void* AllocateRelayPageNear(void* hint) noexcept {
    SYSTEM_INFO systemInfo{};
    GetSystemInfo(&systemInfo);
    const auto granularity = static_cast<std::uintptr_t>(
        systemInfo.dwAllocationGranularity);
    const auto base = reinterpret_cast<std::uintptr_t>(hint)
        & ~(granularity - 1);
    for (std::uintptr_t delta = granularity;
            delta < UINT64_C(0x70000000); delta += granularity) {
        if (base > std::numeric_limits<std::uintptr_t>::max() - delta) break;
        if (auto* memory = VirtualAlloc(
                reinterpret_cast<void*>(base + delta),
                systemInfo.dwPageSize,
                MEM_COMMIT | MEM_RESERVE,
                PAGE_READWRITE)) {
            return memory;
        }
    }
    return nullptr;
}

bool WriteAbsoluteJumpRelay(
        std::uint8_t* destination, const void* target) noexcept {
    if (!destination || !target) return false;
    std::array<std::uint8_t, 14> relay{
        0xFF,0x25,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    };
    const auto address = reinterpret_cast<std::uintptr_t>(target);
    std::memcpy(relay.data() + 6, &address, sizeof(address));
    std::memcpy(destination, relay.data(), relay.size());
    return true;
}

bool CreateRelays() noexcept {
    RelayPage = AllocateRelayPageNear(Base + PreCriticalCallRva);
    if (!RelayPage) return false;
    auto* relays = static_cast<std::uint8_t*>(RelayPage);
    if (!WriteAbsoluteJumpRelay(
            relays, reinterpret_cast<const void*>(&HookPreCriticalBonuses))
            || !WriteAbsoluteJumpRelay(
                relays + RelayStride,
                reinterpret_cast<const void*>(&HookPreEvent3))
            || !WriteAbsoluteJumpRelay(relays + 2 * RelayStride,
                reinterpret_cast<const void*>(&MeleeSplashPreConversionRelay))) {
        VirtualFree(RelayPage, 0, MEM_RELEASE);
        RelayPage = nullptr;
        return false;
    }
    DWORD previousProtection{};
    for (std::size_t index = 0; index < SourceRollCalls.size(); ++index) {
        if (!WriteAbsoluteJumpRelay(relays + (3 + index) * RelayStride,
                reinterpret_cast<const void*>(&HookSourceRoll))) return false;
    }
    if (!VirtualProtect(
            RelayPage, RelayBytes, PAGE_EXECUTE_READ, &previousProtection)) {
        VirtualFree(RelayPage, 0, MEM_RELEASE);
        RelayPage = nullptr;
        return false;
    }
    FlushInstructionCache(GetCurrentProcess(), RelayPage, RelayBytes);
    const auto relayAddress = reinterpret_cast<std::uintptr_t>(RelayPage);
    const auto baseAddress = reinterpret_cast<std::uintptr_t>(Base);
    if (relayAddress < baseAddress
            || relayAddress - baseAddress
                > std::numeric_limits<std::uint32_t>::max()) {
        VirtualFree(RelayPage, 0, MEM_RELEASE);
        RelayPage = nullptr;
        return false;
    }
    return true;
}

// ABI 4 limits PatchCallRel32's target RVA to D2R.exe. These validated calls
// target a nearby executable relay instead, so encode the rel32 explicitly
// and submit it through the SDK's expected-byte-checked PatchBytes API.
bool PatchCallToRelay(
        std::uintptr_t rva,
        const void* expected,
        std::uint32_t expectedSize,
        const void* relay,
        std::uint32_t patchSize) noexcept {
    if (expected == nullptr || relay == nullptr || patchSize < 5
            || patchSize > 16 || expectedSize < patchSize) {
        return false;
    }

    const auto nextInstruction = reinterpret_cast<std::uintptr_t>(Base) + rva + 5;
    const auto targetAddress = reinterpret_cast<std::uintptr_t>(relay);
    const auto displacement = static_cast<std::int64_t>(targetAddress)
        - static_cast<std::int64_t>(nextInstruction);
    if (displacement < std::numeric_limits<std::int32_t>::min()
            || displacement > std::numeric_limits<std::int32_t>::max()) {
        Context->LogError(
            "MeleeSplash: relay is outside the rel32 range; patch refused.");
        return false;
    }

    std::array<std::uint8_t, 16> replacement{};
    replacement.fill(0x90);
    replacement[0] = 0xE8;
    const auto rel32 = static_cast<std::int32_t>(displacement);
    std::memcpy(replacement.data() + 1, &rel32, sizeof(rel32));
    // PluginSDK 0.4.0 requires expectedSize == size. The longer signature
    // window is validated separately before installation; submit only the
    // exact bytes this relay replaces so the SDK accepts the mutation.
    if (!Context->PatchBytes(
            rva, expected, patchSize, replacement.data(), patchSize)) {
        Context->LogError(
            "MeleeSplash: SDK byte patch refused a validated relay call.");
        return false;
    }
    AnyMutationInstalled.store(true, std::memory_order_release);
    return true;
}

template <typename Function, std::size_t Size>
bool InstallInline(
        std::uintptr_t rva,
        const std::array<std::uint8_t, Size>& expected,
        Function hook,
        Function* original) noexcept {
    if (!Context->InstallInlineHook(
            rva,
            expected.data(),
            static_cast<std::uint32_t>(expected.size()),
            hook,
            original)) {
        return false;
    }
    AnyMutationInstalled.store(true, std::memory_order_release);
    return true;
}

bool InstallHooks() noexcept {
    if (!CreateRelays()) {
        Context->LogError("MeleeSplash: could not create rel32 relay page.");
        return false;
    }
    const auto inlineInstalled =
        InstallInline(FillDamageValuesRva, FillExpected, HookFill, &OriginalFill)
        && InstallInline(AllocateCombatRecordRva, AllocateExpected, HookAllocate, &OriginalAllocate)
        && InstallInline(ConsumeCombatRecordRva, ConsumeExpected, HookConsume, &OriginalConsume)
        && InstallInline(ExecuteEventsRva, ExecuteExpected, HookExecute, &OriginalExecute)
        && InstallInline(EventFunc7Rva, Event7Expected, HookEvent7, &OriginalEvent7)
        && InstallInline(EventFunc9Rva, Event9Expected, HookEvent9, &OriginalEvent9)
        && InstallInline(EventFunc14Rva, Event14Expected, HookEvent14, &OriginalEvent14)
        && InstallInline(EventFunc15Rva, Event15Expected, HookEvent15, &OriginalEvent15)
        && InstallInline(EventFunc16Rva, Event16Expected, HookEvent16, &OriginalEvent16)
        && InstallInline(EventFunc19Rva, Event19Expected, HookEvent19, &OriginalEvent19)
        && InstallInline(EventFunc20Rva, Event20Expected, HookEvent20, &OriginalEvent20)
        && InstallInline(EventFunc21Rva, Event21Expected, HookEvent21, &OriginalEvent21)
        && InstallInline(PlayerSkillPositionInputRva,
            PlayerSkillPositionInputExpected, HookPlayerSkillPositionInput,
            &OriginalPlayerSkillPositionInput)
        && InstallInline(PlayerSkillUnitInputRva,
            PlayerSkillUnitInputExpected, HookPlayerSkillUnitInput,
            &OriginalPlayerSkillUnitInput);
    if (!inlineInstalled) return false;

    auto* const relays = static_cast<std::uint8_t*>(RelayPage);
    if (!PatchCallToRelay(
            PreCriticalCallRva,
            PreCriticalCallExpected.data(),
            static_cast<std::uint32_t>(PreCriticalCallExpected.size()),
            relays,
            5)) {
        return false;
    }
    if (!PatchCallToRelay(
            PreEvent3CallRva,
            PreEvent3CallExpected.data(),
            static_cast<std::uint32_t>(PreEvent3CallExpected.size()),
            relays + RelayStride,
            5)) {
        return false;
    }
    if ((ActiveConfig.enablePhysicalConversion || NeedsSourceCapture())
            && !PatchCallToRelay(PreConversionRva,
                PreConversionExpected.data(),
                static_cast<std::uint32_t>(PreConversionExpected.size()),
                relays + 2 * RelayStride,
                8)) {
        return false;
    }
    if (NeedsSourceCapture()) {
        for (std::size_t index = 0; index < SourceRollCalls.size(); ++index) {
            const auto* expected = index == 4
                ? PoisonRollContext.data() + 0x1B
                : SourceRollContexts[index].data() + 0x13;
            if (!PatchCallToRelay(SourceRollCalls[index], expected, 5,
                    relays + (3 + index) * RelayStride, 5)) return false;
        }
    }
    return true;
}

void ResolveNativeFunctions() noexcept {
    ApplyDamageBonuses = At<ApplyDamageBonusesFn>(ApplyDamageBonusesRva);
    ApplyDamageEvent = At<ApplyDamageEventFn>(ApplyDamageEventRva);
    CalculateTotalDamage = At<DamageStageFn>(CalculateTotalDamageRva);
    FinalizeDamage = At<DamageStageFn>(FinalizeDamageRva);
    CopyDamage = At<CopyDamageFn>(CopyDamageRva);
    DestroyDamage = At<DestroyDamageFn>(DestroyDamageRva);
    EnumerateUnits = At<EnumerateUnitsFn>(EnumerateUnitsRva);
    CanDamageTarget = At<CanDamageTargetFn>(CanDamageTargetRva);
    GetRoom = At<GetUnitPointerFn>(GetRoomRva);
    GetUsedSkill = At<GetUnitPointerFn>(GetUsedSkillRva);
    ResolveActiveWeapon = At<GetUnitPointerFn>(ResolveActiveWeaponRva);
    GetDataContextFromUnit = At<GetDataContextFromUnitFn>(
        GetDataContextFromUnitRva);
    GetItemsRecord = At<GetItemsRecordFn>(GetItemsRecordRva);
    GetServerUnit = At<GetServerUnitFn>(GetServerUnitRva);
    GetLayeredStat = At<GetStatFn>(GetLayeredStatRva);
    GetUnitStat = At<GetStatFn>(GetUnitStatRva);
    CheckState = At<CheckStateFn>(CheckStateRva);
    GetSeed = At<GetSeedFn>(GetSeedRva);
    RollLimitedRandom = At<RollLimitedRandomFn>(RollLimitedRandomRva);
    GetWeaponMasteryChance = At<GetWeaponMasteryChanceFn>(
        GetWeaponMasteryChanceRva);
    GetPathX = At<GetPathCoordinateFn>(GetPathXRva);
    GetPathY = At<GetPathCoordinateFn>(GetPathYRva);
}

auto ExecutableDirectory() -> std::filesystem::path {
    std::array<wchar_t, 32768> buffer{};
    const auto length = GetModuleFileNameW(
        nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) return {};
    return std::filesystem::path(buffer.data(), buffer.data() + length)
        .parent_path();
}

auto LoadSkillNameIndex(
        const D2RL::PluginContext* context,
        SkillNameIndex& skillIdsByName,
        std::string& error) -> bool {
    skillIdsByName.clear();
    error.clear();
    const auto modRoot = FindActiveModRootPath(context);
    if (modRoot.empty()) {
        error = "could not resolve the active mod data directory";
        return false;
    }

    const auto directPath = modRoot / L"data/global/excel/skills.txt";
    const auto basePath = modRoot / L"data/global/excel/base/skills.txt";
    std::error_code fileError;
    for (const auto& candidate : {directPath, basePath}) {
        fileError.clear();
        if (!std::filesystem::is_regular_file(candidate, fileError) || fileError) {
            continue;
        }
        std::ifstream input(candidate, std::ios::binary);
        if (!input) {
            error = "could not read " + candidate.string();
            return false;
        }
        const std::string text{
            std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        if (!ParseSkillNameIndex(text, skillIdsByName, error)) {
            error = candidate.string() + ": " + error;
            return false;
        }
        return true;
    }

    const auto compilerRoot = modRoot / L"d2rloader/data/compiler";
    fileError.clear();
    if (std::filesystem::is_directory(compilerRoot, fileError) && !fileError) {
        std::vector<std::filesystem::path> compilerSkillFiles;
        std::filesystem::directory_iterator entry(compilerRoot, fileError);
        const std::filesystem::directory_iterator end;
        while (!fileError && entry != end) {
            const auto candidate = entry->path()
                / L"data/global/excel/skills.txt";
            std::error_code candidateError;
            if (std::filesystem::is_regular_file(candidate, candidateError)
                    && !candidateError) {
                compilerSkillFiles.push_back(candidate);
            }
            entry.increment(fileError);
        }
        std::sort(compilerSkillFiles.begin(), compilerSkillFiles.end());
        for (const auto& candidate : compilerSkillFiles) {
            std::ifstream input(candidate, std::ios::binary);
            if (!input) continue;
            const std::string text{
                std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
            if (!ParseSkillNameIndex(text, skillIdsByName, error)) {
                error = candidate.string() + ": " + error;
                return false;
            }
            return true;
        }
    }

    error = "Skills.txt was not found under the active mod's data/global/excel "
        "folder or D2RLoader compiler data";
    return false;
}

auto LoadSettings(std::int32_t splashStatId) -> bool {
    ActiveConfig = Config{};
    ActiveConfig.enabled = true;
    ActiveConfig.splashDamagePercentStatId = splashStatId;
    ActiveConfig.baseSplashDamagePercent = 0;
    ActiveConfig.enablePhysicalConversion = true;
    ActiveConfig.diagnosticLogging = true;
    ActiveConfig.legacyEvent20Suppression = {
        .enabled = true,
        .statId = splashStatId,
        .layer = 0,
        .playerAttackersOnly = true,
    };

    std::array<char, 16 * 1024> toml{};
    std::uint32_t requiredSize{};
    if (!Context || !Context->ReadConfig(
            toml.data(), static_cast<std::uint32_t>(toml.size()), &requiredSize)
            || requiredSize >= toml.size()) {
        if (Context) {
            Context->LogError(
                "MeleeSplash: could not read the plugin TOML settings.");
        }
        return false;
    }
    const auto terminator = std::find(toml.begin(), toml.end(), '\0');
    if (terminator == toml.end()) {
        Context->LogError(
            "MeleeSplash: plugin TOML settings are not null-terminated.");
        return false;
    }
    const auto textSize = static_cast<std::size_t>(terminator - toml.begin());
    SkillNameIndex skillIdsByName;
    std::string skillNameError;
    const bool skillNamesAvailable = LoadSkillNameIndex(
        Context, skillIdsByName, skillNameError);
    SkillSplashSettings skillSettings;
    std::string parseError;
    if (!ParseSkillSplashSettings(
            std::string_view(toml.data(), textSize), skillIdsByName,
            skillSettings, parseError)) {
        if (!skillNamesAvailable
                && parseError.find("unknown skill name") != std::string::npos) {
            parseError += "; name lookup failed: " + skillNameError;
        }
        char message[512]{};
        std::snprintf(message, sizeof(message),
            "MeleeSplash: invalid skill splash settings (%s).", parseError.c_str());
        Context->LogError(message);
        return false;
    }
    ActiveConfig.activationMode = skillSettings.activationMode;
    ActiveConfig.baseRadiusNormalWeapon = skillSettings.normalWeaponRadiusTiles;
    ActiveConfig.baseRadiusExceptionalEliteWeapon =
        skillSettings.exceptionalEliteWeaponRadiusTiles;
    for (const auto& [skillId, damagePercent]
            : skillSettings.fixedSplashDamagePercentBySkill) {
        auto& override = ActiveConfig.skillOverrides[skillId];
        override.enabled = true;
        override.fixedSplashDamagePercent = damagePercent;
        ActiveConfig.includedSkillIds.push_back(skillId);
    }
    for (const auto& [skillId, radiusTiles] : skillSettings.radiusTilesBySkill) {
        ActiveConfig.skillOverrides[skillId].baseRadiusTiles = radiusTiles;
    }
    ActiveConfigPath = Context->pluginConfigPath
        ? std::filesystem::path(Context->pluginConfigPath)
        : std::filesystem::path("d2rl-melee-splash.toml");

    char message[256]{};
    std::snprintf(message, sizeof(message),
        "MeleeSplash settings: mode=%s fixedSkillRows=%zu skillRadiusRows=%zu "
        "radii=%d/%d.",
        ActivationModeName(ActiveConfig.activationMode),
        skillSettings.fixedSplashDamagePercentBySkill.size(),
        skillSettings.radiusTilesBySkill.size(),
        ActiveConfig.baseRadiusNormalWeapon,
        ActiveConfig.baseRadiusExceptionalEliteWeapon);
    Context->LogInfo(message);
    return true;
}

auto Status(
        D2R::Game::Client*,
        const D2RL::ConsoleCommandContext* command,
        void*) noexcept -> D2RL::ConsoleCommandResult {
    if (!command || !command->plugin) {
        return D2RL::ConsoleCommandResult::Failed;
    }
    try {
        const auto path = ActiveConfigPath.empty()
            ? std::string("<absent>")
            : ActiveConfigPath.string();
        char message[1024]{};
        std::snprintf(
            message,
            sizeof(message),
        "Melee Splash 0.3.7: enabled=%s operational=%s partial=%s config=%s "
            "captures=%llu bursts=%llu secondaryHits=%llu rejectedHits=%llu "
            "rejectedTargets=%llu droppedSidecars=%llu.",
            ActiveConfig.enabled ? "true" : "false",
            Operational.load(std::memory_order_acquire) ? "true" : "false",
            AnyMutationInstalled.load(std::memory_order_acquire)
                && !Operational.load(std::memory_order_acquire) ? "true" : "false",
            path.c_str(),
            static_cast<unsigned long long>(Captures.load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(Bursts.load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(SecondaryHits.load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(RejectedHits.load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(RejectedTargets.load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(DroppedSidecars.load(std::memory_order_relaxed)));
        command->plugin->WriteConsoleMessage(message);
    } catch (...) {
        command->plugin->WriteConsoleMessage(
            "Melee Splash 0.3.7: status unavailable (allocation failure).");
    }
    return D2RL::ConsoleCommandResult::Handled;
}

void ResetState() noexcept {
    Operational.store(false, std::memory_order_release);
    AnyMutationInstalled.store(false, std::memory_order_release);
    Sequence.store(0, std::memory_order_relaxed);
    Captures.store(0, std::memory_order_relaxed);
    Bursts.store(0, std::memory_order_relaxed);
    SecondaryHits.store(0, std::memory_order_relaxed);
    RejectedHits.store(0, std::memory_order_relaxed);
    RejectedTargets.store(0, std::memory_order_relaxed);
    DroppedSidecars.store(0, std::memory_order_relaxed);
    FillDiagnosticLines.store(0, std::memory_order_relaxed);
    SkillPolicyDiagnosticLines.store(0, std::memory_order_relaxed);
    SkillInputDiagnosticLines.store(0, std::memory_order_relaxed);
    try {
        std::scoped_lock lock(SidecarMutex);
        Sidecars.clear();
    } catch (...) {
    }
    try {
        std::scoped_lock lock(RecentSkillInputsMutex);
        RecentSkillInputs.clear();
    } catch (...) {
    }
}

} // namespace
} // namespace RuffnecKk::MeleeSplash

extern "C" void MeleeSplashCaptureBeforeConversion() noexcept {
    RuffnecKk::MeleeSplash::CaptureBeforeConversion();
}

namespace {

constexpr D2RL::PluginInfo Info{
    .infoSize = D2RL::PluginInfoSize,
    .abiVersion = D2RL_PLUGIN_ABI_VERSION,
    .id = "d2rl-melee-splash",
    .name = "Melee Splash",
    .version = "0.3.7",
    .author = "MadMike",
    .description = "Adds configurable melee splash damage to nearby monsters.",
    .flags = D2RL::PluginFlags::Server
        | D2RL::PluginFlags::NativeHooks
        | D2RL::PluginFlags::ModScopedOnly,
};

} // namespace

D2RL_PLUGIN_EXPORT auto D2RLoaderGetPluginInfo() noexcept
        -> const D2RL::PluginInfo* {
    return &Info;
}

D2RL_PLUGIN_EXPORT auto D2RLoaderLoadPlugin(
        const D2RL::PluginContext* context) noexcept -> bool {
    using namespace RuffnecKk::MeleeSplash;
    Context = context;
    Base = context ? reinterpret_cast<std::uint8_t*>(context->exeBase) : nullptr;
    ResetState();
    if (!Context || !Base) return false;
    const auto splashStatId = RunFirstRunInstall(context);
    if (splashStatId < 0) return false;
    try {
        if (!LoadSettings(splashStatId)) return false;
    } catch (...) {
        Context->LogError(
            "MeleeSplash: failed while loading the plugin TOML settings.");
        return false;
    }
    if (!ActiveConfig.enabled) {
        Context->LogInfo(
            "Melee Splash 0.3.7 loaded disabled; no hooks installed.");
        if (!Context->RegisterConsoleCommand(
                "melee-splash", Status, "Show Melee Splash status.")) {
            Context->LogWarn(
                "MeleeSplash: optional status command could not be registered.");
        }
        return true;
    }
    // Native admission uses owned-site fingerprints, independent of build labels.
    ResolveNativeFunctions();
    if (!ValidateOwnedSites()) return false;
    if (!InstallHooks()) {
        Operational.store(false, std::memory_order_release);
        if (AnyMutationInstalled.load(std::memory_order_acquire)) {
            Context->LogError(
                "MeleeSplash: partial hook commit is inert; keep the DLL loaded "
                "and cold-restart after resolving the ownership conflict.");
            if (!Context->RegisterConsoleCommand(
                    "melee-splash", Status, "Show Melee Splash status.")) {
                Context->LogWarn(
                    "MeleeSplash: partial status command could not be registered.");
            }
            return true;
        }
        if (RelayPage) {
            VirtualFree(RelayPage, 0, MEM_RELEASE);
            RelayPage = nullptr;
        }
        Context->LogError(
            "MeleeSplash: hook installation failed before mutation.");
        return false;
    }
    Operational.store(true, std::memory_order_release);
    if (!Context->RegisterConsoleCommand(
            "melee-splash", Status, "Show Melee Splash status.")) {
        Context->LogWarn(
            "MeleeSplash: optional status command could not be registered.");
    }
    try {
        const auto path = ActiveConfigPath.empty()
            ? std::string("<absent>")
            : ActiveConfigPath.string();
        char message[1024]{};
        std::snprintf(
            message,
            sizeof(message),
            "Melee Splash 0.3.7 by MadMike active; stat=%d config=%s.",
            ActiveConfig.splashDamagePercentStatId,
            path.c_str());
        Context->LogInfo(message);
    } catch (...) {
        Context->LogInfo(
            "Melee Splash 0.3.7 by MadMike active; config path unavailable.");
    }
    return true;
}

D2RL_PLUGIN_EXPORT void D2RLoaderUnloadPlugin() noexcept {
    using namespace RuffnecKk::MeleeSplash;
    Operational.store(false, std::memory_order_release);
    try {
        std::scoped_lock lock(SidecarMutex);
        Sidecars.clear();
    } catch (...) {
    }
    Context = nullptr;
}
