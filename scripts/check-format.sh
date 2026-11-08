#!/usr/bin/env bash
set -euo pipefail

CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"

files=$(git ls-files '*.cpp' '*.hpp' '*.h' '*.cc' '*.cxx')

if [[ -z "$files" ]]; then
    echo "No source files found."
    exit 0
fi

echo "Checking formatting..."

while IFS= read -r file; do
    "$CLANG_FORMAT" --dry-run --Werror "$file"
done <<< "$files"

echo "Formatting check passed."
