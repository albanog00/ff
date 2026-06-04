# ff

`ff` is a fast and small C++ file crawler that searches file and directory paths using PCRE2 regular expressions.

It walks one or more directories concurrently and prints matching paths. When output is a terminal, matches are highlighted.

## Features

- PCRE2 regex matching
- Concurrent directory traversal
- File/directory type filtering
- Arena-backed string pool allocations

## Usage

```sh
ff <pattern> <path...> [options]
```

Examples:

```sh
ff "main" .
ff "\\.cpp$" src
ff "test" . --hidden
ff "cache|build" . --type=dir
ff "\\.h$" . --type=file --max-depth=3
```

## Options

| Option | Description |
| --- | --- |
| `-h`, `--help` | Show help |
| `-d=<n>`, `--max-depth=<n>` | Limit directory traversal depth |
| `-H`, `--hidden` | Include hidden files and directories |
| `-t=<types>`, `--type=<types>` | Filter by type: `file`, `f`, `directory`, `dir`, `d` |

Multiple types can be comma-separated:

```sh
ff "foo" . --type=file,dir
```

## Build

Requirements:

- C++23 compiler
- CMake 3.22+
- PCRE2
- spdlog

Build release:

```sh
make release
```

Build debug:

```sh
make debug
```

Install:

```sh
make install
```

## Nix

This repository includes a Nix flake.

Enter the development shell:

```sh
nix develop
```
