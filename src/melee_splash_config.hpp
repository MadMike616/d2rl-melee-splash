#pragma once

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace RuffnecKk::MeleeSplash {

inline constexpr std::int32_t NormalAttackSkillId = 0;
inline constexpr std::int32_t DisabledStatId = -1;
inline constexpr std::int32_t AnyLayer = -1;
inline constexpr std::int32_t MaximumSplashDamagePercent = 1000;
// The native radius helper squares coordinate deltas in signed 32-bit space.
inline constexpr std::int32_t MaximumConfigRadiusTiles = 32767;

enum class ActivationMode {
	AllEligibleMelee,
	Additive,
	Whitelist,
	Blacklist,
};

inline constexpr auto ActivationModeName(ActivationMode mode) noexcept
		-> const char* {
	switch (mode) {
	case ActivationMode::Additive: return "additive";
	case ActivationMode::Whitelist: return "whitelist";
	case ActivationMode::Blacklist: return "blacklist";
	case ActivationMode::AllEligibleMelee:
	default: return "all";
	}
}

struct SkillOverride {
	std::optional<bool> enabled;
	std::optional<std::int32_t> baseRadiusTiles;
	std::optional<std::int32_t> baseSplashDamagePercent;
	std::optional<std::int32_t> fixedSplashDamagePercent;
	std::optional<bool> requireGateStat;
	bool weaponSourceOnly{};
};

struct SkillSplashSettings {
	ActivationMode activationMode{ActivationMode::AllEligibleMelee};
	std::int32_t normalWeaponRadiusTiles{4};
	std::int32_t exceptionalEliteWeaponRadiusTiles{5};
	std::map<std::int32_t, std::int32_t> fixedSplashDamagePercentBySkill;
	std::map<std::int32_t, std::int32_t> radiusTilesBySkill;
};

using SkillNameIndex = std::map<std::string, std::int32_t>;

inline std::string_view Trim(std::string_view text) noexcept {
	while (!text.empty() && (text.front() == ' ' || text.front() == '\t'
			|| text.front() == '\r' || text.front() == '\n')) {
		text.remove_prefix(1);
	}
	while (!text.empty() && (text.back() == ' ' || text.back() == '\t'
			|| text.back() == '\r' || text.back() == '\n')) {
		text.remove_suffix(1);
	}
	return text;
}

inline std::string_view StripTomlComment(std::string_view line) noexcept {
	bool quoted = false;
	bool escaped = false;
	for (std::size_t i = 0; i < line.size(); ++i) {
		const auto ch = line[i];
		if (quoted && escaped) {
			escaped = false;
			continue;
		}
		if (quoted && ch == '\\') {
			escaped = true;
			continue;
		}
		if (ch == '"') {
			quoted = !quoted;
		} else if (ch == '#' && !quoted) {
			return line.substr(0, i);
		}
	}
	return line;
}

inline bool ParseTomlInteger(
		std::string_view text, std::int32_t& result) noexcept {
	text = Trim(text);
	if (text.empty()) return false;
	const auto parsed = std::from_chars(
		text.data(), text.data() + text.size(), result, 10);
	return parsed.ec == std::errc{}
		&& parsed.ptr == text.data() + text.size();
}

inline auto NormalizeSkillName(std::string_view name) -> std::string {
	name = Trim(name);
	std::string normalized;
	normalized.reserve(name.size());
	for (const auto ch : name) {
		normalized.push_back(static_cast<char>(std::toupper(
			static_cast<unsigned char>(ch))));
	}
	return normalized;
}

inline auto SplitTabSeparatedLine(std::string_view line)
		-> std::vector<std::string_view> {
	std::vector<std::string_view> fields;
	while (true) {
		const auto tab = line.find('\t');
		fields.push_back(line.substr(0, tab));
		if (tab == std::string_view::npos) break;
		line.remove_prefix(tab + 1);
	}
	return fields;
}

// Builds the name-to-ID map from the active mod's Skills.txt. The table's
// internal `skill` column is used so names do not depend on localization.
inline auto ParseSkillNameIndex(
		std::string_view skillsTxt,
		SkillNameIndex& skillIdsByName,
		std::string& error) -> bool {
	skillIdsByName.clear();
	error.clear();
	if (skillsTxt.size() >= 3
			&& static_cast<unsigned char>(skillsTxt[0]) == 0xEF
			&& static_cast<unsigned char>(skillsTxt[1]) == 0xBB
			&& static_cast<unsigned char>(skillsTxt[2]) == 0xBF) {
		skillsTxt.remove_prefix(3);
	}
	const auto newline = skillsTxt.find('\n');
	const auto headerLine = newline == std::string_view::npos
		? skillsTxt : skillsTxt.substr(0, newline);
	auto headers = SplitTabSeparatedLine(headerLine);
	std::size_t skillColumn = headers.size();
	std::size_t idColumn = headers.size();
	for (std::size_t i = 0; i < headers.size(); ++i) {
		const auto header = NormalizeSkillName(headers[i]);
		if (header == "SKILL") skillColumn = i;
		else if (header == "*ID") idColumn = i;
	}
	if (skillColumn == headers.size() || idColumn == headers.size()) {
		error = "active Skills.txt must contain the skill and *Id columns";
		return false;
	}
	if (newline == std::string_view::npos) return true;
	skillsTxt.remove_prefix(newline + 1);
	std::size_t lineNumber = 1;
	while (!skillsTxt.empty()) {
		++lineNumber;
		const auto rowEnd = skillsTxt.find('\n');
		const auto row = rowEnd == std::string_view::npos
			? skillsTxt : skillsTxt.substr(0, rowEnd);
		skillsTxt = rowEnd == std::string_view::npos
			? std::string_view{} : skillsTxt.substr(rowEnd + 1);
		const auto fields = SplitTabSeparatedLine(row);
		if (skillColumn >= fields.size() || idColumn >= fields.size()) continue;
		const auto name = Trim(fields[skillColumn]);
		const auto idText = Trim(fields[idColumn]);
		if (name.empty() || idText.empty()) continue;
		std::int32_t skillId{};
		if (!ParseTomlInteger(idText, skillId) || skillId < 0
				|| skillId > std::numeric_limits<std::int16_t>::max()) {
			error = "invalid Skills.txt *Id on line " + std::to_string(lineNumber);
			return false;
		}
		auto [entry, inserted] = skillIdsByName.emplace(
			NormalizeSkillName(name), skillId);
		if (!inserted && entry->second != skillId) {
			error = "duplicate skill name with different IDs in Skills.txt: "
				+ std::string(name);
			return false;
		}
	}
	return true;
}

inline auto ParseSkillKey(
		std::string_view key,
		const SkillNameIndex& skillIdsByName,
		std::int32_t& skillId,
		std::string& error,
		std::size_t lineNumber) -> bool {
	key = Trim(key);
	if (ParseTomlInteger(key, skillId)) {
		if (skillId >= 0
				&& skillId <= std::numeric_limits<std::int16_t>::max()) {
			return true;
		}
		error = "skill ID on line " + std::to_string(lineNumber)
			+ " must be from 0 to "
			+ std::to_string(std::numeric_limits<std::int16_t>::max());
		return false;
	}
	std::string name;
	if (!key.empty() && key.front() == '"') {
		if (key.size() < 2 || key.back() != '"') {
			error = "invalid quoted skill name on line " + std::to_string(lineNumber);
			return false;
		}
		const auto quoted = key.substr(1, key.size() - 2);
		name.reserve(quoted.size());
		for (std::size_t i = 0; i < quoted.size(); ++i) {
			if (quoted[i] == '\\') {
				if (++i >= quoted.size()
						|| (quoted[i] != '\\' && quoted[i] != '"')) {
					error = "invalid escape in skill name on line "
						+ std::to_string(lineNumber);
					return false;
				}
			}
			name.push_back(quoted[i]);
		}
	} else {
		name.assign(key);
	}
	const auto found = skillIdsByName.find(NormalizeSkillName(name));
	if (name.empty() || found == skillIdsByName.end()) {
		error = "unknown skill name on line " + std::to_string(lineNumber)
			+ ": " + name
			+ "; names must match the active Skills.txt skill column, or use a numeric *Id";
		return false;
	}
	skillId = found->second;
	return true;
}

// Reads the small plugin-owned TOML surface used for skill-specific splash.
// Other plugin and installer sections are intentionally ignored.
inline bool ParseSkillSplashSettings(
		std::string_view toml,
		const SkillNameIndex& skillIdsByName,
		SkillSplashSettings& settings,
		std::string& error) {
	settings = SkillSplashSettings{};
	error.clear();
	enum class Section { Other, MeleeSplash, Skills, SkillRadiusTiles };
	auto section = Section::Other;
	std::size_t lineNumber = 0;
	while (!toml.empty()) {
		++lineNumber;
		const auto newline = toml.find('\n');
		const auto rawLine = newline == std::string_view::npos
			? toml : toml.substr(0, newline);
		toml = newline == std::string_view::npos
			? std::string_view{} : toml.substr(newline + 1);
		const auto line = Trim(StripTomlComment(rawLine));
		if (line.empty()) continue;
		if (line.front() == '[' && line.back() == ']') {
			const auto name = Trim(line.substr(1, line.size() - 2));
			if (name == "melee_splash") section = Section::MeleeSplash;
			else if (name == "melee_splash.skills") section = Section::Skills;
			else if (name == "melee_splash.skill_radius_tiles") {
				section = Section::SkillRadiusTiles;
			}
			else section = Section::Other;
			continue;
		}
		if (section == Section::Other) continue;
		const auto equals = line.find('=');
		if (equals == std::string_view::npos) {
			error = "expected key = value on line " + std::to_string(lineNumber);
			return false;
		}
		const auto key = Trim(line.substr(0, equals));
		const auto value = Trim(line.substr(equals + 1));
		if (section == Section::MeleeSplash) {
			if (key == "skill_mode") {
				const auto normalizedMode = NormalizeSkillName(value);
				if (normalizedMode == "\"ALL\"") {
					settings.activationMode = ActivationMode::AllEligibleMelee;
				} else if (normalizedMode == "\"ADDITIVE\"") {
					settings.activationMode = ActivationMode::Additive;
				} else if (normalizedMode == "\"WHITELIST\"") {
					settings.activationMode = ActivationMode::Whitelist;
				} else {
					error = "skill_mode on line " + std::to_string(lineNumber)
						+ " must be \"all\", \"additive\", or \"whitelist\"";
					return false;
				}
				continue;
			}
			if (key != "normal_weapon_radius_tiles"
					&& key != "exceptional_elite_weapon_radius_tiles") continue;
			std::int32_t radiusTiles{};
			if (!ParseTomlInteger(value, radiusTiles)
					|| radiusTiles < 0
					|| radiusTiles > MaximumConfigRadiusTiles) {
				error = "radius in tiles on line " + std::to_string(lineNumber)
					+ " must be from 0 to "
					+ std::to_string(MaximumConfigRadiusTiles);
				return false;
			}
			if (key == "normal_weapon_radius_tiles") {
				settings.normalWeaponRadiusTiles = radiusTiles;
			} else {
				settings.exceptionalEliteWeaponRadiusTiles = radiusTiles;
			}
			continue;
		}
		std::int32_t skillId{};
		if (!ParseSkillKey(key, skillIdsByName, skillId, error, lineNumber)) {
			return false;
		}
		std::int32_t configuredValue{};
		if (!ParseTomlInteger(value, configuredValue)) {
			error = (section == Section::Skills ? "splash percent" : "radius in tiles")
				+ std::string(" on line ") + std::to_string(lineNumber)
				+ " must be an integer";
			return false;
		}
		if (section == Section::Skills) {
			if (configuredValue < 0
					|| configuredValue > MaximumSplashDamagePercent) {
				error = "splash percent on line " + std::to_string(lineNumber)
					+ " must be from 0 to "
					+ std::to_string(MaximumSplashDamagePercent);
				return false;
			}
			if (!settings.fixedSplashDamagePercentBySkill.emplace(
					skillId, configuredValue).second) {
				error = "duplicate skill name or ID on line "
					+ std::to_string(lineNumber);
				return false;
			}
		} else {
			if (configuredValue < 0
					|| configuredValue > MaximumConfigRadiusTiles) {
				error = "radius in tiles on line " + std::to_string(lineNumber)
					+ " must be from 0 to "
					+ std::to_string(MaximumConfigRadiusTiles);
				return false;
			}
			if (!settings.radiusTilesBySkill.emplace(
					skillId, configuredValue).second) {
				error = "duplicate skill name or ID on line "
					+ std::to_string(lineNumber);
				return false;
			}
		}
	}
	return true;
}

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
	const auto* override = FindSkillOverride(config, skillId);
	if (override && override->baseSplashDamagePercent.has_value()) {
		basePercent = *override->baseSplashDamagePercent;
	}
	if (override && override->fixedSplashDamagePercent.has_value()
			&& config.activationMode != ActivationMode::Additive) {
		return std::clamp(*override->fixedSplashDamagePercent,
			0, MaximumSplashDamagePercent);
	}
	auto totalValue = static_cast<std::int64_t>(basePercent)
		+ totalSplashDamagePercent;
	if (config.activationMode == ActivationMode::Additive
			&& override && override->fixedSplashDamagePercent.has_value()) {
		totalValue += std::clamp(*override->fixedSplashDamagePercent,
			0, MaximumSplashDamagePercent);
	}
	const auto total = SaturatingNonNegative(totalValue);
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
