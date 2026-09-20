# Web Flasher 自动发布系统

## 🎯 功能概述

自动化固件项目发布流程：**构建 → 复制 → 配置 → 部署**，一键将新项目添加到 Web Flasher 展示平台。

---

## 🚀 快速开始（3 步发布）

### Step 1: 构建固件

```bash
# PlatformIO 项目
cd examples/your_project
pio run

# ESP-IDF 项目
cd your_firmware
idf.py build
```

### Step 2: 执行发布

```bash
# 返回项目根目录
cd ../..

# 自动模式（最简单）
python .claude/skills/project-publish/scripts/publish.py examples/your_project --auto

# 或指定参数
python .claude/skills/project-publish/scripts/publish.py examples/your_project \
  --id your-id \
  --name "项目名称" \
  --badge NEW \
  --version v1.0.0
```

### Step 3: 验证结果

```bash
cd web_flasher
python serve.py
# 浏览器打开 http://localhost:8000，检查新项目卡片
```

---

## 📋 发布参数

| 参数 | 说明 | 示例 |
|------|------|------|
| `source_project` | 源项目路径 | `examples/51_mic_wifi` |
| `--id` | 唯一标识（小写+连字符） | `k10`, `new-sensor` |
| `--name` | 显示名称 | `"CodeBuddy 发射端"` |
| `--badge` | 徽章（默认 NEW） | `TRANSMIT`, `STABLE`, `DEMO` |
| `--version` | 版本号（默认 v1.0.0） | `v1.2.3` |
| `--icon` | emoji 图标 | `🚀`, `🎮` |
| `--image` | 产品图片路径 | `images/product.jpg` |
| `--deploy` | 部署到 GitHub Pages | - |
| `--auto` | 自动推断所有参数 | - |

---

## 💡 使用场景

### 场景 1：发布新项目

```bash
# 你刚完成一个新的传感器项目
python .claude/skills/project-publish/scripts/publish.py \
  examples/new_sensor \
  --id temp-sensor \
  --name "智能温湿度传感器" \
  --badge NEW \
  --icon 🌡️
```

### 场景 2：更新已有项目

```bash
# K10 固件更新到 v1.0.3
python .claude/skills/project-publish/scripts/publish.py \
  examples/51_mic_wifi \
  --id k10 \
  --name "CodeBuddy 发射端" \
  --badge TRANSMIT \
  --version v1.0.3
```

### 场景 3：批量发布

```bash
# 发布所有示例项目
for example in examples/*/; do
  python .claude/skills/project-publish/scripts/publish.py "$example" --auto
done
```

### 场景 4：发布并部署

```bash
# 发布新项目并推送到 GitHub Pages
python .claude/skills/project-publish/scripts/publish.py \
  dongle_firmware \
  --id dongle \
  --name "CodeBuddy 接收端" \
  --deploy
```

---

## 🔧 技术实现

### 自动化流程

```
1️⃣ 检测构建系统（PlatformIO / ESP-IDF）
    ↓
2️⃣ 定位固件文件
    .pio/build/<env>/{bootloader,partitions,firmware}.bin
    或 build/{bootloader,partition-table,<name>}.bin
    ↓
3️⃣ 提取项目元信息
    - 芯片型号（从 platformio.ini / sdkconfig）
    - Flash 配置（模式、频率、大小）
    - 项目描述（从 README.md / .ino 注释）
    ↓
4️⃣ 复制固件到 web_flasher/firmware/<id>/
    ↓
5️⃣ 更新 index.html 的 FIRMWARE_TARGETS 数组
    ↓
6️⃣ （可选）处理产品图片 + CSS 样式
    ↓
7️⃣ （可选）部署到 GitHub Pages
```

### 关键脚本

| 脚本 | 功能 |
|------|------|
| `publish.py` | 主执行脚本，协调整个发布流程 |
| `extract_metadata.py` | 从源项目提取芯片配置和描述 |
| `add_project.py` | 编辑 index.html，添加项目配置 |

---

## 📁 文件结构

```
project/
├── .claude/skills/project-publish/
│   ├── skill.md              # Claude Code skill 定义
│   ├── README.md             # 详细使用指南
│   ├── QUICKSTART.md         # 本文档
│   └── scripts/
│       ├── publish.py        # 主脚本
│       ├── add_project.py    # HTML 编辑器
│       └── extract_metadata.py  # 元信息提取
│
├── web_flasher/
│   ├── index.html            # 单页应用（会被自动修改）
│   ├── firmware/             # 固件存储目录
│   │   ├── k10/
│   │   ├── dongle/
│   │   └── <new-project>/    # 新项目固件会复制到这里
│   └── images/               # 产品图片
│
└── examples/                 # 源项目
    └── your_project/
```

---

## ✅ 验证清单

发布后检查：

- [ ] `web_flasher/firmware/<id>/` 包含 3 个 bin 文件
- [ ] `index.html` 的 `FIRMWARE_TARGETS` 数组有新项目
- [ ] 运行 `python serve.py`，首页显示新卡片
- [ ] 点击卡片进入烧录界面，分区信息正确
- [ ] 连接设备测试烧录（可选）

---

## 🐛 常见问题

### Q: 固件文件不存在

```bash
# 先构建项目
cd examples/your_project
pio run  # 或 idf.py build
```

### Q: 项目 ID 重复

使用不同的 ID 或让脚本覆盖旧配置（会提示警告）

### Q: 部署失败

检查 git 状态和 `sync_and_push.ps1` 脚本

---

## 📚 完整文档

- **详细使用指南**: `.claude/skills/project-publish/README.md`
- **Skill 定义**: `.claude/skills/project-publish/skill.md`
- **Web Flasher 用户手册**: `web_flasher/README.md`
- **产品扩展指南**: `web_flasher/EXTEND.md`

---

## 🎉 示例输出

```bash
$ python publish.py examples/51_mic_wifi --id k10 --name "CodeBuddy 发射端"

📦 发布项目: /path/to/examples/51_mic_wifi
✅ 构建系统: platformio
📁 复制固件文件到 web_flasher/firmware/k10/
  ✓ bootloader.bin
  ✓ partitions.bin
  ✓ firmware.bin
✅ 固件文件已复制到 web_flasher/firmware/k10/
📋 提取项目元信息...
📝 更新 index.html...
✅ 已添加项目 k10 到 web_flasher/index.html
🔍 验证固件文件完整性...

============================================================
✅ 项目 k10 已发布到 Web Flasher
📂 固件路径: web_flasher/firmware/k10/
🌐 本地预览: python web_flasher/serve.py
============================================================
```

---

**下一步**：运行 `python web_flasher/serve.py` 查看发布结果！
