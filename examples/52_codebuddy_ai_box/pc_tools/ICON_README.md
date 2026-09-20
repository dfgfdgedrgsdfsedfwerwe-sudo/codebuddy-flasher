# 应用图标

如需自定义图标，请将 `.ico` 文件命名为 `icon.ico` 并放置在此目录。

## 创建图标的方法

### 方案 1：在线工具
访问 https://convertio.co/zh/png-ico/ 上传 PNG 图片转换为 ICO

### 方案 2：使用 PIL (Python)
```python
from PIL import Image
img = Image.open("logo.png")
img.save("icon.ico", format="ICO", sizes=[(256, 256)])
```

### 方案 3：使用系统图标
Windows 自带图标位于：
- `C:\Windows\System32\shell32.dll`
- `C:\Windows\System32\imageres.dll`

可用工具提取：`ResourceHacker` 或 `IconsExtract`

## 图标规格

- 格式：ICO
- 推荐尺寸：256×256（支持多尺寸更好）
- 透明背景：建议使用

当前打包脚本中图标行已注释，如有 `icon.ico` 文件可取消注释：
```python
'--icon=icon.ico',
```
