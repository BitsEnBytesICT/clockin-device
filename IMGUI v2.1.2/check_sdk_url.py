#!/usr/bin/env python3
"""Check if STM32MP1 SDK can be downloaded directly from st.com"""
import urllib.request
import re

url = 'https://www.st.com/en/embedded-software/stm32mp1dev.html'
req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64)'})
try:
    html = urllib.request.urlopen(req, timeout=20).read().decode('utf-8', errors='ignore')

    # Look for JSON data that might contain download URLs
    patterns = [
        r'https?://[^\s"\']*SDK[^\s"\']*\.tar\.gz',
        r'https?://[^\s"\']*stm32mp1[^\s"\']*\.tar\.gz',
        r'"downloadUrl"\s*:\s*"([^"]+)"',
        r'"fileUrl"\s*:\s*"([^"]+)"',
        r'download-url[^"]*"([^"]+)"',
    ]

    for p in patterns:
        matches = re.findall(p, html, re.IGNORECASE)
        for m in matches[:5]:
            print(f'Found: {m}')

    if not any(re.findall(p, html, re.IGNORECASE) for p in patterns):
        print('No downloadable SDK URLs found in page')
        print('SDK requires login at: https://www.st.com/en/embedded-software/stm32mp1dev.html#get-software')

except Exception as e:
    print(f'Error: {e}')
