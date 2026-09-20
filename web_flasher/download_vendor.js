// 下载 esptool-js bundle 到本地 vendor/ 目录（离线打包用）
const fs = require("fs");
const path = require("path");

const URL = "https://unpkg.com/esptool-js@0.5.7/bundle.js";
const OUT = path.join(__dirname, "vendor", "esptool-bundle.js");

async function main() {
  console.log("Downloading", URL);
  const resp = await fetch(URL);
  if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
  const text = await resp.text();
  fs.mkdirSync(path.dirname(OUT), { recursive: true });
  fs.writeFileSync(OUT, text, "utf8");
  console.log("Saved to", OUT, `(${text.length} chars)`);
  // 简单验证：ESPLoader 是 esptool-js bundle 的导出
  if (!text.includes("ESPLoader")) throw new Error("bundle 内容异常：缺少 ESPLoader");
  console.log("验证 OK: bundle 含 ESPLoader");
}

main().catch((e) => {
  console.error("下载失败:", e.message);
  process.exit(1);
});
