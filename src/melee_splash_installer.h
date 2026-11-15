#pragma once

#include <cstdint>
#include <filesystem>

namespace D2RL {
struct PluginContext;
}

namespace RuffnecKk::MeleeSplash {

auto FindActiveModRootPath(const D2RL::PluginContext* context) noexcept
	-> std::filesystem::path;

struct FirstRunResult {
	std::int32_t statId{-1};
	bool restartRequired{};
};

auto RunFirstRunInstall(const D2RL::PluginContext* context) noexcept
	-> FirstRunResult;

} // namespace RuffnecKk::MeleeSplash
