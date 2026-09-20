# CodeBuddy 网页固件烧录工具 — 设计文档

日期：2026-08-25
状态：已确认，待实施

## 1. 目标与背景

参考 folotoy AI Passport 的网页烧录工具（`ai-passport.folotoy.cn/tools/web-flasher/`）及其产品展示站点，为 CodeBuddy 无线系统做一个**浏览器内固件烧录工具 + 产品展示首页**。

核心价值：
- 用户无需安装 esptool / PlatformIO / idf.py，打开浏览器即可给 K10 和 Dongle 烧录固件。
- 固件文件只在本机浏览器中处理，不上传服务器。
- 固件目标可扩展，方便后续按不同项目上传不同固件。

## 2. 技术约束（重要）

- 网页烧录依赖 **Web Serial API + esptool-js**。
- **仅支持 Chrome / Edge 桌面版**（Firefox / Safari 不支持 Web Serial）。
- 必须通过 **`localhost` 或 HTTPS** 访问；直接双击用 `file://` 打开无法调用串口。
- 因此需要一个本地服务器启动脚本。

## 3. 目录结构

放在仓库根目录下的 `web_flasher/`：

```
web_flasher/
├── index.html        # 展示首页 + 烧录工具（单文件，内联 CSS/JS）
├── firmware/         # 预置固件 bin
│   ├── k10/          # bootloader.bin, partitions.bin, firmware.bin
│   └── dongle/       # bootloader.bin, partition-table.bin, codebuddy_dongle.bin
├── serve.py          # 本地服务器启动脚本（python -m http.server 封装）
└── README.md         # 使用说明
```

esptool-js 从 CDN 加载（ESM，`https://unpkg.com/esptool-js`），无需 npm 构建。

## 4. 页面结构（三区块，folotoy 风格）

**区块 A — Hero 展示首页**
- 标题 + 标语（CodeBuddy 无线系统）
- 功能亮点卡片：无线麦克风、USB HID 键盘、6 屏状态显示、AI 云脸动画
- "开始烧录" 按钮滚动到工具区

**区块 B — 固件库（可扩展卡片）**
- 从 JS 配置数组 `FIRMWARE_TARGETS[]` 渲染
- 初始两张卡片：K10 发射端、Dongle 接收端
- 每张卡片显示：项目名、描述、芯片型号、分区信息
- 点卡片 → 选中该目标，分区地址与预置文件自动填入工具区

**区块 C — 烧录工具（三步编号流程）**
- 01 连接设备（Web Serial 选串口）
- 02 核对固件（显示所选目标的分区列表，可用预置文件或手选覆盖）
- 03 写入与重启（波特率选择、擦除选项、进度条、设备日志）

## 5. 核心数据结构

```javascript
const FIRMWARE_TARGETS = [
  {
    id: "k10",
    name: "K10 发射端",
    description: "CodeBuddy 麦克风 + ESP-NOW + 6屏状态显示",
    chip: "esp32s3",
    flashMode: "dio", flashFreq: "80m", flashSize: "16MB",
    partitions: [
      { name: "bootloader", offset: 0x0,     file: "firmware/k10/bootloader.bin" },
      { name: "partitions", offset: 0x8000,  file: "firmware/k10/partitions.bin" },
      { name: "app",        offset: 0x10000, file: "firmware/k10/firmware.bin" },
    ],
  },
  {
    id: "dongle",
    name: "Dongle 接收端",
    description: "USB HID + UAC 音频，左Ctrl+F2 录音键",
    chip: "esp32s3",
    flashMode: "dio", flashFreq: "80m", flashSize: "16MB",
    partitions: [
      { name: "bootloader",      offset: 0x0,     file: "firmware/dongle/bootloader.bin" },
      { name: "partition-table", offset: 0x8000,  file: "firmware/dongle/partition-table.bin" },
      { name: "app",             offset: 0x10000, file: "firmware/dongle/codebuddy_dongle.bin" },
    ],
  },
];
```

**扩展方式**：新增固件项目 = 往数组加一条 + 把 bin 放进 `firmware/<id>/`。首页固件库自动多一张卡片。

## 6. 分区地址（已从构建产物核实）

两个目标均为 ESP32-S3，地址布局一致：

| 分区        | 偏移     | K10 文件           | Dongle 文件               |
|-------------|----------|--------------------|---------------------------|
| bootloader  | 0x0      | bootloader.bin     | bootloader.bin            |
| 分区表      | 0x8000   | partitions.bin     | partition-table.bin       |
| app         | 0x10000  | firmware.bin       | codebuddy_dongle.bin      |

Flash 设置：`dio` / `80m` / `16MB`。注意 ESP32-S3 bootloader 在 0x0（不是经典 ESP32 的 0x1000）。K10 分区表为 `factory` 布局，无 `boot_app0.bin`。

## 7. 烧录流程（esptool-js）

1. `navigator.serial.requestPort()` → 用户选串口（USB JTAG/serial debug unit）
2. `new ESPLoader({ transport, baudrate })` → `main()` 连接并识别芯片
3. 校验芯片型号与目标 `chip` 一致（不一致给警告）
4. 组装 `fileArray`（每项 `{ data, address }`），可选先 `eraseFlash()`
5. `writeFlash({ fileArray, flashSize, flashMode, flashFreq, reportProgress })`
6. 完成后 `after: hard_reset` 硬复位重启
7. 全程回调更新进度条 + 设备日志

预置文件通过 `fetch('firmware/...')` 读取（同源）；手选文件通过 `<input type=file>` + `FileReader` 覆盖对应分区。

## 8. 错误处理

- **浏览器检测**：无 `navigator.serial` → 显眼提示"请用 Chrome/Edge 桌面版"，禁用连接按钮。
- **安全上下文**：`!window.isSecureContext` 且非 localhost → 提示必须通过本地服务器访问。
- **连接失败 / 写入中断 / 校验失败**：在设备日志区明确显示错误，不静默失败。
- **芯片不匹配**：识别到的芯片与目标 `chip` 不符时警告并要求确认。
- **文件缺失**：预置 bin `fetch` 404 时提示该目标固件未放置，引导手选。

## 9. 本地服务器脚本

`serve.py`：封装 `http.server`，默认端口 8000，启动后打印访问地址，尽量自动打开浏览器。`README.md` 说明：运行 `python web_flasher/serve.py`，浏览器访问 `http://localhost:8000`，用 Chrome/Edge。

## 10. 非目标（YAGNI）

- 不做纯通用手动地址输入模式（用户明确选了预置目标）。
- 不做完整玩法库 / 多页站点 / 使用指南多页（只做单页展示首页 + 工具）。
- 不做固件云端存储或 OTA。
- 不做 npm / Vite 工程化（单文件 + CDN 足够）。
