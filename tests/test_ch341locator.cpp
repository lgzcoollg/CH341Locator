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
#include <cstring>
#include <cstdlib>

#include "mock_backend.cpp"
#include "../src/CH341Locator.cpp"

static int g_passed = 0, g_failed = 0;
#define TEST(n)  do { std::printf("  %-55s ", n); } while(0)
#define PASS()   do { std::printf("PASS\n"); ++g_passed; } while(0)
#define FAIL(m)  do { std::printf("FAIL: %s\n", m); ++g_failed; } while(0)
#define CHECK(c,m) do { if (c) PASS(); else FAIL(m); } while(0)

static void test_no_devices() {
    CH341LocatorTest::SetScenario(&CH341LocatorTest::kScenarioNoDevices);
    TEST("count = 0");
    CHECK(GetCh341DeviceCount() == 0, "0");
    TEST("GetCh341LocationPath -> NULL");
    CHECK(GetCh341LocationPath(0) == nullptr, "NULL");
    TEST("GetCh341IndexByLocationPathUtf8 -> UINT32_MAX");
    CHECK(GetCh341IndexByLocationPathUtf8("x") == UINT32_MAX, "UINT32_MAX");
}

static void test_single_device() {
    CH341LocatorTest::SetScenario(&CH341LocatorTest::kScenarioSingleCh341);
    TEST("count = 1");
    CHECK(GetCh341DeviceCount() == 1, "1");

    char* p = GetCh341LocationPath(0);
    TEST("GetCh341LocationPath -> non-NULL");
    CHECK(p != nullptr, "non-NULL");
    if (p) std::printf("  path: %s\n", p);

    TEST("reverse lookup match");
    CHECK(GetCh341IndexByLocationPathUtf8(p) == 0, "0");

    TEST("reverse lookup no-match -> UINT32_MAX");
    CHECK(GetCh341IndexByLocationPathUtf8("nonexistent") == UINT32_MAX, "UINT32_MAX");

    TEST("reverse lookup null -> UINT32_MAX");
    CHECK(GetCh341IndexByLocationPathUtf8(nullptr) == UINT32_MAX, "UINT32_MAX");

    TEST("index out of range -> NULL");
    CHECK(GetCh341LocationPath(99) == nullptr, "NULL");

    GetCh341FreeString(p);
}

static void test_two_mixed() {
    CH341LocatorTest::SetScenario(&CH341LocatorTest::kScenarioTwoCh341Mixed);
    TEST("count = 2");
    CHECK(GetCh341DeviceCount() == 2, "2");

    char* p0 = GetCh341LocationPath(0);
    char* p1 = GetCh341LocationPath(1);
    TEST("device 0 path non-NULL");
    CHECK(p0 != nullptr, "p0 non-NULL");
    TEST("device 1 path non-NULL");
    if (p0) std::printf("  path[0]: %s\n", p0);
    if (p1) std::printf("  path[1]: %s\n", p1);

    if (p0 && p1) {
        TEST("device 0 path != device 1 path");
        CHECK(strcmp(p0, p1) != 0, "different paths");
    }

    TEST("device 0 -> index 0");
    CHECK(GetCh341IndexByLocationPathUtf8(p0) == 0, "0");

    TEST("device 1 -> index 1");
    CHECK(GetCh341IndexByLocationPathUtf8(p1) == 1, "1");

    if (p0) {
        TEST("device 0 path no-match device 1");
        CHECK(GetCh341IndexByLocationPathUtf8(p0) != 1, "p0 != index 1");
    }
    if (p1) {
        TEST("device 1 path no-match device 0");
        CHECK(GetCh341IndexByLocationPathUtf8(p1) != 0, "p1 != index 0");
    }

    TEST("index out of range (2) -> NULL");
    CHECK(GetCh341LocationPath(2) == nullptr, "NULL for index 2");

    TEST("index out of range (99) -> NULL");
    CHECK(GetCh341LocationPath(99) == nullptr, "NULL for index 99");

    GetCh341FreeString(p0);
    GetCh341FreeString(p1);
}

int main() {
    std::printf("\nCH341Locator Test Suite\n========================\n\n");
    std::printf("[no devices]\n"); test_no_devices();
    std::printf("\n[single CH341]\n"); test_single_device();
    std::printf("\n[two CH341 + non-CH341]\n"); test_two_mixed();
    std::printf("\n========================\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
