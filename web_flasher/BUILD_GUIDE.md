# CodeBuddy Web Flasher 打包说明

## 方案 B：PyInstaller 启动器 exe

### 准备工作

1. **下载 esptool-js 到本地**（离线支持）：
   ```bash
   node download_vendor.js
   ```
   验证：`vendor/esptool-bundle.js` 应为 ~380KB

2. **安装 PyInstaller**：
   ```bash
   pip install pyinstaller
   ```

### 打包步骤

```bash
python build_launcher.py
```

成功后会生成 `dist/CodeBuddy_Flasher.exe`（约 10-15MB，含固件）

### 使用方法

双击 `CodeBuddy_Flasher.exe`：
- 自动启动本地服务器（端口 8000）
- 自动在浏览器中打开 flasher 页面
- 控制台窗口显示服务器状态
- 关闭控制台窗口即停止服务器

### 清理构建文件

```bash
python build_launcher.py --clean
```

---

## 方案 A：Electron 桌面应用 exe

### 准备工作

1. **初始化 Electron 项目**（如果还没有 node_modules）：
   ```bash
   npm init -y
   npm install electron electron-builder --save-dev
   ```

2. **配置 package.json**（见下方脚本）

### 打包步骤

```bash
npm run build:electron
```

成功后会生成 `dist-electron/CodeBuddy Flasher Setup.exe`（约 150-200MB）

### 使用方法

- 运行安装程序，安装到本地
- 桌面出现快捷方式，双击即启动独立窗口
- 真正的桌面应用，不依赖系统浏览器

---

## 两种方案对比

| 方案 | 体积 | 依赖 | 体验 | 适合场景 |
|------|------|------|------|----------|
| B: PyInstaller 启动器 | ~10-15MB | 需要系统已安装 Chrome/Edge | 浏览器窗口 | 快速分发、体积敏感 |
| A: Electron 桌面应用 | ~150-200MB | 无（内置 Chromium） | 独立窗口 | 完整桌面应用体验 |

---

## 技术细节

### 离线支持

`index.html` 已修改为优先加载本地 `vendor/esptool-bundle.js`：
- 如果本地文件存在 → 完全离线工作
- 如果本地文件不存在 → 回退到 CDN（需联网）

### Web Serial API 要求

两种方案都保证 Web Serial API 可用：
- PyInstaller：依赖系统 Chrome/Edge（原生支持）
- Electron：内置 Chromium（原生支持）

注意：Firefox/Safari 不支持 Web Serial，无法使用此 flasher。

---

## 故障排查

### PyInstaller 打包失败

1. 检查 PyInstaller 版本：`pip show pyinstaller`（建议 5.0+）
2. 检查资源文件是否存在：`vendor/`, `firmware/`, `images/`, `index.html`
3. 查看详细错误：打包过程中的红色输出

### exe 运行后浏览器无法连接设备

1. 确认使用 Chrome 或 Edge 桌面版
2. 确认 URL 是 `http://localhost:8000`（不是 `file://`）
3. 检查控制台是否有"Web Serial 不支持"警告

### Electron 打包体积过大

- 正常现象（内置完整 Chromium）
- 可用 `electron-builder` 的压缩选项减小 ~10-20%

---

## 下一步

- [ ] 完成 esptool-js 下载（`node download_vendor.js`）
- [ ] 测试 PyInstaller 打包
- [ ] 初始化 Electron 项目
- [ ] 测试 Electron 打包
