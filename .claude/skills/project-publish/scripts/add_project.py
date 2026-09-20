#!/usr/bin/env python3
"""向 index.html 添加新项目配置

用法:
  python add_project.py '<json_config>'

示例:
  python add_project.py '{"id":"test","name":"测试项目",...}'
"""
import re
import sys
import json
import os


def add_project(html_path, project_config):
    """向 index.html 的 PROJECTS 数组添加新项目"""

    # 读取 HTML 文件
    with open(html_path, "r", encoding="utf-8") as f:
        content = f.read()

    # 检查项目 ID 是否已存在
    if f'id: "{project_config["id"]}"' in content:
        print(f"⚠️  项目 {project_config['id']} 已存在，将替换现有配置", file=sys.stderr)
        # 删除旧配置
        pattern = r'\{\s*id:\s*"' + re.escape(project_config["id"]) + r'"[\s\S]*?\},\s*'
        content = re.sub(pattern, '', content)

    # 构造新项目的 JavaScript 对象（支持固件和非固件项目）
    project_type = project_config.get("type", "firmware")
    icon = project_config.get('icon', '')
    bg_class = project_config.get('bgClass', '')

    new_entry = f"""  {{
    id: "{project_config['id']}",
    name: "{project_config['name']}",
    type: "{project_type}",
    badge: "{project_config['badge']}",
    version: "{project_config['version']}",
    description: "{project_config['description']}",
    icon: "{icon}","""

    if bg_class:
        new_entry += f"""
    bgClass: "{bg_class}","""

    # 固件项目需要芯片和分区配置
    if project_type == "firmware":
        new_entry += f"""
    chip: "{project_config['chip']}",
    flashMode: "{project_config['flash_mode']}",
    flashFreq: "{project_config['flash_freq']}",
    flashSize: "{project_config['flash_size']}",
    partitions: [
      {{ name: "bootloader", offset: 0x0, file: "firmware/{project_config['id']}/bootloader.bin" }},
      {{ name: "partitions", offset: 0x8000, file: "firmware/{project_config['id']}/partitions.bin" }},
      {{ name: "app", offset: 0x10000, file: "firmware/{project_config['id']}/firmware.bin" }},
    ],"""
    elif project_type == "web":
        # Web 项目直接嵌入 iframe，需要入口 URL
        entry_url = project_config.get(
            "entryUrl", f"projects/{project_config['id']}/index.html"
        )
        new_entry += f"""
    entryUrl: "{entry_url}","""
    else:
        # android/windows 项目只记录下载链接（由前端根据 type 和 id 推断）
        ext_map = {"android": "apk", "windows": "exe"}
        download_ext = ext_map.get(project_type, "zip")
        new_entry += f"""
    downloadUrl: "downloads/{project_config['id']}.{download_ext}","""

    new_entry += "\n  },\n"

    # 在 PROJECTS 数组末尾插入（最后一个 ]; 之前）
    pattern = r'(const PROJECTS\s*=\s*\[[\s\S]*?)\];'

    def replacer(match):
        existing_content = match.group(1)
        return existing_content + new_entry + "\n];"

    new_content = re.sub(pattern, replacer, content)

    if new_content == content:
        print("❌ 未找到 PROJECTS 数组，无法添加项目", file=sys.stderr)
        return False

    # 写回文件
    with open(html_path, "w", encoding="utf-8") as f:
        f.write(new_content)

    print(f"✅ 已添加项目 {project_config['id']} 到 {html_path}")
    return True


def add_css_background(html_path, project_id, image_file):
    """添加产品图片的 CSS 背景样式"""

    with open(html_path, "r", encoding="utf-8") as f:
        content = f.read()

    # 检查样式是否已存在
    if f".card-image.{project_id}-bg" in content:
        print(f"ℹ️  CSS 样式 .{project_id}-bg 已存在，跳过")
        return

    # 生成 CSS 样式
    css_style = f"""  .card-image.{project_id}-bg {{
    background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
    background-image: url('images/{image_file}'), linear-gradient(135deg, #667eea 0%, #764ba2 100%);
    background-size: cover, cover;
    background-position: center;
    background-repeat: no-repeat, no-repeat;
  }}
"""

    # 在最后一个 .card-image.xxx-bg 样式后插入
    pattern = r'(\.card-image\.\w+-bg\s*\{[^}]+\})\s*'
    matches = list(re.finditer(pattern, content))

    if matches:
        # 在最后一个匹配后插入
        last_match = matches[-1]
        insert_pos = last_match.end()
        content = content[:insert_pos] + css_style + content[insert_pos:]
    else:
        # 找不到已有样式，在 </style> 前插入
        content = content.replace('</style>', css_style + '</style>')

    with open(html_path, "w", encoding="utf-8") as f:
        f.write(content)

    print(f"✅ 已添加 CSS 样式 .{project_id}-bg")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("用法: python add_project.py '<json_config>'", file=sys.stderr)
        sys.exit(1)

    try:
        config = json.loads(sys.argv[1])
    except json.JSONDecodeError as e:
        print(f"错误: 无效的 JSON 格式: {e}", file=sys.stderr)
        sys.exit(1)

    # 验证必需字段
    project_type = config.get("type", "firmware")
    base_required = ["id", "name", "badge", "version"]

    if project_type == "firmware":
        required_fields = base_required + ["chip", "flash_mode", "flash_freq", "flash_size"]
    else:
        required_fields = base_required

    for field in required_fields:
        if field not in config:
            print(f"错误: 缺少必需字段: {field}", file=sys.stderr)
            sys.exit(1)

    # 确定 HTML 文件路径
    html_path = "web_flasher/index.html"
    if not os.path.exists(html_path):
        print(f"错误: 找不到 {html_path}", file=sys.stderr)
        sys.exit(1)

    # 添加项目
    success = add_project(html_path, config)

    # 如果有图片，添加 CSS
    if success and "image_file" in config:
        add_css_background(html_path, config["id"], config["image_file"])

    sys.exit(0 if success else 1)
