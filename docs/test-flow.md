# 自动测试流程

```mermaid
flowchart TB
    subgraph CI["GitHub Actions CI"]
        direction LR
        A1["cmake -S . -B build"]
        A2["cmake --build build<br/>(DLL + demo)"]
        A3["cmake --build build --target CH341Locator_test"]
        A4["ctest --verbose"]
        A1 --> A2 --> A3 --> A4
    end

    subgraph TestExec["CH341Locator_test.exe"]
        direction TB
        B0["SetScenario<br/>(选择 Mock 场景)"]
        B1["test_no_devices"]
        B2["test_single_device"]
        B3["test_two_mixed"]
        B0 --> B1 --> B2 --> B3
    end

    subgraph Mock["Mock SetupAPI"]
        C0["MockSetupDiGetClassDevsW"]
        C1["MockSetupDiEnumDeviceInfo"]
        C2["MockSetupDiGetDeviceRegistryPropertyW"]
        C3["预置设备数据<br/>VID/PID + 路径"]
    end

    subgraph Code["被测试的 DLL 代码"]
        D0["GetCh341DeviceCount"]
        D1["GetCh341LocationPathA"]
        D2["GetCh341IndexByLocationPathUtf8"]
        D3["WideToUtf8 / Utf8ToWide<br/>(手动 UTF-8 编解码)"]
        D4["GetPathFirst<br/>(XP→LOCATION_INFORMATION<br/>Vista+→LOCATION_PATHS)"]
    end

    A4 --> TestExec
    TestExec --> Mock
    TestExec --> Code
    Code --> Mock
```

## 测试场景

| 场景 | 模拟设备 | 验证内容 |
|------|---------|---------|
| `kScenarioNoDevices` | 0 个设备 | count=0, path=NULL, 反查=-1 |
| `kScenarioSingleCh341` | 1 个 CH341 | count=1, round-trip path→index, 越界检查 |
| `kScenarioTwoCh341Mixed` | 3 个设备<br/>(CH341 + 非CH341 + CH341) | count=2（跳过非CH341）,<br/>两台路径不同且互不误匹配,<br/>越界检查 |

## 数据流（以 single CH341 为例）

```
SetScenario(&kScenarioSingleCh341)
    │
    ├── GetCh341DeviceCount()
    │       → MockSetupDiGetClassDevsW        返回有效句柄
    │       → MockSetupDiEnumDeviceInfo       返回 1 个设备
    │       → MockSetupDiGetDeviceRegistryPropertyW(SPDRP_HARDWAREID)
    │             返回 "USB\VID_1A86&PID_5523&REV_0100"  → 匹配 CH341
    │       ← count = 1
    │
    ├── GetCh341LocationPathA(0)
    │       → FindCh341(0)                    找到第 0 个 CH341
    │       → GetPathFirst()                  读取 SPDRP_LOCATION_PATHS
    │       → WideToUtf8()                    手动 UTF-8 编码
    │       ← "PCIROOT(0)#PCI(1400)#USBROOT(0)#USB(1)#USB(2)"
    │
    └── GetCh341IndexByLocationPathUtf8(path)
            → Utf8ToWide(path)                手动 UTF-8 解码
            → GetCh341IndexByLocationPath(w)  枚举所有 CH341
            → GetPathFirst()                  读取 SPDRP_LOCATION_PATHS
            → 比较 s == path
            ← index = 0
```
