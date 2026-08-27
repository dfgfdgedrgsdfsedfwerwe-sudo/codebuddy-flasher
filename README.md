# CodeBuddy Web Flasher

基于 Web Serial API 的 ESP32-S3 固件烧录工具，支持浏览器直接烧录无需安装驱动。

## 在线使用

🔗 **访问地址**: https://你的用户名.github.io/codebuddy-flasher/

（部署后替换上面的 URL）

## 支持设备

- **CodeBuddy 发射端（K10）** - 无线键盘 + 音频流
- **CodeBuddy 接收端（Dongle）** - USB 复合设备
- **CodeBuddy 发射端（AI BOX 原子）** - AI 交互功能
- **LVGL 演示** - Gif 播放、UI 组件、游戏等

## 系统要求

- Chrome/Edge 浏览器（版本 89+）
- Windows / macOS / Linux
- USB 数据线连接设备

⚠️ **重要**: Safari 和 Firefox 不支持 Web Serial API，无法使用。

## 使用步骤

1. 访问在线地址
2. 选择目标固件
3. 连接 USB 设备
4. 点击"连接设备"并选择串口
5. 点击"开始写入"

## 技术栈

- **Web Serial API** - 浏览器串口通信
- **esptool-js** - ESP32 烧录工具（JavaScript 移植）
- **纯静态** - 无需后端，完全离线可用

## 本地开发

```bash
# 克隆仓库
git clone https://github.com/你的用户名/codebuddy-flasher.git
cd codebuddy-flasher

# 启动本地服务器（任选其一）
python -m http.server 8000
# 或
npx http-server -p 8000

# 访问 http://localhost:8000
```

## 离线版本

桌面 exe 版本（Windows）请访问主项目仓库下载。

## 许可证

MIT License

---

**开发**: DFRobot UNIHIKER K10 Team  
**更新**: 2026-08-27
