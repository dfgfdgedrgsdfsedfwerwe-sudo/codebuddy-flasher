---
name: publish
description: 发布项目到展示平台 / Publish project to showcase platform
---

# 项目发布命令

使用 `project-publish` skill 将项目发布到 Web Flasher 展示平台。

## 执行流程

1. **询问项目信息**（如果用户未在命令中提供）：
   - 项目源路径（绝对路径或相对路径）
   - 项目 ID（小写英文+连字符，如 `k10`, `ai-passport`）
   - 项目显示名称（中文）
   - 徽章文字（如 `NEW`, `STABLE`, `AI`, `DEMO`）
   - 版本号（如 `v1.0.0`）
   - 项目描述（可选，默认从 README 提取）
   - 项目图标 emoji（可选）

2. **自动检测项目类型**：
   - firmware（ESP32 固件）
   - web（网页应用）
   - android（APK）
   - windows（EXE）

3. **执行发布**：
   - 定位并复制构建产物（bin 文件/dist 目录/apk/exe）
   - 提取项目元信息（芯片型号、flash 配置等）
   - 更新 `web_flasher/index.html` 的 PROJECTS 数组
   - 验证文件完整性

4. **可选部署**：
   - 询问是否部署到 GitHub Pages
   - 如果确认，运行 `web_flasher/sync_and_push.ps1`

## 注意事项

- 固件项目需先构建：`pio run` 或 `idf.py build`
- Web 项目需先构建：`npm run build`
- 项目 ID 重复时会替换现有配置
- Windows 下如果遇到编码问题，手动执行发布流程

## 示例用法

```
/project-publish examples/51_mic_wifi --id k10 --name "CodeBuddy K10" --version v1.0.3
/project-publish dongle_firmware --auto
/project-publish
```

最后一种不带参数，会交互式询问所有信息。
