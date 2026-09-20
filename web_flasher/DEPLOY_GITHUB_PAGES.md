# GitHub Pages 部署指南

## 📋 准备工作

`deploy_pages/` 目录已准备好，包含：
- ✅ index.html（主页面）
- ✅ firmware/（10 个目标的固件，共 12MB）
- ✅ vendor/esptool-bundle.js（离线 esptool）
- ✅ images/（设备图片）
- ✅ .nojekyll（禁用 Jekyll 处理）
- ✅ README.md（项目说明）
- ✅ Git 仓库已初始化并提交

---

## 🚀 部署步骤

### 第一步：在 GitHub 创建仓库

1. 访问 https://github.com/new
2. 填写仓库信息：
   - **Repository name**: `codebuddy-flasher`（可自定义）
   - **Description**: `ESP32-S3 Web Flasher - Browser-based firmware tool`
   - **Public** ✅（GitHub Pages 免费版要求公开）
   - **⚠️ 不要勾选**：Add a README file, .gitignore, license（保持空仓库）
3. 点击 "Create repository"

### 第二步：推送代码

在创建仓库后，GitHub 会显示推送命令。**在 `deploy_pages/` 目录下**执行：

```bash
cd C:\Users\4090\Desktop\dfk10_arduino_demo-master\web_flasher\deploy_pages

# 添加远程仓库（替换你的用户名）
git remote add origin https://github.com/你的用户名/codebuddy-flasher.git

# 推送到 GitHub
git push -u origin main
```

**如果遇到认证问题**：
- 使用 GitHub Personal Access Token（推荐）
- 或用 GitHub Desktop 可视化推送

### 第三步：启用 GitHub Pages

#### 方法 A：通过网页设置（推荐）

1. 推送成功后，访问仓库页面
2. 点击 **Settings**（设置）
3. 左侧菜单找到 **Pages**
4. 在 "Build and deployment" 下：
   - **Source**: 选择 "Deploy from a branch"
   - **Branch**: 选择 `main` 和 `/root`
   - 点击 **Save**
5. 等待 1-2 分钟，页面顶部会显示：
   ```
   ✅ Your site is live at https://你的用户名.github.io/codebuddy-flasher/
   ```

#### 方法 B：通过命令行（需要 gh CLI）

如果你安装了 `gh` CLI：

```bash
# 在 deploy_pages/ 目录下
gh repo view --web  # 打开仓库页面
# 或直接启用 Pages
gh api repos/{owner}/{repo}/pages -X POST -f source[branch]=main -f source[path]=/
```

---

## ✅ 验证部署

### 1. 检查部署状态

访问仓库的 **Actions** 标签页，会看到 "pages build and deployment" 工作流：
- 🟡 黄色圆圈 = 正在部署
- ✅ 绿色勾 = 部署成功
- ❌ 红色叉 = 部署失败（查看日志）

### 2. 访问站点

部署成功后，访问：
```
https://你的用户名.github.io/codebuddy-flasher/
```

### 3. 测试功能

- [ ] 页面加载正常，看到 10 个固件卡片
- [ ] 点击"CodeBuddy 发射端（AI BOX 原子）"，能看到 3 个分区
- [ ] 连接 USB 设备，点击"连接设备"，能选择串口
- [ ] 控制台没有 404 错误（固件文件都能加载）

---

## 🔧 更新固件

当你修改了固件或页面，重新部署：

```bash
cd deploy_pages/

# 复制新的固件文件（从 web_flasher 目录）
cp ../firmware/codebuddy_ai_box/*.bin firmware/codebuddy_ai_box/

# 或复制新的 index.html
cp ../index.html .

# 提交并推送
git add -A
git commit -m "Update firmware"
git push

# GitHub Actions 会自动重新部署（1-2 分钟）
```

---

## 📝 自定义域名（可选）

如果你有自己的域名（如 `flasher.codebuddy.com`）：

1. 在 Settings → Pages → Custom domain 填写域名
2. 在域名 DNS 设置中添加 CNAME 记录：
   ```
   flasher  CNAME  你的用户名.github.io
   ```
3. 等待 DNS 生效（几分钟到几小时）
4. GitHub 会自动配置 HTTPS 证书

---

## ⚠️ 常见问题

### Q1: 推送失败 "Permission denied"
**A**: 需要配置 GitHub 认证：
- 生成 Personal Access Token: https://github.com/settings/tokens
- 权限勾选 `repo`
- 推送时用 Token 作为密码

### Q2: 页面显示 404
**A**: 检查：
1. Pages 是否已启用（Settings → Pages）
2. Branch 是否选对（应该是 `main`）
3. 等待 1-2 分钟让部署完成

### Q3: 固件文件 404
**A**: 
1. 确认 `.nojekyll` 文件存在（禁用 Jekyll）
2. 检查 `firmware/` 目录的路径大小写（Linux 区分大小写）
3. 清除浏览器缓存重试

### Q4: "navigator.serial is not defined"
**A**: 
1. 确认使用 Chrome/Edge（Safari/Firefox 不支持）
2. 确认访问的是 HTTPS（GitHub Pages 自动提供）
3. 不是在隐私/无痕模式下

### Q5: 国内访问慢
**A**: GitHub Pages 在国内有时较慢。备选方案：
- Cloudflare Pages（国内相对快）
- Vercel / Netlify（可能需要梯子）
- 自己的服务器 + Nginx

---

## 📊 部署后的文件结构

```
https://你的用户名.github.io/codebuddy-flasher/
├── index.html                         # 主页面
├── README.md                          # 项目说明
├── .nojekyll                         # 禁用 Jekyll
├── vendor/
│   └── esptool-bundle.js            # 离线 esptool
├── firmware/
│   ├── k10/                         # CodeBuddy 发射端
│   ├── dongle/                      # CodeBuddy 接收端
│   ├── codebuddy_ai_box/            # AI BOX 原子
│   ├── 30_lvgl_Gif/                 # LVGL Gif 演示
│   └── ...                          # 其他演示
└── images/
    ├── K10.jpg
    ├── dongle.jpg
    └── ...
```

---

## 🔗 相关链接

- **GitHub Pages 文档**: https://docs.github.com/en/pages
- **Web Serial API**: https://developer.mozilla.org/en-US/docs/Web/API/Web_Serial_API
- **esptool-js**: https://github.com/espressif/esptool-js

---

## 💡 下一步优化

部署成功后可以考虑：

- [ ] 添加 Google Analytics 统计访问量
- [ ] 在 README.md 里加入使用截图
- [ ] 创建 issues 模板收集用户反馈
- [ ] 添加 CHANGELOG.md 记录固件版本历史
- [ ] 配置 GitHub Actions 自动从主项目构建并更新固件

---

**准备完成，现在可以开始部署了！**

有问题随时在仓库开 issue 或联系维护者。
