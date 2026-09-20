# 多类型项目扩展指南

本文档说明如何向 Project Showcase 平台添加新项目。

## 支持的项目类型

| 类型 | type 字段 | 徽章颜色 | 资源目录 | 交互方式 |
|------|-----------|---------|---------|---------|
| 固件 | `firmware` | 绿色 #22c55e | `firmware/<id>/` | Web Serial 烧录 |
| 网页 | `web` | 蓝色 #3b82f6 | `projects/<id>/` | 新标签页打开 |
| Android | `android` | 橙色 #f97316 | `downloads/<id>/` | 详情页 + APK 下载 |
| Windows | `windows` | 紫色 #a855f7 | `downloads/<id>/` | 详情页 + EXE 下载 |

## 快速开始

添加一个新项目只需 3 步：

### 1. 准备项目资源

根据项目类型，将资源放入对应目录：

**固件项目**：
```
web_flasher/
└── firmware/
    └── my-new-product/
        ├── bootloader.bin
        ├── partition-table.bin
        └── app.bin
```

**Web 项目**：
```
web_flasher/
└── projects/
    └── my-web-app/
        ├── index.html
        ├── style.css
        └── app.js
```

**Android 项目**：
```
web_flasher/
└── downloads/
    └── my-android-app/
        └── app-release.apk
```

**Windows 项目**：
```
web_flasher/
└── downloads/
    └── my-windows-app/
        └── setup.exe
```

### 2. 编辑 index.html 的配置数组

打开 `web_flasher/index.html`，找到 `PROJECTS` 数组（约第 377 行），在末尾添加新项目：

#### 固件项目示例

```javascript
const PROJECTS = [
  // ... 已有项目
  {
    id: "my-new-product",
    name: "我的新固件",
    type: "firmware",                        // 类型：firmware
    badge: "NEW",
    version: "v1.0.0",
    description: "ESP32-S3 + 传感器 + 蓝牙 5.0",
    icon: "🚀",
    chip: "esp32s3",
    flashMode: "dio",
    flashFreq: "80m",
    flashSize: "16MB",
    partitions: [
      { name: "bootloader", offset: 0x0, file: "firmware/my-new-product/bootloader.bin" },
      { name: "partition-table", offset: 0x8000, file: "firmware/my-new-product/partition-table.bin" },
      { name: "app", offset: 0x10000, file: "firmware/my-new-product/app.bin" },
    ],
  },
];
```

#### Web 项目示例

```javascript
{
  id: "my-web-app",
  name: "我的 Web 应用",
  type: "web",                               // 类型：web
  badge: "DEMO",
  version: "v1.0.0",
  description: "React + TypeScript 仪表板应用",
  icon: "🌐",
  url: "projects/my-web-app/index.html",     // 必填：应用入口
},
```

#### Android 项目示例

```javascript
{
  id: "my-android-app",
  name: "我的 Android 应用",
  type: "android",                           // 类型：android
  badge: "STABLE",
  version: "v2.0.1",
  description: "Android 9+ 移动端应用，支持蓝牙和 NFC",
  icon: "📱",
  downloadUrl: "downloads/my-android-app/app-release.apk",  // 必填：APK 下载地址
  fileSize: "25.3 MB",                       // 可选
  minSdk: "28",                              // 可选
},
```

#### Windows 项目示例

```javascript
{
  id: "my-windows-app",
  name: "我的 Windows 应用",
  type: "windows",                           // 类型：windows
  badge: "PRO",
  version: "v1.5.0",
  description: "Windows 10+ 桌面应用，支持串口通信和数据采集",
  icon: "🖥️",
  downloadUrl: "downloads/my-windows-app/setup.exe",  // 必填：EXE 下载地址
  fileSize: "15.2 MB",                       // 可选
  minWindows: "10",                          // 可选
},
```

### 3. （可选）添加产品图片或截图

在 `web_flasher/images/` 下放入图片：

```
web_flasher/
└── images/
    └── my-new-product.jpg      # 产品实物图或应用截图（建议 680×480px）
```

然后在 `index.html` 的 CSS 部分（约第 145 行）添加背景图样式：

```css
.card-image.my-new-product-bg {
  background: linear-gradient(135deg, #ff9a9e 0%, #fad0c4 100%);  /* 兜底渐变 */
  background-image: url('images/my-new-product.jpg'), linear-gradient(135deg, #ff9a9e 0%, #fad0c4 100%);
  background-size: cover, cover;
  background-position: center;
  background-repeat: no-repeat, no-repeat;
}
```

**注意**：CSS class 名 `my-new-product-bg` 必须是 `{id}-bg` 的格式（`id` 来自第 2 步的配置）。

---

## 类型自动检测

使用 `detect_type.py` 脚本或 `add_project.py --auto` 可自动推断项目类型：

| 检测文件 | 推断类型 |
|---------|---------|
| `platformio.ini` 或 `sdkconfig` | firmware |
| `package.json` | web |
| `build.gradle` | android |
| `*.sln` 或 `*.vcxproj` | windows |

```bash
# 自动检测并添加项目
python .claude/skills/project-publish/scripts/add_project.py \
  --project-dir /path/to/project \
  --name "项目名" \
  --badge "NEW" \
  --version "v1.0.0"
```

---

## 详细说明

### 固件项目配置

`partitions` 数组每项必须包含：

- `name`: 分区名称（自定义，用于日志显示）
- `offset`: 烧录地址（16 进制，如 `0x10000`）
- `file`: 固件文件路径（相对于 `web_flasher/` 目录）

**常见 ESP32-S3 分区布局：**

| 分区           | 偏移地址   | 说明                     |
|----------------|------------|--------------------------|
| bootloader     | `0x0`      | 引导加载程序             |
| partition-table| `0x8000`   | 分区表                   |
| app            | `0x10000`  | 主应用程序（固件）       |
| nvs            | `0x9000`   | NVS 存储（可选，已集成在 app 中时无需单独烧录） |

**获取分区地址：**
- PlatformIO 项目：查看 `.pio/build/{env}/partitions.csv`
- ESP-IDF 项目：查看 `build/partition_table/partition-table.bin`（用 `idf.py partition_table` 生成）

### 固件项目配置

#### 分区配置说明

`partitions` 数组每项必须包含：

- `name`: 分区名称（自定义，用于日志显示）
- `offset`: 烧录地址（16 进制，如 `0x10000`）
- `file`: 固件文件路径（相对于 `web_flasher/` 目录）

**常见 ESP32-S3 分区布局：**

| 分区           | 偏移地址   | 说明                     |
|----------------|------------|--------------------------|
| bootloader     | `0x0`      | 引导加载程序             |
| partition-table| `0x8000`   | 分区表                   |
| app            | `0x10000`  | 主应用程序（固件）       |
| nvs            | `0x9000`   | NVS 存储（可选，已集成在 app 中时无需单独烧录） |

**获取分区地址：**
- PlatformIO 项目：查看 `.pio/build/{env}/partitions.csv`
- ESP-IDF 项目：查看 `build/partition_table/partition-table.bin`（用 `idf.py partition_table` 生成）

#### 芯片型号参考

| 芯片     | `chip` 字段值 |
|----------|---------------|
| ESP32-S3 | `esp32s3`     |
| ESP32-S2 | `esp32s2`     |
| ESP32-C3 | `esp32c3`     |
| ESP32    | `esp32`       |

### Flash 配置参考（固件项目专用）

**flashMode**（模式）：
- `dio` — Dual I/O（推荐，兼容性好）
- `qio` — Quad I/O（速度更快，需硬件支持）
- `dout` — Dual Output
- `qout` — Quad Output

**flashFreq**（频率）：
- `80m` — 80 MHz（推荐，ESP32-S3 默认）
- `40m` — 40 MHz
- `26m` — 26 MHz
- `20m` — 20 MHz

**flashSize**（大小）：
- `4MB`, `8MB`, `16MB`, `32MB` 等

**如何确定参数：**
- PlatformIO：查看 `platformio.ini` 的 `board_build.flash_mode` 和 `board_upload.flash_size`
- ESP-IDF：查看 `sdkconfig` 的 `CONFIG_ESPTOOLPY_FLASHMODE` 和 `CONFIG_ESPTOOLPY_FLASHSIZE`

### Web 项目配置

**必填字段**：
- `type: "web"` — 项目类型
- `url` — 应用入口路径（相对于 `web_flasher/`，如 `"projects/my-app/index.html"`）

**可选字段**：
- `icon` — emoji 图标（如 "🌐"）
- `description` — 应用简介

**目录结构建议**：
```
web_flasher/projects/my-app/
├── index.html      # 入口文件
├── style.css
├── app.js
└── assets/         # 静态资源
```

### Android 项目配置

**必填字段**：
- `type: "android"` — 项目类型
- `downloadUrl` — APK 下载地址（相对于 `web_flasher/`，如 `"downloads/my-app/app.apk"`）

**可选字段**：
- `fileSize` — 文件大小（如 "25.3 MB"）
- `minSdk` — 最低 Android 版本（如 "28" 表示 Android 9）
- `icon` — emoji 图标（如 "📱"）

### Windows 项目配置

**必填字段**：
- `type: "windows"` — 项目类型
- `downloadUrl` — EXE/MSI 下载地址（相对于 `web_flasher/`，如 `"downloads/my-app/setup.exe"`）

**可选字段**：
- `fileSize` — 文件大小（如 "15.2 MB"）
- `minWindows` — 最低 Windows 版本（如 "10"）
- `icon` — emoji 图标（如 "🖥️"）

### 徽章（badge）建议

| 徽章文字 | 适用场景                 |
|----------|--------------------------|
| STABLE   | 稳定版                   |
| BETA     | 测试版                   |
| NEW      | 新产品                   |
| PRO      | 专业版/付费版            |
| READY    | 可用/已验证              |
| TRANSMIT | 发射端（如 K10）         |
| RECEIVE  | 接收端（如 Dongle）      |

### 版本号（version）建议

遵循语义化版本：`vMAJOR.MINOR.PATCH`

- `v1.0.0` — 正式发布
- `v1.2.3` — 功能更新
- `v2.0.0` — 重大更新（不兼容旧版）

### 产品描述（description）建议

**好的描述**（突出技术特性）：
- ✅ "ESP32-S3 + BME280 环境传感器，蓝牙 5.0 + LoRa 双模通信"
- ✅ "4G LTE 物联网网关，支持 Modbus RTU/TCP，8 路继电器控制"

**避免的描述**（过于业务/营销）：
- ❌ "为智慧农业打造的革命性解决方案"
- ❌ "AI 赋能的下一代智能硬件"

---

## 完整示例

### 示例 1：添加固件项目（智能温湿度传感器）

#### 步骤 1：准备文件

```bash
# 创建固件目录
mkdir -p web_flasher/firmware/temp-sensor

# 复制固件文件（示例路径，实际根据你的构建输出）
cp .pio/build/temp-sensor/bootloader.bin web_flasher/firmware/temp-sensor/
cp .pio/build/temp-sensor/partitions.bin web_flasher/firmware/temp-sensor/
cp .pio/build/temp-sensor/firmware.bin web_flasher/firmware/temp-sensor/

# （可选）放置产品图片
cp product-photos/temp-sensor.jpg web_flasher/images/
```

#### 步骤 2：编辑 index.html

在 `PROJECTS` 数组末尾添加：

```javascript
{
  id: "temp-sensor",
  name: "智能温湿度传感器",
  type: "firmware",
  badge: "STABLE",
  version: "v2.1.0",
  description: "ESP32-C3 + SHT40 高精度传感器，WiFi + 低功耗蓝牙，支持 MQTT",
  icon: "🌡️",
  chip: "esp32c3",
  flashMode: "dio",
  flashFreq: "80m",
  flashSize: "4MB",
  partitions: [
    { name: "bootloader", offset: 0x0, file: "firmware/temp-sensor/bootloader.bin" },
    { name: "partitions", offset: 0x8000, file: "firmware/temp-sensor/partitions.bin" },
    { name: "app", offset: 0x10000, file: "firmware/temp-sensor/firmware.bin" },
  ],
},
```

#### 步骤 3：添加背景图样式（可选）

在 CSS 的 `.card-image.dongle-bg` 后面添加：

```css
.card-image.temp-sensor-bg {
  background: linear-gradient(135deg, #a8edea 0%, #fed6e3 100%);
  background-image: url('images/temp-sensor.jpg'), linear-gradient(135deg, #a8edea 0%, #fed6e3 100%);
  background-size: cover, cover;
  background-position: center;
  background-repeat: no-repeat, no-repeat;
}
```

#### 步骤 4：验证

```bash
# 启动服务器
cd web_flasher
python serve.py

# 浏览器打开 http://localhost:8000
# 首页应显示新增的固件项目卡片（绿色徽章）
```

### 示例 2：添加 Web 项目

```bash
# 1. 构建 Web 应用
cd my-web-dashboard
npm run build

# 2. 复制构建产物
cp -r dist ../web_flasher/projects/web-dashboard

# 3. 在 PROJECTS 数组添加配置
{
  id: "web-dashboard",
  name: "数据监控仪表板",
  type: "web",
  badge: "DEMO",
  version: "v1.0.0",
  description: "实时设备数据可视化，支持 WebSocket 更新",
  icon: "📊",
  url: "projects/web-dashboard/index.html",
}
```

### 示例 3：添加 Android 项目

```bash
# 1. 构建 APK
cd my-android-app
./gradlew assembleRelease

# 2. 复制 APK
mkdir -p ../web_flasher/downloads/android-app
cp app/build/outputs/apk/release/app-release.apk ../web_flasher/downloads/android-app/

# 3. 在 PROJECTS 数组添加配置
{
  id: "android-app",
  name: "移动端控制应用",
  type: "android",
  badge: "STABLE",
  version: "v2.0.0",
  description: "Android 移动端设备控制，支持蓝牙和网络通信",
  icon: "📱",
  downloadUrl: "downloads/android-app/app-release.apk",
  fileSize: "18.5 MB",
  minSdk: "24",
}
```

### 示例 4：添加 Windows 项目

```bash
# 1. 构建 Windows 安装包
cd my-windows-app
msbuild /p:Configuration=Release

# 2. 复制 EXE
mkdir -p ../web_flasher/downloads/windows-app
cp Release/MyApp.exe ../web_flasher/downloads/windows-app/setup.exe

# 3. 在 PROJECTS 数组添加配置
{
  id: "windows-app",
  name: "桌面端调试工具",
  type: "windows",
  badge: "PRO",
  version: "v1.3.0",
  description: "Windows 桌面应用，支持串口通信和固件烧录",
  icon: "🖥️",
  downloadUrl: "downloads/windows-app/setup.exe",
  fileSize: "12.8 MB",
  minWindows: "10",
}
```

---

## 常见问题

### Q1: 添加项目后首页不显示卡片？

**排查：**
1. 按 F12 打开浏览器 Console，看是否有 JavaScript 报错
2. 检查 `id` 字段是否唯一（不能与已有项目重复）
3. 检查 `type` 字段是否为 `"firmware"` / `"web"` / `"android"` / `"windows"` 之一
4. 检查配置数组语法（最后一项后面不要有逗号，除非还有下一项）
5. 硬刷新页面（Ctrl+Shift+R）清除缓存

### Q2: 卡片显示了，但点击后没反应？

**按项目类型排查：**

**固件项目**：
- 检查 `firmware/` 下的 bin 文件是否存在，路径拼写是否正确
- 按 F12 看 Console 和 Network 标签，是否有 404 错误
- 检查 `chip`/`flashMode`/`flashFreq` 是否与固件构建配置一致

**Web 项目**：
- 检查 `url` 指向的 HTML 文件是否存在
- 验证路径是否正确（相对于 `web_flasher/` 目录）

**Android/Windows 项目**：
- 检查 `downloadUrl` 指向的文件是否存在
- 验证文件路径拼写正确

### Q3: 背景图不显示？

**排查：**
1. 图片文件名与 CSS 中的 `url('images/xxx.jpg')` 是否一致（大小写敏感）
2. 图片格式是否为浏览器支持的格式（推荐 JPG/PNG，WebP 需检查兼容性）
3. CSS class 名 `xxx-bg` 的 `xxx` 是否与配置中的 `id` 一致
4. 按 F12 → Network 标签，刷新页面，看图片请求是否 200 成功

### Q4: 如何更新已有项目？

**固件项目**：替换 `firmware/<id>/` 下的 bin 文件，刷新页面即可（无需改代码）。

**Web 项目**：更新 `projects/<id>/` 下的构建产物。

**Android/Windows 项目**：替换 `downloads/<id>/` 下的安装包文件。

### Q5: 如何下架某个项目？

在 `PROJECTS` 数组中删除对应项目的整个对象（包括外层的 `{}` 和后面的逗号）。

---

## 高级配置

### 多版本支持

目前每个项目只显示一个版本。如需多版本选择，有两种方案：

**方案 1：作为不同项目**
```javascript
{ id: "product-v1", name: "产品名 v1.0", type: "firmware", ... },
{ id: "product-v2", name: "产品名 v2.0", type: "firmware", ... },
```

**方案 2：修改代码支持版本下拉**（需自行实现）
- 在 `PROJECTS` 中每个项目改为 `versions: [{version, partitions/url/downloadUrl}, ...]`
- 在交互界面添加版本选择下拉框
- 根据选中版本加载对应资源

### 固件下载功能

如果想提供"下载固件到本地"功能（不烧录），可在烧录界面添加下载按钮：

```javascript
async function downloadFirmware() {
  const fileArray = await loadFileArray();
  fileArray.forEach(p => {
    const a = document.createElement('a');
    a.href = URL.createObjectURL(new Blob([new Uint8Array([...p.data].map(c => c.charCodeAt(0)))]));
    a.download = `0x${p.address.toString(16)}.bin`;
    a.click();
  });
}
```

### 自定义烧录参数（固件项目）

如果某个固件项目需要特殊参数（如不同的 baudrate），在 `PROJECTS` 加字段：

```javascript
{
  // ... 其他字段
  customBaudrate: 921600,  // 自定义波特率
}
```

然后在 `flashFirmware()` 中读取：

```javascript
const baudrate = selectedTarget.customBaudrate || parseInt(document.getElementById("baud").value, 10);
```

### Web 项目外链

如果 Web 应用托管在外部服务器，`url` 字段可使用完整 URL：

```javascript
{
  id: "external-app",
  type: "web",
  url: "https://example.com/my-app",  // 外部链接
  // ...
}
```

---

## 支持

遇到问题？
1. 查看浏览器 Console（F12）的错误信息
2. 阅读本文档的"常见问题"章节
3. 检查 `web_flasher/README.md` 的基础使用说明
