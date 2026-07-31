#include "app/workbenches/IntegrationDerivativeLab/IntegrationDerivativeLabWorkbench.hpp"

#include "app/workbenches/IntegrationDerivativeLab/SimulationIntegrationDerivativeLab.hpp"
#include "engine/SimulationRegistry.hpp"

namespace ndde {

void register_integration_derivative_lab_workbench(SimulationRegistry& registry) {
    registry.add_runtime<SimulationIntegrationDerivativeLab>("Integration & Derivative Lab");
}

} // namespace ndde
