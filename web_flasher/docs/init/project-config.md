# 项目配置报告

> 由 `/init` 技能自动生成，描述项目的初始化状态与环境配置。
> 重新运行 `/init` 会覆盖本文件。

**生成时间**：2026-09-16 18:34:34
**生成工具**：init-skill
**配置版本**：1.0

---

## 1. 项目基本信息

| 项 | 值 |
|----|-----|
| 项目名称 | `web_flasher` |
| 项目路径 | `web_flasher` |
| 项目类型 | **Web 应用 + Electron 桌面应用** |
| 设备型号 | **ESP32-S3 多产品烧录平台** |
| 功能模块 | **Web Serial API 固件烧录工具** |
| 技术栈 | **HTML5 + Web Serial + esptool-js + Electron** |
| 文档版本 | V1.0 |

---

## 2. 技术架构

### 2.1 前端技术

| 项 | 值 |
|----|-----|
| 核心技术 | 纯 HTML5 单页应用 |
| 烧录引擎 | esptool-js 0.5.7 |
| 浏览器 API | Web Serial API |
| 支持浏览器 | Chrome/Edge 89+ 桌面版 |
| 样式系统 | 内联 CSS（深色主题） |
| 模块化 | ES6 模块 |

### 2.2 桌面应用

| 项 | 值 |
|----|-----|
| 框架 | Electron 28.0.0 |
| 主进程 | electron-main.js |
| 构建工具 | electron-builder 24.9.0 |
| 输出格式 | NSIS 安装包 + Portable 便携版 |
| 应用 ID | com.codebuddy.flasher |

### 2.3 部署方式

| 方式 | 说明 | 状态 |
|------|------|------|
| 本地服务器 | `serve.py` (Python HTTP Server, 127.0.0.1:8000) | ✅ 可用 |
| Electron 桌面 | `dist-electron/CodeBuddy Flasher 1.0.0.exe` | ✅ 已构建 |
| GitHub Pages | `deploy_pages/` + `sync_and_push.ps1` | ✅ 支持 |

---

## 3. 支持的产品

当前平台支持 **10 个固件产品**：

| 产品 ID | 名称 | 徽章 | 版本 | 芯片 | Flash |
|---------|------|------|------|------|-------|
| `k10` | CodeBuddy 发射端 | TRANSMIT | v1.0.2 | ESP32-S3 | 16MB |
| `dongle` | CodeBuddy 接收端 | RECEIVE | v1.1.0 | ESP32-S3 | 16MB |
| `codebuddy_ai_box` | AI BOX 原子版 | AI BOX | v1.0.0 | ESP32-S3 | 16MB |
| `30_lvgl_Gif` | LVGL Gif 演示 | DEMO | v1.0.0 | ESP32-S3 | 16MB |
| `32_DEMO_MJPEG` | MJPEG 视频播放器 | VIDEO | v1.0.0 | ESP32-S3 | 16MB |
| `40_Snake_Game` | 贪吃蛇游戏 | GAME | v1.0.0 | ESP32-S3 | 16MB |
| `42_demo_ui_lcd` | LVGL 图形界面 | UI | v1.0.0 | ESP32-S3 | 16MB |
| `22_SC207aH` | SC7A20H 传感器 | SENSOR | v1.0.0 | ESP32-S3 | 16MB |
| `43_RTOS_face` | 人脸识别 | LVGL | v1.0.0 | ESP32-S3 | 16MB |
| `44_mp3Player2` | MP3 播放器 | AUDIO | v1.0.0 | ESP32-S3 | 16MB |

**固件文件存储**：`firmware/<产品id>/{bootloader,partitions,firmware}.bin`

---

## 4. 依赖检查

### 4.1 运行时依赖（浏览器环境）

| 依赖 | 类型 | 版本 | 状态 |
|------|------|------|------|
| `esptool-js` | JavaScript 库 | 0.5.7 | ✅ 本地已缓存 (179KB) |
| Web Serial API | 浏览器 API | - | ✅ Chrome/Edge 支持 |

**说明**：`esptool-js` 优先从 `vendor/esptool-bundle.js` 加载（离线），失败时回退到 CDN。

### 4.2 开发依赖（本地构建）

| 依赖 | 类型 | 版本 | 状态 |
|------|------|------|------|
| `python` | CLI | 3.11.9 | ✅ 已安装 |
| `node` | CLI | v24.18.0 | ✅ 已安装 |
| `npm` | CLI | 11.16.0 | ✅ 已安装 |
| `electron` | npm 包 | 28.0.0 | ✅ 已安装 |
| `electron-builder` | npm 包 | 24.9.0 | ✅ 已安装 |

### 4.3 可选依赖

| 依赖 | 用途 | 状态 |
|------|------|------|
| `pyinstaller` | 打包 Python 启动器 | 可选 |
| `osslsigncode` | Windows 代码签名 | 可选 |

---

## 5. 项目结构

```
web_flasher/
├── index.html                    # 单页应用主文件（首页 + 烧录界面）
├── serve.py                      # Python HTTP 服务器（127.0.0.1:8000）
├── electron-main.js              # Electron 主进程入口
├── package.json                  # Electron 项目配置
├── firmware/                     # 固件文件存储（10 个产品）
│   ├── k10/
│   ├── dongle/
│   ├── codebuddy_ai_box/
│   └── ...
├── images/                       # 产品实物图
│   ├── K10.jpg
│   └── dongle.jpg
├── vendor/                       # 离线依赖
│   └── esptool-bundle.js         # esptool-js 本地副本
├── deploy_pages/                 # GitHub Pages 部署目录
├── dist-electron/                # Electron 构建输出
│   ├── CodeBuddy Flasher 1.0.0.exe  # Portable 便携版（73MB）
│   └── CodeBuddy Flasher Setup 1.0.0.exe  # NSIS 安装包（188KB）
├── docs/
│   └── init/
│       └── project-config.md     # 本文档
├── README.md                     # 用户使用指南
├── EXTEND.md                     # 产品扩展指南
└── BUILD_GUIDE.md                # 构建指南
```

---

## 6. 关键文件说明

| 文件 | 作用 | 说明 |
|------|------|------|
| `index.html` | 单页应用 | 包含首页卡片 + 烧录界面 + 全部 JavaScript 逻辑 |
| `serve.py` | 本地服务器 | 绑定 127.0.0.1:8000，禁用缓存，自动打开浏览器 |
| `electron-main.js` | Electron 主进程 | 创建窗口，启用 Web Serial API |
| `download_vendor.js` | 依赖下载脚本 | 从 CDN 下载 esptool-js 到 vendor/ |
| `sync_and_push.ps1` | 部署脚本 | 同步固件文件到 deploy_pages/ 并推送 GitHub Pages |
| `check_files.py` | 完整性检查 | 验证所有产品的固件文件是否存在 |
| `build_launcher.py` | 打包脚本 | 用 PyInstaller 打包 serve.py 为 exe |

---

## 7. 使用流程

### 7.1 Web 模式（推荐）

```bash
# 启动本地服务器
cd web_flasher
python serve.py

# 浏览器自动打开 http://localhost:8000
# 1. 选择产品卡片
# 2. 连接设备（USB 串口）
# 3. 开始写入
```

### 7.2 Electron 桌面模式

```bash
# 开发模式
npm start

# 构建便携版
npm run build:electron

# 构建安装包
npm run build:electron:nsis
```

### 7.3 添加新产品

参考 `EXTEND.md` 文档：
1. 创建 `firmware/<产品id>/` 目录，放入 bin 文件
2. 编辑 `index.html` 的 `FIRMWARE_TARGETS` 数组
3. （可选）添加产品图片到 `images/` 并添加 CSS 样式

---

## 8. 构建产物

| 产物 | 路径 | 大小 | 说明 |
|------|------|------|------|
| Electron 便携版 | `dist-electron/CodeBuddy Flasher 1.0.0.exe` | 73MB | 双击运行，无需安装 |
| Electron 安装包 | `dist-electron/CodeBuddy Flasher Setup 1.0.0.exe` | 188KB | NSIS 安装程序 |
| GitHub Pages | `deploy_pages/` | - | 静态网站部署包 |
| Python 启动器 | `dist/launcher.exe` | - | PyInstaller 打包（可选）|

---

## 9. 环境要求

### 9.1 用户侧（运行烧录工具）

| 环境 | 要求 |
|------|------|
| 操作系统 | Windows 10+ / macOS / Linux |
| 浏览器 | Chrome 89+ / Edge 89+ 桌面版 |
| 访问协议 | localhost 或 HTTPS（不能用 file://） |
| 硬件接口 | USB 串口（CH340/CP2102/内置 JTAG） |

### 9.2 开发侧（修改/构建）

| 环境 | 要求 |
|------|------|
| Python | 3.8+ |
| Node.js | 18+ |
| npm | 9+ |
| Git | （部署 GitHub Pages 时需要） |

---

## 10. 参数来源

**自动检测**：
- `project.type`：从 `package.json` 和 `index.html` 推断
- `tech.frontend`：分析 `index.html` 结构
- `tech.desktop`：检测 `electron-main.js` 和 `package.json`
- `products.list`：解析 `index.html` 的 `FIRMWARE_TARGETS` 数组
- `dependencies.versions`：执行 `python --version`, `node --version` 等

**用户提供**：
- `project.module`："Web Serial API 固件烧录工具"
- `project.docVersion`：V1.0

---

## 11. 后续建议

1. **使用 Web 模式快速测试**
   ```bash
   cd web_flasher
   python serve.py
   ```

2. **验证固件文件完整性**
   ```bash
   python check_files.py
   ```

3. **更新产品固件**
   - 重新构建固件后，复制 bin 到 `firmware/<产品id>/`
   - 刷新浏览器即可（无需修改代码）

4. **添加新产品**
   - 参考 `EXTEND.md` 文档
   - 编辑 `index.html` 的 `FIRMWARE_TARGETS` 数组

5. **部署到 GitHub Pages**
   ```powershell
   .\sync_and_push.ps1
   ```

6. **构建 Electron 桌面版**
   ```bash
   npm run build:electron
   ```

---

## 12. 已知限制

| 限制 | 说明 | 解决方案 |
|------|------|----------|
| 仅支持 Chrome/Edge | Web Serial API 浏览器限制 | 使用 Electron 桌面版（内置 Chromium） |
| 不能用 file:// | Web Serial 需要 Secure Context | 必须通过 `serve.py` 或 Electron 运行 |
| 固件文件不上传 | 纯本地处理，固件在浏览器内读取 | 优势：隐私安全，无服务器成本 |
| 单次只能烧录 1 个设备 | Web Serial 限制 | 连续烧录：烧完一个，重连下一个 |

---

## 13. 故障排查

| 问题 | 可能原因 | 解决方法 |
|------|----------|----------|
| 浏览器报"不支持 Web Serial" | 用了 Firefox/Safari | 改用 Chrome/Edge 桌面版 |
| 连接设备失败 | 驱动未安装 / 端口被占用 | 安装 CH340/CP2102 驱动，关闭占用串口的程序 |
| 固件文件 404 | bin 文件缺失或路径错误 | 运行 `python check_files.py` 检查 |
| 芯片识别错误 | 连接了错误的设备 | 确认设备型号，检查 USB 线是否支持数据传输 |
| Electron 打包失败 | node_modules 损坏 | 删除 `node_modules/` 和 `package-lock.json`，重新 `npm install` |

---

**文档结束**
