# 技术交底书

## 发明名称

一种跨平台 USB 设备物理端口定位方法及系统

## 技术领域

本发明涉及 USB 设备识别与定位技术，具体涉及一种将 USB 设备（尤其是 USB-to-I2C 桥接器 CH341）的物理端口拓扑映射为操作系统统一标识的方法及其跨平台实现。

## 背景技术

### 现有问题

在工业自动化、测试测量领域，常通过 USB 连接多个 CH341（USB-to-I2C 桥接芯片）设备来控制传感器、执行器等外设。每个 CH341 插在不同的 USB 物理端口上，对应不同的 I2C 总线（如 Linux 上的 /dev/i2c-2、/dev/i2c-3 等）。

现有技术存在以下问题：

1. **动态编号不稳定**：操作系统为 USB 设备分配的动态编号（如 Linux 的 i2c-N、Windows 的设备序号）在重启或拔插后可能改变，导致原本固定在 /dev/i2c-2 上的温度传感器变成 /dev/i2c-5，应用程序找不到设备。

2. **依赖固定的 USB 物理端口**：虽然 USB 物理端口本身固定（如主板背后的某个 USB 插座），但传统方案需要手动绑定或依赖 udev 规则，不具备跨平台通用性。

3. **缺少统一的跨平台抽象**：Windows 使用 SetupAPI 的 SPDRP_LOCATION_PATHS，Linux 使用 sysfs 的 i2c adapter 路径，macOS 使用 IOKit 的 locationID——各有各的概念和格式，无统一的编程接口。

### 已有方案及其局限

- **udev 规则绑定**（Linux）：通过编写 udev 规则将 USB 端口路径映射到固定符号链接。局限：仅限 Linux，配置复杂，需要 root 权限。
- **Windows SetupAPI 直接调用**：开发人员直接调用 SetupDiGetDeviceRegistryProperty 读取 SPDRP_LOCATION_PATHS。局限：仅限 Windows，API 复杂，需深入理解 Windows 设备管理。
- **libusb 枚举**：通过 libusb 枚举 USB 设备并获取拓扑路径。局限：仅获取 USB 枚举路径，无法获取 I2C 总线编号，在 macOS 上缺少 I2C 抽象层。

## 发明内容

### 技术方案概述

本发明提供一种统一的跨平台方法，用于：

1. 枚举系统中所有目标 USB 设备（如 CH341，VID=0x1A86, PID=0x5512/0x5523）；
2. 将每个设备映射到其在 USB 物理端口拓扑中的固定路径；
3. 将该路径进一步映射到操作系统赋予的 I2C 总线编号或其他总线标识；
4. 通过单一 C 语言 API 实现正向查询（编号→路径）和反向查询（路径→编号）。

### 核心技术特征

**特征 1：分平台适配层 + 统一抽象接口**

设计一个平台无关的后端接口 `Ch341Backend_Enumerate()`，各平台独立实现：

- Windows 平台：通过 SetupAPI 读取 `SPDRP_LOCATION_PATHS`（Vista+）或 `SPDRP_LOCATION_INFORMATION`（XP）；返回 USB 拓扑路径字符串。
- Linux 平台：通过 sysfs 遍历 `/sys/class/i2c-adapter/`，读取 `name` 文件匹配驱动名称（如 "i2c-ch341-usb"），解析出 `i2c-N` 总线编号，格式化为 `/dev/i2c-N` 字符串返回。
- macOS 平台：通过 IOKit 框架匹配 `IOUSBHostDevice`，读取 `locationID` 属性，格式化为 `0xXXXXXXXX` 字符串返回。

**特征 2：统一 UTF-8 字符串标识**

各平台的拓扑路径统一以 UTF-8 字符串输出，不暴露平台相关的 wchar_t 或 char16_t 差异。调用方无需关心底层编码。

**特征 3：平台无关的手动 UTF-8/UTF-16 编解码**

内部实现一套不依赖操作系统 API 的手动 UTF-8 ↔ UTF-16 转换器，确保在 Windows XP 等不支持 CP_UTF8 的老旧系统上也能正确工作。

**特征 4：双向映射 API**

- 正向：`GetCh341LocationPath(index)` → 给定设备序号，返回 USB 拓扑路径字符串。Linux 上序号即为 i2c-N 中的 N。
- 反向：`GetCh341IndexByLocationPathUtf8(path)` → 给定路径字符串，返回设备序号。用于持久化配置后的重新识别。

**特征 5：基于 Mock 的跨平台测试框架**

构建一套可替换的 Mock Backend，在单元测试中模拟各平台的后端行为，无需真实硬件即可验证正向/反向映射的正确性。

### 有益效果

1. **端口稳定性**：USB 物理端口路径在重启和拔插后保持不变，解决动态编号漂移问题。
2. **跨平台一致性**：同一套 API 在三平台上行为一致，应用程序无需为每个平台编写不同代码。
3. **开箱即用**：Linux 上直接返回 `/dev/i2c-N`，调用方拿到即可 `open()` 通信。
4. **零外部依赖**：仅使用操作系统原生 API（Windows SetupAPI、Linux sysfs、macOS IOKit），无需额外安装库。

## 权利要求建议

1. 一种跨平台 USB 设备物理端口定位方法，其特征在于，包括：根据目标 USB 设备的 VID 和 PID，在操作系统底层枚举设备；根据操作系统类型，调用对应的原生接口获取该设备在 USB 物理拓扑中的固定路径；将该路径统一格式化为 UTF-8 字符串；建立设备序号与该字符串的双向映射。

2. 根据权利要求 1 所述的方法，其特征在于，所述操作系统为 Windows 时，通过 SetupAPI 读取 SPDRP_LOCATION_PATHS 或 SPDRP_LOCATION_INFORMATION 属性；为 Linux 时，通过 sysfs 遍历 I2C adapter 的 name 文件匹配驱动名称，获取 i2c-N 总线编号；为 macOS 时，通过 IOKit 框架获取 USB 设备的 locationID 属性。

3. 根据权利要求 1 所述的方法，其特征在于，所述 UTF-8 字符串在 Linux 平台上为 "/dev/i2c-N" 格式，直接可用于打开 I2C 总线设备文件。

4. 根据权利要求 1 所述的方法，其特征在于，通过一次枚举建立当前所有目标设备的索引表，正向查询从索引表按序号获取路径，反向查询遍历索引表按字符串匹配获取序号。

5. 一种计算机可读存储介质，其上存储有计算机程序，所述程序被处理器执行时实现权利要求 1-4 任一项所述的方法。

## 附图说明建议

建议包含以下示意图：

- **图 1**：系统架构图，展示三层结构（公共 API 层 → 平台适配层 → 操作系统原生 API 层）
- **图 2**：Linux 平台数据流：CH341 插入 → USB sysfs → I2C adapter sysfs → /dev/i2c-N → 公共 API
- **图 3**：Windows 平台数据流：SetupAPI 枚举 → SPDRP_LOCATION_PATHS → 公共 API
- **图 4**：macOS 平台数据流：IOKit 枚举 → locationID → 公共 API
- **图 5**：正向查询与反向查询的流程对比

---

*本文档为技术交底书草案，供专利代理人参考，不构成正式法律文件。*
