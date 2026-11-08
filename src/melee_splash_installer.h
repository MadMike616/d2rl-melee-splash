#pragma once

#include <cstdint>
#include <filesystem>

namespace D2RL {
struct PluginContext;
}

namespace RuffnecKk::MeleeSplash {

auto FindActiveModRootPath(const D2RL::PluginContext* context) noexcept
	-> std::filesystem::path;

auto RunFirstRunInstall(const D2RL::PluginContext* context) noexcept
	-> std::int32_t;

} // namespace RuffnecKk::MeleeSplash
