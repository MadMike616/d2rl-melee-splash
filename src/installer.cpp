#include "melee_splash_installer.h"
#include "melee_splash_stat_lookup.hpp"
#include "install_resource_ids.h"

#include <D2RLPlugin/context.h>

#include <Windows.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace RuffnecKk::MeleeSplash {
namespace {

namespace fs = std::filesystem;
using Bytes = std::vector<std::uint8_t>;
using Columns = std::map<std::string, std::size_t>;

constexpr wchar_t InstallMutexName[] = L"Local\\D2RLoader.ModFirstRunInstall.v1";
constexpr std::int32_t MaximumPropertyId = 65535;
constexpr std::string_view StatName = MeleeSplashStatName;
constexpr std::string_view PropertyName = "meleesplash";
constexpr std::string_view ModifierStringKey = "D2RLMeleeSplashDamage";
constexpr std::string_view SuffixName = "of Splashing";
constexpr std::array<const wchar_t*, 5> RequiredFiles{
	L"data/global/excel/itemstatcost.txt",
	L"data/global/excel/properties.txt",
	L"data/global/excel/magicsuffix.txt",
	L"data/local/lng/strings/item-modifiers.json",
	L"data/local/lng/strings/item-nameaffixes.json",
};

class FirstRunInstallLock {
public:
	explicit FirstRunInstallLock() {
		handle_ = CreateMutexW(nullptr, FALSE, InstallMutexName);
		if (handle_ == nullptr) {
			throw std::runtime_error("could not create the shared first-run install lock");
		}
		const auto result = WaitForSingleObject(handle_, 60000);
		if (result != WAIT_OBJECT_0 && result != WAIT_ABANDONED) {
			CloseHandle(handle_);
			handle_ = nullptr;
			throw std::runtime_error("timed out waiting for another plugin's first-run install");
		}
		owns_ = true;
	}

	FirstRunInstallLock(const FirstRunInstallLock&) = delete;
	auto operator=(const FirstRunInstallLock&) -> FirstRunInstallLock& = delete;

	~FirstRunInstallLock() {
		if (owns_) {
			ReleaseMutex(handle_);
		}
		if (handle_ != nullptr) {
			CloseHandle(handle_);
		}
	}

private:
	HANDLE handle_{};
	bool owns_{};
};

auto ResourceBytes(int id) -> Bytes {
	const auto module = reinterpret_cast<HMODULE>(&__ImageBase);
	const auto resource = FindResourceW(module, MAKEINTRESOURCEW(id), RT_RCDATA);
	if (resource == nullptr) {
		throw std::runtime_error("an embedded first-run data resource is missing");
	}
	const auto size = SizeofResource(module, resource);
	const auto loaded = LoadResource(module, resource);
	const auto* const data = static_cast<const std::uint8_t*>(LockResource(loaded));
	if (size == 0 || data == nullptr) {
		throw std::runtime_error("an embedded first-run data resource is empty");
	}
	return Bytes(data, data + size);
}

auto BytesToString(const Bytes& bytes) -> std::string {
	return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

auto PathText(fs::path path) -> std::string {
	const auto utf8 = path.make_preferred().u8string();
	return {reinterpret_cast<const char*>(utf8.data()), utf8.size()};
}

auto ReadFile(const fs::path& path) -> Bytes {
	std::ifstream stream(path, std::ios::binary);
	if (!stream) {
		throw std::runtime_error("could not open " + PathText(path));
	}
	return Bytes(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

auto ReadText(const fs::path& path) -> std::string {
	return BytesToString(ReadFile(path));
}

auto SplitLines(std::string_view text) -> std::vector<std::string> {
	std::vector<std::string> lines;
	std::size_t start = 0;
	while (start < text.size()) {
		const auto end = text.find('\n', start);
		const auto length = end == std::string_view::npos ? text.size() - start : end - start;
		auto line = std::string(text.substr(start, length));
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}
		if (!line.empty()) {
			lines.push_back(std::move(line));
		}
		if (end == std::string_view::npos) {
			break;
		}
		start = end + 1;
	}
	return lines;
}

auto SplitTsv(std::string_view line) -> std::vector<std::string> {
	std::vector<std::string> fields;
	std::size_t start = 0;
	while (true) {
		const auto end = line.find('\t', start);
		if (end == std::string_view::npos) {
			fields.emplace_back(line.substr(start));
			break;
		}
		fields.emplace_back(line.substr(start, end - start));
		start = end + 1;
	}
	return fields;
}

auto NormalizeTsvFields(std::string_view line, std::size_t expectedCount) -> std::vector<std::string> {
	auto fields = SplitTsv(line);
	while (fields.size() > expectedCount && fields.back().empty()) {
		fields.pop_back();
	}
	if (fields.size() > expectedCount) {
		throw std::runtime_error("a text table row has non-empty columns beyond its header");
	}
	fields.resize(expectedCount);
	return fields;
}

auto JoinTsv(const std::vector<std::string>& fields) -> std::string {
	std::string result;
	for (std::size_t i = 0; i < fields.size(); ++i) {
		if (i != 0) {
			result.push_back('\t');
		}
		result += fields[i];
	}
	return result;
}

auto ParseHeader(std::string_view line) -> std::vector<std::string> {
	auto header = SplitTsv(line);
	if (!header.empty() && header.front().starts_with("\xEF\xBB\xBF")) {
		header.front().erase(0, 3);
	}
	while (!header.empty() && header.back().empty()) {
		header.pop_back();
	}
	std::set<std::string> names;
	for (const auto& name : header) {
		if (name.empty() || !names.insert(name).second) {
			throw std::runtime_error("a text table has an empty or duplicate column name");
		}
	}
	return header;
}

auto MakeColumns(const std::vector<std::string>& header) -> Columns {
	Columns columns;
	for (std::size_t i = 0; i < header.size(); ++i) {
		columns.emplace(header[i], i);
	}
	return columns;
}

auto RequireColumn(const Columns& columns, std::string_view name) -> std::size_t {
	const auto found = columns.find(std::string(name));
	if (found == columns.end()) {
		throw std::runtime_error("a text table is missing a required column");
	}
	return found->second;
}

auto ReadInteger(std::string_view value, std::string_view what) -> std::int32_t {
	std::int32_t parsed{};
	const auto* const first = value.data();
	const auto* const last = value.data() + value.size();
	const auto [end, error] = std::from_chars(first, last, parsed);
	if (error != std::errc{} || end != last) {
		throw std::runtime_error(std::string("could not parse ") + std::string(what));
	}
	return parsed;
}

auto ChooseFreeId(const std::set<std::int32_t>& used, std::int32_t preferred, std::int32_t maximum)
	-> std::int32_t {
	if (preferred >= 0 && preferred <= maximum && used.find(preferred) == used.end()) {
		return preferred;
	}
	for (std::int32_t candidate = preferred + 1; candidate <= maximum; ++candidate) {
		if (used.find(candidate) == used.end()) {
			return candidate;
		}
	}
	for (std::int32_t candidate = 0; candidate < preferred; ++candidate) {
		if (used.find(candidate) == used.end()) {
			return candidate;
		}
	}
	throw std::runtime_error("no free row ID is available in a text table");
}

auto MapTemplateRow(
		const std::vector<std::string>& sourceHeader,
		std::string_view sourceRow,
		const std::vector<std::string>& targetHeader,
		const std::map<std::string, std::string>& overrides,
		const std::vector<std::string>& requiredColumns) -> std::string {
	const auto sourceColumns = MakeColumns(sourceHeader);
	const auto targetColumns = MakeColumns(targetHeader);
	for (const auto& name : requiredColumns) {
		(void)RequireColumn(sourceColumns, name);
		(void)RequireColumn(targetColumns, name);
	}
	auto sourceFields = NormalizeTsvFields(sourceRow, sourceHeader.size());
	std::vector<std::string> targetFields(targetHeader.size());
	for (std::size_t i = 0; i < sourceHeader.size(); ++i) {
		const auto target = targetColumns.find(sourceHeader[i]);
		if (target != targetColumns.end()) {
			targetFields[target->second] = sourceFields[i];
		}
	}
	for (const auto& [name, value] : overrides) {
		targetFields[RequireColumn(targetColumns, name)] = value;
	}
	return JoinTsv(targetFields);
}

auto BuildResourceRow(
		int resourceId,
		const std::vector<std::string>& targetHeader,
		const std::map<std::string, std::string>& overrides,
		const std::vector<std::string>& requiredColumns) -> std::string {
	const auto sourceLines = SplitLines(BytesToString(ResourceBytes(resourceId)));
	if (sourceLines.size() < 2) {
		throw std::runtime_error("an embedded text-table template is incomplete");
	}
	const auto sourceHeader = ParseHeader(sourceLines[0]);
	return MapTemplateRow(sourceHeader, sourceLines[1], targetHeader, overrides, requiredColumns);
}

auto AppendRows(std::string_view existing, const std::vector<std::string>& rows) -> std::string {
	if (rows.empty()) {
		return std::string(existing);
	}
	const std::string newline = existing.find("\r\n") == std::string_view::npos ? "\n" : "\r\n";
	std::string result(existing);
	if (result.empty() || result.back() != '\n') {
		result += newline;
	}
	for (const auto& row : rows) {
		result += row;
		result += newline;
	}
	return result;
}

struct StatMerge {
	std::string text;
	std::int32_t id;
	std::set<std::string> tooltipKeys;
};

auto MergeItemStatCost(std::string_view existing) -> StatMerge {
	const auto lines = SplitLines(existing);
	if (lines.empty()) {
		throw std::runtime_error("itemstatcost.txt is empty");
	}
	const auto header = ParseHeader(lines.front());
	const auto columns = MakeColumns(header);
	const auto statColumn = RequireColumn(columns, "Stat");
	const auto idColumn = RequireColumn(columns, "*ID");
	const auto positiveStringColumn = RequireColumn(columns, "descstrpos");
	const auto negativeStringColumn = RequireColumn(columns, "descstrneg");
	std::set<std::int32_t> usedIds;
	std::set<std::int32_t> duplicateIds;
	std::optional<std::int32_t> statId;
	std::set<std::string> tooltipKeys;
	for (std::size_t i = 1; i < lines.size(); ++i) {
		const auto fields = NormalizeTsvFields(lines[i], header.size());
		if (!fields[idColumn].empty()) {
			const auto id = ReadInteger(fields[idColumn], "an item-stat ID");
			if (id >= 0 && id <= MaximumMeleeSplashStatId && !usedIds.insert(id).second) {
				duplicateIds.insert(id);
			}
		}
		if (fields[statColumn] != StatName) continue;
		if (statId.has_value()) {
			throw std::runtime_error("itemstatcost.txt contains duplicate melee-splash stat rows");
		}
		statId = ReadInteger(fields[idColumn], "the melee-splash stat ID");
		for (const auto column : {positiveStringColumn, negativeStringColumn}) {
			if (!fields[column].empty()) tooltipKeys.insert(fields[column]);
		}
	}
	if (statId.has_value()) {
		if (*statId < 0 || *statId > MaximumMeleeSplashStatId || duplicateIds.contains(*statId)) {
			throw std::runtime_error("the existing melee-splash stat ID is invalid or shared with another row");
		}
		return {std::string(existing), *statId, std::move(tooltipKeys)};
	}

	const auto selectedId = ChooseFreeId(usedIds, 398, MaximumMeleeSplashStatId);
	const auto row = BuildResourceRow(
		IDR_MELEESPLASH_ITEMSTATCOST, header,
		{{"Stat", std::string(StatName)}, {"*ID", std::to_string(selectedId)},
		 {"descstrpos", std::string(ModifierStringKey)}, {"descstrneg", std::string(ModifierStringKey)}},
		{"Stat", "*ID", "descstrpos", "descstrneg"});
	return {AppendRows(existing, {row}), selectedId, {std::string(ModifierStringKey)}};
}

auto MergeProperties(std::string_view existing) -> std::string {
	const auto lines = SplitLines(existing);
	if (lines.empty()) {
		throw std::runtime_error("properties.txt is empty");
	}
	const auto header = ParseHeader(lines.front());
	const auto columns = MakeColumns(header);
	const auto codeColumn = RequireColumn(columns, "code");
	const auto idColumn = RequireColumn(columns, "*Id");
	const auto statColumn = RequireColumn(columns, "stat1");
	std::set<std::int32_t> usedIds;
	std::set<std::int32_t> duplicateIds;
	std::optional<std::int32_t> propertyId;
	for (std::size_t i = 1; i < lines.size(); ++i) {
		const auto fields = NormalizeTsvFields(lines[i], header.size());
		if (!fields[idColumn].empty()) {
			const auto id = ReadInteger(fields[idColumn], "a property ID");
			if (id >= 0 && id <= MaximumPropertyId && !usedIds.insert(id).second) {
				duplicateIds.insert(id);
			}
		}
		if (fields[codeColumn] != PropertyName) continue;
		if (propertyId.has_value()) {
			throw std::runtime_error("properties.txt contains duplicate melee-splash properties");
		}
		if (fields[statColumn] != StatName) {
			throw std::runtime_error("the existing melee-splash property references a different stat");
		}
		propertyId = ReadInteger(fields[idColumn], "the melee-splash property ID");
	}
	if (propertyId.has_value()) {
		if (*propertyId < 0 || *propertyId > MaximumPropertyId || duplicateIds.contains(*propertyId)) {
			throw std::runtime_error("the existing melee-splash property ID is invalid or shared with another row");
		}
		return std::string(existing);
	}
	const auto selectedId = ChooseFreeId(usedIds, 314, MaximumPropertyId);
	const auto row = BuildResourceRow(
		IDR_MELEESPLASH_PROPERTIES, header,
		{{"code", std::string(PropertyName)}, {"*Id", std::to_string(selectedId)},
		 {"stat1", std::string(StatName)}},
		{"code", "*Id", "stat1"});
	return AppendRows(existing, {row});
}

struct SuffixMerge {
	std::string text;
	std::set<std::string> affixKeys;
};

auto MergeMagicSuffix(std::string_view existing) -> SuffixMerge {
	const auto lines = SplitLines(existing);
	if (lines.empty()) {
		throw std::runtime_error("magicsuffix.txt is empty");
	}
	const auto header = ParseHeader(lines.front());
	const auto columns = MakeColumns(header);
	const auto nameColumn = RequireColumn(columns, "Name");
	const auto typeColumn = RequireColumn(columns, "itype1");
	const auto propertyColumn = RequireColumn(columns, "mod1code");
	const auto sourceLines = SplitLines(BytesToString(ResourceBytes(IDR_MELEESPLASH_MAGICSUFFIX)));
	if (sourceLines.size() != 4) {
		throw std::runtime_error("the embedded charm suffix template must have three rows");
	}
	const auto sourceHeader = ParseHeader(sourceLines.front());
	std::vector<std::string> missing;
	std::set<std::string> affixKeys;
	for (std::size_t templateIndex = 1; templateIndex < sourceLines.size(); ++templateIndex) {
		const auto mapped = MapTemplateRow(
			sourceHeader, sourceLines[templateIndex], header, {},
			{"Name", "itype1", "mod1code", "mod1min", "mod1max"});
		const auto expected = NormalizeTsvFields(mapped, header.size());
		bool found = false;
		bool conflictingName = false;
		for (std::size_t i = 1; i < lines.size(); ++i) {
			const auto fields = NormalizeTsvFields(lines[i], header.size());
			if (fields[typeColumn] != expected[typeColumn]) continue;
			if (fields[propertyColumn] == PropertyName) {
				if (fields[nameColumn].empty()) throw std::runtime_error("a melee-splash charm suffix has no name");
				affixKeys.insert(fields[nameColumn]);
				found = true;
			} else if (fields[nameColumn] == SuffixName) {
				conflictingName = true;
			}
		}
		if (!found) {
			if (conflictingName) {
				throw std::runtime_error("magicsuffix.txt has a conflicting of Splashing charm row");
			}
			missing.push_back(mapped);
			affixKeys.emplace(SuffixName);
		}
	}
	return {AppendRows(existing, missing), std::move(affixKeys)};
}

auto IsWhitespace(char value) noexcept -> bool {
	return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

auto ExtractJsonObjects(std::string_view source) -> std::vector<std::string> {
	std::vector<std::string> objects;
	std::size_t start = std::string_view::npos;
	std::uint32_t depth = 0;
	bool inString = false;
	bool escaped = false;
	for (std::size_t i = 0; i < source.size(); ++i) {
		const char character = source[i];
		if (inString) {
			if (escaped) {
				escaped = false;
			} else if (character == '\\') {
				escaped = true;
			} else if (character == '"') {
				inString = false;
			}
			continue;
		}
		if (character == '/' && i + 1 < source.size()) {
			if (source[i + 1] == '/') {
				i = source.find('\n', i + 2);
				if (i == std::string_view::npos) break;
				continue;
			}
			if (source[i + 1] == '*') {
				const auto end = source.find("*/", i + 2);
				if (end == std::string_view::npos) {
					throw std::runtime_error("a localization JSON file has an unterminated comment");
				}
				i = end + 1;
				continue;
			}
		}
		if (character == '"') {
			inString = true;
		} else if (character == '{') {
			if (depth == 0) {
				start = i;
			}
			++depth;
		} else if (character == '}') {
			if (depth == 0) {
				throw std::runtime_error("a localization JSON file has unmatched braces");
			}
			--depth;
			if (depth == 0) {
				objects.emplace_back(source.substr(start, i - start + 1));
			}
		}
	}
	if (depth != 0 || inString) {
		throw std::runtime_error("a localization JSON file has an incomplete object");
	}
	return objects;
}

auto JsonKey(const std::string& object) -> std::optional<std::string> {
	static const std::regex expression(R"REGEX("Key"\s*:\s*"((?:\\.|[^"\\])*)")REGEX");
	std::smatch match;
	if (!std::regex_search(object, match, expression)) return std::nullopt;
	return match[1].str();
}

auto LocalizationKeys(std::string_view text) -> std::set<std::string> {
	const auto first = text.find_first_not_of(" \t\r\n\xEF\xBB\xBF");
	const auto close = text.find_last_not_of(" \t\r\n");
	if (first == std::string_view::npos || text[first] != '[' || text[close] != ']') {
		throw std::runtime_error("a localization file is not a top-level JSON array");
	}
	std::set<std::string> keys;
	for (const auto& object : ExtractJsonObjects(text)) {
		if (auto key = JsonKey(object); key.has_value()) {
			keys.insert(std::move(*key));
		}
	}
	return keys;
}

void CollectLocalizationIds(std::string_view text, std::set<std::uint32_t>& ids) {
	for (std::size_t i = 0; i < text.size();) {
		if (text.substr(i).starts_with("//")) {
			i = text.find('\n', i + 2);
			if (i == std::string_view::npos) break;
			continue;
		}
		if (text.substr(i).starts_with("/*")) {
			const auto end = text.find("*/", i + 2);
			if (end == std::string_view::npos) throw std::runtime_error("an unterminated localization comment was found");
			i = end + 2;
			continue;
		}
		if (text[i++] != '"') continue;
		const auto start = i;
		while (i < text.size() && text[i] != '"') {
			if (text[i++] == '\\' && i < text.size()) ++i;
		}
		if (i == text.size()) throw std::runtime_error("an unterminated localization string was found");
		const bool isId = text.substr(start, i - start) == "id";
		++i;
		if (!isId) continue;
		const auto colon = text.find_first_not_of(" \t\r\n", i);
		if (colon == std::string_view::npos || text[colon] != ':') continue;
		const auto digit = text.find_first_not_of(" \t\r\n", colon + 1);
		if (digit == std::string_view::npos || text[digit] < '0' || text[digit] > '9') continue;
		std::uint32_t id{};
		const auto [end, error] = std::from_chars(text.data() + digit, text.data() + text.size(), id);
		if (error == std::errc{}) ids.insert(id);
		i = static_cast<std::size_t>(end - text.data());
	}
}

auto MergeLocalizedJson(
		std::string_view existing,
		int resourceId,
		std::string_view expectedKey,
		std::set<std::uint32_t>& usedIds) -> std::string {
	const auto first = existing.find_first_not_of(" \t\r\n\xEF\xBB\xBF");
	const auto close = existing.find_last_not_of(" \t\r\n");
	const auto templates = ExtractJsonObjects(BytesToString(ResourceBytes(resourceId)));
	if (templates.size() != 1
		|| JsonKey(templates.front()) != std::optional<std::string>(expectedKey)) {
		throw std::runtime_error("an embedded localization template has an unexpected layout");
	}
	auto addition = templates.front();
	std::uint32_t candidate = 63000;
	while (candidate < 65000 && !usedIds.insert(candidate).second) ++candidate;
	if (candidate == 65000) throw std::runtime_error("no free melee-splash localization ID is available");

	static const std::regex zeroId(R"REGEX("id"\s*:\s*0(?!\d))REGEX");
	std::smatch zeroIdMatch;
	if (!std::regex_search(addition, zeroIdMatch, zeroId)) {
		throw std::runtime_error("an embedded localization template has no zero string ID");
	}
	const auto zeroPosition = static_cast<std::size_t>(zeroIdMatch.position() + zeroIdMatch.length() - 1);
	addition.replace(zeroPosition, 1, std::to_string(candidate));

	std::size_t last = first;
	bool inString = false;
	bool escaped = false;
	for (std::size_t i = first + 1; i < close; ++i) {
		const auto ch = existing[i];
		if (inString) {
			if (escaped) escaped = false;
			else if (ch == '\\') escaped = true;
			else if (ch == '"') inString = false;
		} else if (existing.substr(i).starts_with("//")) {
			i = existing.find('\n', i + 2);
			if (i == std::string_view::npos || i >= close) break;
			continue;
		} else if (existing.substr(i).starts_with("/*")) {
			const auto end = existing.find("*/", i + 2);
			if (end == std::string_view::npos || end >= close) {
				throw std::runtime_error("a localization JSON file has an unterminated comment");
			}
			i = end + 1;
			continue;
		} else if (ch == '"') {
			inString = true;
		}
		if (!IsWhitespace(ch)) last = i;
	}
	if (existing[last] != '[' && existing[last] != '}' && existing[last] != ',') {
		throw std::runtime_error("a localization JSON array has an unsupported ending");
	}
	const std::string newline = existing.find("\r\n") == std::string_view::npos ? "\n" : "\r\n";
	std::string result(existing.substr(0, last + 1));
	if (existing[last] == '}') result += ',';
	result.append(existing.substr(last + 1, close - last - 1));
	result += newline;
	result += addition;
	result += newline;
	result.append(existing.substr(close));
	return result;
}

bool SameName(const fs::path& left, const fs::path& right) {
	return CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_EQUAL;
}

bool IsWithin(const fs::path& root, const fs::path& path) {
	auto candidate = path.begin();
	for (const auto& component : root) {
		if (candidate == path.end() || !SameName(component, *candidate)) return false;
		++candidate;
	}
	return candidate != path.end();
}

struct ModPaths {
	fs::path dataRoot;
	fs::path loaderRoot;
	fs::path configFile;
};

auto FindModPaths(const D2RL::PluginContext* context) -> ModPaths {
	if (context == nullptr || !context->activeMod || !context->activeMod[0]
		|| !context->pluginConfigPath || !context->pluginConfigPath[0]) {
		throw std::runtime_error("D2RLoader did not provide the active mod and plugin configuration path");
	}
	const std::string_view name(context->activeMod);
	const fs::path activeMod(std::u8string(name.begin(), name.end()));
	if (activeMod.has_parent_path() || activeMod.has_root_path()
		|| activeMod == L"." || activeMod == L".." || activeMod.filename() != activeMod) {
		throw std::runtime_error("D2RLoader provided an invalid active mod name");
	}

	const auto configFile = fs::absolute(context->pluginConfigPath).lexically_normal();
	const auto configDirectory = configFile.parent_path();
	const auto loaderDirectory = configDirectory.parent_path();
	const auto modDirectory = loaderDirectory.parent_path();
	if (!SameName(configDirectory.filename(), L"config") || !SameName(loaderDirectory.filename(), L"d2rloader")
		|| !SameName(modDirectory.filename(), activeMod)) {
		throw std::runtime_error("D2RLoader's configuration is not inside the active mod: " + PathText(configFile));
	}

	const auto modRoot = fs::canonical(modDirectory);
	const auto loaderRoot = fs::weakly_canonical(loaderDirectory);
	if (!IsWithin(modRoot, loaderRoot) || !IsWithin(loaderRoot, fs::weakly_canonical(configFile))) {
		throw std::runtime_error("D2RLoader's configuration path points outside the active mod");
	}
	const auto package = modRoot / (activeMod.native() + L".mpq");
	if (!fs::exists(package) && fs::is_directory(modRoot / L"data")) {
		return {modRoot, loaderRoot, configFile};
	}
	const auto dataRoot = fs::weakly_canonical(package);
	if (!IsWithin(modRoot, dataRoot)) {
		throw std::runtime_error("the loose data package points outside the active mod");
	}
	return {dataRoot, loaderRoot, configFile};
}

bool CheckRequiredFile(const D2RL::PluginContext& context, const fs::path& root, const fs::path& relative) {
	const auto path = root / relative;
	std::error_code error;
	const auto status = fs::status(path, error);
	if (fs::is_regular_file(status)) {
		const auto resolved = fs::canonical(path, error);
		if (!error && IsWithin(root, resolved) && !fs::is_symlink(fs::symlink_status(path))) return true;
	}
	const auto message = std::string(status.type() == fs::file_type::not_found
		? "Melee Splash: missing required file. Put it here: "
		: "Melee Splash: expected a readable, loose file inside the mod here: ") + PathText(path);
	context.LogError(message.c_str());
	return false;
}

auto ReadModText(const fs::path& root, const fs::path& relative) -> std::string {
	const auto path = root / relative;
	const auto resolved = fs::canonical(path);
	if (!IsWithin(root, resolved) || !fs::is_regular_file(resolved) || fs::is_symlink(fs::symlink_status(path))) {
		throw std::runtime_error("invalid mod file: " + PathText(path));
	}
	return ReadText(resolved);
}

auto ReadLocalizationIds(const fs::path& root) -> std::set<std::uint32_t> {
	const fs::path directory(L"data/local/lng/strings");
	std::set<std::uint32_t> used;
	for (const auto& file : fs::directory_iterator(root / directory)) {
		if (!SameName(file.path().extension(), L".json") || !file.is_regular_file()) continue;
		try {
			CollectLocalizationIds(ReadModText(root, directory / file.path().filename()), used);
		} catch (const std::exception& error) {
			throw std::runtime_error("could not index string IDs in " + PathText(file.path()) + ": " + error.what());
		}
	}
	return used;
}

void RequireCustomStrings(const std::set<std::string>& expected, const std::set<std::string>& present,
		std::string_view builtIn, const fs::path& path) {
	for (const auto& key : expected) {
		if (key != builtIn && !present.contains(key)) {
			throw std::runtime_error("missing custom localization key " + key + " in " + PathText(path));
		}
	}
}

auto Timestamp() -> std::wstring {
	const auto now = std::chrono::system_clock::now();
	const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
	const auto time = std::chrono::system_clock::to_time_t(now);
	tm localTime{};
	localtime_s(&localTime, &time);
	wchar_t buffer[64]{};
	swprintf_s(buffer, std::size(buffer), L"%04d%02d%02d-%02d%02d%02d-%03lld-%lu",
		localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday,
		localTime.tm_hour, localTime.tm_min, localTime.tm_sec,
		static_cast<long long>(milliseconds), GetCurrentProcessId());
	return buffer;
}

void WriteAtomic(const fs::path& target, std::string_view data, bool replaceExisting = true) {
	fs::create_directories(target.parent_path());
	auto temporary = target;
	temporary += L".d2rl-melee-splash-" + std::to_wstring(GetCurrentProcessId())
		+ L"-" + std::to_wstring(GetTickCount64()) + L".tmp";
	try {
		std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
		stream.write(data.data(), static_cast<std::streamsize>(data.size()));
		stream.close();
		const auto flags = MOVEFILE_WRITE_THROUGH | (replaceExisting ? MOVEFILE_REPLACE_EXISTING : 0);
		if (!stream || !MoveFileExW(temporary.c_str(), target.c_str(), flags)) {
			throw std::runtime_error("could not write " + PathText(target));
		}
	} catch (...) {
		std::error_code ignored;
		fs::remove(temporary, ignored);
		throw;
	}
}

struct Edit {
	fs::path relative;
	std::string before, after;
};

void ApplyEdits(const ModPaths& paths, const std::vector<Edit>& edits,
		std::optional<std::string_view> initialConfig) {
	fs::path backupRoot;
	if (!edits.empty()) {
		backupRoot = fs::weakly_canonical(paths.loaderRoot / L"backups/d2rl-melee-splash" / Timestamp());
		if (!IsWithin(paths.loaderRoot, backupRoot)) {
			throw std::runtime_error("the backup path points outside the mod's loader folder");
		}
		for (const auto& edit : edits) {
			const auto backup = backupRoot / edit.relative;
			if (ReadModText(paths.dataRoot, edit.relative) != edit.before) {
				throw std::runtime_error("a mod file changed during setup: " + PathText(paths.dataRoot / edit.relative));
			}
			WriteAtomic(backup, edit.before, false);
			if (ReadText(backup) != edit.before) {
				throw std::runtime_error("could not verify " + PathText(backup));
			}
		}
	}

	std::size_t written = 0;
	try {
		for (const auto& edit : edits) {
			if (ReadModText(paths.dataRoot, edit.relative) != edit.before) {
				throw std::runtime_error("a mod file changed during setup: " + PathText(paths.dataRoot / edit.relative));
			}
			WriteAtomic(paths.dataRoot / edit.relative, edit.after);
			++written;
		}
		if (initialConfig.has_value()) WriteAtomic(paths.configFile, *initialConfig, false);
	} catch (...) {
		bool restored = true;
		while (written != 0) {
			const auto& edit = edits[--written];
			try {
				if (ReadModText(paths.dataRoot, edit.relative) != edit.after) restored = false;
				else WriteAtomic(paths.dataRoot / edit.relative, edit.before);
			} catch (...) {
				restored = false;
			}
		}
		if (!restored) {
			throw std::runtime_error("could not restore every edited file; original files are in " + PathText(backupRoot));
		}
		throw;
	}
}

constexpr std::string_view DefaultConfig =
		"[melee_splash]\n"
		"# Modes: all = a listed skill replaces item meleesplash; additive = its value adds to item meleesplash.\n"
		"# whitelist = only listed skills splash, using each listed value instead of item meleesplash.\n"
		"# Use skill_mode = \"additive\" to combine item meleesplash with the configured skill values.\n"
		"skill_mode = \"all\"\n"
		"# Default radius by weapon tier, in tiles. Exceptional and elite weapons share one value.\n"
		"normal_weapon_radius_tiles = 4\n"
		"exceptional_elite_weapon_radius_tiles = 5\n"
		"# Detailed combat logs can slow down play. Enable only while troubleshooting.\n"
		"diagnostic_logging = false\n"
		"\n[melee_splash.skills]\n"
		"# Keys accept names from the active Skills.txt skill column or numeric *Id. Names are case-insensitive.\n"
		"# Names resolve against active mod data; numeric keys refer to literal IDs. Quote names with spaces.\n"
		"# Values are fixed splash percentages from 0 to 1000; 0 disables a listed skill.\n"
		"# Only supported melee hit paths splash; whitelisting a ranged projectile skill usually has no effect.\n"
		"# With additive, these skill values add to item meleesplash. With all, they replace it.\n"
		"# With whitelist, only listed skills splash and use exactly these values.\n"
		"# Whirlwind = 100\n"
		"# Bash = 100\n"
		"# Other examples: 151 = 100 or \"Left Hand Swing\" = 50\n"
		"\n[melee_splash.skill_radius_tiles]\n"
		"# Optional per-skill radius overrides, in tiles; these replace both weapon-tier defaults.\n"
		"# Radius rows do not enable a skill or add it to the whitelist.\n"
		"# Optional examples (uncomment to use):\n"
		"# Whirlwind = 8\n"
		"# Bash = 8\n"
		"# Numeric skill IDs also work, for example 151 = 8\n";

auto Install(const D2RL::PluginContext& context) -> FirstRunResult {
	const auto paths = FindModPaths(&context);
	FirstRunInstallLock installLock;
	if (fs::is_regular_file(paths.dataRoot)) {
		const auto message = "Melee Splash: unpack the mod before setup; its data package is a file: "
			+ PathText(paths.dataRoot);
		context.LogError(message.c_str());
		return {};
	}

	bool haveAll = true;
	for (const auto* relative : RequiredFiles) {
		haveAll &= CheckRequiredFile(context, paths.dataRoot, relative);
	}
	if (!haveAll) return {};

	enum Target : std::size_t { ItemStatCost, Properties, MagicSuffix, ModifierStrings, AffixStrings };
	std::array<std::string, RequiredFiles.size()> original;
	for (std::size_t i = 0; i < original.size(); ++i) {
		original[i] = ReadModText(paths.dataRoot, RequiredFiles[i]);
	}

	auto stat = MergeItemStatCost(original[ItemStatCost]);
	auto properties = MergeProperties(original[Properties]);
	auto suffix = MergeMagicSuffix(original[MagicSuffix]);
	const auto modifierKeys = LocalizationKeys(original[ModifierStrings]);
	const auto affixKeys = LocalizationKeys(original[AffixStrings]);
	RequireCustomStrings(stat.tooltipKeys, modifierKeys, ModifierStringKey,
		paths.dataRoot / RequiredFiles[ModifierStrings]);
	RequireCustomStrings(suffix.affixKeys, affixKeys, SuffixName,
		paths.dataRoot / RequiredFiles[AffixStrings]);

	const bool addModifier = stat.tooltipKeys.contains(std::string(ModifierStringKey))
		&& !modifierKeys.contains(std::string(ModifierStringKey));
	const bool addAffix = suffix.affixKeys.contains(std::string(SuffixName))
		&& !affixKeys.contains(std::string(SuffixName));
	auto usedIds = addModifier || addAffix ? ReadLocalizationIds(paths.dataRoot) : std::set<std::uint32_t>{};
	auto modifiers = addModifier
		? MergeLocalizedJson(original[ModifierStrings], IDR_MELEESPLASH_ITEM_MODIFIER_STRING, ModifierStringKey, usedIds)
		: original[ModifierStrings];
	auto affixes = addAffix
		? MergeLocalizedJson(original[AffixStrings], IDR_MELEESPLASH_AFFIX_NAME_STRING, SuffixName, usedIds)
		: original[AffixStrings];

	// Compare the actual files, including previously staged and customized installs.
	std::vector<Edit> edits;
	const auto queue = [&](Target target, std::string text) {
		if (original[target] != text) {
			edits.push_back({RequiredFiles[target], std::move(original[target]), std::move(text)});
		}
	};
	queue(ItemStatCost, std::move(stat.text));
	queue(Properties, std::move(properties));
	queue(MagicSuffix, std::move(suffix.text));
	queue(ModifierStrings, std::move(modifiers));
	queue(AffixStrings, std::move(affixes));

	std::optional<std::string_view> initialConfig;
	const auto configStatus = fs::symlink_status(paths.configFile);
	if (configStatus.type() == fs::file_type::not_found) {
		initialConfig = DefaultConfig;
	} else if (!fs::is_regular_file(configStatus)) {
		throw std::runtime_error("the plugin configuration is not a regular file: " + PathText(paths.configFile));
	}
	ApplyEdits(paths, edits, initialConfig);

	if (!edits.empty()) {
		context.LogInfo("Melee Splash: first-run install complete. Restart D2RLoader to load the splash data.");
	} else {
		context.LogInfo("Melee Splash: existing installation found; reusing its mod data.");
	}
	return {stat.id, !edits.empty()};
}

} // namespace

auto FindActiveModRootPath(const D2RL::PluginContext* context) noexcept
		-> std::filesystem::path {
	try {
		return FindModPaths(context).dataRoot;
	} catch (...) {
		return {};
	}
}

auto RunFirstRunInstall(const D2RL::PluginContext* context) noexcept -> FirstRunResult {
	if (context == nullptr) {
		return {};
	}
	try {
		return Install(*context);
	} catch (const std::exception& exception) {
		char message[512]{};
		std::snprintf(message, sizeof(message),
			"Melee Splash: first-run setup failed. %s",
			exception.what());
		context->LogError(message);
	} catch (...) {
		context->LogError("Melee Splash: first-run setup failed unexpectedly.");
	}
	return {};
}

} // namespace RuffnecKk::MeleeSplash
