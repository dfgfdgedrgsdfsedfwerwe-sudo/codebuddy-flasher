以下是在 Windows 系统上安装 `ffmpeg` 的步骤：

**步骤一：下载 `ffmpeg`**
1. 打开 `ffmpeg` 的官方网站：[https://ffmpeg.org/](https://ffmpeg.org/)。
2. 点击页面上的 "Download" 链接。
3. 在 "Windows Builds" 部分，选择一个 Windows 版本的 `ffmpeg` 进行下载。一般推荐下载 "Windows Builds by BtbN" 的 "ffmpeg-master-latest-win64-gpl.zip"（适用于 64 位 Windows 系统），如果你使用的是 32 位 Windows 系统，可以选择对应的 32 位版本。


**步骤二：解压文件**
1. 找到下载好的 `.zip` 文件，通常位于你的浏览器默认的下载文件夹中。
2. 右键点击该文件，选择 "Extract All..."（解压全部）。
3. 选择一个你想要解压到的目标文件夹，例如 `C:\ffmpeg`。


**步骤三：添加 `ffmpeg` 到系统环境变量**
1. 按下 `Win + S` 键，搜索 "环境变量"，然后点击 "编辑系统环境变量"。
2. 在弹出的 "系统属性" 对话框中，点击 "环境变量" 按钮。
3. 在 "系统变量" 部分，找到 "Path" 变量，点击 "编辑"。
4. 点击 "新建" 按钮，然后输入 `ffmpeg` 的可执行文件所在的完整路径，例如 `C:\ffmpeg\bin`。
5. 点击 "确定" 关闭所有打开的对话框。


**步骤四：验证安装是否成功**
1. 打开命令提示符（按下 `Win + R`，输入 `cmd`，然后按回车键）。
2. 在命令提示符中输入 `ffmpeg -version`，然后按回车键。
3. 如果看到 `ffmpeg` 的版本信息，说明安装成功；如果显示 "ffmpeg 不是内部或外部命令，也不是可运行的程序或批处理文件"，可能是环境变量设置有误，你可以检查步骤三中的环境变量设置是否正确。


通过以上步骤，你可以在 Windows 系统上成功安装 `ffmpeg` 并开始使用它进行多媒体文件的处理。在后续使用 `ffmpeg` 时，你可以在命令提示符中输入各种 `ffmpeg` 命令，例如转换视频格式、提取音频等，如 `ffmpeg -i input.mp4 output.avi` 可以将 `input.mp4` 转换为 `output.avi`。请根据自己的实际需求调整输入文件和输出文件的名称，并使用不同的 `ffmpeg` 命令参数来完成不同的任务。


用ffmpeg 把avi视频转化为 mjpeg文件，分辨率320*240
ffmpeg -i write_system.avi -vf "scale=320:240:force_original_aspect_ratio=decrease,pad=320:240:(ow-iw)/2:(oh-ih)/2" -c:v mjpeg output.mjpeg


ffmpeg -i write_system.avi -s 320x240 -c:v mjpeg output2.mjpeg

ffmpeg -i gongfu.avi -s 320x240 -r 25 -c:v mjpeg gongfu.mjpeg