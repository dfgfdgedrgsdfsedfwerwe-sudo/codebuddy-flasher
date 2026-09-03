# 串口协调机制设计文档

**日期**: 2026-09-02  
**项目**: CodeBuddy AI BOX PC 工具  
**版本**: 1.0

---

## 问题背景

当前 PC 端有两个工具通过同一串口（COM6）与 Dongle 通信：
1. **realtime_monitor.py** — 持续推送状态数据（Token/Git/AI）
2. **ask_user_via_box.py** — 按需发送决策请求

**核心冲突**：Windows 串口是独占资源，同一时刻只能一个进程打开。当监控脚本持有 COM6 时，决策工具无法打开串口，导致 `PermissionError(13, '拒绝访问')`。

**已验证现象**：
- 监控运行时，决策工具报错无法访问串口
- 停止监控后，决策工具可正常工作
- 决策界面本身功能完整（60 秒超时，中文显示，触摸确认）

---

## 设计目标

1. **无冲突**：决策工具启动时，监控自动释放串口
2. **自动恢复**：决策完成后，监控自动重新接管串口
3. **容错性**：处理崩溃、超时、多实例等异常情况
4. **透明性**：用户无需手动停止/启动监控脚本

---

## 设计方案

### 核心机制：文件信号协调

通过文件系统作为进程间通信（IPC）的信号通道：

```
pc_tools/.runtime/
├── monitor.pid      # 监控进程 PID（监控启动时创建）
└── pause.flag       # 暂停信号（决策工具创建，内容=决策进程 PID）
```

**工作流程**：
1. 监控脚本启动时，写入 `monitor.pid`，持有 COM6
2. 决策工具启动时，检测到 `monitor.pid` 存在，写入 `pause.flag`
3. 监控脚本检测到 `pause.flag`，关闭 COM6，进入等待循环
4. 决策工具打开 COM6，执行决策交互
5. 决策完成后，删除 `pause.flag`
6. 监控脚本检测到 flag 消失，重新打开 COM6 继续推送

---

## 详细设计

### 1. 监控脚本改动（realtime_monitor.py）

#### 启动阶段
```python
runtime_dir = Path(__file__).parent / '.runtime'
runtime_dir.mkdir(exist_ok=True)
pid_file = runtime_dir / 'monitor.pid'
pid_file.write_text(str(os.getpid()))
```

#### 主循环检查
```python
def check_pause_signal():
    pause_flag = runtime_dir / 'pause.flag'
    if not pause_flag.exists():
        return False
    
    # 检查 flag 内 PID 是否有效
    try:
        paused_by_pid = int(pause_flag.read_text().strip())
        if not psutil.pid_exists(paused_by_pid):
            # 决策进程已死，清理残留 flag
            pause_flag.unlink()
            return False
    except:
        pass
    
    return True

def wait_for_resume(ser):
    """释放串口，等待 pause.flag 消失"""
    ser.close()
    print("[监控] 检测到决策请求，暂停状态推送...")
    
    start = time.time()
    while time.time() - start < 120:  # 超时 120 秒
        time.sleep(0.5)
        if not (runtime_dir / 'pause.flag').exists():
            print("[监控] 恢复状态推送")
            return serial.Serial(port, 115200, timeout=0.5)
    
    # 超时强制恢复
    print("[监控] 暂停超时，强制恢复")
    (runtime_dir / 'pause.flag').unlink(missing_ok=True)
    return serial.Serial(port, 115200, timeout=0.5)
```

#### 主循环集成
```python
while True:
    if check_pause_signal():
        ser = wait_for_resume(ser)
    
    # 正常推送逻辑
    send_status_frames(ser)
    time.sleep(interval)
```

#### 退出清理
```python
def cleanup():
    pid_file.unlink(missing_ok=True)

import atexit
atexit.register(cleanup)
```

---

### 2. 决策工具改动（ask_user_via_box.py）

#### 启动前协调
```python
def request_serial_access(port):
    """请求串口访问权，如需要则通知监控暂停"""
    runtime_dir = Path(__file__).parent / '.runtime'
    runtime_dir.mkdir(exist_ok=True)
    
    monitor_pid_file = runtime_dir / 'monitor.pid'
    pause_flag = runtime_dir / 'pause.flag'
    
    # 检查监控是否在运行
    monitor_running = False
    if monitor_pid_file.exists():
        try:
            monitor_pid = int(monitor_pid_file.read_text().strip())
            monitor_running = psutil.pid_exists(monitor_pid)
            if not monitor_running:
                monitor_pid_file.unlink()  # 清理失效的 PID 文件
        except:
            pass
    
    if not monitor_running:
        # 监控未运行，直接使用串口
        return None
    
    # 监控在运行，请求暂停
    if pause_flag.exists():
        # 已有其他决策在等待，超时退出
        print("[决策] 检测到其他决策正在进行，等待...")
        for _ in range(60):  # 等待最多 30 秒（0.5s × 60）
            time.sleep(0.5)
            if not pause_flag.exists():
                break
        else:
            raise RuntimeError("串口繁忙，请稍后重试")
    
    # 写入暂停信号
    pause_flag.write_text(str(os.getpid()))
    time.sleep(2)  # 等待监控释放串口
    
    return pause_flag

def release_serial_access(pause_flag):
    """释放串口访问权"""
    if pause_flag and pause_flag.exists():
        pause_flag.unlink()
```

#### 集成到 ask_decision()
```python
def ask_decision(port, title, options, timeout=30):
    pause_flag = None
    try:
        pause_flag = request_serial_access(port)
        
        # 原有逻辑：打开串口、发送决策、等待回复
        ser = serial.Serial(port, 115200, timeout=0.5)
        # ... 发送和接收逻辑 ...
        
        return result
    
    except Exception as e:
        print(f"\n[错误] {e}\n")
        return None
    
    finally:
        release_serial_access(pause_flag)
```

---

## 错误处理

### 场景 1：决策工具崩溃，pause.flag 残留
**监控端处理**：
- `check_pause_signal()` 检查 flag 内 PID 是否存活
- 进程已死 → 自动删除 flag，无需等待
- 超时 120 秒 → 强制删除 flag 并恢复

### 场景 2：监控脚本未运行
**决策端处理**：
- 检测 `monitor.pid` 不存在或进程已死
- 不创建 `pause.flag`，直接使用串口
- 正常完成后退出

### 场景 3：多个决策同时启动
**决策端处理**：
- 第二个决策检测到 `pause.flag` 已存在
- 等待最多 30 秒（0.5s 轮询）
- 超时则报错 `RuntimeError("串口繁忙，请稍后重试")`

### 场景 4：监控正在推送时决策到达
**时序保证**：
- 决策写入 `pause.flag` 后等待 2 秒
- 监控在每次推送**前**检查 flag（不是推送中）
- 最坏情况：决策多等一个推送周期（60 秒）

---

## 性能影响

| 指标 | 改动前 | 改动后 | 影响 |
|------|--------|--------|------|
| 监控推送延迟 | 0 | +10ms（文件检查） | 可忽略 |
| 决策启动延迟 | 失败 | +2 秒（等待监控释放） | 可接受 |
| CPU 开销 | 低 | 低（0.5s 轮询） | 无明显增加 |
| 磁盘 I/O | 无 | 极少（两个小文件） | 可忽略 |

---

## 用户体验变化

**改动前**（手动协调）：
```bash
# 1. 停止监控
Get-Process python | Stop-Process

# 2. 运行决策
python ask_user_via_box.py COM6 "问题" "选项1" "选项2"

# 3. 重启监控
python realtime_monitor.py COM6 ../.. 60 &
```

**改动后**（自动协调）：
```bash
# 直接运行决策，监控自动暂停+恢复
python ask_user_via_box.py COM6 "问题" "选项1" "选项2"
```

**透明性**：用户完全不需要关心监控脚本状态。

---

## 测试验证清单

### 正常流程
- [ ] 监控运行中，启动决策 → 监控暂停 → 决策完成 → 监控恢复
- [ ] 监控未运行，启动决策 → 决策直接执行
- [ ] 决策完成后，监控串口重连成功，继续推送

### 异常流程
- [ ] 决策工具崩溃（Ctrl+C）→ 监控检测到 PID 失效，自动恢复
- [ ] 决策超时（60s+）→ 监控超时强制恢复（120s）
- [ ] 两个决策同时启动 → 第二个等待第一个完成
- [ ] 监控推送中途收到暂停信号 → 下一轮循环前检查并暂停

### 边界情况
- [ ] `.runtime/` 目录不存在 → 自动创建
- [ ] `pause.flag` 内容损坏 → 监控忽略并清理
- [ ] 监控重启（`monitor.pid` 变化）→ 旧 PID 文件被覆盖

---

## 实现优先级

1. **P0（核心功能）**：监控暂停/恢复机制
2. **P0（核心功能）**：决策请求/释放串口
3. **P1（容错）**：崩溃清理（PID 检查）
4. **P1（容错）**：超时强制恢复
5. **P2（优化）**：多实例排队等待

---

## 依赖库

新增依赖（需要更新 `pc_tools/README.md`）：
```bash
pip install pyserial psutil
```

`psutil` 用于进程存活检查（`psutil.pid_exists(pid)`）。

---

## 后续优化方向

1. **日志记录**：暂停/恢复事件写入 `.runtime/coordination.log`
2. **可视化**：监控脚本显示当前状态（运行中/已暂停）
3. **配置化**：超时时间可通过配置文件调整
4. **多串口支持**：不同串口的监控和决策独立协调

---

## 总结

通过文件信号协调机制，实现了监控脚本和决策工具的无冲突共存。核心优势：
- ✅ 用户透明：无需手动停止/启动监控
- ✅ 自动恢复：决策完成后监控自动接管
- ✅ 容错性强：处理崩溃、超时、多实例
- ✅ 性能影响小：文件检查开销可忽略

实现改动集中在两个文件的启动和清理逻辑，不影响核心业务功能。
