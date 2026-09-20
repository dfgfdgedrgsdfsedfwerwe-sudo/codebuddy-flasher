# project:publish 使用指南

## 快速开始

### 方式 1：使用 Claude Code skill

```bash
# 最简单：自动推断所有参数
/project:publish examples/51_mic_wifi --auto

# 指定关键参数
/project:publish examples/30_lvgl_Gif --id lvgl-gif --name "LVGL Gif 演示" --badge DEMO

# 完整参数 + 部署
/project:publish dongle_firmware --id dongle --name "CodeBuddy 接收端" --badge RECEIVE --version v1.1.0 --deploy
```

### 方式 2：直接运行 Python 脚本

```bash
cd .claude/skills/project-publish/scripts

# 最简模式
python publish.py examples/51_mic_wifi --auto

# 自定义参数
python publish.py examples/30_lvgl_Gif \
  --id lvgl-gif \
  --name "LVGL Gif 演示" \
  --badge DEMO \
  --version v1.0.0

# 带产品图片
python publish.py examples/40_Snake_Game \
  --id snake-game \
  --name "贪吃蛇游戏" \
  --badge GAME \
  --image ../../../images/snake.jpg

# 发布并部署
python publish.py dongle_firmware \
  --id dongle \
  --name "CodeBuddy 接收端" \
  --deploy
```

---

## 参数说明

| 参数 | 必选 | 说明 | 示例 |
|------|------|------|------|
| `source_project` | ✅ | 源项目路径 | `examples/51_mic_wifi` |
| `--id` | 条件必选 | 项目唯一标识（小写英文+连字符） | `k10`, `dongle`, `new-sensor` |
| `--name` | 条件必选 | 项目显示名称 | `"CodeBuddy 发射端"` |
| `--badge` | ❌ | 徽章文字（默认 NEW） | `TRANSMIT`, `STABLE`, `BETA` |
| `--version` | ❌ | 版本号（默认 v1.0.0） | `v1.0.0`, `v2.1.3` |
| `--description` | ❌ | 项目描述（默认自动提取） | `"ESP32-S3 + I2S 音频流"` |
| `--icon` | ❌ | emoji 图标（默认空） | `🚀`, `🎮`, `🌡️` |
| `--image` | ❌ | 产品图片路径 | `images/product.jpg` |
| `--deploy` | ❌ | 是否部署到 GitHub Pages | - |
| `--auto` | ❌ | 自动推断 ID 和名称 | - |

**注意**：
- 使用 `--auto` 时，`--id` 和 `--name` 可省略，会从路径自动推断
- 不使用 `--auto` 时，`--id` 和 `--name` 必须提供

---

## 执行流程

### 1. 构建系统检测

```
✅ 自动识别 PlatformIO 或 ESP-IDF
✅ PlatformIO: 从 platformio.ini 读取环境名
✅ ESP-IDF: 从 build/ 目录定位固件
```

### 2. 固件文件定位

**PlatformIO 项目**：
```
.pio/build/<env>/
├── bootloader.bin
├── partitions.bin
└── firmware.bin
```

**ESP-IDF 项目**：
```
<project>/build/
├── bootloader/bootloader.bin
├── partition_table/partition-table.bin
└── <project_name>.bin
```

### 3. 复制到 web_flasher

```
web_flasher/firmware/<project_id>/
├── bootloader.bin
├── partitions.bin
└── firmware.bin
```

### 4. 更新 index.html

在 `FIRMWARE_TARGETS` 数组添加新项目配置：

```javascript
{
  id: "new-project",
  name: "新项目名称",
  badge: "NEW",
  version: "v1.0.0",
  description: "项目描述...",
  icon: "🚀",
  chip: "esp32s3",
  flashMode: "dio",
  flashFreq: "80m",
  flashSize: "16MB",
  partitions: [
    { name: "bootloader", offset: 0x0, file: "firmware/new-project/bootloader.bin" },
    { name: "partitions", offset: 0x8000, file: "firmware/new-project/partitions.bin" },
    { name: "app", offset: 0x10000, file: "firmware/new-project/firmware.bin" },
  ],
}
```

### 5. 部署（可选）

运行 `web_flasher/sync_and_push.ps1` 推送到 GitHub Pages

---

## 使用示例

### 示例 1：发布 PlatformIO 项目

```bash
# 场景：刚完成 examples/51_mic_wifi 的开发，想添加到烧录平台

# 步骤 1：构建固件
cd examples/51_mic_wifi
pio run

# 步骤 2：发布到 Web Flasher
cd ../..
python .claude/skills/project-publish/scripts/publish.py \
  examples/51_mic_wifi \
  --id k10 \
  --name "CodeBuddy 发射端" \
  --badge TRANSMIT \
  --version v1.0.2 \
  --description "ESP-NOW + I2S 麦克风音频流，2 按键无线键盘，LVGL 多屏状态显示"

# 输出：
# ✅ 固件文件已复制到 web_flasher/firmware/k10/
# ✅ 已添加项目 k10 到 web_flasher/index.html
# ✅ 项目 k10 已发布到 Web Flasher
```

### 示例 2：发布 ESP-IDF 项目

```bash
# 场景：Dongle 固件更新到 v1.1.0

# 步骤 1：构建固件
cd dongle_firmware
idf.py build

# 步骤 2：发布并部署
cd ..
python .claude/skills/project-publish/scripts/publish.py \
  dongle_firmware \
  --id dongle \
  --name "CodeBuddy 接收端" \
  --badge RECEIVE \
  --version v1.1.0 \
  --deploy

# 输出：
# ✅ 固件文件已复制到 web_flasher/firmware/dongle/
# ✅ 已添加项目 dongle 到 web_flasher/index.html
# 🚀 正在部署到 GitHub Pages...
# ✅ 已推送到 GitHub Pages
```

### 示例 3：自动模式（最快）

```bash
# 场景：批量发布多个示例项目

for example in examples/30_lvgl_Gif examples/40_Snake_Game examples/42_demo_ui_lcd; do
  python .claude/skills/project-publish/scripts/publish.py $example --auto
done

# 每个项目自动：
# - ID: 从路径推断（30-lvgl-gif, 40-snake-game, 42-demo-ui-lcd）
# - 名称: 从路径推断（30 Lvgl Gif, 40 Snake Game, 42 Demo Ui Lcd）
# - 描述: 从 README.md 提取
# - 芯片配置: 从 platformio.ini/sdkconfig 提取
```

### 示例 4：带产品图片

```bash
# 场景：有产品实物图，想在卡片上展示

python .claude/skills/project-publish/scripts/publish.py \
  examples/40_Snake_Game \
  --id snake-game \
  --name "贪吃蛇游戏" \
  --badge GAME \
  --image images/snake_photo.jpg

# 额外操作：
# ✅ 产品图片已复制: web_flasher/images/snake-game.jpg
# ✅ 已添加 CSS 样式 .snake-game-bg
```

---

## 验证发布结果

### 本地测试

```bash
cd web_flasher
python serve.py

# 浏览器自动打开 http://localhost:8000
# 检查：
# 1. 首页是否显示新项目卡片
# 2. 点击卡片，进入烧录界面
# 3. 连接设备，验证固件文件是否正确
```

### 检查固件文件

```bash
cd web_flasher
python check_files.py

# 输出：
# ✅ k10: 所有文件存在
# ✅ dongle: 所有文件存在
# ✅ new-project: 所有文件存在
```

### 验证 index.html

```bash
cd web_flasher
grep "id: \"new-project\"" index.html

# 应输出类似：
#     id: "new-project",
```

---

## 常见问题

### Q1: 固件文件不存在

**错误**：
```
❌ 固件文件不存在: .pio/build/51_mic_wifi/firmware.bin
   提示: 请先构建项目（pio run 或 idf.py build）
```

**解决**：
```bash
# PlatformIO
cd examples/51_mic_wifi
pio run

# ESP-IDF
cd dongle_firmware
idf.py build
```

---

### Q2: 项目 ID 重复

**错误**：
```
⚠️  项目 k10 已存在，将替换现有配置
```

**说明**：这是正常的更新流程，旧配置会被新配置替换。

**如果不想覆盖**，使用不同的 ID：
```bash
--id k10-v2
```

---

### Q3: 部署失败

**错误**：
```
❌ 部署失败: fatal: not a git repository
```

**解决**：
```bash
# 确保在 git 仓库根目录
git status

# 检查 sync_and_push.ps1 是否存在
ls web_flasher/sync_and_push.ps1
```

---

### Q4: 无法识别构建系统

**错误**：
```
❌ 无法识别构建系统（需要 platformio.ini 或 CMakeLists.txt）
```

**解决**：
- PlatformIO 项目：确保父目录有 `platformio.ini`
- ESP-IDF 项目：确保项目目录有 `CMakeLists.txt`

---

## 高级用法

### 批量发布所有示例

```bash
# 创建批量发布脚本
cat > publish_all.sh <<'EOF'
#!/bin/bash
for example in examples/*/; do
  if [ -f "$example/*.ino" ] || [ -f "$example/main.cpp" ]; then
    echo "发布 $example"
    python .claude/skills/project-publish/scripts/publish.py "$example" --auto
  fi
done
EOF

chmod +x publish_all.sh
./publish_all.sh
```

### 自定义徽章颜色

编辑 `web_flasher/index.html` 的 CSS 部分：

```css
.card-badge {
  background: var(--accent);  /* 绿色 */
  color: #000;
}

/* 为特定徽章自定义颜色 */
.card-badge.beta {
  background: #f59e0b;  /* 橙色 */
}
```

然后在发布时：
```bash
--badge "BETA beta"  # 添加 CSS class
```

---

## 目录结构

```
.claude/skills/project-publish/
├── skill.md                    # 技能定义文档
├── README.md                   # 本文档
└── scripts/
    ├── publish.py              # 主执行脚本
    ├── add_project.py          # 添加项目到 index.html
    └── extract_metadata.py     # 提取项目元信息
```

---

## 技术细节

### 元信息提取逻辑

1. **芯片型号**：
   - PlatformIO: 从 `platformio.ini` 的 `board` 字段
   - ESP-IDF: 从 `sdkconfig` 的 `CONFIG_IDF_TARGET_*`

2. **Flash 配置**：
   - PlatformIO: 从 `platformio.ini` 的 `board_build.flash_*`
   - ESP-IDF: 从 `sdkconfig` 的 `CONFIG_ESPTOOLPY_*`

3. **描述文本**：
   - 优先从 `README.md` 第一段提取
   - 其次从 `.ino` 文件注释提取
   - 最后使用空字符串

### index.html 更新机制

使用正则表达式匹配 `const FIRMWARE_TARGETS = [...]` 结构，在数组末尾插入新项目配置。

**幂等性**：重复发布相同 ID 的项目会替换旧配置，不会产生重复条目。

---

## 未来增强

- [ ] 支持自定义分区表（非标准 3 分区）
- [ ] 支持多语言描述（中英文切换）
- [ ] 自动生成产品缩略图（从固件截屏）
- [ ] 版本管理（保留历史版本）
- [ ] CI/CD 集成（GitHub Actions 自动发布）

---

## 反馈与贡献

遇到问题或有改进建议？

1. 查看本文档的"常见问题"章节
2. 检查脚本输出的错误信息
3. 在项目仓库提交 Issue
