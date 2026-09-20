# project:publish - 多类型项目发布工具

---
name: project-publish
description: >
  多类型项目发布工具 / Multi-type project publishing tool.
  支持发布固件（K10/Dongle）、Web应用、Android APK、Windows可执行文件到项目展示平台。
  自动检测项目类型，处理特定类型的构建/打包/部署流程。
  触发词/Triggers: publish project, 发布项目, publish firmware, 发布固件, publish web, 发布网页, publish app, 发布应用。
  (Responds in the user's input language.)
---

## Language Matching

**Detect the user's input language and respond in the same language throughout all outputs.**

- Chinese input → Chinese prompts, configuration files, deployment logs
- English input → English prompts, configuration files, deployment logs
- Mixed input → Use the dominant language

Apply this to: user-facing prompts, generated JavaScript config, commit messages, deployment logs, and error messages.

---

## 技能定位

自动化多类型项目发布流程，将项目添加到 Project Showcase 展示平台（`web_flasher/`），并可选部署到 GitHub Pages。

**支持的项目类型**：
- **firmware** — ESP32 固件项目（PlatformIO 或 ESP-IDF），提供在线烧录
- **web** — Web 应用项目（package.json + dist/），提供在线预览
- **android** — Android APK（build.gradle + *.apk），提供下载链接
- **windows** — Windows 可执行文件（*.sln/*.exe），提供下载链接

**适用场景**：
- 刚完成一个新项目（固件/Web/Android/Windows），想添加到展示平台
- 更新已有项目的版本
- 批量发布多个项目

---

## 输入参数

用户通过 prompt 或交互式询问提供：

### 必选参数
- `$SOURCE_PROJECT`：源项目路径（如 `examples/51_mic_wifi` 或 `dongle_firmware`）
- `$PROJECT_ID`：项目唯一标识（小写英文+连字符，如 `k10`, `dongle`, `new-sensor`）
- `$PROJECT_NAME`：显示名称（中文，如 "CodeBuddy 发射端"）
- `$BADGE`：左上角徽章文字（如 `TRANSMIT`, `NEW`, `STABLE`）
- `$VERSION`：版本号（如 `v1.0.0`）

### 可选参数
- `$DESCRIPTION`：项目描述（默认从源项目 README 提取）
- `$ICON`：emoji 图标（默认 ""，留空则用背景图）
- `$IMAGE`：产品图片路径（如有实物图，放到 `web_flasher/images/<project_id>.jpg`）
- `$DEPLOY`：是否部署到 GitHub Pages（默认 `false`）

---

## 使用示例

支持四种项目类型的自动发布：

### 固件项目（PlatformIO/ESP-IDF）

```bash
# 自动检测类型并发布
publish --project-dir examples/51_mic_wifi --name "CodeBuddy K10" --version v1.0.3

# 或手动指定参数
python .claude/skills/project-publish/scripts/add_project.py \
  --project-dir examples/51_mic_wifi \
  --name "CodeBuddy K10" \
  --badge "TRANSMIT" \
  --version "v1.0.3"
```

### 网页项目（package.json + dist/）

```bash
# 自动检测类型并发布
publish --project-dir /path/to/web-app --name "Dashboard" --version v1.0.0

# 手动指定参数
python .claude/skills/project-publish/scripts/add_project.py \
  --project-dir /path/to/web-app \
  --name "数据监控仪表板" \
  --badge "DEMO" \
  --version "v1.0.0"
```

### Android 项目（build.gradle + *.apk）

```bash
# 自动检测类型并发布
publish --project-dir /path/to/android-app --name "Mobile App" --version v1.0.0

# 手动指定参数
python .claude/skills/project-publish/scripts/add_project.py \
  --project-dir /path/to/android-app \
  --name "移动端控制应用" \
  --badge "STABLE" \
  --version "v2.0.0"
```

### Windows 项目（*.exe/*.msi）

```bash
# 自动检测类型并发布
publish --project-dir /path/to/windows-app --name "Desktop App" --version v1.0.0

# 手动指定参数
python .claude/skills/project-publish/scripts/add_project.py \
  --project-dir /path/to/windows-app \
  --name "桌面端调试工具" \
  --badge "PRO" \
  --version "v1.3.0"
```

**项目类型自动检测**：脚本根据目录内容推断类型（`platformio.ini`/`sdkconfig` → firmware, `package.json` → web, `build.gradle` → android, `*.sln` → windows），无需手动指定。

---

## 执行流程

### Phase 0: 参数收集与验证

#### 0.1 识别源项目类型

使用 `detect_type.py` 自动检测：

```python
from detect_type import detect_project_type

project_type = detect_project_type(source_project_dir)
# Returns: 'firmware' | 'web' | 'android' | 'windows'
```

检测规则：
- 存在 `platformio.ini` 或 `sdkconfig` → **firmware**
- 存在 `package.json` → **web**
- 存在 `build.gradle` → **android**
- 存在 `*.sln` 或 `*.vcxproj` → **windows**

#### 0.2 定位资源文件

**固件项目（PlatformIO）**：
```
.pio/build/<env>/
├── bootloader.bin
├── partitions.bin
└── firmware.bin
```

**固件项目（ESP-IDF）**：
```
<project>/build/
├── bootloader/bootloader.bin
├── partition_table/partition-table.bin
└── <project_name>.bin
```

**Web 项目**：
```
<project>/dist/          # 或 build/ 目录
├── index.html
├── style.css
└── app.js
```

**Android 项目**：
```
<project>/app/build/outputs/apk/release/
└── app-release.apk
```

**Windows 项目**：
```
<project>/Release/       # 或 bin/Release/
└── app.exe
```

#### 0.3 交互式参数补全

使用 `AskUserQuestion` 询问缺失参数：

```javascript
const answers = await AskUserQuestion({
  questions: [
    {
      question: "项目唯一标识（英文小写+连字符，如 k10, new-sensor）？",
      header: "项目 ID",
      options: [
        { label: "自动生成", description: "从源项目路径自动推断" },
        { label: "手动输入", description: "自定义 ID" }
      ],
      multiSelect: false
    },
    {
      question: "项目显示名称（中文）？",
      header: "显示名称",
      options: [
        { label: "从 README 提取", description: "自动读取项目文档" },
        { label: "手动输入", description: "自定义名称" }
      ],
      multiSelect: false
    },
    {
      question: "徽章文字（卡片左上角）？",
      header: "徽章",
      options: [
        { label: "NEW", description: "新项目" },
        { label: "STABLE", description: "稳定版" },
        { label: "BETA", description: "测试版" },
        { label: "DEMO", description: "演示项目" },
        { label: "其他", description: "自定义" }
      ],
      multiSelect: false
    },
    {
      question: "是否部署到 GitHub Pages？",
      header: "部署",
      options: [
        { label: "仅本地", description: "只更新 web_flasher/ 本地文件" },
        { label: "部署", description: "运行 sync_and_push.ps1 推送到 GitHub" }
      ],
      multiSelect: false
    }
  ]
});
```

---

### Phase 1: 复制项目资源

根据项目类型复制到对应目录：

```bash
case "$PROJECT_TYPE" in
  firmware)
    TARGET_DIR="web_flasher/firmware/$PROJECT_ID"
    mkdir -p "$TARGET_DIR"
    # 复制 bootloader, partitions, firmware 等 bin 文件
    ;;
  web)
    TARGET_DIR="web_flasher/projects/$PROJECT_ID"
    mkdir -p "$TARGET_DIR"
    # 复制 dist/ 或 build/ 下的构建产物
    ;;
  android)
    TARGET_DIR="web_flasher/downloads/$PROJECT_ID"
    mkdir -p "$TARGET_DIR"
    # 复制 APK 文件
    ;;
  windows)
    TARGET_DIR="web_flasher/downloads/$PROJECT_ID"
    mkdir -p "$TARGET_DIR"
    # 复制 EXE/MSI 文件
    ;;
esac
echo "资源文件已复制到 $TARGET_DIR/"
```

---

### Phase 2: 提取项目元信息

使用 `scripts/extract_metadata.py` 自动提取：

```python
#!/usr/bin/env python3
"""从源项目提取元信息"""
import re, json, sys

def extract_metadata(source_path):
    meta = {
        "chip": "esp32s3",          # 默认值
        "flash_size": "16MB",
        "flash_mode": "dio",
        "flash_freq": "80m"
    }
    
    # 从 platformio.ini 提取
    ini_path = f"{source_path}/../platformio.ini"
    if os.path.exists(ini_path):
        with open(ini_path) as f:
            content = f.read()
            # 提取芯片型号
            if "esp32s3" in content:
                meta["chip"] = "esp32s3"
            elif "esp32s2" in content:
                meta["chip"] = "esp32s2"
            # 提取 flash 配置
            if m := re.search(r"flash_size\s*=\s*(\w+)", content):
                meta["flash_size"] = m.group(1)
    
    # 从 README 提取描述
    readme_path = f"{source_path}/README.md"
    if os.path.exists(readme_path):
        with open(readme_path, encoding="utf-8") as f:
            lines = f.readlines()
            # 提取第一段非标题文本作为描述
            for line in lines:
                if line.strip() and not line.startswith("#"):
                    meta["description"] = line.strip()[:100]
                    break
    
    return meta

if __name__ == "__main__":
    meta = extract_metadata(sys.argv[1])
    print(json.dumps(meta, ensure_ascii=False))
```

---

### Phase 3: 更新 index.html

使用 `scripts/add_project.py` 自动编辑 `PROJECTS` 数组：

```python
#!/usr/bin/env python3
"""向 index.html 添加新项目配置"""
import re, sys, json

def add_project(html_path, project_config):
    with open(html_path, "r", encoding="utf-8") as f:
        content = f.read()
    
    # 构造新项目的 JavaScript 对象
    new_entry = f"""  {{
    id: "{project_config['id']}",
    name: "{project_config['name']}",
    badge: "{project_config['badge']}",
    version: "{project_config['version']}",
    description: "{project_config['description']}",
    icon: "{project_config.get('icon', '')}",
    chip: "{project_config['chip']}",
    flashMode: "{project_config['flash_mode']}",
    flashFreq: "{project_config['flash_freq']}",
    flashSize: "{project_config['flash_size']}",
    partitions: [
      {{ name: "bootloader", offset: 0x0, file: "firmware/{project_config['id']}/bootloader.bin" }},
      {{ name: "partitions", offset: 0x8000, file: "firmware/{project_config['id']}/partitions.bin" }},
      {{ name: "app", offset: 0x10000, file: "firmware/{project_config['id']}/firmware.bin" }},
    ],
  }},"""
    
    # 在 PROJECTS 数组末尾插入（最后一个 ] 之前）
    pattern = r"(const PROJECTS = \[[\s\S]*?)\];"
    replacement = r"\1" + new_entry + "\n];"
    content = re.sub(pattern, replacement, content)
    
    with open(html_path, "w", encoding="utf-8") as f:
        f.write(content)
    
    print(f"✅ 已添加项目 {project_config['id']} 到 index.html")

if __name__ == "__main__":
    config = json.loads(sys.argv[1])
    add_project("web_flasher/index.html", config)
```

---

### Phase 4: 处理产品图片（可选）

```bash
if [ -f "$IMAGE_PATH" ]; then
  cp "$IMAGE_PATH" "web_flasher/images/$PROJECT_ID.jpg"
  echo "✅ 产品图片已复制"
  
  # 添加 CSS 背景样式
  cat >> web_flasher/index.html <<EOF
.card-image.$PROJECT_ID-bg {
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  background-image: url('images/$PROJECT_ID.jpg'), linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  background-size: cover, cover;
  background-position: center;
  background-repeat: no-repeat, no-repeat;
}
EOF
else
  echo "ℹ️  未提供产品图片，将使用 emoji 图标或渐变背景"
fi
```

---

### Phase 5: 验证完整性

```bash
python web_flasher/check_files.py
# 检查新添加项目的固件文件是否完整
```

---

### Phase 6: 部署到 GitHub Pages（可选）

```powershell
if ($DEPLOY -eq "true") {
  cd web_flasher
  .\sync_and_push.ps1
  echo "✅ 已推送到 GitHub Pages"
}
```

---

## 错误处理

| 场景 | 处理 |
|------|------|
| 项目类型无法检测 | 提示用户检查项目目录结构或手动指定类型 |
| 资源文件不存在 | **固件**：提示运行 `pio run` 或 `idf.py build`<br>**Web**：提示运行 `npm run build`<br>**Android**：提示运行 `./gradlew assembleRelease`<br>**Windows**：提示运行 `msbuild` |
| 项目 ID 重复 | 询问用户是更新现有项目还是使用新 ID |
| index.html 语法错误 | 备份原文件，提供回滚方案 |
| GitHub Pages 推送失败 | 提示检查 git 配置和网络连接 |

---

## 完成条件

1. ✅ 项目资源复制到对应目录（`firmware/` / `projects/` / `downloads/`）
2. ✅ `index.html` 的 `PROJECTS` 数组已更新，包含正确的 `type` 字段
3. ✅ 资源文件完整性验证通过
4. ✅ （可选）产品图片和 CSS 样式已添加
5. ✅ （可选）GitHub Pages 部署成功

---

## 输出

生成的配置示例（固件项目）：

```javascript
{
  id: "new-sensor",
  name: "智能温湿度传感器",
  type: "firmware",
  badge: "NEW",
  version: "v1.0.0",
  description: "ESP32-C3 + SHT40 高精度传感器，WiFi + 低功耗蓝牙，支持 MQTT",
  icon: "🌡️",
  chip: "esp32c3",
  flashMode: "dio",
  flashFreq: "80m",
  flashSize: "4MB",
  partitions: [
    { name: "bootloader", offset: 0x0, file: "firmware/new-sensor/bootloader.bin" },
    { name: "partitions", offset: 0x8000, file: "firmware/new-sensor/partitions.bin" },
    { name: "app", offset: 0x10000, file: "firmware/new-sensor/firmware.bin" },
  ],
}
```

生成的配置示例（Web 项目）：

```javascript
{
  id: "dashboard",
  name: "数据监控仪表板",
  type: "web",
  badge: "DEMO",
  version: "v1.0.0",
  description: "实时设备数据可视化",
  icon: "📊",
  url: "projects/dashboard/index.html",
}
```

最终输出：
```
已发布项目 new-sensor (firmware) 到 Project Showcase
资源路径: web_flasher/firmware/new-sensor/
本地预览: python web_flasher/serve.py
在线地址: https://<your-github>.github.io/<repo>/web_flasher/
```
