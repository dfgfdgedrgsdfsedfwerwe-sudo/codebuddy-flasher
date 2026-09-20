#!/usr/bin/env python3
"""自动发布项目到 Web Flasher / 下载中心

支持多种项目类型：firmware / web / android / windows

用法:
  python publish.py <source_project> [options]

示例:
  python publish.py examples/51_mic_wifi --id k10 --name "CodeBuddy 发射端"
  python publish.py dongle_firmware --id dongle --deploy
  python publish.py examples/30_lvgl_Gif --auto
  python publish.py web_flasher --id flasher --name "Web Flasher"
"""
import os
import sys
import re
import json
import shutil
import argparse
import subprocess
from detect_type import detect_project_type


def detect_build_system(source_path):
    """检测构建系统类型"""
    # 检查是否在父目录有 platformio.ini
    parent_ini = os.path.join(os.path.dirname(source_path), "platformio.ini")
    if os.path.exists(parent_ini):
        return "platformio", parent_ini

    # 检查是否有 CMakeLists.txt (ESP-IDF)
    if os.path.exists(os.path.join(source_path, "CMakeLists.txt")):
        return "esp-idf", None

    return None, None


def get_platformio_env(ini_path):
    """从 platformio.ini 获取默认环境名"""
    with open(ini_path, encoding="utf-8") as f:
        content = f.read()
        if m := re.search(r'default_envs\s*=\s*(\S+)', content):
            return m.group(1).strip()
    return None


def locate_firmware_files(source_path, build_system):
    """定位固件文件"""
    files = {}

    if build_system == "platformio":
        # 获取环境名
        parent_dir = os.path.dirname(source_path)
        ini_path = os.path.join(parent_dir, "platformio.ini")
        env_name = get_platformio_env(ini_path)

        if not env_name:
            print("❌ 无法从 platformio.ini 获取环境名", file=sys.stderr)
            return None

        build_dir = os.path.join(parent_dir, ".pio", "build", env_name)

        files["bootloader"] = os.path.join(build_dir, "bootloader.bin")
        files["partitions"] = os.path.join(build_dir, "partitions.bin")
        files["firmware"] = os.path.join(build_dir, "firmware.bin")

    elif build_system == "esp-idf":
        build_dir = os.path.join(source_path, "build")

        files["bootloader"] = os.path.join(build_dir, "bootloader", "bootloader.bin")
        files["partitions"] = os.path.join(build_dir, "partition_table", "partition-table.bin")

        # 查找主固件文件（通常是 <project_name>.bin）
        for file in os.listdir(build_dir):
            if file.endswith(".bin") and file not in ["bootloader.bin", "partition-table.bin"]:
                files["firmware"] = os.path.join(build_dir, file)
                break

    # 验证文件存在
    for name, path in files.items():
        if not os.path.exists(path):
            print(f"❌ 固件文件不存在: {path}", file=sys.stderr)
            print(f"   提示: 请先构建项目（pio run 或 idf.py build）", file=sys.stderr)
            return None

    return files


def copy_firmware_files(files, target_dir):
    """复制固件文件到目标目录"""
    os.makedirs(target_dir, exist_ok=True)

    for name in ["bootloader", "partitions", "firmware"]:
        src = files[name]
        dst = os.path.join(target_dir, f"{name}.bin")
        shutil.copy2(src, dst)
        print(f"  ✓ {name}.bin")

    print(f"✅ 固件文件已复制到 {target_dir}/")


def extract_metadata(source_path):
    """提取项目元信息"""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    extract_script = os.path.join(script_dir, "extract_metadata.py")

    result = subprocess.run(
        [sys.executable, extract_script, source_path],
        capture_output=True,
        text=True
    )

    if result.returncode != 0:
        print(f"⚠️  元信息提取失败: {result.stderr}", file=sys.stderr)
        return {}

    try:
        return json.loads(result.stdout)
    except json.JSONDecodeError:
        return {}


def add_to_index_html(project_config):
    """添加项目到 index.html"""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    add_script = os.path.join(script_dir, "add_project.py")

    config_json = json.dumps(project_config, ensure_ascii=False)

    result = subprocess.run(
        [sys.executable, add_script, config_json],
        capture_output=True,
        text=True
    )

    if result.returncode != 0:
        print(f"❌ 添加项目失败: {result.stderr}", file=sys.stderr)
        return False

    print(result.stdout.strip())
    return True


def deploy_to_github_pages():
    """部署到 GitHub Pages"""
    script_path = "web_flasher/sync_and_push.ps1"

    if not os.path.exists(script_path):
        print(f"⚠️  部署脚本不存在: {script_path}", file=sys.stderr)
        return False

    print("🚀 正在部署到 GitHub Pages...")
    result = subprocess.run(
        ["powershell", "-File", script_path],
        capture_output=True,
        text=True
    )

    if result.returncode != 0:
        print(f"❌ 部署失败: {result.stderr}", file=sys.stderr)
        return False

    print("✅ 已推送到 GitHub Pages")
    return True


def publish_firmware(source_path, args):
    """发布固件项目（原有逻辑）"""
    # 1. 检测构建系统
    build_system, ini_path = detect_build_system(source_path)
    if not build_system:
        print("❌ 无法识别构建系统（需要 platformio.ini 或 CMakeLists.txt）", file=sys.stderr)
        return False

    print(f"✅ 构建系统: {build_system}")

    # 2. 定位固件文件
    firmware_files = locate_firmware_files(source_path, build_system)
    if not firmware_files:
        return False

    # 3. 提取元信息
    print("📋 提取项目元信息...")
    metadata = extract_metadata(source_path)

    # 4. 确定项目 ID
    if args.auto and not args.id:
        project_id = os.path.basename(source_path).replace("_", "-").lower()
    else:
        project_id = args.id

    if not project_id:
        print("❌ 缺少项目 ID，请使用 --id 参数", file=sys.stderr)
        return False

    # 5. 确定项目名称
    if args.auto and not args.name:
        project_name = os.path.basename(source_path).replace("_", " ").title()
    else:
        project_name = args.name

    if not project_name:
        print("❌ 缺少项目名称，请使用 --name 参数", file=sys.stderr)
        return False

    # 6. 复制固件文件
    target_dir = f"web_flasher/firmware/{project_id}"
    print(f"📁 复制固件文件到 {target_dir}/")
    copy_firmware_files(firmware_files, target_dir)

    # 7. 处理产品图片
    image_file = None
    if args.image and os.path.exists(args.image):
        image_dest = f"web_flasher/images/{project_id}.jpg"
        shutil.copy2(args.image, image_dest)
        image_file = f"{project_id}.jpg"
        print(f"✅ 产品图片已复制: {image_dest}")

    # 8. 构造项目配置
    project_config = {
        "id": project_id,
        "name": project_name,
        "badge": args.badge,
        "version": args.version,
        "description": args.description or metadata.get("description", ""),
        "icon": args.icon,
        "chip": metadata.get("chip", "esp32s3"),
        "flash_mode": metadata.get("flash_mode", "dio"),
        "flash_freq": metadata.get("flash_freq", "80m"),
        "flash_size": metadata.get("flash_size", "16MB"),
        "type": "firmware",
    }

    if image_file:
        project_config["image_file"] = image_file
        project_config["bgClass"] = f"{project_id}-bg"

    # 9. 添加到 index.html
    print("📝 更新 index.html...")
    if not add_to_index_html(project_config):
        return False

    # 10. 验证固件文件
    print("🔍 验证固件文件完整性...")
    check_script = "web_flasher/check_files.py"
    if os.path.exists(check_script):
        subprocess.run([sys.executable, check_script])

    # 输出总结
    print("\n" + "="*60)
    print(f"✅ 固件项目 {project_id} 已发布到 Web Flasher")
    print(f"📂 固件路径: {target_dir}/")
    print(f"🌐 本地预览: python web_flasher/serve.py")
    print("="*60)

    return True


def publish_web(source_path, args):
    """发布 Web 项目"""
    print("📦 发布 Web 项目...")

    # 1. 运行构建命令
    package_json = os.path.join(source_path, "package.json")
    if not os.path.exists(package_json):
        print("❌ 找不到 package.json", file=sys.stderr)
        return False

    # 检测包管理器
    if os.path.exists(os.path.join(source_path, "pnpm-lock.yaml")):
        pkg_manager = "pnpm"
    elif os.path.exists(os.path.join(source_path, "bun.lockb")):
        pkg_manager = "bun"
    else:
        pkg_manager = "npm"

    print(f"🔨 使用 {pkg_manager} 构建...")
    result = subprocess.run(
        [pkg_manager, "run", "build"],
        cwd=source_path,
        capture_output=True,
        text=True
    )

    if result.returncode != 0:
        print(f"❌ 构建失败: {result.stderr}", file=sys.stderr)
        return False

    # 2. 查找构建输出目录
    dist_candidates = ["dist", "build", "out", ".next/standalone"]
    dist_dir = None
    for candidate in dist_candidates:
        candidate_path = os.path.join(source_path, candidate)
        if os.path.exists(candidate_path):
            dist_dir = candidate_path
            break

    if not dist_dir:
        print("❌ 找不到构建输出目录（dist/build/out）", file=sys.stderr)
        return False

    print(f"✅ 找到构建输出: {dist_dir}")

    # 3. 确定项目 ID
    project_id = args.id or os.path.basename(source_path).replace("_", "-").lower()
    project_name = args.name or os.path.basename(source_path).replace("_", " ").title()

    # 4. 复制构建产物到 projects/ 目录
    projects_dir = f"web_flasher/projects/{project_id}"

    # 清空目标目录（避免旧文件残留）
    if os.path.exists(projects_dir):
        shutil.rmtree(projects_dir)

    shutil.copytree(dist_dir, projects_dir)
    print(f"✅ 已部署到: {projects_dir}")

    # 5. 更新 PROJECTS 数组
    project_config = {
        "id": project_id,
        "type": "web",
        "name": project_name,
        "version": args.version,
        "description": args.description or "",
        "badge": args.badge,
        "icon": args.icon or "🌐",
        "entryUrl": f"projects/{project_id}/index.html",
    }

    if not add_to_index_html(project_config):
        print("⚠️  更新 PROJECTS 数组失败", file=sys.stderr)

    print("\n" + "="*60)
    print(f"✅ Web 项目 {project_id} 已发布")
    print(f"📂 部署路径: {projects_dir}/")
    print(f"🌐 访问地址: projects/{project_id}/index.html")
    print("="*60)

    return True


def publish_android(source_path, args):
    """发布 Android 项目"""
    print("📦 发布 Android 项目...")

    # 1. 查找 APK 文件
    apk_files = []
    for root, dirs, files in os.walk(source_path):
        for file in files:
            if file.endswith(".apk"):
                apk_files.append(os.path.join(root, file))

    if not apk_files:
        print("❌ 找不到 APK 文件（请先构建项目）", file=sys.stderr)
        return False

    # 使用最新的 APK
    apk_file = max(apk_files, key=os.path.getmtime)
    print(f"✅ 找到 APK: {apk_file}")

    # 2. 确定项目 ID
    project_id = args.id or os.path.basename(source_path).replace("_", "-").lower()
    project_name = args.name or os.path.basename(source_path).replace("_", " ").title()

    # 3. 复制到下载目录
    downloads_dir = "web_flasher/downloads"
    os.makedirs(downloads_dir, exist_ok=True)

    dest_apk = os.path.join(downloads_dir, f"{project_id}.apk")
    shutil.copy2(apk_file, dest_apk)
    print(f"✅ 已复制: {dest_apk}")

    # 4. 更新 android.json
    index_file = os.path.join(downloads_dir, "android.json")
    if os.path.exists(index_file):
        with open(index_file, "r", encoding="utf-8") as f:
            android_index = json.load(f)
    else:
        android_index = {"apps": []}

    # 移除旧条目
    android_index["apps"] = [a for a in android_index["apps"] if a["id"] != project_id]

    # 添加新条目
    android_index["apps"].append({
        "id": project_id,
        "name": project_name,
        "version": args.version,
        "description": args.description or "",
        "file": f"{project_id}.apk",
        "size": os.path.getsize(dest_apk),
    })

    with open(index_file, "w", encoding="utf-8") as f:
        json.dump(android_index, f, ensure_ascii=False, indent=2)

    print(f"✅ 已更新 {index_file}")

    # 5. 更新 PROJECTS 数组
    project_config = {
        "id": project_id,
        "type": "android",
        "name": project_name,
        "version": args.version,
        "description": args.description or "",
        "badge": args.badge,
        "icon": args.icon or "📱",
        "downloadUrl": f"downloads/{project_id}.apk",
    }

    if not add_to_index_html(project_config):
        print("⚠️  更新 PROJECTS 数组失败", file=sys.stderr)

    print("\n" + "="*60)
    print(f"✅ Android 项目 {project_id} 已发布")
    print(f"📦 APK: {dest_apk}")
    print("="*60)

    return True


def publish_windows(source_path, args):
    """发布 Windows 项目"""
    print("📦 发布 Windows 项目...")

    # 1. 查找可执行文件或安装包
    installer_files = []
    for root, dirs, files in os.walk(source_path):
        for file in files:
            if file.endswith((".exe", ".msi")):
                installer_files.append(os.path.join(root, file))

    if not installer_files:
        print("❌ 找不到 .exe 或 .msi 文件（请先构建项目）", file=sys.stderr)
        return False

    # 使用最新的文件
    installer_file = max(installer_files, key=os.path.getmtime)
    print(f"✅ 找到安装包: {installer_file}")

    # 2. 确定项目 ID
    project_id = args.id or os.path.basename(source_path).replace("_", "-").lower()
    project_name = args.name or os.path.basename(source_path).replace("_", " ").title()

    # 3. 复制到下载目录
    downloads_dir = "web_flasher/downloads"
    os.makedirs(downloads_dir, exist_ok=True)

    file_ext = os.path.splitext(installer_file)[1]
    dest_file = os.path.join(downloads_dir, f"{project_id}{file_ext}")
    shutil.copy2(installer_file, dest_file)
    print(f"✅ 已复制: {dest_file}")

    # 4. 更新 windows.json
    index_file = os.path.join(downloads_dir, "windows.json")
    if os.path.exists(index_file):
        with open(index_file, "r", encoding="utf-8") as f:
            windows_index = json.load(f)
    else:
        windows_index = {"apps": []}

    # 移除旧条目
    windows_index["apps"] = [a for a in windows_index["apps"] if a["id"] != project_id]

    # 添加新条目
    windows_index["apps"].append({
        "id": project_id,
        "name": project_name,
        "version": args.version,
        "description": args.description or "",
        "file": f"{project_id}{file_ext}",
        "size": os.path.getsize(dest_file),
    })

    with open(index_file, "w", encoding="utf-8") as f:
        json.dump(windows_index, f, ensure_ascii=False, indent=2)

    print(f"✅ 已更新 {index_file}")

    # 5. 更新 PROJECTS 数组
    project_config = {
        "id": project_id,
        "type": "windows",
        "name": project_name,
        "version": args.version,
        "description": args.description or "",
        "badge": args.badge,
        "icon": args.icon or "🖥️",
        "downloadUrl": f"downloads/{project_id}{file_ext}",
    }

    if not add_to_index_html(project_config):
        print("⚠️  更新 PROJECTS 数组失败", file=sys.stderr)

    print("\n" + "="*60)
    print(f"✅ Windows 项目 {project_id} 已发布")
    print(f"📦 安装包: {dest_file}")
    print("="*60)

    return True


def main():
    parser = argparse.ArgumentParser(description="自动发布项目到 Web Flasher / 下载中心")
    parser.add_argument("source_project", help="源项目路径（如 examples/51_mic_wifi）")
    parser.add_argument("--id", help="项目唯一标识（小写英文+连字符）")
    parser.add_argument("--name", help="项目显示名称")
    parser.add_argument("--badge", default="NEW", help="徽章文字（默认 NEW）")
    parser.add_argument("--version", default="v1.0.0", help="版本号（默认 v1.0.0）")
    parser.add_argument("--description", help="项目描述（默认自动提取）")
    parser.add_argument("--icon", default="", help="emoji 图标（默认空）")
    parser.add_argument("--image", help="产品图片路径")
    parser.add_argument("--deploy", action="store_true", help="部署到 GitHub Pages")
    parser.add_argument("--auto", action="store_true", help="自动推断所有参数")

    args = parser.parse_args()

    source_path = os.path.abspath(args.source_project)

    if not os.path.exists(source_path):
        print(f"❌ 源项目路径不存在: {source_path}", file=sys.stderr)
        sys.exit(1)

    print(f"📦 发布项目: {source_path}")

    # 1. 检测项目类型
    try:
        project_type = detect_project_type(source_path)
        print(f"✅ 项目类型: {project_type}")
    except ValueError as e:
        print(f"❌ {e}", file=sys.stderr)
        sys.exit(1)

    # 2. 根据类型调用对应的发布处理器
    handlers = {
        "firmware": publish_firmware,
        "web": publish_web,
        "android": publish_android,
        "windows": publish_windows,
    }

    handler = handlers.get(project_type)
    if not handler:
        print(f"❌ 不支持的项目类型: {project_type}", file=sys.stderr)
        sys.exit(1)

    # 3. 执行发布
    success = handler(source_path, args)

    if not success:
        sys.exit(1)

    # 4. 部署到 GitHub Pages（可选）
    if args.deploy:
        deploy_to_github_pages()


if __name__ == "__main__":
    main()
