#!/usr/bin/env python3
import sys

def convert_newlines(input_file, output_file=None):
    """
    将文件中的\n换行符转换为\r\n
    :param input_file: 输入文件路径
    :param output_file: 输出文件路径(默认覆盖原文件)
    """
    if output_file is None:
        output_file = input_file
    
    try:
        # 读取文件内容
        with open(input_file, 'rb') as f:
            content = f.read()
        
        # 替换换行符
        content = content.replace(b'\n', b'\r\n')
        
        # 写入文件
        with open(output_file, 'wb') as f:
            f.write(content)
        
        print(f"成功转换文件: {input_file} -> {output_file}")
    
    except Exception as e:
        print(f"处理文件时出错: {e}")
        sys.exit(1)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"用法: {sys.argv[0]} <输入文件> [输出文件]")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else None
    
    convert_newlines(input_file, output_file)