#!/usr/bin/env python3
import os
import sys

# CGI must send HTTP headers before the response body.
print("Content-Type: text/html\r")
print("\r")

print("""<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>CGI Environment Variables</title>
  <style>
    body { font-family: monospace; background: #1e1e1e; color: #d4d4d4; padding: 20px; }
    h2   { color: #569cd6; }
    table{ border-collapse: collapse; width: 100%; }
    th   { background: #2d2d2d; color: #9cdcfe; text-align: left; padding: 8px 12px; }
    td   { padding: 6px 12px; border-bottom: 1px solid #333; vertical-align: top; }
    tr:hover td { background: #2a2a2a; }
    .key { color: #4ec9b0; white-space: nowrap; }
    .val { color: #ce9178; word-break: break-all; }
    .section { margin-top: 30px; }
  </style>
</head>
<body>
<h2>CGI Environment Variables</h2>
""")

# Standard CGI environment variable group.
CGI_VARS = [
    "REQUEST_METHOD",
    "REQUEST_URI",
    "QUERY_STRING",
    "CONTENT_TYPE",
    "CONTENT_LENGTH",
    "SCRIPT_NAME",
    "SCRIPT_FILENAME",
    "PATH_INFO",
    "PATH_TRANSLATED",
    "SERVER_NAME",
    "SERVER_PORT",
    "SERVER_PROTOCOL",
    "SERVER_SOFTWARE",
    "GATEWAY_INTERFACE",
    "REMOTE_ADDR",
    "REMOTE_PORT",
    "REMOTE_HOST",
    "REMOTE_USER",
    "AUTH_TYPE",
    "HTTP_HOST",
    "HTTP_USER_AGENT",
    "HTTP_ACCEPT",
    "HTTP_ACCEPT_LANGUAGE",
    "HTTP_ACCEPT_ENCODING",
    "HTTP_CONNECTION",
    "HTTP_COOKIE",
    "HTTP_REFERER",
    "REDIRECT_STATUS",
]

def row(key, val):
    val = val if val != "" else "<em style='color:#555'>(empty)</em>"
    print(f'<tr><td class="key">{key}</td><td class="val">{val}</td></tr>')

# 1. Standard CGI variables.
print('<div class="section">')
print('<h3 style="color:#dcdcaa">Standard CGI Variables</h3>')
print('<table><tr><th>Variable</th><th>Value</th></tr>')
for key in CGI_VARS:
    row(key, os.environ.get(key, "<em style='color:#555'>(not set)</em>"))
print('</table></div>')

# 2. All other environment variables.
extra = {k: v for k, v in os.environ.items() if k not in CGI_VARS}
print('<div class="section">')
print('<h3 style="color:#dcdcaa">Other Environment Variables</h3>')
print('<table><tr><th>Variable</th><th>Value</th></tr>')
for key in sorted(extra.keys()):
    row(key, extra[key])
print('</table></div>')

# 3. Request body, if present.
print('<div class="section">')
print('<h3 style="color:#dcdcaa">Request Body</h3>')
content_length = int(os.environ.get("CONTENT_LENGTH") or 0)
if content_length > 0:
    body = sys.stdin.buffer.read(content_length)
    print(f'<pre style="background:#2d2d2d;padding:12px;border-radius:4px;">{body.decode("utf-8", errors="replace")}</pre>')
else:
    print('<p style="color:#555">(no body)</p>')
print('</div>')

print("</body></html>")
