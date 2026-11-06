#pragma once

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <limits>

namespace RuffnecKk::MeleeSplash {

inline constexpr std::uint16_t CriticalDeadlyResultFlag = 0x2000;
// The native squared-distance helper adds dx² + dy² in signed 32-bit space.
// 32767 keeps the worst-case diagonal sum below INT32_MAX.
inline constexpr std::int32_t MaximumSafeNativeRadius = 32767;

struct NativeSeedPair {
    std::uint32_t low{};
    std::uint32_t high{};
};

struct Native92777CriticalChances {
    std::int32_t weaponMastery{};
    std::int32_t passiveCritical{};
    std::int32_t deadlyStrike{};
};

enum class Native92777CriticalOutcome {
    None,
    WeaponMastery,
    PassiveCritical,
    DeadlyStrike,
};

inline std::uint32_t RollNativePercent(NativeSeedPair& seed) noexcept {
    const auto next = static_cast<std::uint64_t>(seed.low)
            * UINT64_C(0x6AC690C5)
        + seed.high;
    seed.low = static_cast<std::uint32_t>(next);
    seed.high = static_cast<std::uint32_t>(next >> 32);
    return seed.low % 100U;
}

inline Native92777CriticalOutcome RollNative92777CriticalDeadly(
        NativeSeedPair& seed,
        const Native92777CriticalChances& chances,
        bool allowWeaponMastery = true) noexcept {
    const auto succeeds = [&](std::int32_t chance) noexcept {
        return chance > 0
            && static_cast<std::int32_t>(RollNativePercent(seed)) < chance;
    };
    if (allowWeaponMastery && succeeds(chances.weaponMastery)) {
        return Native92777CriticalOutcome::WeaponMastery;
    }
    if (succeeds(chances.passiveCritical)) {
        return Native92777CriticalOutcome::PassiveCritical;
    }
    if (succeeds(chances.deadlyStrike)) {
        return Native92777CriticalOutcome::DeadlyStrike;
    }
    return Native92777CriticalOutcome::None;
}

inline std::int32_t ApplyNative92777CriticalMultiplier(
        std::int32_t physical,
        Native92777CriticalOutcome outcome) noexcept {
    if (outcome == Native92777CriticalOutcome::None) return physical;
    // D2R 3.2.92777 uses `add eax,eax`: reproduce the native modulo-2^32
    // behavior without invoking signed-overflow undefined behavior in C++.
    const auto bits = static_cast<std::uint32_t>(physical);
    return static_cast<std::int32_t>(bits + bits);
}

inline std::int32_t ScaleFixedDamage(
        std::int32_t value, std::int32_t percent) noexcept {
    if (value <= 0 || percent <= 0) return 0;
    const auto scaled = static_cast<std::int64_t>(value) * percent / 100;
    return static_cast<std::int32_t>(std::min<std::int64_t>(
        scaled, std::numeric_limits<std::int32_t>::max()));
}

inline std::int32_t ClampNativeRadius(std::int32_t radius) noexcept {
    return std::clamp(radius, 0, MaximumSafeNativeRadius);
}

struct PoisonSourceEntry {
    std::uint32_t sourceKey{};
    std::int32_t damage{};
    std::int32_t length{};
};
static_assert(sizeof(PoisonSourceEntry) == 12);

inline bool CaptureSourceAddition(std::int32_t accumulator,
        std::int32_t returned, std::int32_t& addition) noexcept {
    const auto delta = static_cast<std::int64_t>(returned) - accumulator;
    if (accumulator < 0 || delta < 0 || delta > INT32_MAX) return false;
    addition = static_cast<std::int32_t>(delta);
    return true;
}

inline bool RetainOwnerPoisonEntries(PoisonSourceEntry* entries,
        std::size_t& count, std::size_t prefix) noexcept {
    if (prefix > count || (count != 0 && !entries)) return false;
    for (std::size_t i = prefix; i < count; ++i) {
        if (entries[i].damage < 0 || entries[i].length < 0) return false;
    }
    if (count > prefix) std::move(entries + prefix, entries + count, entries);
    count -= prefix;
    return true;
}

struct ConversionDamage {
    std::int32_t physical{};
    std::int32_t fire{};
    std::int32_t lightning{};
    std::int32_t magic{};
    std::int32_t cold{};
    std::int32_t poison{};
    std::int32_t coldLength{};
    std::int32_t poisonLength{};
    std::int32_t burnLength{};
    std::int32_t freezeLength{};
};

inline bool SupportsConversion(std::uint8_t type, std::int32_t percent) noexcept {
    return (type <= 5 || type == 10 || type == 11 || type == 12)
        && percent >= 0 && percent <= 100;
}

// Called after the secondary's own critical result. Added elemental damage
// remains independent; only the converted physical amount changes channel.
inline bool ConvertPhysicalDamage(
        ConversionDamage& damage, std::uint8_t type,
        std::int32_t percent) noexcept {
    if (!SupportsConversion(type, percent) || damage.physical < 0) return false;
    if (type == 0) return true;
    // The captured native branch divides first for large fixed-point values.
    const auto amount = damage.physical > 0x100000
        ? (damage.physical / 100) * percent
        : static_cast<std::int32_t>(
            static_cast<std::int64_t>(damage.physical) * percent / 100);
    auto* destination = &damage.fire;
    switch (type) {
    case 1: destination = &damage.fire; break;
    case 2: destination = &damage.lightning; break;
    case 3: destination = &damage.magic; break;
    case 4: destination = &damage.cold; break;
    case 5: destination = &damage.poison; break;
    case 11: destination = &damage.fire; break;
    case 12: destination = &damage.cold; break;
    default: return false;
    }
    const auto addition = type == 5 ? amount / 8 : amount;
    const auto total = static_cast<std::int64_t>(*destination) + addition;
    if (*destination < 0 || total > std::numeric_limits<std::int32_t>::max()) {
        return false;
    }
    damage.physical -= amount;
    *destination = static_cast<std::int32_t>(total);
    if (type == 4) damage.coldLength = std::max(damage.coldLength, 50);
    if (type == 5) damage.poisonLength = std::max(damage.poisonLength, 50);
    if (type == 11) damage.burnLength = std::max(damage.burnLength, 50);
    if (type == 12) damage.freezeLength = std::max(damage.freezeLength, 50);
    return true;
}

} // namespace RuffnecKk::MeleeSplash
