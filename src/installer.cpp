#include "melee_splash_installer.h"
#include "install_resource_ids.h"

#include <D2RLPlugin/context.h>

#include <Windows.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cwchar>
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
#include <system_error>
#include <utility>
#include <vector>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace RuffnecKk::MeleeSplash {
namespace {

namespace fs = std::filesystem;
using Bytes = std::vector<std::uint8_t>;
using Columns = std::map<std::string, std::size_t>;

constexpr wchar_t InstallMutexName[] = L"Local\\D2RLoader.ModFirstRunInstall.v1";
constexpr std::int32_t MaximumStatId = 1023;
constexpr std::int32_t MaximumPropertyId = 65535;
constexpr std::string_view StatName = "item_melee_splash";
constexpr std::string_view PropertyName = "meleesplash";
constexpr std::string_view ModifierStringKey = "D2RLMeleeSplashDamage";
constexpr std::string_view ModifierString = "%d%% melee splash damage";
constexpr std::string_view SuffixName = "of Splashing";
constexpr std::string_view SuffixString = "of Splashing";

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

auto StringToBytes(std::string_view text) -> Bytes {
	return Bytes(text.begin(), text.end());
}

auto ReadFile(const fs::path& path) -> Bytes {
	std::ifstream stream(path, std::ios::binary);
	if (!stream) {
		throw std::runtime_error("could not open an installation target");
	}
	return Bytes(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

auto ReadText(const fs::path& path) -> std::string {
	return BytesToString(ReadFile(path));
}

auto ReadTextOrCascBase(const fs::path& path, const fs::path& cascBasePath) -> std::string {
	std::error_code error;
	const bool exists = fs::exists(path, error);
	if (error) {
		throw std::runtime_error("could not inspect an installation target");
	}
	if (exists) {
		if (!fs::is_regular_file(path, error) || error) {
			throw std::runtime_error("an installation target exists but is not a regular file");
		}
		return ReadText(path);
	}
	const bool baseExists = fs::exists(cascBasePath, error);
	if (error || !baseExists) {
		throw std::runtime_error(
			"missing CASC-extracted base file: " + cascBasePath.filename().string());
	}
	if (!fs::is_regular_file(cascBasePath, error) || error) {
		throw std::runtime_error(
			"CASC-extracted base path is not a regular file: " + cascBasePath.filename().string());
	}
	return ReadText(cascBasePath);
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

auto MergeItemStatCost(std::string_view existing) -> std::pair<std::string, std::int32_t> {
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
	std::optional<std::size_t> statLine;
	std::int32_t statId = -1;
	std::vector<std::string> addedRows;
	auto result = std::string(existing);
	for (std::size_t lineIndex = 1; lineIndex < lines.size(); ++lineIndex) {
		auto fields = NormalizeTsvFields(lines[lineIndex], header.size());
		if (!fields[idColumn].empty()) {
			const auto id = ReadInteger(fields[idColumn], "an item-stat ID");
			if (id >= 0 && id <= MaximumStatId) {
				usedIds.insert(id);
			}
		}
		if (fields[statColumn] != StatName) {
			continue;
		}
		if (statLine.has_value()) {
			throw std::runtime_error("itemstatcost.txt contains duplicate melee-splash stat rows");
		}
		statLine = lineIndex;
		statId = ReadInteger(fields[idColumn], "the melee-splash stat ID");
		if (statId < 0 || statId > MaximumStatId) {
			throw std::runtime_error("the existing melee-splash stat ID is outside the supported range");
		}
		fields[positiveStringColumn] = std::string(ModifierStringKey);
		fields[negativeStringColumn] = std::string(ModifierStringKey);
		const auto replacement = JoinTsv(fields);
		if (replacement != lines[lineIndex]) {
			const auto position = result.find(lines[lineIndex]);
			if (position == std::string::npos) {
				throw std::runtime_error("could not update the existing melee-splash stat row safely");
			}
			result.replace(position, lines[lineIndex].size(), replacement);
		}
	}
	if (statLine.has_value()) {
		return { std::move(result), statId };
	}
	statId = ChooseFreeId(usedIds, 398, MaximumStatId);
	addedRows.push_back(BuildResourceRow(
		IDR_MELEESPLASH_ITEMSTATCOST, header,
		{{"Stat", std::string(StatName)}, {"*ID", std::to_string(statId)},
		 {"descstrpos", std::string(ModifierStringKey)}, {"descstrneg", std::string(ModifierStringKey)}},
		{"Stat", "*ID", "descstrpos", "descstrneg"}));
	return { AppendRows(result, addedRows), statId };
}

auto MergeProperties(std::string_view existing) -> std::pair<std::string, std::int32_t> {
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
	std::optional<std::int32_t> propertyId;
	for (std::size_t lineIndex = 1; lineIndex < lines.size(); ++lineIndex) {
		const auto fields = NormalizeTsvFields(lines[lineIndex], header.size());
		if (!fields[idColumn].empty()) {
			const auto id = ReadInteger(fields[idColumn], "a property ID");
			if (id >= 0 && id <= MaximumPropertyId) {
				usedIds.insert(id);
			}
		}
		if (fields[codeColumn] != PropertyName) {
			continue;
		}
		if (propertyId.has_value()) {
			throw std::runtime_error("properties.txt contains duplicate melee-splash properties");
		}
		if (fields[statColumn] != StatName) {
			throw std::runtime_error("the existing melee-splash property references a different stat");
		}
		propertyId = ReadInteger(fields[idColumn], "the melee-splash property ID");
	}
	if (propertyId.has_value()) {
		return { std::string(existing), *propertyId };
	}
	const auto selectedId = ChooseFreeId(usedIds, 314, MaximumPropertyId);
	const auto row = BuildResourceRow(
		IDR_MELEESPLASH_PROPERTIES, header,
		{{"code", std::string(PropertyName)}, {"*Id", std::to_string(selectedId)},
		 {"stat1", std::string(StatName)}},
		{"code", "*Id", "stat1"});
	return { AppendRows(existing, {row}), selectedId };
}

auto MergeMagicSuffix(std::string_view existing) -> std::string {
	const auto existingLines = SplitLines(existing);
	if (existingLines.empty()) {
		throw std::runtime_error("magicsuffix.txt is empty");
	}
	const auto header = ParseHeader(existingLines.front());
	const auto columns = MakeColumns(header);
	const auto nameColumn = RequireColumn(columns, "Name");
	const auto typeColumn = RequireColumn(columns, "itype1");
	const auto propertyColumn = RequireColumn(columns, "mod1code");
	const auto minimumColumn = RequireColumn(columns, "mod1min");
	const auto maximumColumn = RequireColumn(columns, "mod1max");
	const auto sourceLines = SplitLines(BytesToString(ResourceBytes(IDR_MELEESPLASH_MAGICSUFFIX)));
	if (sourceLines.size() != 4) {
		throw std::runtime_error("the embedded charm suffix template must have three rows");
	}
	const auto sourceHeader = ParseHeader(sourceLines.front());
	std::vector<std::string> missing;
	for (std::size_t templateIndex = 1; templateIndex < sourceLines.size(); ++templateIndex) {
		const auto mapped = MapTemplateRow(
			sourceHeader, sourceLines[templateIndex], header, {},
			{"Name", "itype1", "mod1code", "mod1min", "mod1max"});
		const auto expected = NormalizeTsvFields(mapped, header.size());
		bool found = false;
		for (std::size_t lineIndex = 1; lineIndex < existingLines.size(); ++lineIndex) {
			const auto fields = NormalizeTsvFields(existingLines[lineIndex], header.size());
			if (fields[nameColumn] != SuffixName || fields[typeColumn] != expected[typeColumn]) {
				continue;
			}
			if (found || fields != expected
				|| fields[propertyColumn] != PropertyName
				|| fields[minimumColumn] != expected[minimumColumn]
				|| fields[maximumColumn] != expected[maximumColumn]) {
				throw std::runtime_error("magicsuffix.txt has a conflicting of Splashing charm row");
			}
			found = true;
		}
		if (!found) {
			missing.push_back(mapped);
		}
	}
	return AppendRows(existing, missing);
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

auto JsonStringProperty(const std::string& object, std::string_view name) -> std::optional<std::string> {
	const std::regex expression("\\\"" + std::string(name) + "\\\"\\s*:\\s*\\\"((?:\\\\.|[^\\\"\\\\])*)\\\"");
	std::smatch match;
	if (!std::regex_search(object, match, expression) || match.size() < 2) {
		return std::nullopt;
	}
	return match[1].str();
}

auto JsonNumericProperty(const std::string& object, std::string_view name) -> std::optional<std::uint32_t> {
	const std::regex expression("\\\"" + std::string(name) + "\\\"\\s*:\\s*(\\d+)");
	std::smatch match;
	if (!std::regex_search(object, match, expression) || match.size() < 2) {
		return std::nullopt;
	}
	const auto parsed = std::stoul(match[1].str());
	if (parsed > 65535) {
		return std::nullopt;
	}
	return static_cast<std::uint32_t>(parsed);
}

auto MergeLocalizedJson(
		std::string_view existing,
		int resourceId,
		std::string_view expectedKey,
		std::string_view expectedEnglish) -> std::string {
	const auto first = existing.find_first_not_of(" \t\r\n\xEF\xBB\xBF");
	const auto close = existing.find_last_not_of(" \t\r\n");
	if (first == std::string_view::npos || close == std::string_view::npos
		|| existing[first] != '[' || existing[close] != ']') {
		throw std::runtime_error("a localization file is not a top-level JSON array");
	}
	const auto objects = ExtractJsonObjects(existing);
	std::set<std::uint32_t> usedIds;
	bool keyAlreadyExists = false;
	for (const auto& object : objects) {
		if (const auto id = JsonNumericProperty(object, "id"); id.has_value()) {
			usedIds.insert(*id);
		}
		const auto key = JsonStringProperty(object, "Key");
		if (!key.has_value() || *key != expectedKey) {
			continue;
		}
		const auto english = JsonStringProperty(object, "enUS");
		if (!english.has_value() || *english != expectedEnglish) {
			throw std::runtime_error("a localization key already exists with different English text");
		}
		keyAlreadyExists = true;
	}
	if (keyAlreadyExists) {
		return std::string(existing);
	}

	const auto templates = ExtractJsonObjects(BytesToString(ResourceBytes(resourceId)));
	if (templates.size() != 1) {
		throw std::runtime_error("an embedded localization template has an unexpected layout");
	}
	auto addition = templates.front();
	if (JsonStringProperty(addition, "Key") != std::optional<std::string>(expectedKey)
		|| JsonStringProperty(addition, "enUS") != std::optional<std::string>(expectedEnglish)) {
		throw std::runtime_error("an embedded localization template has unexpected text");
	}
	std::uint32_t candidate = usedIds.empty() ? 1 : (*usedIds.rbegin() < 65535 ? *usedIds.rbegin() + 1 : 1);
	bool found = false;
	for (std::uint32_t attempt = 0; attempt < 65535; ++attempt) {
		if (candidate == 0 || candidate > 65535) {
			candidate = 1;
		}
		if (usedIds.find(candidate) == usedIds.end()) {
			found = true;
			break;
		}
		++candidate;
	}
	if (!found) {
		throw std::runtime_error("a localization file has no free string ID");
	}
	const std::regex zeroId(R"REGEX("id"\s*:\s*0(?!\d))REGEX");
	std::smatch zeroIdMatch;
	if (JsonNumericProperty(addition, "id") != std::optional<std::uint32_t>(0)
		|| !std::regex_search(addition, zeroIdMatch, zeroId)) {
		throw std::runtime_error("an embedded localization template has no zero string ID");
	}
	const auto zeroPosition = static_cast<std::size_t>(
		zeroIdMatch.position() + zeroIdMatch.length() - 1);
	addition.replace(zeroPosition, 1, std::to_string(candidate));

	const auto arrayOpen = existing.find('[', first);
	const auto innerStart = arrayOpen + 1;
	std::size_t innerEnd = close;
	while (innerEnd > innerStart && IsWhitespace(existing[innerEnd - 1])) {
		--innerEnd;
	}
	const bool hasEntries = existing.find_first_not_of(" \t\r\n", innerStart) < close;
	std::string result(existing.substr(0, innerEnd));
	if (hasEntries && result.back() != ',') {
		result.push_back(',');
	}
	const std::string newline = existing.find("\r\n") == std::string_view::npos ? "\n" : "\r\n";
	result += newline;
	result += addition;
	result += newline;
	result.append(existing.substr(close));
	return result;
}

auto FindActiveModRoot(const D2RL::PluginContext* context) -> fs::path {
	if (context == nullptr || context->activeMod == nullptr || context->activeMod[0] == '\0') {
		return {};
	}
	const fs::path activeMod(context->activeMod);
	if (activeMod.has_parent_path() || activeMod.has_root_path()
		|| activeMod == L"." || activeMod == L".." || activeMod.filename() != activeMod) {
		return {};
	}
	std::vector<fs::path> candidates;
	const fs::path modDirectory = context->modDirectory == nullptr ? fs::path{} : fs::path(context->modDirectory);
	const fs::path scopeRoot = context->scopeRootDirectory == nullptr ? fs::path{} : fs::path(context->scopeRootDirectory);
	const auto packageName = activeMod.native() + std::wstring(L".mpq");
	const auto addBase = [&](const fs::path& base) {
		if (base.empty()) {
			return;
		}
		candidates.push_back(base / activeMod / packageName);
		candidates.push_back(base / packageName);
		candidates.push_back(base / activeMod);
		candidates.push_back(base);
	};
	addBase(modDirectory);
	if (!scopeRoot.empty()) {
		candidates.push_back(scopeRoot / activeMod / packageName);
		candidates.push_back(scopeRoot / packageName);
		candidates.push_back(scopeRoot / activeMod);
		candidates.push_back(scopeRoot / L"mods" / activeMod / packageName);
		addBase(scopeRoot);
	}
	constexpr std::array<const wchar_t*, 5> targets{
		L"data/global/excel/itemstatcost.txt",
		L"data/global/excel/properties.txt",
		L"data/global/excel/magicsuffix.txt",
		L"data/local/lng/strings/item-modifiers.json",
		L"data/local/lng/strings/item-nameaffixes.json",
	};
	std::set<std::wstring> seen;
	std::optional<fs::path> bestCandidate;
	std::size_t bestFileCount = 0;
	for (const auto& candidate : candidates) {
		const auto normalized = candidate.lexically_normal();
		if (!seen.insert(normalized.native()).second) {
			continue;
		}
		const auto folderName = normalized.filename();
		if (folderName != activeMod && folderName != fs::path(packageName)) {
			continue;
		}
		std::error_code error;
		if (!fs::is_directory(normalized, error) || error) {
			continue;
		}
		std::size_t fileCount = 0;
		for (const auto* relative : targets) {
			const auto target = normalized / relative;
			error.clear();
			if (fs::exists(target, error) && !error
				&& fs::is_regular_file(target, error) && !error) {
				++fileCount;
			}
			if (error) {
				fileCount = 0;
				break;
			}
		}
		if (!bestCandidate.has_value() || fileCount > bestFileCount) {
			bestCandidate = normalized;
			bestFileCount = fileCount;
		}
		if (fileCount == targets.size()) {
			return normalized;
		}
	}
	return bestCandidate.value_or(fs::path{});
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

void WriteAtomic(const fs::path& target, const Bytes& data) {
	std::error_code error;
	fs::create_directories(target.parent_path(), error);
	if (error) {
		throw std::runtime_error("could not create an installation directory");
	}
	const auto temporary = target.wstring() + L".d2rl-melee-splash-" + std::to_wstring(GetCurrentProcessId()) + L".tmp";
	{
		std::ofstream stream(fs::path(temporary), std::ios::binary | std::ios::trunc);
		if (!stream) {
			throw std::runtime_error("could not create a temporary installation file");
		}
		stream.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
		stream.flush();
		if (!stream) {
			stream.close();
			fs::remove(temporary, error);
			throw std::runtime_error("could not write a temporary installation file");
		}
	}
	if (!MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
		fs::remove(temporary, error);
		throw std::runtime_error("could not atomically replace an installation file");
	}
}

auto MarkerStatId(const fs::path& markerPath) -> std::optional<std::int32_t> {
	std::error_code error;
	const bool exists = fs::exists(markerPath, error);
	if (error) {
		throw std::runtime_error("could not inspect the melee-splash TOML marker");
	}
	if (!exists) {
		return std::nullopt;
	}
	if (!fs::is_regular_file(markerPath, error) || error) {
		throw std::runtime_error("the melee-splash TOML marker path is not a regular file");
	}
	const auto marker = ReadText(markerPath);
	const std::regex complete(R"REGEX((?:^|\n)\s*complete\s*=\s*true\s*(?:\r?$))REGEX", std::regex_constants::icase | std::regex_constants::multiline);
	const std::regex statIdExpression(R"REGEX((?:^|\n)\s*stat_id\s*=\s*(\d+)\s*(?:\r?$))REGEX", std::regex_constants::icase | std::regex_constants::multiline);
	std::smatch completeMatch;
	std::smatch statMatch;
	if (!std::regex_search(marker, completeMatch, complete)
		|| !std::regex_search(marker, statMatch, statIdExpression) || statMatch.size() < 2) {
		throw std::runtime_error("the melee-splash TOML marker is incomplete or missing stat_id");
	}
	const auto statId = ReadInteger(statMatch[1].str(), "the melee-splash TOML stat_id");
	if (statId < 0 || statId > MaximumStatId) {
		throw std::runtime_error("the melee-splash TOML stat_id is outside the supported range");
	}
	return statId;
}

auto Install(const D2RL::PluginContext* context) -> std::int32_t {
	if (context->pluginConfigPath == nullptr || context->pluginConfigPath[0] == L'\0') {
		throw std::runtime_error("D2RLoader did not provide the plugin TOML path");
	}
	const fs::path loaderConfigPath(context->pluginConfigPath);
	const auto configDirectory = loaderConfigPath.parent_path();
	if (configDirectory.empty()) {
		throw std::runtime_error("D2RLoader's plugin config directory could not be resolved");
	}
	const auto markerPath = configDirectory / L"d2rl-melee-splash.toml";
	if (const auto statId = MarkerStatId(markerPath); statId.has_value()) {
		context->LogInfo("Melee Splash: installation marker found; skipping mod-file edits.");
		return *statId;
	}

	const auto modRoot = FindActiveModRoot(context);
	if (modRoot.empty()) {
		throw std::runtime_error("could not resolve the active mod directory");
	}
	FirstRunInstallLock installLock;
	if (const auto statId = MarkerStatId(markerPath); statId.has_value()) {
		context->LogInfo("Melee Splash: another plugin instance completed installation; skipping mod-file edits.");
		return *statId;
	}

	const auto statCostRelative = fs::path(L"data/global/excel/itemstatcost.txt");
	const auto propertiesRelative = fs::path(L"data/global/excel/properties.txt");
	const auto suffixRelative = fs::path(L"data/global/excel/magicsuffix.txt");
	const auto modifierStringsRelative = fs::path(L"data/local/lng/strings/item-modifiers.json");
	const auto affixStringsRelative = fs::path(L"data/local/lng/strings/item-nameaffixes.json");
	const auto cascBaseDirectory = configDirectory / L"d2rl-melee-splash" / L"base";
	const auto [mergedStatCost, statId] = MergeItemStatCost(
		ReadTextOrCascBase(modRoot / statCostRelative, cascBaseDirectory / L"itemstatcost.txt"));
	const auto [mergedProperties, propertyId] = MergeProperties(
		ReadTextOrCascBase(modRoot / propertiesRelative, cascBaseDirectory / L"properties.txt"));
	const auto mergedSuffixes = MergeMagicSuffix(
		ReadTextOrCascBase(modRoot / suffixRelative, cascBaseDirectory / L"magicsuffix.txt"));
	const auto mergedModifierStrings = MergeLocalizedJson(
		ReadTextOrCascBase(modRoot / modifierStringsRelative, cascBaseDirectory / L"item-modifiers.json"),
		IDR_MELEESPLASH_ITEM_MODIFIER_STRING,
		ModifierStringKey, ModifierString);
	const auto mergedAffixStrings = MergeLocalizedJson(
		ReadTextOrCascBase(modRoot / affixStringsRelative, cascBaseDirectory / L"item-nameaffixes.json"),
		IDR_MELEESPLASH_AFFIX_NAME_STRING,
		SuffixName, SuffixString);

	std::map<fs::path, Bytes> desired;
	desired.emplace(statCostRelative, StringToBytes(mergedStatCost));
	desired.emplace(propertiesRelative, StringToBytes(mergedProperties));
	desired.emplace(suffixRelative, StringToBytes(mergedSuffixes));
	desired.emplace(modifierStringsRelative, StringToBytes(mergedModifierStrings));
	desired.emplace(affixStringsRelative, StringToBytes(mergedAffixStrings));

	const auto loaderDirectory = configDirectory.parent_path();
	if (loaderDirectory.empty()) {
		throw std::runtime_error("the mod's D2RLoader folder could not be resolved");
	}
	const auto backupRoot = loaderDirectory / L"backups/d2rl-melee-splash" / Timestamp();
	std::error_code error;
	fs::create_directories(backupRoot, error);
	if (error) {
		throw std::runtime_error("could not create a first-run backup directory");
	}
	std::vector<fs::path> createdFromBase;
	for (const auto& [relative, bytes] : desired) {
		(void)bytes;
		const auto original = modRoot / relative;
		const bool exists = fs::exists(original, error);
		if (error) {
			throw std::runtime_error("could not inspect an installation target before backup");
		}
		if (!exists) {
			createdFromBase.push_back(relative);
			continue;
		}
		if (!fs::is_regular_file(original, error) || error) {
			throw std::runtime_error("an installation target is not a regular file");
		}
		const auto backup = backupRoot / relative;
		fs::create_directories(backup.parent_path(), error);
		if (error || !fs::copy_file(original, backup, fs::copy_options::none, error) || error) {
			throw std::runtime_error("a first-run backup file could not be created; mod files were not changed");
		}
	}
	if (!createdFromBase.empty()) {
		std::string manifest = "These target files were absent and were created from player-supplied CASC base files:\n";
		for (const auto& relative : createdFromBase) {
			manifest += relative.generic_string();
			manifest += '\n';
		}
		WriteAtomic(backupRoot / L"created-from-casc-base.txt", StringToBytes(manifest));
	}

	std::size_t changedFiles = 0;
	std::size_t createdFiles = 0;
	for (const auto& [relative, bytes] : desired) {
		const auto target = modRoot / relative;
		std::error_code targetError;
		const bool exists = fs::exists(target, targetError);
		if (targetError) {
			throw std::runtime_error("could not inspect an installation target before writing");
		}
		if (!exists || ReadFile(target) != bytes) {
			if (!exists) {
				++createdFiles;
			}
			WriteAtomic(target, bytes);
			++changedFiles;
		}
	}

	const std::string marker =
		"[installation]\n"
		"complete = true\n"
		"plugin_version = \"0.3.1\"\n"
		"stat_id = " + std::to_string(statId) + "\n"
		"property_id = " + std::to_string(propertyId) + "\n";
	WriteAtomic(markerPath, StringToBytes(marker));
	char message[256]{};
	std::snprintf(message, sizeof(message),
		"Melee Splash: first-run install completed; %zu data files updated, %zu created from supplied CASC base files, backup created, stat_id=%d.",
		changedFiles, createdFiles, statId);
	context->LogInfo(message);
	return statId;
}

} // namespace

auto RunFirstRunInstall(const D2RL::PluginContext* context) noexcept -> std::int32_t {
	if (context == nullptr) {
		return -1;
	}
	try {
		return Install(context);
	} catch (const std::exception& exception) {
		char message[512]{};
		std::snprintf(message, sizeof(message),
			"Melee Splash: first-run installation failed (%s); no completion marker was created.",
			exception.what());
		context->LogError(message);
	} catch (...) {
		context->LogError("Melee Splash: first-run installation failed unexpectedly; no completion marker was created.");
	}
	return -1;
}

} // namespace RuffnecKk::MeleeSplash
