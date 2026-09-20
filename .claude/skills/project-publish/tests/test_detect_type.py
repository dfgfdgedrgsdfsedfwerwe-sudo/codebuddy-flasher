#!/usr/bin/env python3
import pytest
from pathlib import Path
from scripts.detect_type import detect_project_type

def test_detect_firmware_platformio(tmp_path):
    (tmp_path / 'platformio.ini').touch()
    assert detect_project_type(str(tmp_path)) == 'firmware'

def test_detect_firmware_espidf(tmp_path):
    (tmp_path / 'sdkconfig').touch()
    assert detect_project_type(str(tmp_path)) == 'firmware'

def test_detect_web(tmp_path):
    (tmp_path / 'package.json').touch()
    assert detect_project_type(str(tmp_path)) == 'web'

def test_detect_android(tmp_path):
    (tmp_path / 'build.gradle').touch()
    assert detect_project_type(str(tmp_path)) == 'android'

def test_detect_windows_sln(tmp_path):
    (tmp_path / 'project.sln').touch()
    assert detect_project_type(str(tmp_path)) == 'windows'

def test_detect_windows_vcxproj(tmp_path):
    (tmp_path / 'project.vcxproj').touch()
    assert detect_project_type(str(tmp_path)) == 'windows'

def test_unknown_project(tmp_path):
    with pytest.raises(ValueError, match="Cannot determine project type"):
        detect_project_type(str(tmp_path))

def test_nonexistent_dir():
    with pytest.raises(ValueError, match="does not exist"):
        detect_project_type('/nonexistent/dir')
