// Copyright (c) Rux contributors.
// SPDX-License-Identifier: MIT

#include "Rux/Cli/Cli.h"

namespace Rux {
int Cli::RunVersion(GlobalOptions const &) {
    PrintVersion();
    return 0;
}
} // namespace Rux
