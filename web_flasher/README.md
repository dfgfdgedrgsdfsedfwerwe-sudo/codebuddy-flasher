# Project Showcase

多类型项目展示平台，支持固件在线烧录、Web 应用预览、Android/Windows 应用下载。

## 使用

### 固件项目
1. 运行本地服务器：
   ```
   python web_flasher/serve.py
   ```
2. 用 **Chrome 或 Edge 桌面版** 打开 `http://localhost:8000`
   （不支持 Firefox/Safari；不能直接双击 HTML 用 file:// 打开）
3. 首页选择固件类型项目卡片（绿色徽章），进入烧录界面
4. 用数据线连接设备，点 "连接设备"，选择 "USB JTAG/serial debug unit (COMxx)"
5. 点 "开始写入"，等待进度条完成，设备自动重启

### Web 项目
点击卡片后，新标签页打开在线演示地址。

### Android/Windows 项目
点击卡片进入详情页，下载 APK/EXE 文件到本地安装。

## 功能特性

- **多项目类型支持**：固件、Web、Android、Windows 四种类型
- **多产品支持**：首页卡片画廊，点击进入对应交互流程
- **本地处理**：固件文件仅在浏览器本地读取，不上传服务器
- **连续烧录**：支持烧录多个设备/产品，自动断开旧连接、引导重连
- **手选覆盖**：可用本地 bin 文件覆盖预置固件
- **芯片校验**：烧录前校验芯片型号，防止误刷
- **实物图支持**：卡片背景可显示产品实物图（自适应横/竖图）

## 项目类型支持

Project Showcase 平台支持四种项目类型：

| 类型 | 徽章颜色 | 交互方式 |
|------|---------|---------|
| 固件 (firmware) | 绿色 #22c55e | 点击进入 Web Serial 烧录界面 |
| 网页 (web) | 蓝色 #3b82f6 | 新标签页打开在线演示 |
| Android | 橙色 #f97316 | 详情页 + APK 下载 |
| Windows | 紫色 #a855f7 | 详情页 + EXE/MSI 下载 |

### 添加新项目

使用 project-publish skill 自动添加：

```bash
# 自动检测项目类型并添加
python .claude/skills/project-publish/scripts/add_project.py \
  --project-dir /path/to/project \
  --name "项目名" \
  --badge "BADGE" \
  --version "v1.0.0" \
  --description "项目描述"
```

详细的扩展指南请参考 `EXTEND.md`。

## 添加新产品

详见 **[EXTEND.md](EXTEND.md)** — 完整的项目扩展指南。

简要步骤：
1. 准备项目资源（固件 bin / 网页源码 / APK / EXE）
2. 运行 `add_project.py` 或手动编辑 `index.html` 的 `PROJECTS` 数组
3. （可选）把产品图放到 `images/<产品id>.jpg` 并加 CSS 背景样式

类型自动检测：脚本根据项目目录内容自动判断类型（`platformio.ini` → firmware, `package.json` → web, `build.gradle` → android, `*.sln` → windows）。

## 更新已有项目

**固件项目**：重新构建后，把新的 bin 复制到 `firmware/<产品id>/` 覆盖即可，刷新页面生效：
- K10: `.pio/build/51_mic_wifi/{bootloader,partitions,firmware}.bin`
- Dongle: `dongle_firmware/build/{bootloader/bootloader,partition_table/partition-table}.bin` 和 `dongle_firmware/build/codebuddy_dongle.bin`

**Web 项目**：更新 `projects/<项目id>/` 下的构建产物。

**Android/Windows 项目**：替换 `downloads/<项目id>/` 下的安装包文件。

## 目录结构

```
web_flasher/
├── index.html          # 单页应用（首页卡片 + 烧录/详情界面 + 全部逻辑）
├── serve.py            # 本地服务器（绑定 127.0.0.1:8000）
├── firmware/           # 固件项目 bin 文件
│   ├── k10/
│   └── dongle/
├── projects/           # Web 项目构建产物（HTML/CSS/JS）
├── downloads/          # Android APK / Windows EXE 安装包
├── images/             # 产品实物图和项目截图
├── README.md           # 本文档
└── EXTEND.md           # 新增项目扩展指南
```

## 技术说明

- **烧录引擎**：[esptool-js](https://github.com/espressif/esptool-js)（CDN 加载，版本锁定 0.5.7）
- **通信**：Web Serial API（Chrome/Edge 89+）
- **安全上下文**：需 localhost 或 HTTPS
- **服务器**：Python `http.server`，仅绑定 127.0.0.1（不暴露局域网）
