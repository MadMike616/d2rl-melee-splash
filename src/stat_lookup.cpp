#include "melee_splash_stat_lookup.hpp"

#include <D2RLPlugin/api.h>
#include <Windows.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstring>

namespace RuffnecKk::MeleeSplash {
namespace {

using D2RL::DataTables::Bank;
using D2RL::DataTables::TableView;

constexpr std::array Banks{Bank::Classic, Bank::Lod, Bank::Rotw};
constexpr std::size_t ItemStatCostRowSize = 0x144;
constexpr std::size_t NativeRowsOffset = 0x1258;
constexpr std::size_t NativeRowCountOffset = 0x1260;
constexpr std::size_t NativeLinkerOffset = 0x1270;
constexpr std::uint32_t MaximumTableRows = 4096;

constexpr std::uintptr_t GetDataTablesRva = 0x300A90;
constexpr std::uintptr_t GetUnitBankRva = 0x34A0E0;
constexpr std::uintptr_t GetLinkCountRva = 0xA12400;
constexpr std::uintptr_t GetLinkNameRva = 0xA12420;
constexpr std::uintptr_t LinkerLayoutWitnessRva = 0x31E35E;

// Read-only native accessors and the ItemStatCost linker layout witness.
// References: RuffDood D2RLoader Suite (ISC12 and the DataTables atlas).
constexpr std::uint8_t GetDataTablesBytes[]{
    0x48,0x83,0xEC,0x28,0x0F,0xB6,0xC1,0x48,0x89,0x44,0x24,0x38,
    0x48,0x83,0xF8,0x04,0x72,0x19,0x48,0x8D,0x44,0x24,0x38,0x48,
    0x8D,0x4C,0x24,0x40,0x48,0x89,0x44,0x24,0x40,0xE8,0x7A,0xD3,
    0xFF,0xFF,0x84,0xC0,0x74,0x01,0xCC,0x48,0x8B,0x44,0x24,0x38,
    0x48,0x8D,0x0D,0xB9,0x9A,0x79,0x02,0x48,0x03,0xC0,0x48,0x8B,
    0x04,0xC1,0x48,0x83,0xC4,0x28,0xC3,
};
constexpr std::uint8_t GetUnitBankBytes[]{
    0x48,0x83,0xEC,0x28,0x48,0x85,0xC9,0x75,0x1A,0x88,0x4C,0x24,
    0x30,0x48,0x8D,0x4C,0x24,0x30,0xE8,0x49,0xC7,0xFF,0xFF,0x84,
    0xC0,0x74,0x01,0xCC,0x32,0xC0,0x48,0x83,0xC4,0x28,0xC3,0x0F,
    0xB6,0x81,0xBD,0x01,0x00,0x00,0x48,0x83,0xC4,0x28,0xC3,
};
constexpr std::uint8_t GetLinkCountBytes[]{
    0x48,0x85,0xC9,0x74,0x05,0x48,0x8B,0x41,0x28,0xC3,0x33,0xC0,0xC3,
};
constexpr std::uint8_t GetLinkNameBytes[]{
    0x40,0x53,0x48,0x83,0xEC,0x60,0x48,0x63,0xDA,0x48,0x85,0xC9,
    0x75,0x20,0x88,0x4C,0x24,0x70,0x48,0x8D,0x4C,0x24,0x70,0xE8,
    0xD4,0xDC,0xFF,0xFF,0x84,0xC0,0x74,0x01,0xCC,
};
constexpr std::uint8_t LinkerLayoutBytes[]{
    0x48,0xC7,0x45,0xD8,0x11,0x00,0x00,0x00,0x48,0x8D,0x05,0x1F,
    0xC3,0x9D,0x01,0x4C,0x89,0x6D,0xE0,0x48,0x89,0x45,0xD0,0x48,
    0x8D,0x05,0xDC,0xE5,0x9D,0x01,0x48,0x89,0x45,0xF0,0x48,0x8D,
    0x9E,0x70,0x12,0x00,0x00,
};

using DataTablesFn = const std::uint8_t*(__fastcall*)(std::uint8_t);
using UnitBankFn = std::uint8_t(__fastcall*)(const void*);
using LinkCountFn = std::uint64_t(__fastcall*)(const void*);
using LinkNameFn = const char*(__fastcall*)(const void*, std::int32_t, std::int32_t);

DataTablesFn GetDataTables{};
UnitBankFn GetUnitBank{};
LinkCountFn GetLinkCount{};
LinkNameFn GetLinkName{};
const D2RL::DataTableService* TableApi{};
std::int32_t LooseStatHint{UnresolvedMeleeSplashStatId};
std::array<std::atomic<std::int32_t>, Banks.size()> BankStatIds{
    UnresolvedMeleeSplashStatId, UnresolvedMeleeSplashStatId, UnresolvedMeleeSplashStatId,
};

template <std::size_t Size>
bool CheckAccessor(const D2RL::PluginContext* context, std::uintptr_t rva,
        const std::uint8_t (&bytes)[Size]) noexcept {
    if (context->CheckExpectedBytes(rva, bytes, static_cast<std::uint32_t>(Size))) return true;
    char message[160]{};
    std::snprintf(message, sizeof(message),
        "Melee Splash: stat lookup witness at RVA 0x%zX did not match; item splash is disabled.",
        static_cast<std::size_t>(rva));
    context->LogWarn(message);
    return false;
}

const void* FindLinker(Bank bank, const TableView& view) noexcept {
    __try {
        const auto* tables = GetDataTables(static_cast<std::uint8_t>(bank));
        if (!tables) return nullptr;
        const void* rows{};
        std::uint64_t count{};
        const void* linker{};
        std::memcpy(&rows, tables + NativeRowsOffset, sizeof(rows));
        std::memcpy(&count, tables + NativeRowCountOffset, sizeof(count));
        std::memcpy(&linker, tables + NativeLinkerOffset, sizeof(linker));
        if (rows != view.rows || count != view.rowCount || !linker
                || GetLinkCount(linker) != count) return nullptr;
        return linker;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

bool IsSplashName(const void* linker, std::uint32_t ordinal, bool* isMatch) noexcept {
    __try {
        const auto* name = GetLinkName(linker, static_cast<std::int32_t>(ordinal), 0);
        if (!name) return false;
        for (std::size_t i = 0; i < sizeof(MeleeSplashStatName); ++i) {
            if (name[i] != MeleeSplashStatName[i]) {
                *isMatch = false;
                return true;
            }
        }
        *isMatch = true;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool ReadRowId(const void* rows, std::uint32_t ordinal, std::uint16_t* out) noexcept {
    __try {
        const auto* row = static_cast<const std::uint8_t*>(rows)
            + static_cast<std::size_t>(ordinal) * ItemStatCostRowSize;
        std::memcpy(out, row, sizeof(*out));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

StatLookupResult ResolveBankStat(const D2RL::PluginContext* context,
        Bank bank, std::uint64_t revision) noexcept {
    using namespace D2RL::DataTables;
    TableView view{.structSize = TableViewSize};
    if (TableApi->getTable(context, bank, TableId::ItemStatCost, &view) != Result::Success
            || !HasTableViewField(&view, TableViewRequiredSize)
            || view.bank != bank || view.tableId != TableId::ItemStatCost || view.revision != revision
            || !view.rows || view.rowSize != ItemStatCostRowSize
            || view.rowCount == 0 || view.rowCount > MaximumTableRows) return {};
    const auto* linker = FindLinker(bank, view);
    if (!linker) return {};
    return FindMeleeSplashStat(view.rowCount,
        [linker](std::uint32_t ordinal) noexcept -> std::optional<bool> {
            bool matches{};
            if (!IsSplashName(linker, ordinal, &matches)) return std::nullopt;
            return matches;
        },
        [&view](std::uint32_t ordinal) noexcept -> std::optional<std::uint16_t> {
            std::uint16_t id{};
            if (!ReadRowId(view.rows, ordinal, &id)) return std::nullopt;
            return id;
        });
}

void __cdecl OnDataTablesLoaded(const D2RL::PluginContext* context,
        const D2RL::Lifecycle::DataTablesLoadedEvent* event, void*) noexcept {
    ClearMeleeSplashStatBindings();
    if (!D2RL::Lifecycle::HasDataTablesLoadedEventField(
            event, D2RL::Lifecycle::DataTablesLoadedEventRequiredSize)
            || event->revision == 0) return;
    constexpr std::array bankNames{"Classic", "LoD", "RotW"};
    for (std::size_t i = 0; i < Banks.size(); ++i) {
        const auto result = ResolveBankStat(context, Banks[i], event->revision);
        char message[224]{};
        if (result.status == StatLookupStatus::Found) {
            BankStatIds[i].store(result.id, std::memory_order_release);
            std::snprintf(message, sizeof(message),
                "Melee Splash: verified loaded %s item_melee_splash stat_id=%d (loose table ID=%d).",
                bankNames[i], result.id, LooseStatHint);
            context->LogInfo(message);
        } else {
            const char* reason = result.status == StatLookupStatus::Missing ? "stat is absent"
                : result.status == StatLookupStatus::Ambiguous ? "duplicate stat name"
                : "table or linker could not be verified";
            std::snprintf(message, sizeof(message),
                "Melee Splash: %s item splash disabled (%s).", bankNames[i], reason);
            context->LogWarn(message);
        }
    }
}

} // namespace

bool RegisterMeleeSplashStatLookup(const D2RL::PluginContext* context,
        std::int32_t looseStatHint) noexcept {
    const D2RL::LifecycleService* lifecycle{};
    if (context->QueryService(&TableApi) != D2RL::ServiceQueryResult::Success
            || !D2RL::HasDataTableServiceField(TableApi,
                offsetof(D2RL::DataTableService, getTable) + sizeof(D2RL::DataTables::GetTableFn))
            || !TableApi->getTable
            || context->QueryService(&lifecycle) != D2RL::ServiceQueryResult::Success
            || !D2RL::HasLifecycleServiceField(lifecycle, offsetof(D2RL::LifecycleService,
                registerDataTablesLoadedListener) + sizeof(lifecycle->registerDataTablesLoadedListener))
            || !lifecycle->registerDataTablesLoadedListener) {
        context->LogWarn("Melee Splash: loaded-table services unavailable; item splash is disabled.");
        return false;
    }
    if (!CheckAccessor(context, GetDataTablesRva, GetDataTablesBytes)
            || !CheckAccessor(context, GetUnitBankRva, GetUnitBankBytes)
            || !CheckAccessor(context, GetLinkCountRva, GetLinkCountBytes)
            || !CheckAccessor(context, GetLinkNameRva, GetLinkNameBytes)
            || !CheckAccessor(context, LinkerLayoutWitnessRva, LinkerLayoutBytes)) return false;

    GetDataTables = reinterpret_cast<DataTablesFn>(context->exeBase + GetDataTablesRva);
    GetUnitBank = reinterpret_cast<UnitBankFn>(context->exeBase + GetUnitBankRva);
    GetLinkCount = reinterpret_cast<LinkCountFn>(context->exeBase + GetLinkCountRva);
    GetLinkName = reinterpret_cast<LinkNameFn>(context->exeBase + GetLinkNameRva);
    LooseStatHint = looseStatHint;

    const D2RL::Lifecycle::DataTablesLoadedListener listener{
        .structSize = D2RL::Lifecycle::DataTablesLoadedListenerSize,
        .callback = OnDataTablesLoaded,
    };
    D2RL::Lifecycle::ListenerHandle handle{D2RL::Lifecycle::InvalidHandle};
    if (lifecycle->registerDataTablesLoadedListener(context, &listener, &handle)
                != D2RL::Lifecycle::Result::Success
            || handle == D2RL::Lifecycle::InvalidHandle) {
        context->LogWarn("Melee Splash: could not watch loaded tables; item splash is disabled.");
        return false;
    }
    return true;
}

void ClearMeleeSplashStatBindings() noexcept {
    for (auto& id : BankStatIds) id.store(UnresolvedMeleeSplashStatId, std::memory_order_release);
}

std::int32_t MeleeSplashStatIdForUnit(const void* unit) noexcept {
    if (!unit || !GetUnitBank) return UnresolvedMeleeSplashStatId;
    __try {
        const auto bank = GetUnitBank(unit);
        if (bank < 1 || bank > BankStatIds.size()) return UnresolvedMeleeSplashStatId;
        return BankStatIds[bank - 1].load(std::memory_order_acquire);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return UnresolvedMeleeSplashStatId;
    }
}

std::int32_t VerifiedMeleeSplashStatId(std::size_t bankIndex) noexcept {
    return bankIndex < BankStatIds.size()
        ? BankStatIds[bankIndex].load(std::memory_order_acquire) : UnresolvedMeleeSplashStatId;
}

} // namespace RuffnecKk::MeleeSplash

