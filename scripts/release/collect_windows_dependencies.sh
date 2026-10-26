#!/usr/bin/env bash
# Run after staging the DLL closure. Preserve exact MSYS2 sources and notices.
set -euo pipefail
runtime="$(cygpath -u "$1")"
destination="$(cygpath -u "$2")"
mkdir -p "$destination/archives" "$destination/notices" "$destination/metadata"
packages=()
while IFS= read -r dll; do
    dll="${dll//$'\r'/}"
    [[ -z "$dll" ]] && continue
    packages+=("$(pacman -Qqo "/ucrt64/bin/$dll")")
done < "$runtime/dependency-dlls.txt"
printf '%s\n' "${packages[@]}" | sort -u > "$destination/metadata/packages.txt"
while IFS= read -r package; do
    [[ -z "$package" ]] && continue
    version="$(pacman -Q "$package" | cut -d' ' -f2)"
    binary_url="$(pacman -Sp --print-format '%l' --nodeps "$package=$version")"
    binary_archive="$destination/metadata/$package.pkg.tar.zst"
    curl --fail --location --retry 4 --retry-all-errors "$binary_url" -o "$binary_archive"
    bsdtar -xOf "$binary_archive" .PKGINFO > "$destination/metadata/$package.PKGINFO"
    package_base="$(sed -n 's/^pkgbase = //p' "$destination/metadata/$package.PKGINFO")"
    [[ "$package_base" =~ ^[a-zA-Z0-9+_.-]+$ ]] || { echo "Invalid package base"; exit 1; }
    archive="$package_base-${version#*:}.src.tar.zst"
    source_url="https://repo.msys2.org/mingw/sources/$archive"
    if [[ ! -f "$destination/archives/$archive" ]]; then
        curl --fail --location --retry 4 --retry-all-errors "$source_url" -o "$destination/archives/$archive"
        curl --fail --location --retry 4 --retry-all-errors "$source_url.sig" -o "$destination/archives/$archive.sig"
        pacman-key --verify "$destination/archives/$archive.sig"
    fi
    printf '%s\t%s\t%s\n' "$package" "$version" "$source_url" >> "$destination/metadata/dependencies.tsv"
    # Copy license files from the actual installed package, preserving paths.
    while IFS= read -r notice; do
        [[ -f "$notice" ]] || continue
        target="$destination/notices/$package/${notice#/}"
        mkdir -p "$(dirname "$target")"
        cp "$notice" "$target"
    done < <(pacman -Qlq "$package" | grep '/share/licenses/' || true)
    rm "$binary_archive"
done < "$destination/metadata/packages.txt"
(cd "$destination" && sha256sum archives/* > SHA256SUMS)
