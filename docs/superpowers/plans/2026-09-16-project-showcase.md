# Project Showcase Platform Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Transform the existing firmware-only Web Flasher into a unified Project Showcase platform that supports firmware, web, Android, and Windows project types with category filtering, type-specific interactions, and automated publishing.

**Architecture:** Single-page application (SPA) with three views (home, flash, detail). Unified `PROJECTS` array replaces `FIRMWARE_TARGETS` with type-specific fields. Publishing tool (`project-publish` skill) auto-detects project type and handles type-specific metadata extraction and file copying.

**Tech Stack:** Vanilla JavaScript, Web Serial API (existing), GitHub Pages hosting, Python publishing scripts, PowerShell deployment scripts

**Spec:** `docs/superpowers/specs/2026-09-16-project-showcase-platform-design.md`

## Global Constraints

- Firmware烧录功能保持向后兼容（现有 K10/Dongle 项目不受影响）
- Web Serial API 仅 Chrome/Edge 支持（现有限制）
- 所有项目资源托管在同一 GitHub Pages 仓库
- 分块写入限制：单次文件写入不超过 100 行（本计划遵守此约束）
- 徽章颜色：firmware=#22c55e, web=#3b82f6, android=#f97316, windows=#a855f7
- 目录结构：`firmware/`, `projects/`, `downloads/`, `images/`
- 统一数据结构：所有项目类型使用 `PROJECTS` 数组，包含通用字段 + 类型专属字段

---

### Task 1: Frontend - 数据结构重构

**Files:**
- Modify: `web_flasher/index.html:432-610` (FIRMWARE_TARGETS 数组定义)
- Modify: `web_flasher/index.html:612` (全局 window 赋值)

**Interfaces:**
- Consumes: 现有 `FIRMWARE_TARGETS` 数组结构（10 个固件项目）
- Produces: 
  ```javascript
  const PROJECTS = [
    {
      id: string,           // 唯一标识
      type: "firmware" | "web" | "android" | "windows",
      name: string,
      badge: string,
      version: string,
      description: string,
      icon: string,         // Emoji 或空
      techStack: string[],
      // firmware 专属
      chip?: string,
      flashMode?: string,
      flashFreq?: string,
      flashSize?: string,
      partitions?: Array<{name, offset, file}>,
      // web 专属
      entryUrl?: string,
      screenshots?: string[],
      repoUrl?: string,
      // android/windows 专属
      downloadUrl?: string,
      fileSize?: string,
      minSdk?: number,      // android
      minOs?: string,       // windows
      features?: string[],
      installGuide?: string,
      changelog?: Array<{version, date, changes[]}>,
    }
  ]
  ```

- [ ] **步骤 1: 备份现有 FIRMWARE_TARGETS 定义**

Run: 复制 `index.html` 行 432-610 内容到临时文件
Expected: 备份成功，用于参考

- [ ] **步骤 2: 重命名数组并扩展字段**

找到 `const FIRMWARE_TARGETS = [` (行 432)，替换为：
```javascript
const PROJECTS = [
```

对于每个现有固件项目（k10, dongle 等），添加：
```javascript
type: "firmware",
techStack: ["ESP32-S3", "ESP-NOW", "LVGL"],  // 根据实际项目填写
```

示例（k10 项目）：
```javascript
{
  id: "k10",
  type: "firmware",  // 新增
  name: "CodeBuddy 发射端",
  badge: "TRANSMIT",
  version: "v1.0.2",
  description: "ESP-NOW + I2S 麦克风音频流，2 按键无线键盘，LVGL 多屏状态显示",
  icon: "",
  bgClass: "k10-bg",
  techStack: ["ESP32-S3", "ESP-NOW", "LVGL", "I2S"],  // 新增
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
```

- [ ] **步骤 3: 更新全局引用**

找到 `window.FIRMWARE_TARGETS = FIRMWARE_TARGETS;` (行 612)，替换为：
```javascript
window.PROJECTS = PROJECTS;
```

Run: 全局搜索替换 `FIRMWARE_TARGETS` → `PROJECTS`
Expected: 所有引用更新

- [ ] **步骤 4: 验证数据结构**

```bash
cd web_flasher
python serve.py
```
浏览器打开，Console 检查：`window.PROJECTS` 数组存在且包含 `type` 字段
Expected: 无 JS 错误，现有固件卡片仍可点击进入烧录

- [ ] **步骤 5: Commit**

```bash
git add web_flasher/index.html
git commit -m "refactor(web-flasher): rename FIRMWARE_TARGETS to PROJECTS

- Rename global array to support multi-type projects
- Add type='firmware' and techStack fields to all entries
- Update window global reference
- Prepare for web/android/windows project types

BREAKING CHANGE: FIRMWARE_TARGETS renamed to PROJECTS"
```

---

### Task 2: Frontend - 分类标签 UI

**Files:**
- Modify: `web_flasher/index.html` (CSS 部分新增 `.category-tabs` 样式)
- Modify: `web_flasher/index.html` (HTML hero 区域后新增标签容器)
- Modify: `web_flasher/index.html` (JS 新增筛选逻辑)

**Interfaces:**
- Consumes: `PROJECTS` 数组（Task 1 产物，每个项目含 `type` 字段）
- Produces: 
  - HTML 元素 `<div class="category-tabs">` 及 5 个按钮
  - JS 函数 `filterProjects(type)`, `renderProjectCards()`
  - 全局变量 `let currentFilter = 'all'`

- [ ] **步骤 1: 添加 CSS 样式**

找到 `<style>` 标签（行 8-180 区域），在 `.project-card` 样式后新增：
```css
.category-tabs {
  display: flex;
  gap: 12px;
  justify-content: center;
  margin-bottom: 40px;
}

.tab {
  padding: 10px 24px;
  border: 2px solid var(--border);
  border-radius: 8px;
  background: transparent;
  color: var(--text);
  cursor: pointer;
  font-size: 16px;
  font-weight: 500;
  transition: all 0.2s;
}

.tab:hover {
  background: rgba(34, 197, 94, 0.1);
}

.tab.active {
  background: var(--accent);
  color: #000;
  border-color: var(--accent);
}

.tab.active[data-filter="web"] {
  background: #3b82f6;
  border-color: #3b82f6;
}

.tab.active[data-filter="android"] {
  background: #f97316;
  border-color: #f97316;
}

.tab.active[data-filter="windows"] {
  background: #a855f7;
  border-color: #a855f7;
}
```

Run: 浏览器检查：标签按钮正确显示
Expected: 5 个标签水平排列，默认"全部"激活

- [ ] **步骤 2: 添加 HTML 结构**

找到 Hero 区域结束标签 `</div>` 后（约行 300），插入：
```html
<div class="category-tabs">
  <button class="tab active" data-filter="all" onclick="filterProjects('all')">全部</button>
  <button class="tab" data-filter="firmware" onclick="filterProjects('firmware')">固件</button>
  <button class="tab" data-filter="web" onclick="filterProjects('web')">网页</button>
  <button class="tab" data-filter="android" onclick="filterProjects('android')">Android</button>
  <button class="tab" data-filter="windows" onclick="filterProjects('windows')">Windows</button>
</div>
```

Run: 浏览器刷新
Expected: Hero 区域下方显示标签栏

- [ ] **步骤 3: 实现筛选逻辑**

在 `<script>` 标签开头（行 432 前）添加：
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
  const container = document.querySelector('.project-grid');
  
  const filteredProjects = currentFilter === 'all' 
    ? PROJECTS 
    : PROJECTS.filter(p => p.type === currentFilter);
  
  // 清空容器
  container.innerHTML = '';
  
  // 重新渲染卡片（复用现有 createCard 逻辑）
  filteredProjects.forEach(project => {
    const card = createProjectCard(project);
    container.appendChild(card);
  });
}
```

Run: `python serve.py`，点击不同标签
Expected: 卡片列表动态过滤，标签切换激活状态

- [ ] **步骤 4: 提取卡片创建逻辑**

将现有的卡片创建代码（行 682-704）封装为函数：
```javascript
function createProjectCard(project) {
  const card = document.createElement('div');
  card.className = 'project-card';
  if (project.bgClass) card.classList.add(project.bgClass);
  
  card.innerHTML = `
    <div class="card-image ${project.icon ? '' : project.bgClass}">
      ${project.icon ? `<div class="card-icon">${project.icon}</div>` : ''}
    </div>
    <div class="card-content">
      <div class="card-badge">${project.badge}</div>
      <h3>${project.name}</h3>
      <p class="version">${project.version}</p>
      <p class="description">${project.description}</p>
    </div>
  `;
  
  card.onclick = () => showProjectDetail(project.id);
  return card;
}
```

Run: 验证现有卡片仍可点击
Expected: 无 JS 错误，卡片交互正常

- [ ] **步骤 5: Commit**

```bash
git add web_flasher/index.html
git commit -m "feat(web-flasher): add category filter tabs

- Add 5-tab filter UI (全部/固件/网页/Android/Windows)
- Implement filterProjects() and renderProjectCards()
- Extract createProjectCard() for reuse
- Dynamic card rendering based on project type
- Active tab styling with type-specific colors"
```

---

### Task 3: Frontend - 详情页视图

**Files:**
- Modify: `web_flasher/index.html` (HTML 新增 `detail-view` 容器)
- Modify: `web_flasher/index.html` (CSS 新增 `.detail-*` 样式)
- Modify: `web_flasher/index.html` (JS 新增 `renderDetailView()` 函数)

**Interfaces:**
- Consumes: `PROJECTS` 数组（Task 1），`showView(viewName)` 函数（Task 4）
- Produces:
  - HTML 元素 `<div id="detail-view">`
  - JS 函数 `renderDetailView(project)` — 渲染 android/windows 项目详情
  - CSS 类 `.detail-header`, `.detail-description`, `.detail-actions` 等

- [ ] **步骤 1: 添加 detail-view HTML 容器**

在 `flash-view` 之后（约行 420），插入：
```html
<div id="detail-view" style="display: none;">
  <!-- 内容由 JS 动态生成 -->
</div>
```

Run: 浏览器刷新，无报错
Expected: 新容器不可见，不影响现有布局

- [ ] **步骤 2: 添加详情页 CSS 样式**

在 `<style>` 标签中，`.category-tabs` 样式后新增：
```css
.detail-header {
  display: flex;
  align-items: center;
  gap: 20px;
  margin-bottom: 24px;
}

.detail-icon {
  font-size: 48px;
}

.detail-title h2 {
  margin: 0 0 8px 0;
}

.detail-title .version {
  color: var(--text-secondary);
  font-size: 14px;
}

.badge-android { background: #f97316; color: #fff; }
.badge-windows { background: #a855f7; color: #fff; }
.badge-firmware { background: #22c55e; color: #000; }
.badge-web { background: #3b82f6; color: #fff; }

.detail-description {
  font-size: 16px;
  line-height: 1.6;
  margin-bottom: 24px;
}

.tech-stack {
  display: flex;
  gap: 8px;
  flex-wrap: wrap;
  margin-bottom: 32px;
}

.tag {
  padding: 4px 12px;
  background: rgba(255,255,255,0.1);
  border-radius: 4px;
  font-size: 13px;
}

.screenshots {
  display: flex;
  gap: 12px;
  overflow-x: auto;
  margin-bottom: 32px;
}

.screenshots img {
  max-height: 300px;
  border-radius: 8px;
  border: 1px solid var(--border);
}

.detail-actions {
  display: flex;
  gap: 12px;
  margin-top: 32px;
}

.btn-primary {
  padding: 12px 32px;
  background: var(--accent);
  color: #000;
  border: none;
  border-radius: 8px;
  font-size: 16px;
  font-weight: 600;
  cursor: pointer;
}

.btn-secondary {
  padding: 12px 32px;
  background: transparent;
  color: var(--text);
  border: 2px solid var(--border);
  border-radius: 8px;
  font-size: 16px;
  cursor: pointer;
}

.changelog-item h4 {
  margin: 16px 0 8px;
}

.changelog-item .date {
  color: var(--text-secondary);
  font-size: 13px;
}
```

Run: 浏览器刷新
Expected: 无样式冲突

- [ ] **步骤 3: 实现 renderDetailView 函数**

在 `<script>` 标签中，`renderProjectCards()` 后新增：
```javascript
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
    
    ${project.screenshots && project.screenshots.length > 0 ? `
      <section>
        <h3>应用截图</h3>
        <div class="screenshots">
          ${project.screenshots.map(img => `<img src="${img}" alt="Screenshot">`).join('')}
        </div>
      </section>
    ` : ''}
    
    ${project.features ? `
      <section>
        <h3>功能特性</h3>
        <ul>
          ${project.features.map(f => `<li>${f}</li>`).join('')}
        </ul>
      </section>
    ` : ''}
    
    ${project.installGuide ? `
      <section>
        <h3>安装说明</h3>
        <p>${project.installGuide}</p>
      </section>
    ` : ''}
    
    ${project.changelog && project.changelog.length > 0 ? `
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
    ` : ''}
    
    <div class="detail-actions">
      <button class="btn-primary" onclick="window.open('${project.downloadUrl}')">
        下载 (${project.fileSize})
      </button>
      ${project.repoUrl ? `
        <button class="btn-secondary" onclick="window.open('${project.repoUrl}')">
          源码仓库
        </button>
      ` : ''}
      <button class="btn-secondary" onclick="showView('home'); currentFilter='all'; filterProjects('all')">
        返回首页
      </button>
    </div>
  `;
}
```

Run: 浏览器 Console 输入 `renderDetailView(PROJECTS[0])`
Expected: detail-view 内容更新（但仍不可见）

- [ ] **步骤 4: Commit**

```bash
git add web_flasher/index.html
git commit -m "feat(web-flasher): add project detail view

- Add detail-view HTML container
- Implement renderDetailView() with screenshots/features/changelog sections
- Add CSS for detail page layout and badges
- Support download/repo links with file size display"
```

---

### Task 4: Frontend - 卡片点击路由

**Files:**
- Modify: `web_flasher/index.html` (JS 新增 `showProjectDetail()` 函数)
- Modify: `web_flasher/index.html` (JS 更新 `createProjectCard()` 的 `onclick` 事件)

**Interfaces:**
- Consumes: `PROJECTS` 数组（Task 1），`renderDetailView()` 函数（Task 3），`showView()` 函数
- Produces: JS 函数 `showProjectDetail(projectId)` — 根据项目类型分发路由

- [ ] **步骤 1: 实现 showProjectDetail 函数**

在 `<script>` 标签中，`renderDetailView()` 后新增：
```javascript
function showProjectDetail(projectId) {
  const project = PROJECTS.find(p => p.id === projectId);
  if (!project) {
    console.error(`项目 ${projectId} 不存在`);
    return;
  }
  
  if (project.type === 'firmware') {
    // 现有固件烧录逻辑
    selectTarget(project);
    showView('flash');
  } else if (project.type === 'web') {
    // 新标签页打开网页项目
    window.open(project.entryUrl, '_blank');
  } else if (project.type === 'android' || project.type === 'windows') {
    // 进入详情页
    renderDetailView(project);
    showView('detail');
  }
}
```

Run: 浏览器 Console 输入 `showProjectDetail('k10')`
Expected: 进入烧录界面（现有逻辑）

- [ ] **步骤 2: 更新卡片点击事件**

找到 `createProjectCard()` 函数（Task 2 步骤 4 创建），将：
```javascript
card.onclick = () => showProjectDetail(project.id);
```

替换为：
```javascript
card.onclick = () => showProjectDetail(project.id);
```

注意：此步骤已在 Task 2 完成，无需修改（验证即可）

Run: 点击固件卡片 → 烧录界面，点击网页卡片 → 新标签页
Expected: 路由正确分发

- [ ] **步骤 3: 添加返回首页导航**

在现有 `<nav>` 标签中（约行 200），找到"返回首页"按钮，更新 `onclick`：
```html
<button onclick="showView('home'); currentFilter='all'; filterProjects('all')">
  返回首页
</button>
```

Run: 详情页点击"返回首页"
Expected: 回到首页，标签重置为"全部"

- [ ] **步骤 4: Commit**

```bash
git add web_flasher/index.html
git commit -m "feat(web-flasher): implement project detail routing

- Add showProjectDetail() to route by project type
- firmware → flash-view (existing)
- web → open in new tab
- android/windows → detail-view
- Update nav bar return button to reset filter state"
```

---

### Task 5: Frontend - 卡片技术栈标签显示

**Files:**
- Modify: `web_flasher/index.html` (JS 更新 `createProjectCard()` 渲染逻辑)
- Modify: `web_flasher/index.html` (CSS 新增 `.card-tech-stack` 和 `.card-type-badge` 样式)

**Interfaces:**
- Consumes: `PROJECTS` 数组（Task 1，每个项目含 `type` 和 `techStack` 字段）
- Produces: 卡片渲染逻辑支持技术栈标签和类型徽章显示

- [ ] **步骤 1: 添加卡片技术栈样式**

在 `<style>` 标签中，`.card-content` 样式后新增：
```css
.card-tech-stack {
  display: flex;
  gap: 6px;
  flex-wrap: wrap;
  margin-top: 12px;
}

.card-tech-stack .tag {
  padding: 2px 8px;
  background: rgba(255,255,255,0.08);
  border-radius: 3px;
  font-size: 11px;
  color: rgba(255,255,255,0.7);
}

.card-type-badge {
  display: inline-block;
  padding: 4px 10px;
  border-radius: 4px;
  font-size: 12px;
  font-weight: 600;
  margin-bottom: 8px;
}

.card-type-badge.firmware { background: #22c55e; color: #000; }
.card-type-badge.web { background: #3b82f6; color: #fff; }
.card-type-badge.android { background: #f97316; color: #fff; }
.card-type-badge.windows { background: #a855f7; color: #fff; }
```

Run: 浏览器刷新
Expected: CSS 加载无报错

- [ ] **步骤 2: 更新 createProjectCard 函数**

找到 `createProjectCard()` 函数（Task 2 步骤 4 创建），将 `card.innerHTML` 替换为：
```javascript
card.innerHTML = `
  <div class="card-image ${project.icon ? '' : project.bgClass}">
    ${project.icon ? `<div class="card-icon">${project.icon}</div>` : ''}
  </div>
  <div class="card-content">
    <span class="card-type-badge ${project.type}">${project.type.toUpperCase()}</span>
    <div class="card-badge">${project.badge}</div>
    <h3>${project.name}</h3>
    <p class="version">${project.version}</p>
    <p class="description">${project.description}</p>
    ${project.techStack && project.techStack.length > 0 ? `
      <div class="card-tech-stack">
        ${project.techStack.map(tech => `<span class="tag">${tech}</span>`).join('')}
      </div>
    ` : ''}
  </div>
`;
```

Run: 浏览器刷新，查看卡片
Expected: 每张卡片顶部显示类型徽章（firmware/web/android/windows），底部显示技术栈标签

- [ ] **步骤 3: 验证卡片显示**

Run: `python serve.py`
浏览器打开，检查：
- K10 卡片显示 "FIRMWARE" 绿色徽章 + "ESP32-S3", "ESP-NOW", "LVGL", "I2S" 标签
- 点击不同分类标签，卡片类型徽章颜色正确

Expected: 卡片渲染完整，类型徽章和技术栈标签正确显示

- [ ] **步骤 4: Commit**

```bash
git add web_flasher/index.html
git commit -m "feat(web-flasher): add tech stack tags and type badges to cards

- Add card-tech-stack and card-type-badge CSS styles
- Update createProjectCard() to render type badge and tech stack tags
- Badge colors match project type (firmware/web/android/windows)
- Tech stack tags display at card bottom"
```

---

### Task 6: Backend - Skill 重命名

**Files:**
- Rename: `.claude/skills/flasher-publish/` → `.claude/skills/project-publish/`
- Modify: `.claude/skills/project-publish/skill.md` (名称和描述)

**Interfaces:**
- Consumes: 现有 flasher-publish skill 目录结构
- Produces: 重命名后的 project-publish skill，保留所有脚本和文档

- [ ] **步骤 1: 重命名 skill 目录**

Run:
```bash
cd .claude/skills
git mv flasher-publish project-publish
```

Expected: 目录重命名，git 历史保留

- [ ] **步骤 2: 更新 skill.md 元数据**

编辑 `.claude/skills/project-publish/skill.md`，将：
```markdown
---
name: flasher-publish
description: >
  Firmware flasher publishing tool / 固件烧录器发布工具...
---
```

替换为：
```markdown
---
name: project-publish
description: >
  Project showcase publishing tool / 项目展示平台发布工具. 
  支持四种项目类型：固件（firmware）、网页（web）、Android 应用、Windows 应用。
  自动检测项目类型，提取元数据，复制文件到指定目录，更新 index.html。
  Supports four project types: firmware, web, Android, Windows.
  Auto-detects type, extracts metadata, copies files, updates index.html.
  触发词/Triggers: publish, 发布, upload project, 上传项目, add project, 添加项目。
  (Responds in the user's input language.)
---
```

Run: `git diff .claude/skills/project-publish/skill.md`
Expected: 元数据更新正确

- [ ] **步骤 3: Commit**

```bash
git add .claude/skills/
git commit -m "refactor(skill): rename flasher-publish to project-publish

- Rename skill directory from flasher-publish to project-publish
- Update skill.md name and description for multi-type support
- Prepare for firmware/web/android/windows project publishing"
```

---

### Task 7: Backend - 类型检测模块

**Files:**
- Create: `.claude/skills/project-publish/scripts/detect_type.py` (类型检测逻辑)

**Interfaces:**
- Consumes: 项目目录路径（字符串）
- Produces: 
  ```python
  def detect_project_type(project_dir: str) -> str:
      """Returns: 'firmware' | 'web' | 'android' | 'windows'"""
  ```

- [ ] **步骤 1: 创建类型检测模块**

创建 `.claude/skills/project-publish/scripts/detect_type.py`：
```python
#!/usr/bin/env python3
"""Project type detector for project-publish skill."""

import os
from pathlib import Path

def detect_project_type(project_dir: str) -> str:
    """
    Detect project type based on directory structure and files.
    
    Returns: 'firmware' | 'web' | 'android' | 'windows'
    Raises: ValueError if type cannot be determined
    """
    path = Path(project_dir)
    if not path.exists():
        raise ValueError(f"Project directory {project_dir} does not exist")
    
    # 检测固件项目
    if (path / 'platformio.ini').exists() or (path / 'sdkconfig').exists():
        return 'firmware'
    
    # 检测网页项目
    if (path / 'package.json').exists():
        return 'web'
    
    # 检测 Android 项目
    if (path / 'build.gradle').exists() or (path / 'app' / 'build.gradle').exists():
        return 'android'
    
    # 检测 Windows 项目
    if (path.glob('*.sln') or path.glob('*.vcxproj')):
        return 'windows'
    
    # 尝试通过文件扩展名推断
    files = list(path.rglob('*'))
    extensions = {f.suffix for f in files if f.is_file()}
    
    if '.apk' in extensions:
        return 'android'
    if '.exe' in extensions or '.msi' in extensions:
        return 'windows'
    if '.html' in extensions or '.js' in extensions:
        return 'web'
    if '.bin' in extensions or '.hex' in extensions:
        return 'firmware'
    
    raise ValueError(f"Cannot determine project type for {project_dir}")
```

- [ ] **步骤 2: 编写类型检测测试**

创建 `.claude/skills/project-publish/tests/test_detect_type.py`：
```python
#!/usr/bin/env python3
import pytest
import tempfile
from pathlib import Path
from scripts.detect_type import detect_project_type

def test_detect_firmware_platformio(tmp_path):
    (tmp_path / 'platformio.ini').touch()
    assert detect_project_type(str(tmp_path)) == 'firmware'

def test_detect_firmware_espidf(tmp_path):
    (tmp_path / 'sdkconfig').touch()
    assert detect_project_type(str(tmp_path)) == 'firmware'

def test_detect_web(tmp_path):
    (tmp_path / 'package.json').touch()
    assert detect_project_type(str(tmp_path)) == 'web'

def test_detect_android(tmp_path):
    (tmp_path / 'build.gradle').touch()
    assert detect_project_type(str(tmp_path)) == 'android'

def test_detect_windows(tmp_path):
    (tmp_path / 'project.sln').touch()
    assert detect_project_type(str(tmp_path)) == 'windows'

def test_detect_by_extension(tmp_path):
    (tmp_path / 'app.apk').touch()
    assert detect_project_type(str(tmp_path)) == 'android'

def test_unknown_project(tmp_path):
    with pytest.raises(ValueError, match="Cannot determine project type"):
        detect_project_type(str(tmp_path))
```

- [ ] **步骤 3: 运行测试**

Run:
```bash
cd .claude/skills/project-publish
pytest tests/test_detect_type.py -v
```

Expected: 所有测试通过

- [ ] **步骤 4: Commit**

```bash
git add .claude/skills/project-publish/scripts/detect_type.py
git add .claude/skills/project-publish/tests/test_detect_type.py
git commit -m "feat(project-publish): add project type detection module

- Create detect_type.py with detect_project_type() function
- Support firmware (platformio.ini/sdkconfig), web (package.json), android (build.gradle), windows (.sln)
- Fallback detection via file extensions (.apk, .exe, .html, .bin)
- Add unit tests for all project types"
```

---

### Task 8: Backend - publish.py 多类型重构

**Files:**
- Modify: `.claude/skills/project-publish/scripts/publish.py`
- Modify: `.claude/skills/project-publish/scripts/detect_type.py` (add import)

**Interfaces:**
- Consumes: 
  - `detect_type.detect_project_type(project_dir: str) -> str`
  - CLI arguments: `--project-dir`, `--name`, `--version`, `--force`
- Produces: 
  - 项目包：`firmware/<id>-<version>.bin`, `projects/<id>-<version>.zip`, `downloads/<id>.apk`, `downloads/<id>.exe`
  - 索引更新：`firmware/index.json`, `projects/index.json`

- [ ] **步骤 1: 重构 publish.py 结构**

Modify `.claude/skills/project-publish/scripts/publish.py`，添加类型分发逻辑：

```python
#!/usr/bin/env python3
"""Multi-type project publishing tool."""

import sys
from pathlib import Path
from detect_type import detect_project_type

def publish_firmware(project_dir, name, version):
    """Publish firmware project (existing logic)."""
    # 现有固件发布逻辑保持不变
    pass

def publish_web(project_dir, name, version):
    """Publish web project."""
    # 1. 构建 (npm run build / vite build)
    # 2. 压缩 dist/ 为 projects/<id>-<version>.zip
    # 3. 更新 projects/index.json
    pass

def publish_android(project_dir, name, version):
    """Publish Android project."""
    # 1. 查找 *.apk 文件
    # 2. 复制到 downloads/<id>.apk
    # 3. 更新 downloads/android.json（包含 fileSize, features, changelog）
    pass

def publish_windows(project_dir, name, version):
    """Publish Windows project."""
    # 1. 查找 *.exe 或 *.msi
    # 2. 复制到 downloads/<id>.exe
    # 3. 更新 downloads/windows.json
    pass

def main():
    # Parse args...
    project_type = detect_project_type(project_dir)
    
    handlers = {
        'firmware': publish_firmware,
        'web': publish_web,
        'android': publish_android,
        'windows': publish_windows,
    }
    
    handler = handlers.get(project_type)
    if not handler:
        raise ValueError(f"Unknown project type: {project_type}")
    
    handler(project_dir, name, version)
```

- [ ] **步骤 2: 实现 publish_web**

```python
def publish_web(project_dir: str, name: str, version: str):
    """
    Build and publish web project.
    1. Run npm run build / vite build
    2. Compress dist/ to projects/<id>-<version>.zip
    3. Update projects/index.json
    """
    from subprocess import run
    import shutil
    
    path = Path(project_dir)
    
    # 检测构建工具
    if (path / 'package.json').exists():
        run(['npm', 'run', 'build'], cwd=path, check=True)
    else:
        raise ValueError("No package.json found for web project")
    
    # 查找 dist 目录
    dist_dir = path / 'dist'
    if not dist_dir.exists():
        raise ValueError("No dist/ directory after build")
    
    # 压缩
    project_id = name.lower().replace(' ', '-')
    zip_path = f"projects/{project_id}-{version}.zip"
    shutil.make_archive(zip_path.replace('.zip', ''), 'zip', dist_dir)
    
    # 更新索引
    update_index('projects/index.json', {
        'id': project_id,
        'name': name,
        'version': version,
        'zipUrl': zip_path,
        'timestamp': datetime.now().isoformat()
    })
```

- [ ] **步骤 3: 实现 publish_android**

```python
def publish_android(project_dir: str, name: str, version: str):
    """
    Publish Android APK.
    1. Find *.apk
    2. Copy to downloads/<id>.apk
    3. Update downloads/android.json
    """
    import shutil
    
    path = Path(project_dir)
    apk_files = list(path.rglob('*.apk'))
    
    if not apk_files:
        raise ValueError("No .apk file found in project")
    
    apk_file = apk_files[0]  # 取第一个
    project_id = name.lower().replace(' ', '-')
    dest = Path('downloads') / f"{project_id}.apk"
    dest.parent.mkdir(exist_ok=True)
    shutil.copy2(apk_file, dest)
    
    file_size_mb = dest.stat().st_size / (1024 * 1024)
    
    update_index('downloads/android.json', {
        'id': project_id,
        'name': name,
        'version': version,
        'downloadUrl': f"downloads/{project_id}.apk",
        'fileSize': f"{file_size_mb:.1f} MB",
        'timestamp': datetime.now().isoformat()
    })
```

- [ ] **步骤 4: 实现 publish_windows**

```python
def publish_windows(project_dir: str, name: str, version: str):
    """
    Publish Windows executable.
    1. Find *.exe or *.msi
    2. Copy to downloads/<id>.exe
    3. Update downloads/windows.json
    """
    import shutil
    
    path = Path(project_dir)
    exe_files = list(path.rglob('*.exe')) + list(path.rglob('*.msi'))
    
    if not exe_files:
        raise ValueError("No .exe or .msi file found")
    
    exe_file = exe_files[0]
    project_id = name.lower().replace(' ', '-')
    dest = Path('downloads') / f"{project_id}{exe_file.suffix}"
    dest.parent.mkdir(exist_ok=True)
    shutil.copy2(exe_file, dest)
    
    file_size_mb = dest.stat().st_size / (1024 * 1024)
    
    update_index('downloads/windows.json', {
        'id': project_id,
        'name': name,
        'version': version,
        'downloadUrl': f"downloads/{project_id}{exe_file.suffix}",
        'fileSize': f"{file_size_mb:.1f} MB",
        'timestamp': datetime.now().isoformat()
    })
```

预期输出示例（index.html 中新增的 PROJECTS 条目）：
```javascript
{
  id: "codebuddy-web",
  type: "web",
  name: "CodeBuddy Dashboard",
  badge: "WEB",
  version: "v1.0.0",
  description: "AI 编程助手数据看板",
  icon: "📊",
  techStack: ["React", "Vite", "Tailwind"],
  entryUrl: "projects/codebuddy-web-v1.0.0/index.html",
  screenshots: ["images/codebuddy-web-1.png"],
  repoUrl: "https://github.com/...",
  downloads: [
    { version: "v1.0.0", zipUrl: "projects/codebuddy-web-v1.0.0.zip", date: "2026-09-16" }
  ]
}
```

- [ ] **步骤 5: 测试多类型发布**

Run（测试固件发布，现有逻辑）:
```bash
cd .claude/skills/project-publish
python scripts/publish.py --project-dir ../../examples/51_mic_wifi --name "CodeBuddy K10" --version v1.0.3
```
Expected: 检测为 firmware，调用 publish_firmware，生成 firmware/k10-v1.0.3.bin

Run（测试网页发布，模拟项目）:
```bash
mkdir -p /tmp/mock-web-project
echo '{"name":"test","scripts":{"build":"echo building"}}' > /tmp/mock-web-project/package.json
mkdir -p /tmp/mock-web-project/dist
echo '<html>test</html>' > /tmp/mock-web-project/dist/index.html
python scripts/publish.py --project-dir /tmp/mock-web-project --name "Test Web" --version v1.0.0
```
Expected: 检测为 web，调用 publish_web，生成 projects/test-web-v1.0.0.zip

Run（测试 Android 发布，模拟项目）:
```bash
mkdir -p /tmp/mock-android-project
echo 'apply plugin: "com.android.application"' > /tmp/mock-android-project/build.gradle
mkdir -p /tmp/mock-android-project/app/build/outputs/apk
echo 'mock apk' > /tmp/mock-android-project/app/build/outputs/apk/app.apk
python scripts/publish.py --project-dir /tmp/mock-android-project --name "Test Android" --version v1.0.0
```
Expected: 检测为 android，调用 publish_android，生成 downloads/test-android.apk

Run（测试 Windows 发布，模拟项目）:
```bash
mkdir -p /tmp/mock-windows-project
echo 'mock exe' > /tmp/mock-windows-project/app.exe
python scripts/publish.py --project-dir /tmp/mock-windows-project --name "Test Windows" --version v1.0.0
```
Expected: 检测为 windows，调用 publish_windows，生成 downloads/test-windows.exe

- [ ] **步骤 6: Commit**

```bash
git add .claude/skills/project-publish/scripts/publish.py
git commit -m "feat(project-publish): multi-type publish handlers

- Refactor publish.py with handler dispatch pattern
- Add publish_web: npm build, zip dist, update projects/index.json
- Add publish_android: find apk, copy to downloads, update android.json
- Add publish_windows: find exe/msi, copy to downloads, update windows.json
- Type-specific metadata extraction (fileSize, screenshots, features)
- All handlers tested with mock projects"
```

---

### Task 9: Backend - add_project.py 多类型适配

**Files:**
- Modify: `.claude/skills/project-publish/scripts/add_project.py`

**Interfaces:**
- Consumes: 
  - `detect_type.detect_project_type(project_dir: str) -> str`
  - CLI arguments: `--project-dir`, `--name`, `--badge`, `--version`, `--description`, `--icon`
- Produces: 更新后的 `index.html`（PROJECTS 数组新增条目）

- [ ] **步骤 1: 更新正则匹配逻辑**

Modify `.claude/skills/project-publish/scripts/add_project.py`，找到 `FIRMWARE_TARGETS` 正则（约行 20），替换为：
```python
# 正则匹配 const PROJECTS = [...]; 结构
PROJECTS_PATTERN = re.compile(
    r'(const PROJECTS = \[)(.*?)(\];)',
    re.DOTALL
)
```

找到插入新项目的逻辑（约行 45-60），更新为：
```python
def add_project_to_html(html_path, project_entry):
    """将新项目条目插入 index.html 的 PROJECTS 数组。"""
    with open(html_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    match = PROJECTS_PATTERN.search(content)
    if not match:
        raise ValueError("Cannot find const PROJECTS = [...]; in index.html")
    
    prefix, projects_str, suffix = match.groups()
    
    # 插入新条目（在数组末尾，最后一个逗号后）
    new_content = f"{prefix}{projects_str.rstrip()},\n{project_entry}\n{suffix}"
    
    with open(html_path, 'w', encoding='utf-8') as f:
        f.write(new_content)
```

- [ ] **步骤 2: 添加类型检测集成**

在文件开头添加导入：
```python
from detect_type import detect_project_type
```

在 `main()` 函数中（约行 80），添加类型检测：
```python
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--project-dir', required=True)
    parser.add_argument('--name', required=True)
    parser.add_argument('--badge', required=True)
    parser.add_argument('--version', required=True)
    parser.add_argument('--description', required=True)
    parser.add_argument('--icon', default='')
    args = parser.parse_args()
    
    # 自动检测项目类型
    project_type = detect_project_type(args.project_dir)
    print(f"Detected project type: {project_type}")
    
    # 构造项目条目（根据类型选择字段）
    if project_type == 'firmware':
        entry = generate_firmware_entry(args)
    elif project_type == 'web':
        entry = generate_web_entry(args)
    elif project_type == 'android':
        entry = generate_android_entry(args)
    elif project_type == 'windows':
        entry = generate_windows_entry(args)
    
    add_project_to_html('web_flasher/index.html', entry)
```

- [ ] **步骤 3: 实现类型专属条目生成**

添加四个生成函数：
```python
def generate_firmware_entry(args):
    """生成固件项目条目（现有逻辑）。"""
    return f"""  {{
    id: "{args.name.lower().replace(' ', '-')}",
    type: "firmware",
    name: "{args.name}",
    badge: "{args.badge}",
    version: "{args.version}",
    description: "{args.description}",
    icon: "{args.icon}",
    techStack: ["ESP32-S3"],
    chip: "esp32s3",
    flashMode: "dio",
    flashFreq: "80m",
    flashSize: "16MB",
    partitions: [
      {{ name: "bootloader", offset: 0x0, file: "firmware/{args.name.lower()}/bootloader.bin" }},
      {{ name: "partitions", offset: 0x8000, file: "firmware/{args.name.lower()}/partitions.bin" }},
      {{ name: "app", offset: 0x10000, file: "firmware/{args.name.lower()}/firmware.bin" }},
    ],
  }}"""

def generate_web_entry(args):
    """生成网页项目条目。"""
    return f"""  {{
    id: "{args.name.lower().replace(' ', '-')}",
    type: "web",
    name: "{args.name}",
    badge: "{args.badge}",
    version: "{args.version}",
    description: "{args.description}",
    icon: "{args.icon}",
    techStack: ["React", "Vite"],
    entryUrl: "projects/{args.name.lower().replace(' ', '-')}-{args.version}/index.html",
    screenshots: [],
    repoUrl: "",
  }}"""

def generate_android_entry(args):
    """生成 Android 项目条目。"""
    return f"""  {{
    id: "{args.name.lower().replace(' ', '-')}",
    type: "android",
    name: "{args.name}",
    badge: "{args.badge}",
    version: "{args.version}",
    description: "{args.description}",
    icon: "{args.icon}",
    techStack: ["Android"],
    downloadUrl: "downloads/{args.name.lower().replace(' ', '-')}.apk",
    fileSize: "TBD",
    minSdk: 24,
    features: [],
    installGuide: "下载后直接安装",
    changelog: [],
  }}"""

def generate_windows_entry(args):
    """生成 Windows 项目条目。"""
    return f"""  {{
    id: "{args.name.lower().replace(' ', '-')}",
    type: "windows",
    name: "{args.name}",
    badge: "{args.badge}",
    version: "{args.version}",
    description: "{args.description}",
    icon: "{args.icon}",
    techStack: ["Windows"],
    downloadUrl: "downloads/{args.name.lower().replace(' ', '-')}.exe",
    fileSize: "TBD",
    minOs: "Windows 10",
    features: [],
    installGuide: "下载后双击安装",
    changelog: [],
  }}"""
```

- [ ] **步骤 4: 测试 add_project.py**

Run（测试添加固件项目）:
```bash
python scripts/add_project.py \
  --project-dir ../../examples/51_mic_wifi \
  --name "CodeBuddy K10" \
  --badge "TRANSMIT" \
  --version "v1.0.2" \
  --description "Test firmware project"
```
Expected: 输出 "Detected project type: firmware"，index.html 的 PROJECTS 数组末尾新增固件条目

Run（测试添加网页项目）:
```bash
python scripts/add_project.py \
  --project-dir /tmp/mock-web-project \
  --name "Test Dashboard" \
  --badge "WEB" \
  --version "v1.0.0" \
  --description "Test web project"
```
Expected: 输出 "Detected project type: web"，PROJECTS 数组新增 web 条目，含 entryUrl 字段

- [ ] **步骤 5: Commit**

```bash
git add .claude/skills/project-publish/scripts/add_project.py
git commit -m "feat(project-publish): adapt add_project.py for multi-type

- Update FIRMWARE_TARGETS regex to match PROJECTS array
- Add detect_project_type integration for auto type detection
- Generate type-specific entries (firmware/web/android/windows)
- Each type includes correct fields per PROJECTS data structure"
```

---

### Task 10: 文档更新

**Files:**
- Modify: `web_flasher/README.md`
- Modify: `.claude/skills/project-publish/skill.md`

**Interfaces:**
- Consumes: 所有前置 Task 产物（PROJECTS 数据结构、分类筛选、详情页、多类型发布）
- Produces: 更新后的用户文档和 skill 文档

- [ ] **步骤 1: 更新 web_flasher/README.md**

在现有 README 基础上新增"多项目类型支持"章节：
```markdown
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
```

Run: 查看 README 格式正确
Expected: Markdown 渲染正常

- [ ] **步骤 2: 更新 EXTEND.md**

在 `web_flasher/EXTEND.md` 中，将固件专属的扩展指南更新为多类型通用指南。

主要修改：
- 标题从"添加新固件"改为"添加新项目"
- 增加四种项目类型的添加示例
- 更新目录结构说明（新增 projects/, downloads/, images/）
- 说明类型自动检测机制

Run: `git diff web_flasher/EXTEND.md`
Expected: 内容更新为多类型指南

- [ ] **步骤 3: 更新 skill.md**

编辑 `.claude/skills/project-publish/skill.md`，更新使用说明和示例：

```markdown
## Usage

支持四种项目类型的自动发布：

**固件项目**（PlatformIO/ESP-IDF）:
```bash
publish --project-dir examples/51_mic_wifi --name "CodeBuddy K10" --version v1.0.3
```

**网页项目**（package.json + dist/）:
```bash
publish --project-dir /path/to/web-app --name "Dashboard" --version v1.0.0
```

**Android 项目**（build.gradle + *.apk）:
```bash
publish --project-dir /path/to/android-app --name "Mobile App" --version v1.0.0
```

**Windows 项目**（*.exe/*.msi）:
```bash
publish --project-dir /path/to/windows-app --name "Desktop App" --version v1.0.0
```

项目类型自动检测，无需手动指定。
```

Run: 验证 skill.md 语法正确
Expected: 前端 skill 列表显示更新后的描述

- [ ] **步骤 4: Commit**

```bash
git add web_flasher/README.md web_flasher/EXTEND.md .claude/skills/project-publish/skill.md
git commit -m "docs: update for multi-type project showcase

- Add project type support table in README
- Update EXTEND.md for firmware/web/android/windows projects
- Add type-specific examples in skill.md
- Document auto-detection mechanism"
```

---

### Task 11: 部署脚本更新

**Files:**
- Modify: `web_flasher/sync_and_push.ps1`

**Interfaces:**
- Consumes: 新增的 projects/, downloads/ 目录
- Produces: 更新后的部署脚本，支持多类型项目同步

- [ ] **步骤 1: 更新 sync_and_push.ps1**

找到脚本中的文件复制逻辑（约行 30-50），添加新目录同步：

```powershell
# 同步固件文件（现有）
Copy-Item -Path "firmware/*" -Destination "gh-pages/firmware/" -Recurse -Force

# 同步网页项目（新增）
if (Test-Path "projects") {
    Copy-Item -Path "projects/*" -Destination "gh-pages/projects/" -Recurse -Force
}

# 同步下载文件（新增）
if (Test-Path "downloads") {
    Copy-Item -Path "downloads/*" -Destination "gh-pages/downloads/" -Recurse -Force
}

# 同步图片资源（新增）
if (Test-Path "images") {
    Copy-Item -Path "images/*" -Destination "gh-pages/images/" -Recurse -Force
}

# 同步主页
Copy-Item -Path "index.html" -Destination "gh-pages/index.html" -Force
Copy-Item -Path "esptool.js" -Destination "gh-pages/esptool.js" -Force
```

Run: `.\sync_and_push.ps1 --dry-run`（如脚本支持）
Expected: 显示将同步的文件列表，包含 projects/, downloads/, images/

- [ ] **步骤 2: 添加部署前检查**

在脚本开头添加目录结构验证：
```powershell
# 检查必需目录
$requiredDirs = @("firmware", "projects", "downloads", "images")
foreach ($dir in $requiredDirs) {
    if (-Not (Test-Path $dir)) {
        Write-Warning "Directory $dir does not exist, creating..."
        New-Item -ItemType Directory -Path $dir -Force
    }
}
```

Run: 执行脚本
Expected: 自动创建缺失目录，无报错

- [ ] **步骤 3: 测试完整部署流程**

Run:
```bash
# 1. 发布一个测试项目
python .claude/skills/project-publish/scripts/publish.py \
  --project-dir /tmp/mock-web-project \
  --name "Test Web" \
  --version v1.0.0

# 2. 运行部署脚本
cd web_flasher
.\sync_and_push.ps1

# 3. 检查 gh-pages 分支
git checkout gh-pages
ls projects/  # 应包含 test-web-v1.0.0.zip
git checkout master
```

Expected: GitHub Pages 站点包含新项目，分类筛选正常工作

- [ ] **步骤 4: Commit**

```bash
git add web_flasher/sync_and_push.ps1
git commit -m "feat(deploy): sync projects/downloads/images directories

- Add projects/ sync for web project zips
- Add downloads/ sync for android/windows binaries
- Add images/ sync for screenshots and icons
- Auto-create missing directories before deployment"
```

---

## Self-Review Checklist

执行计划前的最终检查（AI 自查，无需生成文件）：

- [ ] **Spec 覆盖率**：所有 spec 需求都有对应的 Task 实现？
  - ✅ 数据结构重构（Task 1）
  - ✅ 分类筛选 UI（Task 2）
  - ✅ 详情页视图（Task 3）
  - ✅ 路由分发（Task 4）
  - ✅ 卡片技术栈标签（Task 5）
  - ✅ 类型检测（Task 7）
  - ✅ 多类型发布（Task 8-9）
  - ✅ 文档更新（Task 10）
  - ✅ 部署脚本（Task 11）

- [ ] **占位符扫描**：计划中无 "TBD", "TODO", "待定" 等占位符（代码示例中的 "TBD" 字面量除外）？
  - ✅ 所有步骤包含具体代码或命令
  - ✅ 测试步骤包含完整的 Run + Expected

- [ ] **类型一致性**：Task 间的接口命名、参数类型一致？
  - ✅ `PROJECTS` 数组结构在所有 Task 中保持一致
  - ✅ `detect_project_type()` 返回值为固定的四种字符串
  - ✅ Handler 函数签名统一为 `(project_dir, name, version)`

- [ ] **依赖顺序**：Task 按依赖关系排序？
  - ✅ Task 1（数据结构）在前，Task 2-5（前端 UI）依赖它
  - ✅ Task 7（类型检测）在 Task 8（发布）之前
  - ✅ Task 10（文档）在所有功能实现后

