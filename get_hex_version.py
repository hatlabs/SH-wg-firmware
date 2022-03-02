#!/usr/bin/env python3

import sys
import semver

def main():
    input_ = "\n".join(sys.stdin.readlines())
    ver = semver.VersionInfo.parse(input_)
    if ver.prerelease:
        prerelease_num = int(ver.prerelease.split('.')[1])
        print(f"{ver.major:02x}{ver.minor:02x}{ver.patch:02x}{prerelease_num:02x}")
    else:
        print(f"{ver.major:02x}{ver.minor:02x}{ver.patch:02x}ff")

if __name__ == '__main__':
    main()
