# CodeBuddy 网页固件烧录工具 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 构建一个浏览器内固件烧录工具 + 单页产品展示，让用户用 Chrome/Edge 通过 Web Serial 给 K10 和 Dongle 烧录固件，固件目标可扩展。

**Architecture:** 单个 `index.html`（内联 CSS/JS）从 CDN 加载 esptool-js；固件目标由 JS 配置数组 `FIRMWARE_TARGETS[]` 驱动，首页固件库卡片自动渲染；预置 bin 放在 `firmware/<id>/`，通过同源 `fetch` 读取，也支持手选文件覆盖；配 `serve.py` 起本地服务器。

**Tech Stack:** HTML/CSS/vanilla JS (ESM)、esptool-js (CDN)、Web Serial API、Python `http.server`。

## Global Constraints

- 仅支持 Chrome / Edge 桌面版（Web Serial API）。
- 必须经 `localhost` 或 HTTPS 访问；`file://` 无效。
- 所有目标均为 ESP32-S3，flash 设置 `dio` / `80m` / `16MB`。
- 分区地址（两目标一致）：bootloader `0x0`、分区表 `0x8000`、app `0x10000`。
- esptool-js 版本必须在 Task 1 锁定并全程使用同一版本 URL。
- 无自动化测试框架：每个 Task 的验证 = 浏览器手动加载 + DevTools Console 检查 + UI 观察。
- 不上传固件到任何服务器；固件仅在本机浏览器处理。

---

## File Structure

- `web_flasher/index.html` — 单页：Hero 展示 + 固件库卡片 + 三步烧录工具 + 所有 CSS/JS。
- `web_flasher/firmware/k10/` — K10 的 bootloader.bin / partitions.bin / firmware.bin。
- `web_flasher/firmware/dongle/` — Dongle 的 bootloader.bin / partition-table.bin / codebuddy_dongle.bin。
- `web_flasher/serve.py` — 本地服务器启动脚本。
- `web_flasher/README.md` — 使用说明。

由于是单文件应用，JS 逻辑按职责分块（都在 `index.html` 的一个 `<script type="module">` 里），后续任务各自负责一块：配置数组 → UI 渲染 → 串口连接 → 固件加载 → 烧录执行。

---

### Task 1: 项目骨架 + esptool-js 版本锁定 + 环境检测

**Files:**
- Create: `web_flasher/index.html`
- Create: `web_flasher/serve.py`
- Create: `web_flasher/firmware/k10/.gitkeep`
- Create: `web_flasher/firmware/dongle/.gitkeep`

**Interfaces:**
- Consumes: 无。
- Produces: 全局函数 `log(msg)`（向设备日志区追加一行）、`esptoolModule`（Task 1 验证可从 CDN 导入的 ESM 模块对象，含 `ESPLoader`、`Transport`）。锁定的 CDN URL 常量 `ESPTOOL_CDN`。

- [ ] **Step 1: 创建 serve.py**

```python
#!/usr/bin/env python3
"""CodeBuddy 网页烧录工具本地服务器。
用法: python serve.py  然后用 Chrome/Edge 打开 http://localhost:8000
"""
import http.server
import socketserver
import webbrowser
import os

PORT = 8000
os.chdir(os.path.dirname(os.path.abspath(__file__)))

class Handler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

if __name__ == "__main__":
    url = f"http://localhost:{PORT}"
    print(f"CodeBuddy Web Flasher 运行中: {url}")
    print("请用 Chrome 或 Edge 桌面版打开上面的地址。按 Ctrl+C 停止。")
    try:
        webbrowser.open(url)
    except Exception:
        pass
    with socketserver.TCPServer(("", PORT), Handler) as httpd:
        httpd.serve_forever()
```

- [ ] **Step 2: 创建 index.html 骨架（含环境检测与 esptool-js 导入验证）**

```html
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>CodeBuddy 固件烧录工具</title>
<style>
  :root { --bg:#0f1419; --card:#1a2129; --accent:#f7b500; --text:#e8e6e3; --muted:#8b95a1; }
  * { box-sizing:border-box; margin:0; padding:0; }
  body { background:var(--bg); color:var(--text); font-family:system-ui,-apple-system,"Microsoft YaHei",sans-serif; line-height:1.6; }
  #env-banner { padding:12px 20px; text-align:center; font-weight:600; display:none; }
  #env-banner.error { background:#7a1f1f; display:block; }
  #env-banner.ok { background:#1f5c2e; display:block; }
  #log { background:#000; color:#4ade80; font-family:ui-monospace,monospace; font-size:13px; padding:12px; height:180px; overflow-y:auto; white-space:pre-wrap; border-radius:8px; margin-top:12px; }
  .wrap { max-width:960px; margin:0 auto; padding:24px; }
</style>
</head>
<body>
<div id="env-banner"></div>
<div class="wrap">
  <h1>CodeBuddy 固件烧录工具</h1>
  <div id="log">等待连接设备...</div>
</div>
<script type="module">
const ESPTOOL_CDN = "https://unpkg.com/esptool-js@0.5.7/bundle.js";

const logEl = document.getElementById("log");
window.log = (msg) => {
  logEl.textContent += "\n" + msg;
  logEl.scrollTop = logEl.scrollHeight;
};

// 环境检测
const banner = document.getElementById("env-banner");
function checkEnv() {
  if (!("serial" in navigator)) {
    banner.className = "error";
    banner.textContent = "⚠ 当前浏览器不支持 Web Serial，请使用 Chrome 或 Edge 桌面版。";
    return false;
  }
  if (!window.isSecureContext) {
    banner.className = "error";
    banner.textContent = "⚠ 请通过 http://localhost 或 HTTPS 访问（不要直接双击打开 HTML 文件）。";
    return false;
  }
  banner.className = "ok";
  banner.textContent = "✓ 浏览器环境就绪";
  return true;
}
const envOk = checkEnv();

// esptool-js 导入验证
let esptoolModule = null;
if (envOk) {
  try {
    esptoolModule = await import(ESPTOOL_CDN);
    window.esptoolModule = esptoolModule;
    log("esptool-js 加载成功: " + (typeof esptoolModule.ESPLoader === "function" ? "ESPLoader OK" : "缺少 ESPLoader"));
    log("Transport: " + (typeof esptoolModule.Transport === "function" ? "OK" : "缺失"));
  } catch (e) {
    log("esptool-js 加载失败: " + e.message);
  }
}
</script>
</body>
</html>
```

- [ ] **Step 3: 启动服务器**

Run: `python web_flasher/serve.py`
Expected: 终端打印 `CodeBuddy Web Flasher 运行中: http://localhost:8000`，浏览器自动打开。

- [ ] **Step 4: 浏览器验证（手动测试）**

用 Chrome/Edge 打开 `http://localhost:8000`。打开 DevTools Console。
Expected:
- 顶部绿色横幅 "✓ 浏览器环境就绪"
- 日志区出现 "esptool-js 加载成功: ESPLoader OK" 和 "Transport: OK"
- Console 无红色报错

如果 `esptool-js@0.5.7/bundle.js` 报 404 或缺少 ESPLoader，改用 `https://unpkg.com/esptool-js@0.5.7/lib/index.js`，重试直到导入成功，并把可用的 URL 固定为 `ESPTOOL_CDN`。记录最终可用版本到日志。

- [ ] **Step 5: Commit**

```bash
git add web_flasher/index.html web_flasher/serve.py web_flasher/firmware/k10/.gitkeep web_flasher/firmware/dongle/.gitkeep
git commit -m "feat(web-flasher): 项目骨架 + 环境检测 + esptool-js 加载验证"
```

---

### Task 2: 复制预置固件 bin

**Files:**
- Create: `web_flasher/firmware/k10/bootloader.bin`
- Create: `web_flasher/firmware/k10/partitions.bin`
- Create: `web_flasher/firmware/k10/firmware.bin`
- Create: `web_flasher/firmware/dongle/bootloader.bin`
- Create: `web_flasher/firmware/dongle/partition-table.bin`
- Create: `web_flasher/firmware/dongle/codebuddy_dongle.bin`

**Interfaces:**
- Consumes: 无。
- Produces: 六个可被同源 `fetch` 读取的 bin 文件。

- [ ] **Step 1: 复制 K10 固件**

Run:
```bash
cp .pio/build/51_mic_wifi/bootloader.bin web_flasher/firmware/k10/bootloader.bin
cp .pio/build/51_mic_wifi/partitions.bin web_flasher/firmware/k10/partitions.bin
cp .pio/build/51_mic_wifi/firmware.bin  web_flasher/firmware/k10/firmware.bin
```

- [ ] **Step 2: 复制 Dongle 固件**

Run:
```bash
cp dongle_firmware/build/bootloader/bootloader.bin           web_flasher/firmware/dongle/bootloader.bin
cp dongle_firmware/build/partition_table/partition-table.bin web_flasher/firmware/dongle/partition-table.bin
cp dongle_firmware/build/codebuddy_dongle.bin                web_flasher/firmware/dongle/codebuddy_dongle.bin
```

- [ ] **Step 3: 验证文件大小非零**

Run: `ls -la web_flasher/firmware/k10/ web_flasher/firmware/dongle/`
Expected: 六个文件均存在，firmware.bin ≈1.1MB，codebuddy_dongle.bin ≈800KB，bootloader/分区表为 KB 级，无 0 字节文件。

- [ ] **Step 4: Commit**

```bash
git add web_flasher/firmware/
git commit -m "feat(web-flasher): 预置 K10 与 Dongle 固件 bin"
```

---

### Task 3: 固件目标配置数组 + 固件库卡片渲染

**Files:**
- Modify: `web_flasher/index.html`（在 module script 内新增配置与渲染，在 body 新增卡片容器）

**Interfaces:**
- Consumes: 无。
- Produces: 全局常量 `FIRMWARE_TARGETS`（数组，每项 `{id, name, description, chip, flashMode, flashFreq, flashSize, partitions:[{name, offset, file}]}`）；全局变量 `selectedTarget`（当前选中目标对象，初始 `null`）；函数 `selectTarget(id)`（设置 `selectedTarget` 并高亮卡片）；DOM 事件 `target-selected`（`selectTarget` 触发时 dispatch，`detail` 为目标对象）。

- [ ] **Step 1: 在 body 的 h1 之后、log 之前加入卡片容器和工具占位**

```html
  <h2>选择固件</h2>
  <div id="firmware-cards" style="display:flex;gap:16px;flex-wrap:wrap;margin:16px 0;"></div>
  <div id="selected-info" style="margin:16px 0;color:var(--muted);">未选择固件</div>
```

- [ ] **Step 2: 在 module script 顶部（ESPTOOL_CDN 之后）加入配置数组**

```javascript
const FIRMWARE_TARGETS = [
  {
    id: "k10", name: "K10 发射端",
    description: "麦克风 + ESP-NOW + 6屏状态显示",
    chip: "esp32s3", flashMode: "dio", flashFreq: "80m", flashSize: "16MB",
    partitions: [
      { name: "bootloader", offset: 0x0,     file: "firmware/k10/bootloader.bin" },
      { name: "partitions", offset: 0x8000,  file: "firmware/k10/partitions.bin" },
      { name: "app",        offset: 0x10000, file: "firmware/k10/firmware.bin" },
    ],
  },
  {
    id: "dongle", name: "Dongle 接收端",
    description: "USB HID + UAC 音频，左Ctrl+F2 录音键",
    chip: "esp32s3", flashMode: "dio", flashFreq: "80m", flashSize: "16MB",
    partitions: [
      { name: "bootloader",      offset: 0x0,     file: "firmware/dongle/bootloader.bin" },
      { name: "partition-table", offset: 0x8000,  file: "firmware/dongle/partition-table.bin" },
      { name: "app",             offset: 0x10000, file: "firmware/dongle/codebuddy_dongle.bin" },
    ],
  },
];
window.FIRMWARE_TARGETS = FIRMWARE_TARGETS;
let selectedTarget = null;
```

- [ ] **Step 3: 加入卡片渲染与选择逻辑（module script 内，环境检测之后）**

```javascript
const cardsEl = document.getElementById("firmware-cards");
const selInfoEl = document.getElementById("selected-info");

function selectTarget(id) {
  selectedTarget = FIRMWARE_TARGETS.find(t => t.id === id) || null;
  [...cardsEl.children].forEach(c =>
    c.style.outline = (c.dataset.id === id) ? "2px solid var(--accent)" : "none");
  if (selectedTarget) {
    const rows = selectedTarget.partitions
      .map(p => `${p.name} @ 0x${p.offset.toString(16)} → ${p.file.split("/").pop()}`)
      .join("<br>");
    selInfoEl.innerHTML = `已选择 <b>${selectedTarget.name}</b> (${selectedTarget.chip})<br>${rows}`;
    document.dispatchEvent(new CustomEvent("target-selected", { detail: selectedTarget }));
  }
}
window.selectTarget = selectTarget;

FIRMWARE_TARGETS.forEach(t => {
  const card = document.createElement("div");
  card.dataset.id = t.id;
  card.style.cssText = "background:var(--card);border-radius:12px;padding:20px;width:260px;cursor:pointer;";
  card.innerHTML = `<h3 style="color:var(--accent)">${t.name}</h3>
    <p style="color:var(--muted);font-size:14px">${t.description}</p>
    <p style="font-size:12px;margin-top:8px">${t.chip} · ${t.flashSize}</p>`;
  card.onclick = () => selectTarget(t.id);
  cardsEl.appendChild(card);
});
```

- [ ] **Step 4: 浏览器验证（手动测试）**

刷新 `http://localhost:8000`。
Expected:
- 出现两张卡片：K10 发射端、Dongle 接收端
- 点击 K10 卡片 → 卡片出现黄色描边，下方显示 "已选择 K10 发射端 (es32s3)" 及三行分区信息（bootloader @ 0x0 等）
- 点击 Dongle 卡片 → 高亮切换，分区信息更新
- Console 执行 `window.selectedTarget` 返回当前对象

- [ ] **Step 5: Commit**

```bash
git add web_flasher/index.html
git commit -m "feat(web-flasher): 固件目标配置数组 + 固件库卡片渲染"
```

---

### Task 4: 串口连接（Web Serial + esptool-js 芯片识别）

**Files:**
- Modify: `web_flasher/index.html`

**Interfaces:**
- Consumes: `esptoolModule`（`ESPLoader`、`Transport`）、`log()`。
- Produces: 全局变量 `transport`（`Transport` 实例，初始 `null`）、`esploader`（`ESPLoader` 实例，初始 `null`）、`detectedChip`（字符串，识别到的芯片名，初始 `null`）；函数 `connectDevice()`（打开串口、识别芯片）；DOM 按钮 `#btn-connect`。

- [ ] **Step 1: 在 selected-info 之后加入连接按钮**

```html
  <h2>01 · 连接设备</h2>
  <button id="btn-connect" style="background:var(--accent);border:none;padding:10px 20px;border-radius:8px;font-weight:600;cursor:pointer;">连接设备</button>
  <span id="chip-info" style="margin-left:12px;color:var(--muted);"></span>
```

- [ ] **Step 2: 加入连接逻辑（module script 内）**

```javascript
let transport = null, esploader = null, detectedChip = null;
const chipInfoEl = document.getElementById("chip-info");

async function connectDevice() {
  if (!esptoolModule) { log("esptool-js 未就绪"); return; }
  try {
    const port = await navigator.serial.requestPort();
    transport = new esptoolModule.Transport(port, true);
    esploader = new esptoolModule.ESPLoader({
      transport,
      baudrate: parseInt(document.getElementById("baud").value, 10),
      terminal: { clean(){}, writeLine(d){ log(d); }, write(d){ logEl.textContent += d; } },
    });
    log("正在连接并识别芯片...");
    detectedChip = await esploader.main();
    chipInfoEl.textContent = "已连接: " + detectedChip;
    log("芯片识别: " + detectedChip);
  } catch (e) {
    log("连接失败: " + e.message);
    chipInfoEl.textContent = "连接失败";
  }
}
document.getElementById("btn-connect").onclick = connectDevice;
window.connectDevice = connectDevice;
```

- [ ] **Step 3: 在连接按钮附近加入波特率选择（供 connectDevice 读取）**

在 `#btn-connect` 之前插入：
```html
  波特率:
  <select id="baud">
    <option>115200</option><option>230400</option>
    <option selected>460800</option><option>921600</option>
  </select>
```

- [ ] **Step 4: 浏览器验证（手动测试，需插入 K10 或 Dongle）**

把 Dongle（COM10）用数据线插到电脑。刷新页面，点 "连接设备"。
Expected:
- 弹出浏览器串口选择框，选择 "USB JTAG/serial debug unit (COMxx)"
- 日志出现 "正在连接并识别芯片..." → "芯片识别: ESP32-S3"
- `#chip-info` 显示 "已连接: ESP32-S3"

若报 "connect failed"，多试一次（Web Serial 首次握手偶发失败）；确认 esptool-js 版本与 Task 1 一致。

- [ ] **Step 5: Commit**

```bash
git add web_flasher/index.html
git commit -m "feat(web-flasher): Web Serial 串口连接 + 芯片识别"
```

---

### Task 5: 固件文件加载（预置 fetch + 手选覆盖）

**Files:**
- Modify: `web_flasher/index.html`

**Interfaces:**
- Consumes: `selectedTarget`、`log()`、`target-selected` 事件。
- Produces: 函数 `loadFileArray()` → 返回 `Promise<Array<{data:string, address:number}>>`，其中 `data` 为 esptool-js 期望的 binary string（每字符一字节，见 Step 2 说明）；全局 `manualOverrides`（对象，key 为 partition name，value 为 binary string，来自手选文件）；DOM 分区列表容器 `#partition-list`。

- [ ] **Step 1: 加入分区列表容器**

在连接区之后：
```html
  <h2>02 · 核对固件</h2>
  <div id="partition-list" style="margin:12px 0;">请先选择固件</div>
```

- [ ] **Step 2: 加入文件读取工具与分区列表渲染（module script 内）**

说明：esptool-js 的 `writeFlash` 期望 `fileArray[].data` 是 "binary string"（`String.fromCharCode` 逐字节，不是 Uint8Array）。以下 `bufToBinStr` 完成转换。

```javascript
let manualOverrides = {};
const partListEl = document.getElementById("partition-list");

function bufToBinStr(buf) {
  const bytes = new Uint8Array(buf);
  let s = "";
  for (let i = 0; i < bytes.length; i++) s += String.fromCharCode(bytes[i]);
  return s;
}

function renderPartitions() {
  if (!selectedTarget) { partListEl.textContent = "请先选择固件"; return; }
  partListEl.innerHTML = selectedTarget.partitions.map(p => `
    <div style="margin:6px 0;">
      <code>0x${p.offset.toString(16).padStart(4,"0")}</code>
      ${p.name}
      <span style="color:var(--muted)">(${manualOverrides[p.name] ? "手选文件" : p.file.split("/").pop()})</span>
      <input type="file" data-part="${p.name}" style="margin-left:8px;">
    </div>`).join("");
  partListEl.querySelectorAll("input[type=file]").forEach(inp => {
    inp.onchange = async (e) => {
      const f = e.target.files[0];
      if (!f) return;
      manualOverrides[e.target.dataset.part] = bufToBinStr(await f.arrayBuffer());
      log(`已用手选文件覆盖分区 ${e.target.dataset.part}: ${f.name}`);
      renderPartitions();
    };
  });
}
document.addEventListener("target-selected", () => { manualOverrides = {}; renderPartitions(); });

async function loadFileArray() {
  if (!selectedTarget) throw new Error("未选择固件目标");
  const arr = [];
  for (const p of selectedTarget.partitions) {
    let data;
    if (manualOverrides[p.name]) {
      data = manualOverrides[p.name];
    } else {
      const resp = await fetch(p.file);
      if (!resp.ok) throw new Error(`固件文件缺失: ${p.file} (${resp.status})，请手选文件`);
      data = bufToBinStr(await resp.arrayBuffer());
    }
    arr.push({ data, address: p.offset });
  }
  return arr;
}
window.loadFileArray = loadFileArray;
```

- [ ] **Step 3: 浏览器验证（手动测试）**

刷新页面，点 K10 卡片。
Expected:
- "02 · 核对固件" 下出现三行分区（0x0000 bootloader、0x8000 partitions、0x10000 app），每行显示预置文件名和一个文件选择框
- Console 执行 `await window.loadFileArray()` → 返回长度 3 的数组，每项 `{data, address}`，`data.length` 与对应 bin 字节数一致（bootloader ≈15104）
- 用文件框手选任意 bin → 该行标注变 "手选文件"，日志出现覆盖提示

- [ ] **Step 4: Commit**

```bash
git add web_flasher/index.html
git commit -m "feat(web-flasher): 固件加载（预置 fetch + 手选覆盖）"
```

---

### Task 6: 烧录执行（writeFlash + 进度条 + 擦除选项 + 芯片校验）

**Files:**
- Modify: `web_flasher/index.html`

**Interfaces:**
- Consumes: `esploader`、`detectedChip`、`selectedTarget`、`loadFileArray()`、`log()`。
- Produces: 函数 `flashFirmware()`；DOM `#btn-flash`、`#erase-all`、`#progress`。

- [ ] **Step 1: 加入写入区 UI**

```html
  <h2>03 · 写入与重启</h2>
  <label><input type="checkbox" id="erase-all"> 写入前擦除整片 flash</label><br>
  <button id="btn-flash" style="background:#22c55e;border:none;padding:10px 20px;border-radius:8px;font-weight:600;cursor:pointer;margin-top:8px;">开始写入</button>
  <div style="background:#333;border-radius:6px;height:22px;margin-top:12px;overflow:hidden;">
    <div id="progress" style="background:var(--accent);height:100%;width:0%;text-align:center;font-size:13px;color:#000;">0%</div>
  </div>
```

- [ ] **Step 2: 加入烧录逻辑（module script 内）**

```javascript
const progressEl = document.getElementById("progress");

async function flashFirmware() {
  if (!esploader) { log("请先连接设备"); return; }
  if (!selectedTarget) { log("请先选择固件"); return; }
  // 芯片校验
  if (detectedChip && !detectedChip.toLowerCase().replace(/[-\s]/g,"").includes(selectedTarget.chip)) {
    if (!confirm(`识别到 ${detectedChip}，与目标 ${selectedTarget.chip} 不一致，仍要继续?`)) return;
  }
  try {
    const fileArray = await loadFileArray();
    log("开始写入，共 " + fileArray.length + " 个分区...");
    document.getElementById("btn-flash").disabled = true;
    await esploader.writeFlash({
      fileArray,
      flashSize: selectedTarget.flashSize,
      flashMode: selectedTarget.flashMode,
      flashFreq: selectedTarget.flashFreq,
      eraseAll: document.getElementById("erase-all").checked,
      compress: true,
      reportProgress: (idx, written, total) => {
        const pct = Math.round((written / total) * 100);
        progressEl.style.width = pct + "%";
        progressEl.textContent = `分区${idx+1}/${fileArray.length} ${pct}%`;
      },
    });
    log("✓ 写入完成，正在硬复位重启...");
    await esploader.after("hard_reset");
    log("✓ 全部完成");
  } catch (e) {
    log("✗ 写入失败: " + e.message);
  } finally {
    document.getElementById("btn-flash").disabled = false;
  }
}
document.getElementById("btn-flash").onclick = flashFirmware;
window.flashFirmware = flashFirmware;
```

- [ ] **Step 3: 端到端验证（手动测试，需 Dongle 插 COM10）**

刷新页面 → 点 Dongle 卡片 → 点 "连接设备" 选串口（识别 ESP32-S3）→ 点 "开始写入"。
Expected:
- 进度条从 0% 递增，文本显示 "分区1/3 ..." → "分区3/3 100%"
- 日志出现 "✓ 写入完成，正在硬复位重启..." → "✓ 全部完成"
- Dongle 重启后功能正常（左Ctrl+F2 录音键、MAC 覆盖生效）

若 `after` 方法报错（版本差异），改为 `await transport.setDTR(false); await transport.setRTS(false);` 触发复位，或在日志提示用户手动复位。

- [ ] **Step 4: Commit**

```bash
git add web_flasher/index.html
git commit -m "feat(web-flasher): 烧录执行 + 进度条 + 擦除 + 芯片校验"
```

---

### Task 7: Hero 展示区 + README

**Files:**
- Modify: `web_flasher/index.html`
- Create: `web_flasher/README.md`

**Interfaces:**
- Consumes: 无。
- Produces: 无（纯展示与文档）。

- [ ] **Step 1: 在 .wrap 内 h1 之前插入 Hero 区**

```html
  <section style="text-align:center;padding:40px 0;">
    <h1 style="font-size:36px;">CodeBuddy 无线系统</h1>
    <p style="color:var(--muted);font-size:18px;margin-top:8px;">浏览器一键烧录 · 文件不上传服务器</p>
    <div style="display:flex;gap:12px;justify-content:center;flex-wrap:wrap;margin-top:24px;">
      <div style="background:var(--card);padding:16px;border-radius:10px;width:180px;">🎤 无线麦克风<br><span style="color:var(--muted);font-size:13px;">ESP-NOW 音频流</span></div>
      <div style="background:var(--card);padding:16px;border-radius:10px;width:180px;">⌨️ USB HID 键盘<br><span style="color:var(--muted);font-size:13px;">左Ctrl+F2 录音键</span></div>
      <div style="background:var(--card);padding:16px;border-radius:10px;width:180px;">📊 6 屏状态显示<br><span style="color:var(--muted);font-size:13px;">Token/项目/AI云脸</span></div>
    </div>
    <a href="#firmware-cards" style="display:inline-block;margin-top:24px;background:var(--accent);color:#000;padding:12px 28px;border-radius:8px;text-decoration:none;font-weight:600;">开始烧录 ↓</a>
  </section>
```

注意：原来那句独立的 `<h1>CodeBuddy 固件烧录工具</h1>` 删除（Hero 已含标题），保留其后的 "选择固件" 区块。

- [ ] **Step 2: 创建 README.md**

```markdown
# CodeBuddy 网页固件烧录工具

浏览器内给 K10 / Dongle 烧录固件，无需安装 esptool / PlatformIO / ESP-IDF。

## 使用

1. 运行本地服务器：
   ```
   python web_flasher/serve.py
   ```
2. 用 **Chrome 或 Edge 桌面版** 打开 `http://localhost:8000`
   （不支持 Firefox/Safari；不能直接双击 HTML 用 file:// 打开）
3. 选择固件（K10 发射端 / Dongle 接收端）
4. 用数据线连接设备，点 "连接设备"，选择 "USB JTAG/serial debug unit (COMxx)"
5. 点 "开始写入"，等待进度条完成，设备自动重启

## 更新预置固件

重新构建后，把新的 bin 复制到 `firmware/<目标>/`：
- K10: `.pio/build/51_mic_wifi/{bootloader,partitions,firmware}.bin`
- Dongle: `dongle_firmware/build/{bootloader/bootloader,partition_table/partition-table}.bin` 和 `dongle_firmware/build/codebuddy_dongle.bin`

## 新增固件目标

编辑 `index.html` 中的 `FIRMWARE_TARGETS` 数组加一项，并把 bin 放进 `firmware/<新id>/`，首页固件库会自动多一张卡片。
```

- [ ] **Step 3: 浏览器验证（手动测试）**

刷新页面。
Expected:
- 顶部出现 Hero 区（大标题 + 三张功能卡片 + "开始烧录 ↓" 按钮）
- 点 "开始烧录 ↓" 页面滚动到固件卡片区
- 下方烧录三步流程完整可见，功能不受影响

- [ ] **Step 4: Commit**

```bash
git add web_flasher/index.html web_flasher/README.md
git commit -m "feat(web-flasher): Hero 展示区 + 使用说明 README"
```

---

## Self-Review

**Spec coverage:**
- 单文件 + CDN → Task 1 ✓
- 本地服务器 → Task 1 (serve.py) ✓
- 环境/浏览器检测 → Task 1 ✓
- 预置固件 → Task 2 ✓
- 可扩展配置数组 + 卡片 → Task 3 ✓
- 三步流程（连接/核对/写入）→ Task 4/5/6 ✓
- 手选文件覆盖 → Task 5 ✓
- 进度条/擦除/芯片校验/错误处理 → Task 6 ✓
- Hero 展示首页 → Task 7 ✓
- README 使用说明 → Task 7 ✓

**Placeholder scan:** 无 TBD/TODO；每个代码步骤含完整代码；验证步骤为浏览器手动测试（本项目无测试框架，符合 Global Constraints）。

**Type consistency:** `FIRMWARE_TARGETS` 项结构在 Task 3 定义，Task 5/6 一致使用 `partitions[].{name,offset,file}` 与 `flashMode/flashFreq/flashSize`。`loadFileArray()` 返回 `{data(binary string), address}`，与 Task 6 `writeFlash({fileArray})` 一致。`bufToBinStr` 在 Task 5 定义并在同任务内使用。`log`/`esptoolModule`/`selectedTarget`/`esploader`/`detectedChip` 均先定义后消费。

**版本风险已标注：** esptool-js CDN 版本在 Task 1 锁定；`writeFlash` 的 data 格式（binary string）、`after`/复位方法差异均在对应任务给了 fallback。

