#pragma once
// NereusSDR-original helper containers: WaveNode and HF-Auto appear in the
// container catalog as Panels. Placing one starts the helper (and keeps the
// choice in the workspace), so the container and its process persist together.
namespace NereusSDR {

class ContainerContentRegistry;

void registerHelperContent(ContainerContentRegistry& registry);

} // namespace NereusSDR
