#!/usr/bin/env python3
"""从源项目提取元信息

用法:
  python extract_metadata.py <source_path>

输出:
  JSON 格式的项目元信息
"""
import os
import re
import json
import sys


def extract_metadata(source_path):
    """从源项目提取元信息"""
    meta = {
        "chip": "esp32s3",          # 默认值
        "flash_size": "16MB",
        "flash_mode": "dio",
        "flash_freq": "80m",
        "description": ""
    }

    # 从 platformio.ini 提取
    ini_path = os.path.join(source_path, "..", "platformio.ini")
    if os.path.exists(ini_path):
        with open(ini_path, encoding="utf-8") as f:
            content = f.read()

            # 提取芯片型号
            if "esp32s3" in content.lower():
                meta["chip"] = "esp32s3"
            elif "esp32s2" in content.lower():
                meta["chip"] = "esp32s2"
            elif "esp32c3" in content.lower():
                meta["chip"] = "esp32c3"
            elif "esp32c6" in content.lower():
                meta["chip"] = "esp32c6"

            # 提取 flash 配置
            if m := re.search(r"flash_size\s*=\s*(\w+)", content, re.I):
                meta["flash_size"] = m.group(1)

            if m := re.search(r"flash_mode\s*=\s*(\w+)", content, re.I):
                meta["flash_mode"] = m.group(1)

    # 从 sdkconfig 提取（ESP-IDF 项目）
    sdkconfig_path = os.path.join(source_path, "sdkconfig")
    if os.path.exists(sdkconfig_path):
        with open(sdkconfig_path, encoding="utf-8") as f:
            content = f.read()

            # 提取芯片型号
            if "CONFIG_IDF_TARGET_ESP32S3=y" in content:
                meta["chip"] = "esp32s3"
            elif "CONFIG_IDF_TARGET_ESP32S2=y" in content:
                meta["chip"] = "esp32s2"
            elif "CONFIG_IDF_TARGET_ESP32C3=y" in content:
                meta["chip"] = "esp32c3"

            # 提取 flash 配置
            if m := re.search(r'CONFIG_ESPTOOLPY_FLASHSIZE_(\w+)=y', content):
                meta["flash_size"] = m.group(1)

            if m := re.search(r'CONFIG_ESPTOOLPY_FLASHMODE_(\w+)=y', content):
                meta["flash_mode"] = m.group(1).lower()

    # 从 README 提取描述
    for readme_name in ["README.md", "readme.md", "Readme.md"]:
        readme_path = os.path.join(source_path, readme_name)
        if os.path.exists(readme_path):
            with open(readme_path, encoding="utf-8") as f:
                lines = f.readlines()
                # 提取第一段非标题、非空行文本作为描述
                for line in lines:
                    line = line.strip()
                    if line and not line.startswith("#") and not line.startswith("```"):
                        # 截取前 100 字符
                        meta["description"] = line[:100]
                        break
            break

    # 从 .ino 文件提取描述（Arduino 项目）
    for root, dirs, files in os.walk(source_path):
        for file in files:
            if file.endswith(".ino"):
                ino_path = os.path.join(root, file)
                with open(ino_path, encoding="utf-8") as f:
                    content = f.read(500)  # 只读前 500 字符
                    # 提取多行注释中的描述
                    if m := re.search(r'/\*\*(.*?)\*/', content, re.DOTALL):
                        desc = m.group(1).strip()
                        # 清理注释标记
                        desc = re.sub(r'^\s*\*\s*', '', desc, flags=re.MULTILINE)
                        meta["description"] = desc[:100]
                break
        break

    return meta


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("用法: python extract_metadata.py <source_path>", file=sys.stderr)
        sys.exit(1)

    source_path = sys.argv[1]
    if not os.path.exists(source_path):
        print(f"错误: 路径不存在: {source_path}", file=sys.stderr)
        sys.exit(1)

    meta = extract_metadata(source_path)
    print(json.dumps(meta, ensure_ascii=False, indent=2))
