# Project Showcase Platform Design

**日期**: 2026-09-16  
**状态**: 设计中  
**类型**: 架构级重构

---

## 1. 概述

### 1.1 背景

现有的 `web_flasher/` 是一个专门用于 ESP32 固件烧录的 Web Serial 工具。随着项目类型的扩展，需要将其升级为通用的**项目展示平台**，支持展示和分发多种类型的项目：

- **固件项目**（现有）：ESP32 固件，浏览器内一键烧录
- **网页项目**（新增）：前端应用，在线预览
- **Android App**（新增）：APK 下载 + 详情页
- **Windows 桌面端**（新增）：EXE 下载 + 详情页

### 1.2 目标

1. **统一展示**：所有项目在同一个平台展示，统一的卡片网格布局
2. **分类筛选**：按项目类型快速筛选
3. **适配交互**：不同类型项目点击后触发不同行为（烧录 / 预览 / 下载）
4. **自动发布**：通过 `/project:publish` 命令自动添加新项目到平台
5. **单一部署**：所有项目资源托管在同一个 GitHub Pages 仓库

### 1.3 非目标

- 不提供项目源码管理（源码仍在各自的开发目录）
- 不提供在线代码编辑功能
- 不支持用户上传项目（仅开发者通过命令行发布）

---

## 2. 架构设计

### 2.1 平台定位

**名称**: `Project Showcase`（中文副标题："我的项目展示平台"）

**核心功能**:
- 多类型项目统一展示
- 固件项目：Web Serial 烧录
- 网页项目：在线预览
- App/桌面端项目：安装包下载 + 详情页

### 2.2 目录结构

```
web_flasher/                        # 技术目录名保持不变
├── index.html                      # 主页面（改造）
├── serve.py                        # 本地开发服务器
├── sync_and_push.ps1               # 部署脚本（更新）
│
├── firmware/                       # 固件项目文件（保留）
│   ├── k10/
│   │   ├── bootloader.bin
│   │   ├── partitions.bin
│   │   └── firmware.bin
│   ├── dongle/
│   └── ...
│
├── projects/                       # 【新增】网页项目托管
│   ├── todo-app/                   # 网页项目示例
│   │   ├── index.html
│   │   ├── assets/
│   │   └── ...
│   └── dashboard/
│
├── downloads/                      # 【新增】App/桌面端安装包
│   ├── my-android-app/
│   │   ├── app-release.apk
│   │   └── app-v1.0.0.apk          # 支持多版本
│   └── my-windows-tool/
│       ├── setup.exe
│       └── setup-v1.0.0.exe
│
├── images/                         # 产品图片（扩展）
│   ├── K10.jpg                     # 现有固件产品图
│   ├── dongle.jpg
│   ├── todo-app/                   # 【新增】项目截图目录
│   │   ├── screenshot-1.png
│   │   ├── screenshot-2.png
│   │   └── screenshot-3.png
│   └── my-android-app/
│       ├── screen-1.png
│       └── screen-2.png
│
├── vendor/                         # 第三方库（保留）
│   └── esptool-bundle.js
│
└── deploy_pages/                   # GitHub Pages 部署副本
    └── （镜像上述所有文件）
```

### 2.3 数据结构

#### 2.3.1 统一项目配置

现有的 `FIRMWARE_TARGETS` 数组扩展为 `PROJECTS` 数组，统一所有项目类型：

```javascript
const PROJECTS = [
  // 固件项目
  {
    id: "k10",                      // 唯一标识
    type: "firmware",               // 项目类型
    name: "CodeBuddy 发射端",
    badge: "TRANSMIT",              // 徽章文本
    version: "v1.0.2",
    description: "ESP-NOW + I2S 麦克风音频流，2 按键无线键盘，LVGL 多屏状态显示",
    icon: "",                       // 空表示使用实物图
    bgClass: "",                    // 自定义背景 CSS 类
    techStack: ["ESP32-S3", "ESP-NOW", "LVGL"],  // 技术栈标签
    
    // firmware 专属字段
    chip: "esp32s3",
    flashMode: "dio",
    flashFreq: "80m",
    flashSize: "16MB",
    partitions: [
      { name: "bootloader", offset: 0x0, file: "firmware/k10/bootloader.bin" },
      { name: "partitions", offset: 0x8000, file: "firmware/k10/partitions.bin" },
      { name: "app", offset: 0x10000, file: "firmware/k10/firmware.bin" },
    ],
  },

  // 网页项目
  {
    id: "todo-app",
    type: "web",
    name: "Todo 待办应用",
    badge: "WEB",
    version: "v1.0.0",
    description: "基于 React + Tailwind 的待办事项管理应用，支持任务分类、优先级设置",
    icon: "📝",
    techStack: ["React", "Tailwind CSS", "Vite"],
    
    // web 专属字段
    entryUrl: "projects/todo-app/index.html",  // 相对路径
    screenshots: ["images/todo-app/screen1.png", "images/todo-app/screen2.png"],
    repoUrl: "https://github.com/user/todo-app",  // 可选：源码链接
  },

  // Android 项目
  {
    id: "my-android-app",
    type: "android",
    name: "My Android App",
    badge: "ANDROID",
    version: "v2.0.0",
    description: "基于 Kotlin + Jetpack Compose 的 Android 应用",
    icon: "📱",
    techStack: ["Kotlin", "Jetpack Compose", "Material 3"],
    
    // android 专属字段
    downloadUrl: "downloads/my-android-app/app-release.apk",
    fileSize: "24.5 MB",
    minSdk: 24,                     // Android 7.0+
    screenshots: ["images/my-android-app/s1.png", "images/my-android-app/s2.png"],
    features: [
      "Material Design 3 设计语言",
      "支持深色模式",
      "离线数据同步",
    ],
    installGuide: "下载 APK 文件后，在 Android 设备上安装。首次安装需开启「允许安装未知来源应用」。",
    changelog: [
      { version: "v2.0.0", date: "2026-09-15", changes: ["全新 Material 3 UI", "性能优化"] },
      { version: "v1.5.0", date: "2026-08-10", changes: ["新增深色模式"] },
      { version: "v1.0.0", date: "2026-08-01", changes: ["首次发布"] },
    ],
    repoUrl: "https://github.com/user/my-android-app",
  },

  // Windows 项目
  {
    id: "my-windows-tool",
    type: "windows",
    name: "My Windows Tool",
    badge: "WINDOWS",
    version: "v1.0.0",
    description: "基于 Electron 的桌面工具，提供高效的工作流支持",
    icon: "🖥️",
    techStack: ["Electron", "TypeScript", "React"],
    
    // windows 专属字段
    downloadUrl: "downloads/my-windows-tool/setup.exe",
    fileSize: "68 MB",
    minOs: "Windows 10 1809+",
    screenshots: ["images/my-windows-tool/main.png"],
    features: [
      "轻量级系统占用",
      "全局快捷键支持",
      "自动更新检测",
    ],
    installGuide: "双击 setup.exe 安装，安装过程中可能需要管理员权限。",
    changelog: [
      { version: "v1.0.0", date: "2026-09-01", changes: ["首次发布"] },
    ],
    repoUrl: "https://github.com/user/my-windows-tool",
  },
];
```

#### 2.3.2 字段说明

**通用字段**（所有类型必需）:
- `id`: 唯一标识符（kebab-case）
- `type`: 项目类型（`firmware` | `web` | `android` | `windows`）
- `name`: 项目显示名称
- `badge`: 徽章文本（显示在卡片左上角）
- `version`: 版本号（语义化版本）
- `description`: 项目描述（1-2 句话）
- `icon`: 图标（Emoji 或空字符串表示使用实物图）
- `techStack`: 技术栈标签数组

**固件专属字段**:
- `chip`: 芯片型号
- `flashMode`: Flash 模式
- `flashFreq`: Flash 频率
- `flashSize`: Flash 大小
- `partitions`: 分区配置数组

**网页专属字段**:
- `entryUrl`: 入口页面 URL（相对路径）
- `screenshots`: 截图数组（可选）
- `repoUrl`: 源码仓库链接（可选）

**Android/Windows 专属字段**:
- `downloadUrl`: 安装包下载 URL
- `fileSize`: 文件大小
- `minSdk` / `minOs`: 最低系统要求
- `screenshots`: 截图数组
- `features`: 功能特性列表
- `installGuide`: 安装说明
- `changelog`: 更新日志（版本历史）
- `repoUrl`: 源码仓库链接（可选）

---

## 3. 用户界面设计

### 3.1 首页布局

```
┌──────────────────────────────────────────────────────┐
│  nav: Project Showcase              [返回首页]        │
├──────────────────────────────────────────────────────┤
│                                                        │
│              Project Showcase                          │
│        选择项目 · 预览 · 下载 · 一键烧录               │
│                                                        │
│   [全部] [固件] [网页] [Android] [Windows]            │
│                                                        │
│   ┌────────┐  ┌────────┐  ┌────────┐                 │
│   │ [实物] │  │ [实物] │  │   📝   │                 │
│   │ K10    │  │ Dongle │  │ Todo   │                 │
│   │ v1.0.2 │  │ v1.1.0 │  │ v1.0.0 │                 │
│   │ TRANS  │  │ RECEIVE│  │  WEB   │                 │
│   │ ESP... │  │ USB... │  │ React..│                 │
│   └────────┘  └────────┘  └────────┘                 │
│                                                        │
│   ┌────────┐  ┌────────┐                             │
│   │   📱   │  │   🖥️   │                             │
│   │Android │  │Windows │                             │
│   │ v2.0.0 │  │ v1.0.0 │                             │
│   │ANDROID │  │WINDOWS │                             │
│   │Kotlin..│  │Electron│                             │
│   └────────┘  └────────┘                             │
│                                                        │
└──────────────────────────────────────────────────────┘
```

### 3.2 分类标签

首页 Hero 区域下方的分类标签：

```html
<div class="category-tabs">
  <button class="tab active" data-filter="all">全部</button>
  <button class="tab" data-filter="firmware">固件</button>
  <button class="tab" data-filter="web">网页</button>
  <button class="tab" data-filter="android">Android</button>
  <button class="tab" data-filter="windows">Windows</button>
</div>
```

样式：
- 默认状态：透明背景，灰色文字
- 激活状态：对应类型的主题色背景，白色文字
- Hover：半透明主题色背景

### 3.3 项目卡片

卡片设计统一，但根据类型添加不同的视觉标识：

**徽章颜色**:
- `firmware`: 绿色 `#22c55e`（保持现有）
- `web`: 蓝色 `#3b82f6`
- `android`: 橙色 `#f97316`
- `windows`: 紫色 `#a855f7`

**卡片布局**:
```
┌─────────────────────────────┐
│ [BADGE]              [icon] │  ← 徽章 + 图标/实物图
│                             │
│  项目名称                   │  ← 粗体，18px
│  v1.0.0                     │  ← 版本号，12px，灰色
│                             │
│  项目描述文字...             │  ← 14px，2 行省略
│                             │
│  [React] [Vite] [Tailwind] │  ← 技术栈标签
└─────────────────────────────┘
```

### 3.4 交互行为

| 项目类型 | 点击卡片后 | 实现方式 |
|----------|------------|----------|
| `firmware` | 进入烧录界面 | 切换到 `flash-view`（现有逻辑） |
| `web` | 新标签页打开预览 | `window.open(entryUrl, '_blank')` |
| `android` | 进入项目详情页 | 切换到 `detail-view`，显示 Android 项目信息 |
| `windows` | 进入项目详情页 | 切换到 `detail-view`，显示 Windows 项目信息 |

### 3.5 项目详情页（Android/Windows）

详情页布局：

```
┌──────────────────────────────────────────────────────┐
│  nav: Project Showcase              [返回首页]        │
├──────────────────────────────────────────────────────┤
│                                                        │
│  [icon] 项目名称 v2.0.0               [ANDROID]      │
│                                                        │
│  项目描述文字...                                       │
│                                                        │
│  [React] [Kotlin] [Material 3]                       │
│                                                        │
│  ┌──────────────────────────────────────────────┐    │
│  │ [截图轮播]                                   │    │
│  │  < [Screenshot 1]  [Screenshot 2] >          │    │
│  └──────────────────────────────────────────────┘    │
│                                                        │
│  功能特性                                              │
│  ✓ Material Design 3 设计语言                         │
│  ✓ 支持深色模式                                       │
│  ✓ 离线数据同步                                       │
│                                                        │
│  安装说明                                              │
│  下载 APK 文件后，在 Android 设备上安装...             │
│                                                        │
│  更新日志                                              │
│  v2.0.0 (2026-09-15)                                 │
│    • 全新 Material 3 UI                              │
│    • 性能优化                                         │
│  v1.5.0 (2026-08-10)                                 │
│    • 新增深色模式                                     │
│                                                        │
│  ┌─────────────────────────────────────────────┐    │
│  │  下载 (24.5 MB)  |  源码仓库  |  二维码      │    │
│  └─────────────────────────────────────────────┘    │
│                                                        │
└──────────────────────────────────────────────────────┘
```

---

## 4. 自动发布工具

### 4.1 命令重命名

```bash
# 旧命令
/flasher:publish <path> [--options]

# 新命令
/project:publish <path> [--options]
```

### 4.2 类型自动检测

检测顺序（优先级从高到低）：

1. **检测文件扩展名**:
   - `.apk` → `type=android`
   - `.exe` / `.msi` → `type=windows`

2. **检测项目结构**:
   - 存在 `platformio.ini` → `type=firmware`（PlatformIO 项目）
   - 存在 `CMakeLists.txt` + `main/` 目录 → `type=firmware`（ESP-IDF 项目）
   - 存在 `index.html` + (`package.json` 或纯静态文件) → `type=web`

3. **交互式询问**:
   - 检测失败时，通过 `AskUserQuestion` 询问用户项目类型

### 4.3 不同类型的发布流程

#### 4.3.1 固件项目（保持现有逻辑）

```bash
/project:publish examples/51_mic_wifi --type firmware
```

流程：
1. 检测构建系统（PlatformIO / ESP-IDF）
2. 定位固件文件（bootloader / partitions / firmware.bin）
3. 提取元信息（芯片型号、Flash 配置）
4. 复制到 `firmware/<project-id>/`
5. 更新 `PROJECTS` 数组

#### 4.3.2 网页项目

```bash
/project:publish web_projects/todo-app --type web
```

流程：
1. 检测构建产物目录（`dist/` / `build/` / `out/`）
2. 验证 `index.html` 存在
3. 从 `package.json` 提取元信息（名称、版本、依赖）
4. 复制整个构建产物到 `projects/<project-id>/`
5. （可选）生成项目截图（需手动提供或自动截图工具）
6. 更新 `PROJECTS` 数组

#### 4.3.3 Android 项目

```bash
/project:publish builds/MyApp.apk --type android
```

流程：
1. 验证 APK 文件存在
2. 使用 `aapt` / `apkanalyzer` 提取元信息（包名、版本、minSdk）
3. 计算文件大小
4. 复制 APK 到 `downloads/<project-id>/`
5. 要求用户提供：
   - 项目名称、描述
   - 功能特性列表
   - 安装说明
   - 截图文件路径（复制到 `images/<project-id>/`）
6. 更新 `PROJECTS` 数组

#### 4.3.4 Windows 项目

```bash
/project:publish builds/MyTool-setup.exe --type windows
```

流程：
1. 验证 EXE/MSI 文件存在
2. 提取文件元信息（版本、大小）
3. 复制到 `downloads/<project-id>/`
4. 要求用户提供：
   - 项目名称、描述
   - 功能特性列表
   - 安装说明
   - 截图文件路径
5. 更新 `PROJECTS` 数组

### 4.4 命令参数

```bash
/project:publish <path> [options]

options:
  --type <firmware|web|android|windows>   # 显式指定类型，默认 auto
  --id <project-id>                       # 项目唯一标识
  --name <project-name>                   # 项目显示名称
  --version <version>                     # 版本号
  --badge <badge-text>                    # 徽章文本
  --description <text>                    # 项目描述
  --tech-stack <tag1,tag2>                # 技术栈标签（逗号分隔）
  --screenshots <path1,path2>             # 截图文件路径（逗号分隔）
  --icon <emoji>                          # 图标（Emoji 字符）
  --deploy                                # 发布后自动部署到 GitHub Pages
  --auto                                  # 自动推断所有参数（最小交互）
```

### 4.5 Skill 文件结构

```
.claude/skills/project-publish/
├── skill.md                    # Skill 定义（更新）
├── README.md                   # 使用手册（更新）
├── QUICKSTART.md               # 快速开始（更新）
└── scripts/
    ├── publish.py              # 主执行脚本（重构）
    ├── add_project.py          # HTML 编辑器（重构）
    ├── extract_metadata.py     # 元信息提取（扩展）
    ├── detect_type.py          # 【新增】类型检测模块
    └── utils.py                # 【新增】通用工具函数
```

---

## 5. 技术实现

### 5.1 前端改造

#### 5.1.1 视图管理

新增 `detail-view`，总共三个视图：

```javascript
const views = {
  home: document.getElementById('home-view'),
  flash: document.getElementById('flash-view'),
  detail: document.getElementById('detail-view'),  // 新增
};

function showView(viewName) {
  for (let key in views) {
    views[key].style.display = (key === viewName) ? 'block' : 'none';
  }
}
```

#### 5.1.2 分类筛选

```javascript
let currentFilter = 'all';

function filterProjects(type) {
  currentFilter = type;
  renderProjectCards();
  
  // 更新标签激活状态
  document.querySelectorAll('.tab').forEach(tab => {
    tab.classList.toggle('active', tab.dataset.filter === type);
  });
}

function renderProjectCards() {
  const filteredProjects = currentFilter === 'all' 
    ? PROJECTS 
    : PROJECTS.filter(p => p.type === currentFilter);
  
  // 渲染卡片...
}
```

#### 5.1.3 项目详情页

```javascript
function showProjectDetail(projectId) {
  const project = PROJECTS.find(p => p.id === projectId);
  if (!project) return;
  
  // 根据类型渲染不同的详情页
  if (project.type === 'android' || project.type === 'windows') {
    renderDetailView(project);
    showView('detail');
  } else if (project.type === 'web') {
    window.open(project.entryUrl, '_blank');
  } else if (project.type === 'firmware') {
    // 现有固件烧录逻辑
    selectTarget(project);
    showView('flash');
  }
}

function renderDetailView(project) {
  const detailView = document.getElementById('detail-view');
  detailView.innerHTML = `
    <div class="detail-header">
      <div class="detail-icon">${project.icon}</div>
      <div class="detail-title">
        <h2>${project.name} <span class="version">${project.version}</span></h2>
        <span class="badge badge-${project.type}">${project.badge}</span>
      </div>
    </div>
    
    <p class="detail-description">${project.description}</p>
    
    <div class="tech-stack">
      ${project.techStack.map(tech => `<span class="tag">${tech}</span>`).join('')}
    </div>
    
    <div class="screenshots">
      ${project.screenshots.map(img => `<img src="${img}" alt="Screenshot">`).join('')}
    </div>
    
    <section>
      <h3>功能特性</h3>
      <ul>
        ${project.features.map(f => `<li>${f}</li>`).join('')}
      </ul>
    </section>
    
    <section>
      <h3>安装说明</h3>
      <p>${project.installGuide}</p>
    </section>
    
    <section>
      <h3>更新日志</h3>
      ${project.changelog.map(log => `
        <div class="changelog-item">
          <h4>${log.version} <span class="date">${log.date}</span></h4>
          <ul>
            ${log.changes.map(c => `<li>${c}</li>`).join('')}
          </ul>
        </div>
      `).join('')}
    </section>
    
    <div class="detail-actions">
      <button class="btn-primary" onclick="window.open('${project.downloadUrl}')">
        下载 (${project.fileSize})
      </button>
      ${project.repoUrl ? `
        <button class="btn-secondary" onclick="window.open('${project.repoUrl}')">
          源码仓库
        </button>
      ` : ''}
    </div>
  `;
}
```

### 5.2 部署脚本更新

`sync_and_push.ps1` 需要同步新增的目录：

```powershell
# 原有同步逻辑保持不变，新增：

# 同步网页项目
if (Test-Path "projects") {
    Copy-Item "projects" -Destination "deploy_pages/projects" -Recurse -Force
}

# 同步下载文件
if (Test-Path "downloads") {
    Copy-Item "downloads" -Destination "deploy_pages/downloads" -Recurse -Force
}

# 同步截图目录
if (Test-Path "images") {
    Copy-Item "images" -Destination "deploy_pages/images" -Recurse -Force
}
```

---

## 6. 数据流

### 6.1 发布流程

```
开发者本地项目
    ↓
执行 /project:publish <path> [--options]
    ↓
类型检测（auto / 手动指定）
    ↓
提取元信息（自动 + 交互式补全）
    ↓
复制文件到对应目录
  - firmware → web_flasher/firmware/<id>/
  - web → web_flasher/projects/<id>/
  - android/windows → web_flasher/downloads/<id>/
    ↓
更新 index.html 的 PROJECTS 数组
    ↓
（可选）执行 sync_and_push.ps1 部署
    ↓
GitHub Pages 自动更新（1-2 分钟）
    ↓
✅ 项目在 Project Showcase 平台上线
```

### 6.2 用户浏览流程

```
用户访问 https://.../
    ↓
首页显示所有项目卡片
    ↓
点击分类标签筛选
    ↓
点击项目卡片
    ↓
根据项目类型跳转：
  - firmware → flash-view（烧录界面）
  - web → 新标签页打开预览
  - android/windows → detail-view（详情页）
    ↓
下载 / 预览 / 烧录
```

---

## 7. 实施计划

### Phase 1: 前端重构（核心）

**任务**:
1. 重命名：页面标题、Hero 区域文案改为 "Project Showcase"
2. 数据结构：`FIRMWARE_TARGETS` → `PROJECTS`，扩展字段
3. 新增 UI：分类标签、项目详情页
4. 交互逻辑：卡片点击分类处理、筛选逻辑

**文件变更**:
- `web_flasher/index.html`（主要改造）

**验证**:
- 现有固件项目保持可用（烧录功能不受影响）
- 分类标签筛选正常
- 详情页布局正确

### Phase 2: 发布工具重构

**任务**:
1. Skill 重命名：`flasher-publish` → `project-publish`
2. 类型检测：实现 `detect_type.py` 模块
3. 元信息提取：扩展 `extract_metadata.py` 支持 web/android/windows
4. HTML 编辑器：`add_project.py` 适配新数据结构
5. 主脚本：`publish.py` 重构，支持多类型项目流程

**文件变更**:
- `.claude/skills/project-publish/` 所有脚本

**验证**:
- 固件项目发布流程保持不变
- 新类型项目发布成功（手动测试）

### Phase 3: 文档更新

**任务**:
1. 更新 `README.md`（使用手册）
2. 更新 `QUICKSTART.md`（快速开始）
3. 更新 `skill.md`（Skill 定义）
4. 新增示例项目（每种类型至少 1 个）

**验证**:
- 文档覆盖所有项目类型
- 命令示例可直接复制执行

### Phase 4: 部署与测试

**任务**:
1. 本地测试：发布不同类型项目
2. 更新 `sync_and_push.ps1`
3. 部署到 GitHub Pages
4. 功能验证：浏览、筛选、下载、烧录

**验证**:
- 所有项目类型正常展示
- 交互逻辑正确
- 下载链接有效

---

## 8. 风险与限制

### 8.1 风险

1. **GitHub Pages 仓库大小限制**
   - 软限制 1GB，硬限制 5GB
   - 大型 App/桌面端安装包可能快速消耗空间
   - **缓解**: 定期清理旧版本，或考虑移至 GitHub Releases

2. **下载速度**
   - 国内访问 GitHub Pages 可能较慢
   - **缓解**: 提供镜像下载链接（如 Gitee Pages）

3. **兼容性**
   - 现有固件烧录逻辑依赖 Web Serial API（Chrome/Edge）
   - **影响**: 其他浏览器用户无法烧录固件（但可以下载其他类型项目）

### 8.2 限制

1. **不支持用户上传**
   - 仅开发者通过命令行发布
   - 无账号系统、无权限管理

2. **无在线编辑**
   - 项目信息需重新发布才能更新
   - 无后台管理界面

3. **有限的分析数据**
   - GitHub Pages 无内置访问统计
   - 需第三方工具（如 Google Analytics）

---

## 9. 后续扩展

### 9.1 潜在功能

- **搜索功能**：支持项目名称、描述、技术栈搜索
- **排序选项**：按时间、名称、类型排序
- **标签聚合**：按技术栈标签聚合项目（如"所有 React 项目"）
- **下载统计**：通过 GitHub API 或第三方服务统计下载次数
- **二维码生成**：自动生成 APK 下载二维码
- **在线预览增强**：网页项目支持 iframe 内嵌预览
- **自动截图**：使用 Puppeteer 自动生成项目截图
- **版本管理**：支持同一项目的多版本并存

### 9.2 其他项目类型

未来可扩展支持：
- **macOS 项目**：DMG/PKG 安装包
- **Linux 项目**：AppImage/DEB/RPM
- **iOS 项目**：TestFlight 链接（需 Apple 开发者账号）
- **Chrome 扩展**：CRX 文件或 Chrome Web Store 链接
- **VS Code 扩展**：VSIX 文件或 Marketplace 链接

---

## 10. 总结

本设计将现有的固件烧录平台升级为通用的 **Project Showcase**，支持固件、网页、Android、Windows 四种项目类型的统一展示和分发。

**核心改进**:
- 统一的数据结构和 UI 展示
- 分类筛选和适配交互
- 自动化发布工具支持多类型项目
- 单一 GitHub Pages 仓库托管所有资源

**下一步**：进入实施阶段，按 Phase 1-4 顺序完成改造。
