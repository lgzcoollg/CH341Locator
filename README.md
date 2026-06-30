# CH341Locator

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

跨平台 C 库，通过 USB 拓扑路径定位 CH341 设备（VID_1A86 + PID_5512 / PID_5523）。

| 平台 | 路径来源 | index 含义 | 返回值示例 |
|------|---------|-----------|-----------|
| Windows Vista+ | `SPDRP_LOCATION_PATHS` | 枚举序号 | `PCIROOT(0)#PCI(1400)#USBROOT(0)#USB(1)#USB(2)` |
| Windows XP | `SPDRP_LOCATION_INFORMATION` | 枚举序号 | `Port_#0001.Hub_#0002` |
| Linux | I2C adapter sysfs | `i2c-N` 总线编号 | `/dev/i2c-2` |
| macOS | `locationID`（IOKit） | 枚举序号 | `0x14200000` |

## 快速开始

```c
#include "CH341Locator.h"
#include <stdio.h>

int main() {
    uint32_t count = GetCh341DeviceCount();
    for (uint32_t i = 0; i < count; i++) {
        char* path = GetCh341LocationPath(i);
        if (path) {
            printf("Device %u: %s\n", i, path);
            uint32_t idx = GetCh341IndexByLocationPathUtf8(path);
            if (idx != UINT32_MAX) printf("  resolved: %u\n", idx);
            GetCh341FreeString(path);
        }
    }
    return 0;
}
```

## API

```c
// 通用
uint32_t GetCh341DeviceCount(void);
void     GetCh341FreeString(void* ptr);

// 正向：index → USB 拓扑路径（UTF-8）
char*    GetCh341LocationPath(uint32_t iIndex);

// 反向：USB 拓扑路径 → index（UTF-8 输入）
uint32_t GetCh341IndexByLocationPathUtf8(const char* locationPath);

// Windows XP 兼容（仅 Windows，UTF-8）
#if defined(_WIN32)
char*    GetCh341LocationInfo(uint32_t iIndex);
uint32_t GetCh341IndexByLocationInfoUtf8(const char* locationInfo);
#endif
```

## 构建

```powershell
# Windows
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --verbose -C Release

# Linux / macOS
cmake -S . -B build
cmake --build build
ctest --verbose
```

## CI

| 平台 | 构建 |
|------|------|
| GitHub Actions | Windows / Linux / macOS 矩阵构建 + 测试 + Release 打包 |
