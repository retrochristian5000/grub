#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
FRONTEND="$ROOT/build-aux/whp-configure-toolchain"
SCRATCH="$(mktemp -d "${TMPDIR:-/tmp}/grub-whp-llvm.XXXXXX")"
trap 'rm -rf "$SCRATCH"' EXIT
mkdir -p "$SCRATCH/src/build-aux"
cp "$FRONTEND" "$SCRATCH/src/build-aux/whp-configure-toolchain"

cat > "$SCRATCH/src/configure" <<'CONFIGURE'
#!/usr/bin/env bash
set -euo pipefail
{
    printf 'GRUB_TARGET_TOOLCHAIN=%s\n' "${GRUB_TARGET_TOOLCHAIN-}"
    printf 'TARGET_CC=%s\n' "${TARGET_CC-}"
    printf 'TARGET_OBJCOPY=%s\n' "${TARGET_OBJCOPY-}"
    printf 'TARGET_STRIP=%s\n' "${TARGET_STRIP-}"
    printf 'TARGET_NM=%s\n' "${TARGET_NM-}"
    printf 'TARGET_RANLIB=%s\n' "${TARGET_RANLIB-}"
    printf 'TARGET_CFLAGS=%s\n' "${TARGET_CFLAGS-}"
    printf 'TARGET_CPPFLAGS=%s\n' "${TARGET_CPPFLAGS-}"
    printf 'TARGET_CCASFLAGS=%s\n' "${TARGET_CCASFLAGS-}"
    printf 'TARGET_LDFLAGS=%s\n' "${TARGET_LDFLAGS-}"
    printf '%s\n' '---ARGS---'
    printf '%s\n' "$@"
} > "${WHP_TOOLCHAIN_LOG:?}"
CONFIGURE
chmod +x "$SCRATCH/src/configure"

log="$SCRATCH/llvm.log"
WHP_TOOLCHAIN_LOG="$log" \
TARGET_NM=/whp/custom/llvm-nm \
TARGET_CFLAGS='-Os' \
    "$SCRATCH/src/build-aux/whp-configure-toolchain" \
        --with-target-toolchain=llvm \
        --target=i386-none-elf \
        --with-platform=efi

grep -Fxq 'GRUB_TARGET_TOOLCHAIN=llvm' "$log"
grep -Fxq 'TARGET_CC=clang' "$log"
grep -Fxq 'TARGET_OBJCOPY=llvm-objcopy' "$log"
grep -Fxq 'TARGET_STRIP=llvm-strip' "$log"
grep -Fxq 'TARGET_NM=/whp/custom/llvm-nm' "$log"
grep -Fxq 'TARGET_RANLIB=llvm-ranlib' "$log"
grep -Fxq 'TARGET_CFLAGS=-Os --target=i386-none-elf' "$log"
grep -Fxq 'TARGET_CPPFLAGS=--target=i386-none-elf' "$log"
grep -Fxq 'TARGET_CCASFLAGS=--target=i386-none-elf' "$log"
grep -Fxq 'TARGET_LDFLAGS=--target=i386-none-elf -fuse-ld=lld' "$log"
grep -Fxq -- '--target=i386-none-elf' "$log"
grep -Fxq -- '--with-platform=efi' "$log"
if grep -Fq -- '--with-target-toolchain=' "$log"; then
    printf 'toolchain selector leaked into generated configure arguments\n' >&2
    exit 1
fi

log="$SCRATCH/override.log"
WHP_TOOLCHAIN_LOG="$log" \
TARGET_CC=/whp/bin/clang \
TARGET_OBJCOPY=/whp/bin/llvm-objcopy \
TARGET_STRIP=/whp/bin/llvm-strip \
TARGET_NM=/whp/bin/llvm-nm \
TARGET_RANLIB=/whp/bin/llvm-ranlib \
TARGET_CFLAGS='--target=i386-none-elf -Os' \
TARGET_CPPFLAGS='--target=i386-none-elf' \
TARGET_CCASFLAGS='--target=i386-none-elf' \
TARGET_LDFLAGS='--target=i386-none-elf -fuse-ld=/whp/bin/ld.lld' \
    "$SCRATCH/src/build-aux/whp-configure-toolchain" \
        --with-target-toolchain=llvm \
        --target=i386-none-elf

grep -Fxq 'TARGET_CC=/whp/bin/clang' "$log"
grep -Fxq 'TARGET_OBJCOPY=/whp/bin/llvm-objcopy' "$log"
grep -Fxq 'TARGET_STRIP=/whp/bin/llvm-strip' "$log"
grep -Fxq 'TARGET_NM=/whp/bin/llvm-nm' "$log"
grep -Fxq 'TARGET_RANLIB=/whp/bin/llvm-ranlib' "$log"
grep -Fxq 'TARGET_CFLAGS=--target=i386-none-elf -Os' "$log"
grep -Fxq 'TARGET_LDFLAGS=--target=i386-none-elf -fuse-ld=/whp/bin/ld.lld' "$log"

log="$SCRATCH/gnu.log"
WHP_TOOLCHAIN_LOG="$log" TARGET_CC=gcc \
    "$SCRATCH/src/build-aux/whp-configure-toolchain" \
        --with-target-toolchain=gnu \
        --target=i386-none-elf

grep -Fxq 'GRUB_TARGET_TOOLCHAIN=gnu' "$log"
grep -Fxq 'TARGET_CC=gcc' "$log"
grep -Fxq 'TARGET_OBJCOPY=' "$log"
grep -Fxq 'TARGET_LDFLAGS=' "$log"

if WHP_TOOLCHAIN_LOG="$SCRATCH/bad.log" \
   "$SCRATCH/src/build-aux/whp-configure-toolchain" \
       --with-target-toolchain=bogus >/dev/null 2>&1; then
    printf 'invalid target toolchain was accepted\n' >&2
    exit 1
fi

if grep -Fq -- '-Wno-error=unknown-warning-option' "$FRONTEND"; then
    printf 'LLVM frontend must not suppress unknown warning diagnostics\n' >&2
    exit 1
fi

printf 'WHP GRUB LLVM/GNU target-toolchain frontend: verified\n'
