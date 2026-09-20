#!/usr/bin/env python3
"""配置管理器 - 持久化存储应用配置"""
import json
from pathlib import Path
from typing import Any, Dict

DEFAULT_CONFIG = {
    "serial_port": "COM6",
    "ipc_port": 47100,
    "push_interval": 30,
    "auto_start": False,
    "hooks_installed": False,
    "log_level": "INFO",
}


class ConfigManager:
    """配置管理器 - 读写 ~/.codebuddy_bridge/config.json"""

    def __init__(self):
        self.config_dir = Path.home() / ".codebuddy_bridge"
        self.config_file = self.config_dir / "config.json"
        self._config: Dict[str, Any] = {}
        self._load()

    def _load(self):
        """从文件加载配置，文件不存在则使用默认值"""
        if self.config_file.exists():
            try:
                with open(self.config_file, 'r', encoding='utf-8') as f:
                    self._config = json.load(f)
                # 合并默认值（处理新增配置项）
                for key, value in DEFAULT_CONFIG.items():
                    if key not in self._config:
                        self._config[key] = value
            except Exception as e:
                print(f"Failed to load config: {e}, using defaults")
                self._config = DEFAULT_CONFIG.copy()
        else:
            self._config = DEFAULT_CONFIG.copy()
            self._save()

    def _save(self):
        """保存配置到文件"""
        self.config_dir.mkdir(parents=True, exist_ok=True)
        try:
            with open(self.config_file, 'w', encoding='utf-8') as f:
                json.dump(self._config, f, indent=2, ensure_ascii=False)
        except Exception as e:
            print(f"Failed to save config: {e}")

    def get(self, key: str, default: Any = None) -> Any:
        """获取配置值"""
        return self._config.get(key, default)

    def set(self, key: str, value: Any):
        """设置配置值并保存"""
        self._config[key] = value
        self._save()

    def get_all(self) -> Dict[str, Any]:
        """获取所有配置"""
        return self._config.copy()

    def reset(self):
        """重置为默认配置"""
        self._config = DEFAULT_CONFIG.copy()
        self._save()
