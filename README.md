# CR

Run markdown codeblocks by its heading.

For example: to run code blocks under heading [ls](#ls) with arguments `-1`.

```sh
./cr ls -- -1
```

Run without arguments will give you hints of available commands.

## ls

List files

```sh
ls "$@"
```

## Supported languages

- shellscript
- awk
- javascript
- python
- ruby
- php
- batch
- powershell

To Handle unsupported languages, see [c_hello](#c_hello)

## How does this work?

1. Find makrdown file in the current and parrent dir.

   1. Find markdown file with name in `<program>.md`, `.<program>.md`, `README.md`.
   2. Find in the current dir and parrent dir.
   3. Find in case ignore.

2. Parse markdown file into nodes.
3. Find target node by heading.
4. Run codeblocks of the target node.

## Build

Build this program

```sh
program=$(basename "${PWD}")

if ! test "${CC+1}" && command -v zig >/dev/null; then
    CC="zig cc --target=$(uname -m)-linux-musl"
fi

${CC-cc} -o ${program} main.c "$@" ${LDFLAGS-}
du -ahd0 ${program}
file ${program}
```

### Release

Build static and stripped

- [x] static
- [x] stripped
- [x] opt_size

```sh
if [ ${static} -eq 1 ]; then
    LDFLAGS="${LDFLAGS+${LDFLAGS}} -static"
fi
if [ ${stripped} -eq 1 ]; then
    LDFLAGS="${LDFLAGS+${LDFLAGS}} -s"
fi

if [ ${opt_size} -eq 1 ]; then
    LDFLAGS="${LDFLAGS+${LDFLAGS}} -Os"
fi

export LDFLAGS
${MD_EXE} --file=${MD_FILE} build
```

### Install

Install this program

```sh
${MD_EXE} --file=${MD_FILE} build release
program=$(basename "${PWD}")
if command -v sudo >/dev/null; then
    sudo install "${program}" "/usr/local/bin/${program}"
elif test "${PREFIX+1}"; then
    install "${program}" "${PREFIX}/bin/${program}"
fi
```

## Env

Prefixed env

| key     | description           |
| ------- | --------------------- |
| MD_EXE  | path to `<program>`   |
| MD_FILE | path to markdown file |

You can defind env map by creating a table with header `key` and `value`:

| key     | value | description     |
| ------- | ----- | --------------- |
| heading | Env   | Current heading |

You can also define boolean env map by creating a task list:

- [x] item_1
- [ ] item_2

Print env

```sh
for env in \
    MD_EXE \
    MD_FILE \
    heading \
    item_1 \
    item_2; do

    eval echo "${env}=\${${env}}"
done
```

### Sub

Test scoped env

| key     | value |
| ------- | ----- |
| heading | sub   |

```sh
echo "sub: heading=${heading}"
```

## Benchmark

Benchmark this program

```sh
hyperfine "${MD_EXE} env" "$@"
```

## Test

Test this program

| key     | value |
| ------- | ----- |
| heading | Test  |

```sh
${MD_EXE} env
${MD_EXE} env sub
${MD_EXE} test arguments -- foo bar
echo Hello | ${MD_EXE} test stdin
echo "cr file size: $(du -ahd0 ${MD_EXE} | ${MD_EXE} test awk)"
${MD_EXE} test c_hello
```

### Arguments

```sh
echo "shellscript with arguments: $*"
```

```js
console.log(`nodejs with arguments: ${process.argv}`);
```

```python
import sys

print("python with arguments: %s" %(sys.argv))
```

### Error

Test error exit code

```sh
exit_code=$(shuf -i 1-255 -n 1)
echo "Script exits with code ${exit_code}"
exit ${exit_code}
```

### stdin

Read stdin in shellscript

```sh
echo "stdin: $(cat)"
```

### awk

Print first column in awk

```awk
{print $1}
```

### c_hello

Test C Hello World program

```sh
TMPDIR=${TMPDIR:-/tmp}
$MD_EXE -ac test c_hello_source >"${TMPDIR}/hello.c"
cc -o "${TMPDIR}/a" "${TMPDIR}/hello.c"
"${TMPDIR}/a"
```

### c_hello_source

C Hello World source code

```c
#include <stdio.h>
int main() {
    printf("Hello, World! C.\n");
    return 0;
}
```

# Others

## Reset

Reset to the initial commit

```sh
git reset --hard $(git rev-list --max-parents=0 HEAD)
git pull
```

---

Inspired by [mask](https://github.com/jacobdeichert/mask) and [xc](https://github.com/joerdav/xc).
