#!/bin/bash
# Validate every shipped Mach-O, not only the app's advertised minimum version.
set -euo pipefail
app="${1:?usage: verify-compatibility.sh app-bundle}"
minimum=$(/usr/libexec/PlistBuddy -c 'Print :LSMinimumSystemVersion' "$app/Contents/Info.plist")
if [[ "$minimum" != "12.0" ]]; then
    echo "Unexpected advertised minimum: $minimum" >&2
    exit 1
fi
count=0
while IFS= read -r -d '' binary; do
    description=$(file -b "$binary")
    [[ "$description" == *Mach-O* ]] || continue
    lipo "$binary" -verify_arch arm64 x86_64
    versions=$(otool -l "$binary" | awk '/^[[:space:]]*minos / {print $2} /cmd LC_VERSION_MIN_MACOSX/ {legacy=1; next} legacy && /^[[:space:]]*version / {print $2; legacy=0}')
    if [[ -z "$versions" ]]; then
        echo "Missing minimum OS metadata: $binary" >&2
        exit 1
    fi
    while IFS= read -r version; do
        if ! awk -v version="$version" 'BEGIN {split(version,v,"."); exit !(v[1]<12 || (v[1]==12 && v[2]==0 && v[3]+0==0))}'; then
            echo "Requires macOS $version: $binary" >&2
            exit 1
        fi
    done <<< "$versions"
    count=$((count + 1))
done < <(find "$app/Contents" -type f -print0)
[[ "$count" -gt 0 ]] || { echo 'No Mach-O binaries found' >&2; exit 1; }
echo "Verified $count Mach-O files: arm64 + x86_64, minimum OS <= macOS 12.0"
