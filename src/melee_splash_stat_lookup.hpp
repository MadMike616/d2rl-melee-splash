#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace RuffnecKk::MeleeSplash {

inline constexpr char MeleeSplashStatName[] = "item_melee_splash";
inline constexpr std::int32_t MaximumMeleeSplashStatId = 1023;
inline constexpr std::int32_t UnresolvedMeleeSplashStatId = -1;

enum class StatLookupStatus { Found, Missing, Ambiguous, Invalid };

struct StatLookupResult {
    StatLookupStatus status{StatLookupStatus::Invalid};
    std::int32_t id{UnresolvedMeleeSplashStatId};
};

// The linked names and compiled stat records both use the runtime ordinal.
// Reject rather than guessing if a name is duplicated or its record disagrees.
template <typename ReadName, typename ReadId>
StatLookupResult FindMeleeSplashStat(std::uint32_t rowCount, ReadName readName,
        ReadId readId) noexcept {
    std::int32_t statId = UnresolvedMeleeSplashStatId;
    for (std::uint32_t ordinal = 0; ordinal < rowCount; ++ordinal) {
        const auto isSplash = readName(ordinal);
        if (!isSplash.has_value()) return {};
        if (!*isSplash) continue;
        if (statId != UnresolvedMeleeSplashStatId) {
            return {.status = StatLookupStatus::Ambiguous};
        }
        if (ordinal > static_cast<std::uint32_t>(MaximumMeleeSplashStatId)
                || readId(ordinal) != std::optional<std::uint16_t>(static_cast<std::uint16_t>(ordinal))) {
            return {};
        }
        statId = static_cast<std::int32_t>(ordinal);
    }
    return {.status = statId >= 0 ? StatLookupStatus::Found : StatLookupStatus::Missing,
        .id = statId};
}

} // namespace RuffnecKk::MeleeSplash

namespace D2RL {
struct PluginContext;
}

namespace RuffnecKk::MeleeSplash {

bool RegisterMeleeSplashStatLookup(const D2RL::PluginContext* context, std::int32_t looseStatHint) noexcept;
void ClearMeleeSplashStatBindings() noexcept;
std::int32_t MeleeSplashStatIdForUnit(const void* unit) noexcept;
std::int32_t VerifiedMeleeSplashStatId(std::size_t bankIndex) noexcept;

} // namespace RuffnecKk::MeleeSplash

