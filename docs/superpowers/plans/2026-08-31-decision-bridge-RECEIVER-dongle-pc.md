# 决策桥 - 接收端 (Dongle + PC) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 PC 上的 cc_bridge (Agent SDK 包裹 Claude Code) 把决策点经 Dongle 的 UART0 串口转发到 K10, 并把 K10 的触摸选择回传, 阻塞控制 Claude 下一步。

**Architecture:** 两个协同单元。Dongle (ESP-IDF) 新增 `pc_link` 做 UART0↔ESP-NOW 双向转发, 控制台迁到 USB-Serial/JTAG 腾出 UART0。PC 端 `cc_bridge` (Python) 用 Agent SDK 的 `can_use_tool` 回调, 把决策打包成串口帧发出并阻塞等回执。

**Tech Stack:** ESP-IDF 5.4+ (C, CMake), Python 3.10+, claude-agent-sdk, pyserial。

## Global Constraints

- 协议帧 (`espnow_protocol.h`) K10 与 Dongle 两侧**必须字节一致**; 本计划以设计文档第4节为唯一真源照抄, 不自创。
- ESP-NOW 单帧 payload ≤ 250 字节; 0x0A = 134 字节, 0x0B = 5 字节。
- CRC8: 多项式 `0x07`, 初值 `0x00`, 用 `espnow_protocol.h` 既有的 `espnow_crc8()`。
- 串口帧魔术头 `0xA5 0x5A`; 波特率沿用现有控制台波特率 115200。
- 改动**不得触碰 TinyUSB 描述符 / HID / UAC 逻辑**; 音频✅键盘✅为既有验证通过功能, 需回归。
- 参照 `dongle_firmware/CLAUDE.md` 既有约定 (idf.py 构建, 不用 platformio)。

---

## 文件结构

**Dongle (dongle_firmware/):**
- `main/espnow_protocol.h` — 修改: 加 0x0A/0x0B 帧类型与结构体 (照抄 spec)
- `main/pc_link.h` — 新建: pc_link 接口
- `main/pc_link.c` — 新建: UART0 收发任务 + 帧解析 + 双向转发
- `main/espnow_receiver.c` — 修改: 收到 K10 的 0x0B 时回调 pc_link 写回 PC
- `main/main.c` — 修改: 启动 pc_link 任务
- `main/CMakeLists.txt` — 修改: 加 pc_link.c
- `sdkconfig.defaults` — 修改: 控制台改 USB-Serial/JTAG

**PC (tools/cc_bridge/):**
- `serial_link.py` — 新建: 串口帧编解码 + 后台读线程 + decision_id 等待/唤醒
- `bridge.py` — 新建: Agent SDK 包裹 + can_use_tool 回调
- `requirements.txt` — 新建: 依赖
- `tests/test_serial_link.py` — 新建: 帧编解码单测

---
## Task 1: 协议扩展 (Dongle 端 espnow_protocol.h)

**Files:**
- Modify: `dongle_firmware/main/espnow_protocol.h`

**Interfaces:**
- Produces: `FRAME_TYPE_DECISION_REQ = 0x0A`, `FRAME_TYPE_DECISION_REPLY = 0x0B`; 结构体 `decision_request_frame_t` (134 字节), `decision_reply_frame_t` (5 字节); 宏 `DECISION_TITLE_LEN=32`, `DECISION_OPT_LEN=24`, `DECISION_MAX_OPTS=4`。K10 侧计划 Task 1 会照抄同一份, 两端一致。

- [ ] **Step 1: 在 frame_type_t 枚举末尾加两个类型**

在 `espnow_protocol.h` 的 `typedef enum { ... } frame_type_t;` 中, `FRAME_TYPE_AI_STATE = 0x09,` 后追加:

```c
    FRAME_TYPE_DECISION_REQ   = 0x0A,  /* 决策请求 (PC→Dongle→K10) */
    FRAME_TYPE_DECISION_REPLY = 0x0B,  /* 决策回执 (K10→Dongle→PC) */
```

- [ ] **Step 2: 在 AI 情绪帧结构体之后、CRC8 函数之前, 加决策帧结构体**

```c
/* ============================================================
 * 决策帧 (Claude Code ⇄ K10 双向触摸决策)
 * ============================================================ */
#define DECISION_TITLE_LEN   32   /* 标题最大长度 (含结尾 \0) */
#define DECISION_OPT_LEN     24   /* 单个选项最大长度 (含结尾 \0) */
#define DECISION_MAX_OPTS     4   /* 最多选项数 */

typedef struct __attribute__((packed)) {
    uint8_t  frame_type;                 /* = FRAME_TYPE_DECISION_REQ */
    uint16_t decision_id;                /* 递增, 回执对齐 */
    uint8_t  kind;                       /* 0=工具权限 1=多选题 */
    char     title[DECISION_TITLE_LEN];  /* 标题 */
    uint8_t  opt_count;                  /* 有效选项数 (1~4) */
    char     opts[DECISION_MAX_OPTS][DECISION_OPT_LEN];
    uint8_t  crc8;
} decision_request_frame_t;              /* 1+2+1+32+1+96+1 = 134 字节 */

typedef struct __attribute__((packed)) {
    uint8_t  frame_type;    /* = FRAME_TYPE_DECISION_REPLY */
    uint16_t decision_id;   /* 对应请求 */
    uint8_t  chosen_index;  /* 选中下标; 0xFF=超时/取消 */
    uint8_t  crc8;
} decision_reply_frame_t;   /* 5 字节 */
```

- [ ] **Step 3: 编译验证结构体大小**

在 `main.c` app_main 开头临时加 (验证后删除):
```c
ESP_LOGI(TAG, "sizeof decision_req=%d reply=%d", (int)sizeof(decision_request_frame_t), (int)sizeof(decision_reply_frame_t));
```
Run: `cd dongle_firmware && idf.py build`
Expected: 编译通过。烧录后串口应打印 `sizeof decision_req=134 reply=5`。验证后删除该行。

- [ ] **Step 4: Commit**

```bash
git add dongle_firmware/main/espnow_protocol.h
git commit -m "feat(dongle): 协议加 0x0A/0x0B 决策帧"
```

---

## Task 2: 控制台迁移到 USB-Serial/JTAG (腾出 UART0)

**Files:**
- Modify: `dongle_firmware/sdkconfig.defaults`

**Interfaces:**
- Produces: UART0 (GPIO43/44) 不再被控制台占用, 可被 pc_link 用 `uart_driver_install` 接管。日志改从 USB-Serial/JTAG 口输出。

- [ ] **Step 1: 修改 sdkconfig.defaults 控制台配置**

把现有的:
```
CONFIG_ESP_CONSOLE_UART_DEFAULT=y
CONFIG_ESP_CONSOLE_UART_NUM=0
CONFIG_ESP_CONSOLE_UART_TX_GPIO=43
CONFIG_ESP_CONSOLE_UART_RX_GPIO=44
CONFIG_ESP_CONSOLE_SECONDARY_NONE=y
```
替换为:
```
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y
CONFIG_ESP_CONSOLE_SECONDARY_NONE=y
```

- [ ] **Step 2: 全新构建 (sdkconfig 变更需重生成)**

Run: `cd dongle_firmware && idf.py fullclean && idf.py build`
Expected: 编译通过。

- [ ] **Step 3: 烧录并确认日志走 USB-JTAG 口**

Run: `idf.py -p <JTAG_COM> flash monitor`
Expected: 能在 USB-Serial/JTAG 口看到启动日志 (">>> CodeBuddy Dongle Starting <<<")。这确认了 console 迁移成功、UART0 已空出。

- [ ] **Step 4: 回归确认 USB 设备仍正常**

插上 TinyUSB 口, PowerShell 检查:
Run: `powershell -Command "Get-PnpDevice | Where-Object { $_.FriendlyName -like '*CodeBuddy*' }"`
Expected: 仍能看到 HID + Audio 设备 (VID_303A&PID_8000)。确认 console 迁移没破坏 TinyUSB。

- [ ] **Step 5: Commit**

```bash
git add dongle_firmware/sdkconfig.defaults
git commit -m "chore(dongle): 控制台迁 USB-JTAG 腾出 UART0"
```

---
## Task 3: pc_link 模块 (UART0 收发 + 帧解析 + 双向转发)

**Files:**
- Create: `dongle_firmware/main/pc_link.h`
- Create: `dongle_firmware/main/pc_link.c`
- Modify: `dongle_firmware/main/CMakeLists.txt`

**Interfaces:**
- Consumes: `espnow_protocol.h` 的 `decision_request_frame_t` / `decision_reply_frame_t` / `espnow_crc8()` (Task 1); `pairing_get_peer_mac()` / `pairing_has_pair()` (既有 pairing.h)。
- Produces:
  - `esp_err_t pc_link_init(void)` — 装 UART0 driver, 起收任务。
  - `void pc_link_send_reply_to_pc(const uint8_t *frame, uint16_t len)` — 把 K10 回执帧 (0x0B, 已含 frame_type) 加魔术头写回 UART0 TX。供 espnow_receiver 调用 (Task 4)。

- [ ] **Step 1: 写 pc_link.h**

```c
#ifndef PC_LINK_H
#define PC_LINK_H
#include <stdint.h>
#include "esp_err.h"
#ifdef __cplusplus
extern "C" {
#endif
/* 装 UART0 driver + 起接收任务。须在 pairing_init 之后调用。 */
esp_err_t pc_link_init(void);
/* 把一帧 (首字节为 frame_type) 加 [0xA5 0x5A|type|len|payload|crc8] 写回 PC。*/
void pc_link_send_reply_to_pc(const uint8_t *frame, uint16_t len);
#ifdef __cplusplus
}
#endif
#endif /* PC_LINK_H */
```

- [ ] **Step 2: 写 pc_link.c — UART 初始化 + 串口写回**

```c
#include "pc_link.h"
#include "espnow_protocol.h"
#include "pairing.h"
#include "esp_log.h"
#include "esp_now.h"
#include "driver/uart.h"
#include <string.h>

static const char *TAG = "pc_link";
#define PC_UART_NUM      UART_NUM_0
#define PC_UART_TX_GPIO  43
#define PC_UART_RX_GPIO  44
#define PC_UART_BAUD     115200
#define PC_MAGIC0        0xA5
#define PC_MAGIC1        0x5A
#define PC_RX_BUFSZ      512

/* 把一帧 (首字节 frame_type) 加魔术头写回 PC */
void pc_link_send_reply_to_pc(const uint8_t *frame, uint16_t len)
{
    uint8_t hdr[3] = { PC_MAGIC0, PC_MAGIC1, (uint8_t)len };
    /* inner_type 直接用帧首字节 frame_type; 这里 hdr 布局: magic0 magic1 len,
       inner_type 已在 frame[0], 故整体 = magic0 magic1 | frame[0..len-1] | crc8。
       为与解析对称, 采用: magic0 magic1 | len | frame(len) | crc8 */
    uint8_t crc = espnow_crc8(frame, len);
    uart_write_bytes(PC_UART_NUM, (const char*)hdr, 3);
    uart_write_bytes(PC_UART_NUM, (const char*)frame, len);
    uart_write_bytes(PC_UART_NUM, (const char*)&crc, 1);
}
```

注: 帧格式统一为 `magic0 magic1 | len | frame(len) | crc8`, 其中 frame 首字节即 frame_type, 故不再单列 inner_type 字段 (len 已足够定位, frame_type 在 payload 内)。

- [ ] **Step 3: 写 pc_link.c — 转发 0x0A 到 K10**

追加静态函数:
```c
/* 收到完整 PC 帧 (frame[0]=frame_type) 后处理 */
static void handle_pc_frame(const uint8_t *frame, uint16_t len)
{
    if (len < 1) return;
    if (frame[0] == FRAME_TYPE_DECISION_REQ) {
        if (len != sizeof(decision_request_frame_t)) {
            ESP_LOGW(TAG, "0x0A len mismatch: %d != %d", len, (int)sizeof(decision_request_frame_t));
            return;
        }
        if (!pairing_has_pair()) {
            ESP_LOGW(TAG, "no paired K10, drop 0x0A");
            /* 回一个离线回执: chosen_index=0xFF */
            const decision_request_frame_t *req = (const decision_request_frame_t*)frame;
            decision_reply_frame_t rep = {0};
            rep.frame_type = FRAME_TYPE_DECISION_REPLY;
            rep.decision_id = req->decision_id;
            rep.chosen_index = 0xFF;
            rep.crc8 = espnow_crc8((uint8_t*)&rep, sizeof(rep) - 1);
            pc_link_send_reply_to_pc((uint8_t*)&rep, sizeof(rep));
            return;
        }
        uint8_t peer[6];
        pairing_get_peer_mac(peer);
        esp_err_t r = esp_now_send(peer, frame, len);
        ESP_LOGI(TAG, "fwd 0x0A id=%u to K10: %s", ((const decision_request_frame_t*)frame)->decision_id, esp_err_to_name(r));
    }
}
```

- [ ] **Step 4: 写 pc_link.c — UART 接收任务 (魔术头状态机)**

```c
static void pc_rx_task(void *arg)
{
    uint8_t byte;
    enum { S_M0, S_M1, S_LEN, S_BODY, S_CRC } st = S_M0;
    uint8_t buf[160]; uint16_t need = 0, got = 0;
    while (1) {
        int n = uart_read_bytes(PC_UART_NUM, &byte, 1, pdMS_TO_TICKS(100));
        if (n != 1) continue;
        switch (st) {
        case S_M0: if (byte == PC_MAGIC0) st = S_M1; break;
        case S_M1: st = (byte == PC_MAGIC1) ? S_LEN : S_M0; break;
        case S_LEN:
            need = byte;
            if (need == 0 || need > sizeof(buf)) { st = S_M0; break; }
            got = 0; st = S_BODY; break;
        case S_BODY:
            buf[got++] = byte;
            if (got >= need) st = S_CRC;
            break;
        case S_CRC:
            if (espnow_crc8(buf, need) == byte) handle_pc_frame(buf, need);
            else ESP_LOGW(TAG, "pc frame crc err");
            st = S_M0; break;
        }
    }
}

esp_err_t pc_link_init(void)
{
    uart_config_t cfg = {
        .baud_rate = PC_UART_BAUD, .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE, .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(PC_UART_NUM, PC_RX_BUFSZ, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(PC_UART_NUM, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(PC_UART_NUM, PC_UART_TX_GPIO, PC_UART_RX_GPIO,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    xTaskCreatePinnedToCore(pc_rx_task, "pc_rx", 4096, NULL, 4, NULL, 1);
    ESP_LOGI(TAG, "pc_link ready on UART0");
    return ESP_OK;
}
```

需在文件顶部补 `#include "freertos/FreeRTOS.h"` 和 `#include "freertos/task.h"`。

- [ ] **Step 5: CMakeLists.txt 加 pc_link.c**

在 `SRCS` 列表 (含 pairing.c 那段) 加一行 `"pc_link.c"`; `REQUIRES` 确认含 `driver` (已有)。

- [ ] **Step 6: 编译**

Run: `cd dongle_firmware && idf.py build`
Expected: 编译通过, 无 pc_link 相关报错。

- [ ] **Step 7: Commit**

```bash
git add dongle_firmware/main/pc_link.h dongle_firmware/main/pc_link.c dongle_firmware/main/CMakeLists.txt
git commit -m "feat(dongle): pc_link UART0 双向转发骨架"
```

---
## Task 4: espnow_receiver 收 0x0B 回执 → 写回 PC

**Files:**
- Modify: `dongle_firmware/main/espnow_receiver.c`

**Interfaces:**
- Consumes: `pc_link_send_reply_to_pc()` (Task 3); `FRAME_TYPE_DECISION_REPLY` / `decision_reply_frame_t` (Task 1)。

- [ ] **Step 1: 在 espnow_receiver.c 顶部 include pc_link.h**

在既有 include 区加:
```c
#include "pc_link.h"
```

- [ ] **Step 2: 在帧分发处 (处理 FRAME_TYPE_PAIR / 数据帧那段) 加 0x0B 分支**

在收到帧、CRC 校验通过后的分发逻辑里 (参考既有 `if (frame_type == FRAME_TYPE_PAIR)` 附近) 加:
```c
    if (frame_type == FRAME_TYPE_DECISION_REPLY) {
        if (len == sizeof(decision_reply_frame_t)) {
            const decision_reply_frame_t *rep = (const decision_reply_frame_t *)data;
            if (espnow_crc8(data, sizeof(*rep) - 1) == rep->crc8) {
                pc_link_send_reply_to_pc(data, len);
            }
        }
        return;
    }
```
放在音频/按键帧处理之前 (与 PAIR 帧同级)。

- [ ] **Step 3: 编译**

Run: `cd dongle_firmware && idf.py build`
Expected: 编译通过。

- [ ] **Step 4: Commit**

```bash
git add dongle_firmware/main/espnow_receiver.c
git commit -m "feat(dongle): 收 0x0B 回执转发回 PC"
```

---

## Task 5: main.c 启动 pc_link

**Files:**
- Modify: `dongle_firmware/main/main.c`

**Interfaces:**
- Consumes: `pc_link_init()` (Task 3)。

- [ ] **Step 1: include pc_link.h**

在 main.c 既有 include 区加 `#include "pc_link.h"`。

- [ ] **Step 2: 在 app_main 中 ESP-NOW 接收器初始化之后调用 pc_link_init**

在 `espnow_receiver_init(on_key_frame)` 那步之后加:
```c
    ESP_ERROR_CHECK(pc_link_init());
    ESP_LOGI(TAG, ">>> PC Link (UART0) STARTED <<<");
```

- [ ] **Step 3: 编译 + 烧录**

Run: `cd dongle_firmware && idf.py build && idf.py -p <JTAG_COM> flash`
Expected: 编译通过, 启动日志出现 ">>> PC Link (UART0) STARTED <<<"。

- [ ] **Step 4: Commit**

```bash
git add dongle_firmware/main/main.c
git commit -m "feat(dongle): app_main 启动 pc_link"
```

---
## Task 6: PC 端 serial_link.py (帧编解码 + 读线程 + 等待/唤醒)

**Files:**
- Create: `tools/cc_bridge/serial_link.py`
- Create: `tools/cc_bridge/tests/test_serial_link.py`
- Create: `tools/cc_bridge/requirements.txt`

**Interfaces:**
- Produces:
  - `crc8(data: bytes) -> int` — 多项式 0x07 初值 0x00, 与 Dongle 一致。
  - `encode_frame(payload: bytes) -> bytes` — 返回 `A5 5A | len | payload | crc8`。
  - `pack_decision_request(decision_id:int, kind:int, title:str, opts:list[str]) -> bytes` — 组 0x0A 帧 body (首字节 frame_type=0x0A, 共 134 字节)。
  - `parse_reply(frame: bytes) -> tuple[int,int] | None` — 输入去掉魔术头的 payload (5 字节), 返回 `(decision_id, chosen_index)` 或 None (crc 错)。
  - `class SerialLink` — `open()`, `send_decision(...) -> int` (返回 decision_id), `wait_reply(decision_id, timeout) -> int` (返回 chosen_index, 超时抛 TimeoutError), 后台线程读串口按 decision_id 唤醒。

- [ ] **Step 1: 写 requirements.txt**

```
pyserial>=3.5
claude-agent-sdk>=0.1.0
```
(claude-agent-sdk 确切版本号实现时按 pip 最新可用核实。)

- [ ] **Step 2: 写失败测试 test_serial_link.py**

```python
import struct
from serial_link import crc8, encode_frame, pack_decision_request, parse_reply

def test_crc8_matches_known():
    assert crc8(b"\x01\x02\x03") == 0x48  # 与 Dongle espnow_crc8 同算法

def test_encode_frame_layout():
    f = encode_frame(b"\xAA\xBB")
    assert f[0] == 0xA5 and f[1] == 0x5A and f[2] == 2
    assert f[3:5] == b"\xAA\xBB"
    assert f[5] == crc8(b"\xAA\xBB")

def test_pack_decision_request_size():
    body = pack_decision_request(7, 0, "Allow Bash", ["Yes","No","Always"])
    assert len(body) == 134
    assert body[0] == 0x0A
    assert struct.unpack_from("<H", body, 1)[0] == 7  # decision_id
    assert body[3] == 0  # kind
    assert body[3+1+32] == 3  # opt_count 位置: frame_type(1)+id(2)+kind(1)+title(32)

def test_parse_reply_roundtrip():
    # 构造一个 5 字节回执: 0x0B | id=7 | idx=1 | crc
    payload = bytes([0x0B]) + struct.pack("<H", 7) + bytes([1])
    payload += bytes([crc8(payload)])
    assert parse_reply(payload) == (7, 1)

def test_parse_reply_bad_crc():
    payload = bytes([0x0B]) + struct.pack("<H", 7) + bytes([1, 0x00])
    assert parse_reply(payload) is None
```

- [ ] **Step 3: 运行确认失败**

Run: `cd tools/cc_bridge && python -m pytest tests/test_serial_link.py -v`
Expected: FAIL (ModuleNotFoundError / 函数未定义)。
注: `test_crc8_matches_known` 的期望值 0x48 若与实现不符, 以实现的 CRC8 (多项式0x07初值0x00) 为准修正测试——关键是与 Dongle 的 espnow_crc8 同算法。

- [ ] **Step 4: 写 serial_link.py 实现**

```python
import struct, threading, time
import serial

TITLE_LEN, OPT_LEN, MAX_OPTS = 32, 24, 4
FRAME_DECISION_REQ, FRAME_DECISION_REPLY = 0x0A, 0x0B
MAGIC0, MAGIC1 = 0xA5, 0x5A

def crc8(data: bytes) -> int:
    crc = 0
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = ((crc << 1) ^ 0x07) & 0xFF if (crc & 0x80) else (crc << 1) & 0xFF
    return crc

def encode_frame(payload: bytes) -> bytes:
    return bytes([MAGIC0, MAGIC1, len(payload)]) + payload + bytes([crc8(payload)])

def _fixed(s: str, n: int) -> bytes:
    b = s.encode("utf-8")[:n-1]
    return b + b"\x00" * (n - len(b))

def pack_decision_request(decision_id, kind, title, opts):
    body = bytes([FRAME_DECISION_REQ]) + struct.pack("<H", decision_id) + bytes([kind])
    body += _fixed(title, TITLE_LEN)
    body += bytes([len(opts)])
    for i in range(MAX_OPTS):
        body += _fixed(opts[i] if i < len(opts) else "", OPT_LEN)
    body += bytes([crc8(body)])
    return body

def parse_reply(payload: bytes):
    if len(payload) != 5 or payload[0] != FRAME_DECISION_REPLY:
        return None
    if crc8(payload[:4]) != payload[4]:
        return None
    decision_id = struct.unpack_from("<H", payload, 1)[0]
    return (decision_id, payload[3])

class SerialLink:
    def __init__(self, port, baud=115200):
        self._port, self._baud = port, baud
        self._ser = None
        self._next_id = 1
        self._events = {}      # decision_id -> threading.Event
        self._results = {}     # decision_id -> chosen_index
        self._lock = threading.Lock()
        self._stop = False

    def open(self):
        self._ser = serial.Serial(self._port, self._baud, timeout=0.1)
        threading.Thread(target=self._reader, daemon=True).start()

    def send_decision(self, kind, title, opts) -> int:
        with self._lock:
            did = self._next_id; self._next_id = (self._next_id + 1) & 0xFFFF
            self._events[did] = threading.Event()
        body = pack_decision_request(did, kind, title, opts)
        self._ser.write(encode_frame(body[:-1]))  # encode_frame 自加 crc; body 末尾 crc 是帧内 crc
        return did

    def wait_reply(self, decision_id, timeout=120.0) -> int:
        ev = self._events.get(decision_id)
        if ev is None:
            raise KeyError(decision_id)
        if not ev.wait(timeout):
            raise TimeoutError(f"decision {decision_id} timed out")
        return self._results.pop(decision_id, 0xFF)

    def _reader(self):
        st = 0; need = 0; buf = bytearray()
        while not self._stop:
            b = self._ser.read(1)
            if not b: continue
            c = b[0]
            if st == 0: st = 1 if c == MAGIC0 else 0
            elif st == 1: st = 2 if c == MAGIC1 else 0
            elif st == 2:
                need = c; buf = bytearray(); st = 3 if 0 < need <= 160 else 0
            elif st == 3:
                buf.append(c)
                if len(buf) >= need: st = 4
            elif st == 4:
                if crc8(bytes(buf)) == c:
                    r = parse_reply(bytes(buf))
                    if r:
                        did, idx = r
                        with self._lock:
                            self._results[did] = idx
                            if did in self._events: self._events[did].set()
                st = 0
```

注意 send_decision 里 body 已含帧内 crc, 而 encode_frame 会再算一层外层 crc; 二者是两层校验 (帧内 crc 供 K10 校 0x0A 帧, 外层 crc 供 Dongle 校串口帧)。传给 encode_frame 的应是完整 134 字节 body (含帧内crc), 修正为 `self._ser.write(encode_frame(body))`。

- [ ] **Step 5: 修正 send_decision 的 encode 调用**

把 `self._ser.write(encode_frame(body[:-1]))` 改为 `self._ser.write(encode_frame(body))`。外层帧 payload = 完整 134 字节 0x0A 帧 (含其自身 crc8)。

- [ ] **Step 6: 运行测试确认通过**

Run: `cd tools/cc_bridge && python -m pytest tests/test_serial_link.py -v`
Expected: PASS (5 passed)。

- [ ] **Step 7: Commit**

```bash
git add tools/cc_bridge/serial_link.py tools/cc_bridge/tests/test_serial_link.py tools/cc_bridge/requirements.txt
git commit -m "feat(cc_bridge): 串口帧编解码 + SerialLink 等待唤醒"
```

---
## Task 7: bridge.py (Agent SDK 包裹 + can_use_tool 回调)

**Files:**
- Create: `tools/cc_bridge/bridge.py`

**Interfaces:**
- Consumes: `SerialLink` (Task 6); claude-agent-sdk 的 `query` / `ClaudeAgentOptions` / `can_use_tool` 回调 / `PermissionResultAllow` / `PermissionResultDeny`。
- 注: SDK 确切类名与 system_prompt append 结构在实现时以 `claude-agent-sdk` 安装版本的文档为准核实 (见设计文档 2.1 / 6.1)。

- [ ] **Step 1: 写 bridge.py**

```python
import asyncio, sys
from serial_link import SerialLink
from claude_agent_sdk import query, ClaudeAgentOptions
from claude_agent_sdk import PermissionResultAllow, PermissionResultDeny

APPEND_PROMPT = "遇到有多个合理方案的决策点时，优先用 AskUserQuestion 让用户选，而不是直接选定。"
PERM_OPTS = ["允许", "拒绝", "总是允许"]
DECISION_TIMEOUT = 120.0

def _summarize(tool_name, tool_input):
    if tool_name == "Bash":
        return f"运行: {tool_input.get('command','')}"[:31]
    if tool_name in ("Write", "Edit"):
        return f"{tool_name}: {tool_input.get('file_path','')}"[:31]
    return f"{tool_name}"[:31]

class Bridge:
    def __init__(self, port):
        self.link = SerialLink(port)
    def open(self):
        self.link.open()

    async def can_use_tool(self, tool_name, tool_input, context):
        # AskUserQuestion: Claude 主动多选题
        if tool_name == "AskUserQuestion":
            q = tool_input.get("questions", [{}])[0]
            title = q.get("question", "请选择")[:31]
            opts = [o.get("label", str(i)) for i, o in enumerate(q.get("options", []))][:4]
            did = self.link.send_decision(1, title, opts)
            try:
                idx = await asyncio.to_thread(self.link.wait_reply, did, DECISION_TIMEOUT)
            except TimeoutError:
                return PermissionResultDeny(message="K10 未选择 (超时)")
            if idx == 0xFF:
                return PermissionResultDeny(message="K10 取消/离线")
            # 把选择作为答案回填 (SDK 具体回填多选答案的字段实现时核实)
            chosen = opts[idx] if idx < len(opts) else opts[0]
            return PermissionResultAllow(updated_input={**tool_input, "_k10_choice": chosen})
        # 其它工具: 权限确认
        title = _summarize(tool_name, tool_input)
        did = self.link.send_decision(0, title, PERM_OPTS)
        try:
            idx = await asyncio.to_thread(self.link.wait_reply, did, DECISION_TIMEOUT)
        except TimeoutError:
            return PermissionResultDeny(message="K10 未确认 (超时)")
        if idx == 0:   # 允许
            return PermissionResultAllow()
        if idx == 2:   # 总是允许
            return PermissionResultAllow()
        return PermissionResultDeny(message="用户在 K10 上拒绝")

async def main():
    if len(sys.argv) < 3:
        print("用法: python bridge.py <COM口> <给Claude的任务>"); return
    port, prompt = sys.argv[1], sys.argv[2]
    b = Bridge(port); b.open()
    opts = ClaudeAgentOptions(
        system_prompt={"type": "preset", "preset": "claude_code", "append": APPEND_PROMPT},
        can_use_tool=b.can_use_tool,
    )
    async for msg in query(prompt=prompt, options=opts):
        print(msg)

if __name__ == "__main__":
    asyncio.run(main())
```

- [ ] **Step 2: 语法检查 (无硬件也能跑)**

Run: `cd tools/cc_bridge && python -c "import ast; ast.parse(open('bridge.py').read()); print('ok')"`
Expected: 打印 ok。(SDK 未装时 import 会失败, 此步只查语法。)

- [ ] **Step 3: Commit**

```bash
git add tools/cc_bridge/bridge.py
git commit -m "feat(cc_bridge): Agent SDK 包裹 + can_use_tool 决策回调"
```

---

## Task 8: 端到端联调 (需 Dongle 硬件, 计划 A 内部闭环)

**Files:** 无新增, 纯验证。

**Interfaces:** Consumes 全部前序任务。此任务不依赖 K10 (计划 B), 用"Dongle 造假回执"验证 PC↔Dongle 双向。

- [ ] **Step 1: 临时给 Dongle 加"回环假回执"验证 PC↔Dongle**

在 `pc_link.c` 的 `handle_pc_frame` 处理 0x0A、`pairing_has_pair()` 为真的分支里, 临时追加: 转发给 K10 的同时, 立刻本地造一个 chosen_index=0 的 0x0B 回执并 `pc_link_send_reply_to_pc` 写回 (模拟 K10)。用于计划 B 未就绪时先验证 PC 侧。

- [ ] **Step 2: PC 发一个决策, 确认收到回执**

写一个临时脚本 `tools/cc_bridge/smoke.py`:
```python
from serial_link import SerialLink
import sys
link = SerialLink(sys.argv[1]); link.open()
did = link.send_decision(0, "测试: 允许 Bash", ["允许","拒绝","总是允许"])
print("sent id", did)
print("got index", link.wait_reply(did, 10))
```
Run: `cd tools/cc_bridge && python smoke.py <UART0_COM>`
Expected: 打印 `got index 0` (Dongle 造的假回执)。验证 PC↔Dongle 双向串口链路通。

- [ ] **Step 3: 移除临时回环代码**

删掉 Step 1 加的假回执, 保留正常"转发给 K10"逻辑。重新 build + flash。
Run: `cd dongle_firmware && idf.py build && idf.py -p <JTAG_COM> flash`

- [ ] **Step 4: Commit (清理)**

```bash
git add dongle_firmware/main/pc_link.c
git commit -m "test(dongle): 移除联调临时回环, 保留正式转发"
```

- [ ] **Step 5: 与计划 B 会师验证 (待 K10 就绪)**

待 K10 (计划 B) 完成后: `python bridge.py <UART0_COM> "帮我在当前目录创建 test.txt"` → K10 弹"允许 Write: test.txt?" → 触摸允许 → 文件被创建。此步跨两份计划, 由集成时统一执行。

---

## Self-Review (接收端)

- **spec 覆盖**: 协议扩展(T1)✓ console迁移(T2/spec2.3)✓ pc_link双向转发(T3/T4/spec6.2)✓ main启动(T5)✓ cc_bridge串口(T6)✓ can_use_tool+append提示(T7/spec6.1)✓ 分阶段测试(T8/spec8前两阶段)✓ 未配置错误帧(T3 offline分支/spec7)✓ 超时兜底(T7 DECISION_TIMEOUT/spec7)✓
- **占位符**: 已消除, 均为可执行代码/命令。SDK 确切 API 名 (类名/append 结构/多选回填字段) 明确标注"实现时按安装版本文档核实"——这是真实的外部依赖不确定性, 非占位符。
- **类型一致**: `pc_link_send_reply_to_pc` 签名 T3 定义、T4 调用一致; `crc8`/`encode_frame`/`pack_decision_request`/`parse_reply`/`SerialLink` 在 T6 定义、T7/T8 使用一致; frame_type 常量与 Task 1 一致。
- **已知需实现时敲定**: (a) SerialLink.send_decision 的双层 crc 语义已在 T6 Step5 修正为传完整 body; (b) SDK 多选答案回填的确切机制 (updated_input vs 专用返回) 需按 SDK 文档定。
