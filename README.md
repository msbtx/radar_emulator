# radar_emulator

`radar_emulator` 是一款在Windows平台上，使用 [HackRF One](https://greatscottgadgets.com/hackrf/) 软件定义无线电（SDR）外设来模拟Wi-Fi动态频率选择（DFS）雷达信号的应用程序。该工具能够生成符合ETSI（欧洲电信标准协会）和FCC（美国联邦通信委员会）规范的雷达波形，用于测试Wi-Fi设备在5GHz频段的DFS功能。

## 功能特性

* 利用HackRF One硬件生成真实的射频信号。
* 支持模拟ETSI和FCC定义的多种雷达测试波形。
* 可自定义设置雷达信号所在的信道、区域规范、测试类型以及信号周期。
* 命令行界面，易于集成和自动化测试。
* 专为Windows操作系统设计。

## 必备软硬件

1.  **硬件**: [HackRF One](https://greatscottgadgets.com/hackrf/) 一台及配套天线。
2.  **软件**: [Radioconda](https://github.com/ryanvolz/radioconda) - 一个包含了GNU Radio及其他软件无线电工具的软件包。

## 使用指南

### 1. 安装Radioconda

Radioconda是一个集成了SDR常用工具链的软件包，可以极大地简化环境配置过程。

* 访问 Radioconda的GitHub发布页面: [https://github.com/ryanvolz/radioconda/releases](https://github.com/ryanvolz/radioconda/releases)
* 根据您的系统下载最新的Windows安装包（例如 `radioconda-*-Windows-x86_64.exe`）。
* 下载后，在本地运行安装程序，按照向导完成安装。推荐保持默认安装路径。

### 2. 配置环境变量

为了在系统的任何路径下都能直接调用Radioconda环境中的工具，需要将其路径添加到环境变量中。

* 找到Radioconda的安装路径，并进入其 `Library\bin` 子目录。例如，如果您的安装路径是 `C:\Users\YourUser\radioconda`，那么需要添加的路径就是 `C:\Users\YourUser\radioconda\Library\bin`。
* 将此路径添加到系统或当前用户的 `PATH` 环境变量中。

> **提示**: 可通过 “编辑系统环境变量” -> “环境变量...” -> “Path” -> “编辑” -> “新建” 来添加。

### 3. 部署`radar_emulator`

* 将 `radar_emulator.exe` 工具解压到本地。
* **强烈推荐**将该文件直接放入Radioconda的 `Library\bin` 目录下（即上一步添加到环境变量的路径）。这样可以确保在任何CMD窗口中都能直接执行该命令。

### 4. 连接硬件并运行

* 将天线连接到HackRF One的ANT接口。
* 使用USB线将HackRF One连接到PC。
* 打开PC的命令提示符（CMD）窗口。
* 直接执行以下命令即可开始发送雷达信号：
    ```bash
    radar_emulator
    ```
    在默认情况下，该命令会**每隔1秒（1000ms）向52信道（5260 MHz）发送一个ETSI定义的Type 1雷达测试波形**。

## 命令详细参数

您可以通过命令行选项来精确控制生成的雷達信號類型。

**用法:**

Usage: radar_emulator.exe [options]

**选项:**

| 参数           | 描述                                                                                                                              | 默认值     |
| :------------- | :-------------------------------------------------------------------------------------------------------------------------------- | :--------- |
| `-h`           | 显示此帮助信息。                                                                                                                  |            |
| `-c <channel>` | **信道**: 设置雷达信号的中心频率对应信道。 <br>可用信道: `52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140` | `52`       |
| `-r <region>`  | **DFS区域**: 定义雷达信号遵循的规范。 <br>`1`: ETSI (欧洲) <br>`2`: FCC (美国)                                                       | `1`        |
| `-t <type>`    | **雷达测试类型**: 选择要模拟的简化雷达模式 (1-4)。                                                                                | `1`        |
| `-b <period_ms>`| **脉冲周期 (毫秒)**: 设置雷达脉冲串的发送周期，单位为毫秒。                                                                         | `1000.0`   |

---

**示例:**

* **向100信道发送FCC Type 2雷达信号，周期为2.5秒:**
    ```bash
    radar_emulator -c 100 -r 2 -t 2 -b 2500
    ```

* **向64信道发送ETSI Type 4雷达信号，周期为500毫秒:**
    ```bash
    radar_emulator.exe -c 64 -r 1 -t 4 -b 500
    ```

## 免责声明

本工具仅供合法的测试和教育目的使用。用户必须确保其使用行为符合当地的无线电频谱管理法规。任何非法使用或对正常无线通信造成干扰的行为，由用户自行承担责任。
