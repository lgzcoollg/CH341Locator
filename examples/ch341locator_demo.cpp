// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstdio>
#include "CH341Locator.h"

int main() {
    uint32_t count = GetCh341DeviceCount();
    std::printf("CH341 devices found: %u\n\n", count);

    for (uint32_t i = 0; i < count; ++i) {
        std::printf("--- Device %u ---\n", i);

        char* path = GetCh341LocationPath(i);
        if (path) {
            std::printf("USB Topology: %s\n", path);
            uint32_t idx = GetCh341IndexByLocationPathUtf8(path);
            std::printf("  reverse   : %u\n", idx);
            GetCh341FreeString(path);
        } else {
            std::printf("USB Topology: (none)\n");
        }

#if defined(_WIN32)
        char* info = GetCh341LocationInfo(i);
        if (info) {
            std::printf("LocationInfo: %s\n", info);
            uint32_t idx = GetCh341IndexByLocationInfoUtf8(info);
            std::printf("  reverse   : %u\n", idx);
            GetCh341FreeString(info);
        } else {
            std::printf("LocationInfo: (none)\n");
        }
#endif
    }

    return 0;
}
