# CodeBuddy Web Flasher - EXE 打包方案

本目录包含将 Web Flasher 打包为 Windows exe 的完整方案。

## 快速开始

### 1. 准备离线资源

```bash
# 下载 esptool-js 到本地（实现离线烧录）
node download_vendor.js
```

验证：`vendor/esptool-bundle.js` 应为 ~380KB

### 2. 验证文件齐全

```bash
python check_files.py
```

### 3. 选择打包方案

#### 方案 B：PyInstaller 启动器（推荐，体积小）

```bash
# 安装 PyInstaller（如果还没有）
pip install pyinstaller

# 打包
python build_launcher.py

# 输出：dist/CodeBuddy_Flasher.exe (~10-15MB)
```

#### 方案 A：Electron 桌面应用（完整桌面体验）

```bash
# 安装依赖
npm install

# 打包
npm run build:electron

# 输出：dist-electron/CodeBuddy Flasher Setup.exe (~150-200MB)
```

---

## 方案对比

| 特性 | 方案 B (PyInstaller) | 方案 A (Electron) |
|------|---------------------|-------------------|
| **体积** | ~10-15MB | ~150-200MB |
| **依赖** | 需系统安装 Chrome/Edge | 无（内置 Chromium） |
| **窗口** | 系统浏览器窗口 | 独立应用窗口 |
| **启动速度** | 快（<1秒） | 较慢（2-3秒） |
| **适合场景** | 快速分发、U盘工具 | 完整桌面应用 |

---

## 使用方法

### PyInstaller 版本

1. 双击 `CodeBuddy_Flasher.exe`
2. 控制台窗口显示"服务器运行中"
3. 浏览器自动打开 flasher 页面
4. 关闭控制台窗口即停止服务器

### Electron 版本

1. 运行安装程序（首次）
2. 双击桌面快捷方式
3. 在独立窗口中操作
4. 关闭窗口即退出

---

## 离线支持

`index.html` 已修改为优先加载本地 `vendor/esptool-bundle.js`：

- ✓ 本地文件存在 → 完全离线工作
- ✓ 本地文件缺失 → 回退到 CDN（需联网）

---

## 技术要点

### Web Serial API 支持

两种方案都确保 Web Serial API 可用：

- **PyInstaller**：依赖系统 Chrome/Edge（原生支持）
- **Electron**：内置 Chromium（原生支持，需配置 `experimentalFeatures`）

⚠️ Firefox/Safari 不支持 Web Serial API，无法使用。

### 资源打包

两种方案都会打包以下资源：

```
index.html           - 主页面
vendor/              - esptool-js bundle（离线支持）
firmware/            - 固件文件（k10、dongle、示例项目）
images/              - 图片资源
```

---

## 文件说明

| 文件 | 用途 |
|------|------|
| `download_vendor.js` | 下载 esptool-js 到本地 vendor/ |
| `check_files.py` | 验证打包所需文件是否齐全 |
| `launcher.py` | PyInstaller 启动器主程序 |
| `build_launcher.py` | PyInstaller 打包脚本 |
| `electron-main.js` | Electron 主进程 |
| `package.json` | Electron 项目配置 + 打包配置 |
| `BUILD_GUIDE.md` | 详细打包指南（故障排查） |

---

## 故障排查

### vendor 文件下载失败

```bash
# 手动验证网络
curl -I https://unpkg.com/esptool-js@0.5.7/bundle.js

# 或使用浏览器直接下载后放到 vendor/ 目录
```

### PyInstaller 打包报错

```bash
# 检查版本（建议 5.0+）
pip show pyinstaller

# 查看详细日志
python build_launcher.py 2>&1 | tee build.log
```

### Electron 打包报错

```bash
# 清理缓存重试
rm -rf node_modules dist-electron
npm install
npm run build:electron
```

### exe 运行后无法连接设备

1. 确认使用 **Chrome 或 Edge 桌面版**（PyInstaller 版本）
2. 确认 URL 是 `http://localhost:8000`（不是 `file://`）
3. 打开浏览器开发者工具查看错误信息

---

## 开发模式

### 测试 PyInstaller 版本（不打包）

```bash
python launcher.py
```

### 测试 Electron 版本（不打包）

```bash
npm start
```

---

## 清理构建文件

### PyInstaller

```bash
python build_launcher.py --clean
```

### Electron

```bash
rm -rf dist-electron node_modules
```

---

## 下一步

- [ ] 添加应用图标（`.ico` 文件）
- [ ] 配置代码签名（避免 Windows Defender 警告）
- [ ] 添加自动更新功能（Electron 版本可用 `electron-updater`）
- [ ] 多语言支持（中英文切换）

---

## License

见项目根目录 LICENSE 文件
