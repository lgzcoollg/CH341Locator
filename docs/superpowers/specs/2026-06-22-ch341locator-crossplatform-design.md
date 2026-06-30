# CH341Locator 跨平台设计

## 概述

将 CH341Locator 从 Windows-only 扩展到 Linux 和 macOS，保持同一套 C API 语义不变：

```
index（i2c-N / 序号）  ────→  USB 拓扑路径
USB 拓扑路径            ────→  index（i2c-N / 序号）
```

调用方不关心底层 OS，调用同一个函数，拿到统一语义的结果。

## 目标

- 三平台共享同一套 public API 头文件
- 每个平台用对应的原生手段实现 USB 枚举和路径解析
- Linux 返回 `i2c-N` 作为 "index"，USB sysfs 路径作为 "location path"
- macOS 返回 0-based 序号作为 "index"，`locationID` 作为 "location path"
- 所有 A 后缀函数统一输出 UTF-8（手动编码，不依赖 OS 代码页）
- 保持零外部依赖（或仅 libusb 作为可选统一后端）

## 非目标

- 不实现 USB→I2C 数据通信（不读写寄存器）
- 不实现热插拔事件回调
- 不保证 index 在拔插后稳定（依赖各平台动态分配行为）

## 平台对比

| 维度 | Windows | Linux | macOS |
|------|---------|-------|-------|
| USB 枚举 API | `SetupAPI`（`SetupDiGetClassDevsW`） | `sysfs`（`/sys/bus/usb/devices/`）或 `libusb` | `IOKit`（`IOUSBHostDevice`）或 `libusb` |
| 匹配 VID/PID | `SPDRP_HARDWAREID` | `uevent` 文件或 `libusb_get_device_descriptor` | `IOKit` property 匹配或 `libusb` |
| "index" 含义 | 枚举顺序（0, 1, 2…） | **`i2c-N` 总线编号** | 枚举顺序（0, 1, 2…） |
| 拓扑路径来源 | `SPDRP_LOCATION_PATHS` / `SPDRP_LOCATION_INFORMATION` | USB sysfs 设备路径（`1-2.3`） | `locationID`（`0x14200000`） |
| Windows 兼容 | Vista+ / XP 两组 API | 不适用 | 不适用 |
| I2C 总线抽象 | 无（用路径反查 index） | `/sys/class/i2c-adapter/i2c-N/` | 不存在 |
| 编译环境 | MSVC + Windows SDK | GCC/Clang + Linux 头文件 | Clang + Xcode |

## API 设计

### 跨平台统一 API

```c
// ============================================================
// 通用
// ============================================================

/// 返回当前连接的 CH341 设备数量（VID_1A86 + PID_5512/PID_5523）
uint32_t GetCh341DeviceCount(void);

/// 释放 GetCh341LocationPath 返回的字符串
void     GetCh341FreeString(void* ptr);

// ============================================================
// 正向：index（i2c-N / 序号） → USB 拓扑路径
// ============================================================

/// 返回第 iIndex 个 CH341 设备的 USB 拓扑路径（UTF-8）
char*    GetCh341LocationPath(uint32_t iIndex);

/// 返回第 iIndex 个 CH341 设备的 USB 拓扑路径（UTF-16）
wchar_t* GetCh341LocationPathW(uint32_t iIndex);

// ============================================================
// 反向：USB 拓扑路径 → index（i2c-N / 序号）
// ============================================================

/// 根据 USB 拓扑路径反查 index
uint32_t GetCh341IndexByLocationPath(const wchar_t* locationPath);

/// 根据 USB 拓扑路径反查 index（UTF-8 输入）
uint32_t GetCh341IndexByLocationPathUtf8(const char* locationPath);
```

> **注意**：跨平台统一后，不再保留 Windows-only 的 `*Info*` 系 API（`SPDRP_LOCATION_INFORMATION`），因为 Linux/macOS 没有对应概念。XP 兼容场景的用户应继续使用 Windows 专用构建的 DLL。

### 各平台行为

| 函数 | Windows | Linux | macOS |
|------|---------|-------|-------|
| `GetCh341DeviceCount()` | SetupAPI 枚举 | 遍历 sysfs 或 libusb | IOKit 或 libusb |
| `GetCh341LocationPath(i)` | `SPDRP_LOCATION_PATHS` | 将 `i2c-N` 的 `N` 作为 index，返回 USB sysfs 路径 | 返回 `locationID` 格式化字符串 |
| `GetCh341IndexByLocationPath(path)` | 匹配 `SPDRP_LOCATION_PATHS` | 匹配 USB sysfs 路径，返回对应的 `i2c-N` | 匹配 `locationID`，返回 index |

### 路径格式对照

```
Windows → "PCIROOT(0)#PCI(1400)#USBROOT(0)#USB(1)#USB(2)"
Linux   → "1-2.3"                      （USB hub:port 拓扑）
macOS   → "0x14200000"                 （IOKit locationID）
```

## Linux 实现方案

### sysfs 路径结构

```
/sys/bus/usb/devices/
  ├── 1-0:1.0/              # Root hub
  ├── 1-1/                  # USB 端口 1
  │   ├── uevent            # PRODUCT=1a86/5512/100  → VID/PID
  │   ├── devpath           # "1"
  │   └── ...
  ├── 1-2/                  # USB 端口 2
  │   └── ...
  └── 2-0:1.0/              # 第二个 USB 控制器

/sys/class/i2c-adapter/
  ├── i2c-0/                # 系统 I2C
  ├── i2c-1/
  └── i2c-2/                # CH341 驱动创建的 adapter
      ├── name              # "i2c-ch341-usb"
      └── device/           # symlink 到 USB interface
```

### 正向流程（i2c-N → USB 路径）

```
输入: i2c-2（N=2）

1. 读 /sys/class/i2c-adapter/i2c-2/name
   → 内容 "i2c-ch341-usb" ✓ 确认是 CH341
   → 不匹配则跳过

2. 读 /sys/class/i2c-adapter/i2c-2/device/subsystem
   → 确认设备类型

3. 通过 device symlink 追踪到 USB 设备
   /sys/class/i2c-adapter/i2c-2/device
   → ../../../../devices/pci0000:00/0000:00:14.0/usb1/1-2/1-2:1.0/

4. 取 USB 设备路径 "1-2"
   （从路径中提取 usbX/Y-Z 格式的 Y-Z 部分）

返回: "1-2"
```

### 反向流程（USB 路径 → i2c-N）

```
输入: "1-2"

1. 遍历 /sys/class/i2c-adapter/i2c-*/name
   找到所有 name = "i2c-ch341-usb" 的 adapter

2. 对每个匹配的 i2c-N，追踪 device symlink
   到 USB 设备路径

3. 比较 USB 设备路径是否等于 "1-2"

4. 匹配则返回 N
```

### CH341 VID/PID 匹配

读 USB 设备的 `uevent` 文件：
```
PRODUCT=1a86/5512/100
TYPE=0/0/0
```

匹配 `PRODUCT` 字段的前两段等于 `1a86` 和 `5512`（或 `5523`）。

### 注意事项

- `i2c-ch341-usb` 驱动只支持 `PID_5512`，不支持 `PID_5523`（需确认第三方驱动是否更新）
- 需要 `/sys` 文件系统权限（通常普通用户可读）
- `i2c-N` 编号重启后可能变化，但 USB 路径不变
- 需要处理多 CH341 设备映射到多个 `i2c-N` 的情况

## macOS 实现方案

### IOKit 设备树

```
IOUSBHostDevice "CH341" @ 0x14200000
  ├── idVendor   = 0x1a86
  ├── idProduct  = 0x5512
  └── locationID = 0x14200000
```

### 正向流程（index → locationID）

```
输入: index = 1

1. IOKit 枚举所有 IOUSBHostDevice
2. 匹配 idVendor=0x1a86 + (idProduct=0x5512 | 0x5523)
3. 收集到数组，取第 1 个
4. 读取 locationID（uint32）

返回: "0x14200000"（格式化为 8 位 HEX 字符串）
```

### 反向流程（locationID → index）

```
输入: "0x14200000"

1. 解析字符串为 uint32（0x14200000）
2. IOKit 枚举所有 CH341 设备
3. 匹配 locationID
4. 返回设备在列表中的序号
```

### 注意事项

- `IOKit.framework` 在 macOS 上始终可用（系统框架）
- `libusb` 也可用，但需要额外分发 dylib
- `locationID` 在重启和拔插后对同一物理端口不变
- 需要 `com.apple.security.device.usb` 权限（Apple Silicon 上的 sandbox 应用）
- macOS 没有 `i2c-N` 概念，index 只是 0-based 枚举序号

## 构建策略

### 目录结构

```
CH341Locator/
├── include/
│   └── CH341Locator.h          # 统一公共头文件（去掉 windows.h 依赖）
├── src/
│   ├── CH341Locator.cpp        # 平台无关代码（UTF-8 编解码）
│   ├── backend_windows.cpp     # Windows SetupAPI 实现
│   ├── backend_linux.cpp       # Linux sysfs 实现
│   └── backend_macos.mm        # macOS IOKit 实现（Obj-C++）
├── examples/
│   └── ch341locator_demo.cpp   # 跨平台 demo
├── tests/
│   ├── mock_setupapi.h/.cpp    # Windows mock
│   ├── mock_sysfs.h/.cpp       # Linux mock（文件系统模拟）
│   ├── mock_iokit.h/.cpp       # macOS mock
│   └── test_ch341locator.cpp   # 跨平台测试
├── CMakeLists.txt              # 平台分支构建
└── .github/workflows/
    ├── build-windows.yml
    ├── build-linux.yml
    └── build-macos.yml
```

### CMake 平台分支

```cmake
if(WIN32)
    target_sources(CH341Locator PRIVATE src/backend_windows.cpp)
    target_link_libraries(CH341Locator PRIVATE setupapi)
elseif(APPLE)
    target_sources(CH341Locator PRIVATE src/backend_macos.mm)
    target_link_libraries(CH341Locator PRIVATE "-framework IOKit" "-framework CoreFoundation")
elseif(UNIX)
    target_sources(CH341Locator PRIVATE src/backend_linux.cpp)
endif()
```

### 头文件跨平台

当前头文件依赖 `<windows.h>` 和 `WINAPI`/`DWORD`/`ULONG` 等 Windows 类型。跨平台时需要改为标准 C 类型：

| Windows 类型 | 替换为 |
|-------------|--------|
| `DWORD` | `uint32_t` |
| `ULONG` | `uint32_t` |
| `WINAPI` | 空（Windows 上保留 `__stdcall`） |
| `LPSTR` | `char*` |
| `LPWSTR` | `wchar_t*` |
| `LPCSTR` | `const char*` |
| `LPCWSTR` | `const wchar_t*` |

这样头文件不再依赖 `<windows.h>`，三平台共用。

## 测试策略

### 三层 mock

| 平台 | Mock 方法 | 模拟内容 |
|------|----------|---------|
| Windows | 函数指针替换（已实现） | 预置设备表 + SPDRP 返回值 |
| Linux | 临时目录 + 符号链接 | 模拟 `/sys/class/i2c-adapter/` 和 `/sys/bus/usb/devices/` |
| macOS | IOKit 的 C++ 虚函数 hook | 模拟 IOUSBHostDevice 匹配 |

### 跨平台测试用例

- 无设备 → count=0, path=NULL, index=-1
- 单 CH341 → round-trip 一致
- 多设备（含非 CH341 混入）→ 正确计数、互不误匹配
- 路径不存在 / index 越界 → 返回错误

## 优先级与里程碑

| 阶段 | 内容 | 交付 |
|------|------|------|
| **P0** | 头文件跨平台化（去掉 `windows.h` 依赖） | 单头文件三平台可用 |
| **P1** | Linux backend（sysfs 实现） | Linux 版 .so / 测试通过 |
| **P2** | macOS backend（IOKit 实现） | macOS 版 .dylib / 测试通过 |
| **P3** | CI 矩阵扩展（Linux + macOS 构建+测试） | 三个平台 GitHub Actions |
| **P4** |（可选）libusb 统一后端替代 sysfs/IOKit | 减少平台特定代码量 |
