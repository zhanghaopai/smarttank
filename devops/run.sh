#!/bin/bash
set -euo pipefail

# Share the Windows build and runtime setup with the PowerShell entry point.
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
script_path="$script_dir/run.ps1"
arguments=()
while (($#)); do
    case "$1" in
        --example|-Example) arguments+=(-Example) ;;
        --preview|-Preview) arguments+=(-Preview) ;;
        --skip-build|-SkipBuild) arguments+=(-SkipBuild) ;;
        --no-launch|-NoLaunch) arguments+=(-NoLaunch) ;;
        -h|--help)
            printf '%s\n' \
                'Usage: bash devops/run.sh [options] [-- application arguments]' \
                '  --example     Load example dimensions' \
                '  --preview     Use the separate preview build' \
                '  --skip-build  Launch an existing build' \
                '  --no-launch   Configure, build and test only' \
                'Requires Windows PowerShell, Windows CMake/CTest and MSVC.'
            exit 0
            ;;
        --) shift; arguments+=("$@"); break ;;
        *) printf 'Unknown option: %s (use --help)\n' "$1" >&2; exit 2 ;;
    esac
    shift
done

if command -v cygpath >/dev/null 2>&1; then
    script_path="$(cygpath -w "$script_path")"
elif command -v wslpath >/dev/null 2>&1; then
    script_path="$(wslpath -w "$script_path")"
fi

if ! command -v powershell.exe >/dev/null 2>&1; then
    printf '%s\n' 'Windows PowerShell (powershell.exe) is required. Use Git Bash or WSL with Windows interoperability enabled; native Linux/macOS builds are not supported.' >&2
    exit 1
fi

# Explicit path conversion above avoids MSYS rewriting application arguments.
export MSYS2_ARG_CONV_EXCL='*'
exec powershell.exe -NoProfile -File "$script_path" "${arguments[@]}"
