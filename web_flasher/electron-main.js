const { app, BrowserWindow, protocol } = require('electron');
const path = require('path');
const url = require('url');

let mainWindow;

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1200,
    height: 900,
    title: 'CodeBuddy Web Flasher',
    webPreferences: {
      nodeIntegration: false,
      contextIsolation: true,
      enableRemoteModule: false,
      // 关键：启用 Web Serial API
      experimentalFeatures: true,
      // 允许 file:// 协议下的 fetch() 读取本地文件
      webSecurity: true  // 保持开启，通过 protocol 处理
    },
    icon: path.join(__dirname, 'images', 'K10.jpg')
  });

  // 加载本地 index.html
  mainWindow.loadFile('index.html');

  // 开发模式下打开 DevTools
  if (process.env.NODE_ENV === 'development') {
    mainWindow.webContents.openDevTools();
  }

  mainWindow.on('closed', () => {
    mainWindow = null;
  });

  // 拦截新窗口打开（防止外部链接跳出应用）
  mainWindow.webContents.setWindowOpenHandler(({ url }) => {
    require('electron').shell.openExternal(url);
    return { action: 'deny' };
  });
}

// Electron 启动完成后创建窗口
app.whenReady().then(() => {
  // 注册自定义 protocol（可选，用于更灵活的资源加载）
  protocol.registerFileProtocol('app', (request, callback) => {
    const filePath = request.url.replace('app://', '');
    callback({ path: path.join(__dirname, filePath) });
  });

  createWindow();

  // macOS 特性：点击 Dock 图标时重新创建窗口
  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow();
    }
  });
});

// 所有窗口关闭时退出应用（macOS 除外）
app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});

// 处理 Web Serial API 权限请求（Electron 12+ 需要）
app.on('web-contents-created', (event, contents) => {
  contents.session.on('select-serial-port', (event, portList, webContents, callback) => {
    event.preventDefault();

    // 如果有可用端口，选择第一个（用户在网页中已通过 requestPort() 选择）
    if (portList && portList.length > 0) {
      callback(portList[0].portId);
    } else {
      callback(''); // 取消
    }
  });

  contents.session.on('serial-port-added', (event, port) => {
    console.log('Serial port added:', port);
  });

  contents.session.on('serial-port-removed', (event, port) => {
    console.log('Serial port removed:', port);
  });

  // 设置 Serial API 权限检查回调
  contents.session.setPermissionCheckHandler((webContents, permission, requestingOrigin, details) => {
    if (permission === 'serial') {
      return true; // 允许 Serial API
    }
    return false;
  });

  contents.session.setDevicePermissionHandler((details) => {
    if (details.deviceType === 'serial') {
      return true; // 允许串口设备访问
    }
    return false;
  });
});

console.log('CodeBuddy Flasher Electron App 启动中...');
console.log('资源目录:', __dirname);
