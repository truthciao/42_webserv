#!/usr/bin/env python3
import sys

sys.stderr.write("This is an error on stderr\n")
sys.stderr.flush()

print("Content-Type: text/plain\r")
print("\r")
print("Body after stderr output\r")ddddd
