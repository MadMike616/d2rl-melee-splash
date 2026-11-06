#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <vector>

namespace RuffnecKk::MeleeSplash {

inline constexpr std::int32_t NormalAttackSkillId = 0;
inline constexpr std::int32_t DisabledStatId = -1;
inline constexpr std::int32_t AnyLayer = -1;
inline constexpr std::int32_t MaximumSplashDamagePercent = 1000;

enum class ActivationMode {
	AllEligibleMelee,
	Whitelist,
	Blacklist,
};

struct SkillOverride {
	std::optional<bool> enabled;
	std::optional<std::int32_t> baseRadiusTiles;
	std::optional<std::int32_t> baseSplashDamagePercent;
	std::optional<bool> requireGateStat;
	bool weaponSourceOnly{};
};

struct CharmTier {
	std::int32_t statId{};
	std::uint16_t layer{};
	std::int32_t damagePercent{};
	std::int32_t radiusTiles{};
};

struct LegacyEvent20Suppression {
	bool enabled{};
	std::int32_t statId{DisabledStatId};
	std::int32_t layer{AnyLayer};
	bool playerAttackersOnly{true};
};

// The installer assigns one additive 10-bit item stat. Cap the game-facing
// percentage at 1000 even though the packed stat can represent up to 1023.
struct Config {
	bool enabled{true};
	ActivationMode activationMode{ActivationMode::AllEligibleMelee};
	bool allowNormalAttack{true};
	std::vector<std::int32_t> includedSkillIds;
	std::vector<std::int32_t> excludedSkillIds;
	bool requireGateStat{};
	std::int32_t gateStatId{DisabledStatId};
	std::int32_t increasedRadiusStatId{DisabledStatId};
	std::int32_t radiusPercentPerTile{20};
	std::int32_t splashDamagePercentStatId{DisabledStatId};
	std::int32_t baseSplashDamagePercent{};
	std::int32_t baseRadiusNormalWeapon{4};
	std::int32_t baseRadiusExceptionalEliteWeapon{5};
	std::int32_t maximumRadiusTiles{};
	bool diagnosticLogging{true};
	std::map<std::int32_t, SkillOverride> skillOverrides;
	LegacyEvent20Suppression legacyEvent20Suppression;
	bool requireCharmTier{};
	std::vector<CharmTier> charmTiers;
	bool enablePhysicalConversion{};
};

struct UnitIdentity {
	std::int32_t type{};
	std::uint32_t guid{};

	friend constexpr bool operator==(
		const UnitIdentity&, const UnitIdentity&) noexcept = default;
};

template <class ReadMarker>
inline const CharmTier* ResolveCharmTier(
		const Config& config, ReadMarker&& readMarker) {
	for (auto tier = config.charmTiers.rbegin();
			tier != config.charmTiers.rend(); ++tier) {
		if (readMarker(tier->statId, tier->layer) > 0) return &*tier;
	}
	return nullptr;
}

inline bool ContainsSkill(
		const std::vector<std::int32_t>& skills,
		std::int32_t skillId) noexcept {
	return std::find(skills.begin(), skills.end(), skillId) != skills.end();
}

inline const SkillOverride* FindSkillOverride(
		const Config& config, std::int32_t skillId) noexcept {
	const auto entry = config.skillOverrides.find(skillId);
	return entry == config.skillOverrides.end() ? nullptr : &entry->second;
}

inline bool IsSkillEnabled(
		const Config& config, std::int32_t skillId) noexcept {
	if (!config.enabled || skillId < 0
			|| ContainsSkill(config.excludedSkillIds, skillId)) {
		return false;
	}
	if (skillId == NormalAttackSkillId && !config.allowNormalAttack) return false;
	if (config.activationMode == ActivationMode::Whitelist
			&& !ContainsSkill(config.includedSkillIds, skillId)) {
		return false;
	}
	const auto* override = FindSkillOverride(config, skillId);
	return !override || override->enabled.value_or(true);
}

inline bool RequiresGateStat(
		const Config& config, std::int32_t skillId) noexcept {
	if (const auto* override = FindSkillOverride(config, skillId);
			override && override->requireGateStat.has_value()) {
		return *override->requireGateStat;
	}
	return config.requireGateStat;
}

inline std::int32_t SaturatingNonNegative(std::int64_t value) noexcept {
	if (value <= 0) return 0;
	if (value >= std::numeric_limits<std::int32_t>::max()) {
		return std::numeric_limits<std::int32_t>::max();
	}
	return static_cast<std::int32_t>(value);
}

inline std::int32_t ResolveRadiusTiles(
		const Config& config,
		std::int32_t skillId,
		bool exceptionalOrEliteWeapon,
		std::int32_t totalRadiusPercent) noexcept {
	auto baseRadius = exceptionalOrEliteWeapon
		? config.baseRadiusExceptionalEliteWeapon
		: config.baseRadiusNormalWeapon;
	if (const auto* override = FindSkillOverride(config, skillId);
			override && override->baseRadiusTiles.has_value()) {
		baseRadius = *override->baseRadiusTiles;
	}
	const auto bonus = std::max(totalRadiusPercent, 0)
		/ std::max(config.radiusPercentPerTile, 1);
	auto result = SaturatingNonNegative(static_cast<std::int64_t>(baseRadius) + bonus);
	if (config.maximumRadiusTiles > 0) {
		result = std::min(result, config.maximumRadiusTiles);
	}
	return result;
}

inline std::int32_t ResolveSplashDamagePercent(
		const Config& config,
		std::int32_t skillId,
		std::int32_t totalSplashDamagePercent) noexcept {
	auto basePercent = config.baseSplashDamagePercent;
	if (const auto* override = FindSkillOverride(config, skillId);
			override && override->baseSplashDamagePercent.has_value()) {
		basePercent = *override->baseSplashDamagePercent;
	}
	const auto total = SaturatingNonNegative(
		static_cast<std::int64_t>(basePercent) + totalSplashDamagePercent);
	return std::min(total, MaximumSplashDamagePercent);
}

inline bool MatchesLegacyEvent20Suppression(
		const LegacyEvent20Suppression& suppression,
		std::uint32_t packedStatAndLayer,
		bool attackerIsPlayer) noexcept {
	if (!suppression.enabled || suppression.statId < 0 || suppression.layer < 0
			|| (suppression.playerAttackersOnly && !attackerIsPlayer)) {
		return false;
	}
	const auto statId = static_cast<std::uint16_t>(packedStatAndLayer >> 16);
	const auto layer = static_cast<std::uint16_t>(packedStatAndLayer);
	return statId == static_cast<std::uint16_t>(suppression.statId)
		&& layer == static_cast<std::uint16_t>(suppression.layer);
}

inline bool MatchesLegacyEvent20Suppression(
		const Config& config,
		std::uint32_t packedStatAndLayer,
		bool attackerIsPlayer) noexcept {
	return config.enabled && MatchesLegacyEvent20Suppression(
		config.legacyEvent20Suppression, packedStatAndLayer, attackerIsPlayer);
}

inline bool AppendUniqueSecondaryTarget(
		std::vector<UnitIdentity>& targets,
		UnitIdentity candidate,
		UnitIdentity attacker,
		UnitIdentity primary) {
	if (candidate == attacker || candidate == primary
			|| std::find(targets.begin(), targets.end(), candidate) != targets.end()) {
		return false;
	}
	targets.emplace_back(candidate);
	return true;
}

} // namespace RuffnecKk::MeleeSplash
