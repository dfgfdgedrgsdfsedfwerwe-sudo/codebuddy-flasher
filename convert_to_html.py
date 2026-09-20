import markdown

# 读取 markdown
with open('设计指南.md', 'r', encoding='utf-8') as f:
    md_content = f.read()

# 转换为 HTML（启用 fenced_code 扩展）
html_body = markdown.markdown(
    md_content,
    extensions=[
        'markdown.extensions.tables',
        'markdown.extensions.fenced_code',
        'markdown.extensions.toc',
        'markdown.extensions.nl2br'
    ]
)

# 深色主题样式（参考项目报告）
html_template = f"""<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>DFRobot UNIHIKER K10 开发设计指南</title>
    <style>
        * {{
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }}

        body {{
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", "Microsoft YaHei", "PingFang SC", sans-serif;
            line-height: 1.8;
            background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
            color: #e0e0e0;
            padding: 20px;
        }}

        .container {{
            max-width: 1200px;
            margin: 0 auto;
            background: rgba(30, 30, 46, 0.95);
            padding: 60px;
            border-radius: 16px;
            box-shadow: 0 20px 60px rgba(0, 0, 0, 0.5);
        }}

        h1 {{
            color: #00d4ff;
            font-size: 2.8em;
            margin-bottom: 0.3em;
            text-align: center;
            text-shadow: 0 0 20px rgba(0, 212, 255, 0.5);
            border-bottom: 3px solid #00d4ff;
            padding-bottom: 20px;
        }}

        h2 {{
            color: #00d4ff;
            font-size: 2em;
            margin-top: 2em;
            margin-bottom: 1em;
            padding-left: 15px;
            border-left: 5px solid #00d4ff;
        }}

        h3 {{
            color: #5fb3f6;
            font-size: 1.5em;
            margin-top: 1.5em;
            margin-bottom: 0.8em;
        }}

        h4 {{
            color: #7ec8f8;
            font-size: 1.2em;
            margin-top: 1.2em;
            margin-bottom: 0.6em;
        }}

        p {{
            margin-bottom: 1em;
            color: #d0d0d0;
        }}

        blockquote {{
            background: rgba(0, 212, 255, 0.1);
            border-left: 4px solid #00d4ff;
            padding: 15px 20px;
            margin: 20px 0;
            border-radius: 4px;
            font-style: italic;
            color: #a8daff;
        }}

        code {{
            background: rgba(0, 0, 0, 0.4);
            color: #ff79c6;
            padding: 2px 6px;
            border-radius: 3px;
            font-family: "Consolas", "Monaco", monospace;
            font-size: 0.9em;
        }}

        pre {{
            background: #1e1e2e;
            border: 1px solid #3a3a4e;
            border-radius: 8px;
            padding: 20px;
            overflow-x: auto;
            margin: 20px 0;
            box-shadow: inset 0 2px 10px rgba(0, 0, 0, 0.3);
        }}

        pre code {{
            background: none;
            color: #f8f8f2;
            padding: 0;
            font-size: 0.95em;
            line-height: 1.6;
        }}

        table {{
            width: 100%;
            border-collapse: collapse;
            margin: 25px 0;
            background: rgba(20, 20, 30, 0.6);
            border-radius: 8px;
            overflow: hidden;
        }}

        th {{
            background: linear-gradient(135deg, #00d4ff 0%, #0099cc 100%);
            color: #1a1a2e;
            font-weight: 600;
            padding: 15px;
            text-align: left;
        }}

        td {{
            padding: 12px 15px;
            border-bottom: 1px solid rgba(255, 255, 255, 0.1);
            color: #d0d0d0;
        }}

        tr:hover {{
            background: rgba(0, 212, 255, 0.05);
        }}

        tr:last-child td {{
            border-bottom: none;
        }}

        ul, ol {{
            margin-left: 30px;
            margin-bottom: 1em;
        }}

        li {{
            margin-bottom: 0.5em;
            color: #d0d0d0;
        }}

        strong {{
            color: #ffeb3b;
            font-weight: 600;
        }}

        a {{
            color: #00d4ff;
            text-decoration: none;
            border-bottom: 1px solid transparent;
            transition: all 0.3s;
        }}

        a:hover {{
            border-bottom-color: #00d4ff;
        }}

        hr {{
            border: none;
            height: 2px;
            background: linear-gradient(90deg, transparent, #00d4ff, transparent);
            margin: 40px 0;
        }}

        .toc {{
            background: rgba(0, 212, 255, 0.05);
            border: 2px solid #00d4ff;
            border-radius: 8px;
            padding: 25px;
            margin: 30px 0;
        }}

        .toc ul {{
            list-style: none;
            margin-left: 0;
        }}

        .toc li {{
            margin-bottom: 8px;
        }}

        .toc a {{
            color: #5fb3f6;
        }}

        @media (max-width: 768px) {{
            .container {{
                padding: 30px 20px;
            }}

            h1 {{
                font-size: 2em;
            }}

            h2 {{
                font-size: 1.5em;
            }}

            table {{
                font-size: 0.9em;
            }}
        }}

        @media print {{
            body {{
                background: white;
                color: black;
            }}

            .container {{
                background: white;
                box-shadow: none;
            }}

            h1, h2, h3 {{
                color: #333;
            }}
        }}
    </style>
</head>
<body>
    <div class="container">
        {html_body}
    </div>
</body>
</html>"""

# 写入 HTML 文件
with open('设计指南.html', 'w', encoding='utf-8') as f:
    f.write(html_template)

print("HTML 导出成功: 设计指南.html")
