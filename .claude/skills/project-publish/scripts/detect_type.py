#!/usr/bin/env python3
"""Project type detector for project-publish skill."""

from pathlib import Path

def detect_project_type(project_dir: str) -> str:
    """
    Detect project type based on directory structure.

    Args:
        project_dir: Path to project directory

    Returns:
        'firmware' | 'web' | 'android' | 'windows'

    Raises:
        ValueError: If type cannot be determined
    """
    path = Path(project_dir)
    if not path.exists():
        raise ValueError(f"Project directory {project_dir} does not exist")

    # Firmware: PlatformIO or ESP-IDF
    if (path / 'platformio.ini').exists() or (path / 'sdkconfig').exists():
        return 'firmware'

    # Web: package.json
    if (path / 'package.json').exists():
        return 'web'

    # Android: build.gradle
    if (path / 'build.gradle').exists():
        return 'android'

    # Windows: .sln or .vcxproj
    if list(path.glob('*.sln')) or list(path.glob('*.vcxproj')):
        return 'windows'

    raise ValueError(f"Cannot determine project type for {project_dir}")

