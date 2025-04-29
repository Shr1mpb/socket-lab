#!/usr/bin/env python3
import os
import tarfile
import sys
from pathlib import Path

def find_autograde_tar():
    """查找当前py文件上层目录中的autograde.tar"""
    current_script = Path(__file__).resolve()  # 当前py文件的绝对路径
    parent_dir = current_script.parent.parent  # 上层目录
    
    # 在上层目录中查找autograde.tar
    tar_file = parent_dir / 'autograde.tar'
    if tar_file.exists():
        return tar_file
    return None

def extract_and_view_contents(tar_path):
    """解压并显示所有文件内容"""
    # 创建解压目录
    extract_dir = Path("autograde")
    extract_dir.mkdir(exist_ok=True)
    
    print(f"解压 {tar_path} 到 {extract_dir}")
    
    try:
        # 解压文件
        with tarfile.open(tar_path) as tar:
            tar.extractall(path=extract_dir)
        
        # 显示解压后的目录结构
        print("\n📁 解压后的目录结构:")
        for root, dirs, files in os.walk(extract_dir):
            level = root.replace(str(extract_dir), '').count(os.sep)
            indent = ' ' * 4 * level
            print(f"{indent}{os.path.basename(root)}/")
            subindent = ' ' * 4 * (level + 1)
            for f in files:
                print(f"{subindent}{f}")
        
        # 查看每个文件的内容
        print("\n📄 文件内容:")
        for root, _, files in os.walk(extract_dir):
            for file in files:
                file_path = Path(root) / file
                print(f"\n=== {file_path} ===")
                try:
                    with open(file_path, 'r', encoding='utf-8') as f:
                        print(f.read())
                except UnicodeDecodeError:
                    print("(二进制文件，无法显示文本内容)")
                except Exception as e:
                    print(f"读取文件出错: {e}")
                print("=" * 40)
                
    except Exception as e:
        print(f"处理失败: {e}", file=sys.stderr)

def main():
    tar_file = find_autograde_tar()
    
    if not tar_file:
        print("未在上层目录找到 autograde.tar")
        return
    
    extract_and_view_contents(tar_file)

if __name__ == "__main__":
    main()