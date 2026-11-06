#pragma once

#include <cstdint>

namespace D2RL {
struct PluginContext;
}

namespace RuffnecKk::MeleeSplash {

auto RunFirstRunInstall(const D2RL::PluginContext* context) noexcept
	-> std::int32_t;

} // namespace RuffnecKk::MeleeSplash
