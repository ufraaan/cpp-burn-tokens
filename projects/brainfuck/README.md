# brainfuck — Brainfuck Language Interpreter

<div align="center">

**Turing-Complete Esoteric Language Runtime · C++17 · Zero Dependencies**

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/ufraaan/cpp-vibe-code)
[![C++](https://img.shields.io/badge/C++-17-00599C)](https://en.cppreference.com/w/cpp/17)
[![Platform](https://img.shields.io/badge/platform-macOS-lightgrey)](https://www.apple.com/macos)
[![License](https://img.shields.io/badge/license-MIT-blue)]()

*Performance: **888 MIPS** · Tape: **30,000 cells** · Language: **8 instructions** · Zero dependencies*
</div>

---

## Table of Contents

1. [Overview](#overview)
2. [The Brainfuck Language](#the-brainfuck-language)
3. [Usage](#usage)
4. [Architecture](#architecture)
5. [Performance Benchmarks](#performance-benchmarks)
6. [Build System](#build-system)
7. [Tested Configuration](#tested-configuration)

---

## Overview

`brainfuck` is a complete, high-performance interpreter for the [Brainfuck](https://en.wikipedia.org/wiki/Brainfuck) esoteric programming language. It reads a `.bf` source file, compiles it to an internal instruction stream with pre-matched brackets, and executes it against a configurable tape of memory cells.

The interpreter supports debug mode (live tape dump), step mode (pause between instructions), and benchmark mode (instruction count and throughput measurement). All non-command characters are treated as comments, following the language specification.

---

## The Brainfuck Language

Brainfuck operates on a **tape of 30,000 byte cells** (default; configurable via `-t`). A **data pointer** starts at cell 0 and moves left or right. Eight commands control the machine:

| Command | Meaning |
|---|---|
| `>` | Move the data pointer to the right |
| `<` | Move the data pointer to the left |
| `+` | Increment the byte at the data pointer |
| `-` | Decrement the byte at the data pointer |
| `.` | Output the byte at the data pointer (as an ASCII character) |
| `,` | Accept one byte of input and store it at the data pointer |
| `[` | If the byte at the data pointer is zero, jump forward to the matching `]` |
| `]` | If the byte at the data pointer is non-zero, jump back to the matching `[` |

Any character not in `><+-.,[]` is a comment and is silently ignored. Despite only having eight commands, Brainfuck is **Turing-complete** — any computable function can be expressed in it.

### Hello World

```brainfuck
++++++++[>++++[>++>+++>+++>+<<<<-]>+>+>->>+[<]<-]>>.>---.+++++++..+++.>>.<-.<.+++.------.--------.>>+.>++.
```

When run through the interpreter:
```
$ ./brainfuck hello.bf
Hello World!
```

### Multiply: 6 × 8

```brainfuck
+++ +++
[ > ++++ ++++ <- ]
> .
```

This increments cell 0 to 6, then loops 6 times adding 8 to cell 1, and outputs the result (48 = ASCII `'0'`).

---

## Usage

```
Usage:
  ./brainfuck <file.bf>              Execute Brainfuck program
  ./brainfuck -d <file.bf>           Debug mode
  ./brainfuck -s <file.bf>           Step mode
  ./brainfuck -b <file.bf>           Benchmark mode
  ./brainfuck -t <size> <file.bf>    Set tape size (default 30000)
  ./brainfuck -v, --version          Show version
  ./brainfuck -h, --help             Show help
```

### Normal Execution

```bash
$ ./brainfuck program.bf
```

The program reads input from stdin when it encounters the `,` command. Output from the `.` command is written directly to stdout.

### Debug Mode (`-d`)

Shows the tape state, instruction pointer, current command, and accumulated output after every step:

```
$ echo "" | ./brainfuck -d multiply.bf

--- [0] IP=0 PTR=0 ---
  instr: '+'
  tape[0..6]: [  0]    0     0     0     0     0

--- [1] IP=1 PTR=0 ---
  instr: '+'
  tape[0..6]: [  1]    0     0     0     0     0
...
```

### Step Mode (`-s`)

Same as debug mode, but pauses before each instruction and waits for the Enter key:

```
$ echo "" | ./brainfuck -s multiply.bf
  [press Enter]
```

### Benchmark Mode (`-b`)

Runs the program without instruction limits and reports total instruction count, wall-clock time, and instructions per second:

```
$ ./brainfuck -b bench.bf

[benchmark] 4363503478 instructions in 4912.02 ms  (888332234 IPS)
```

### Custom Tape Size

```bash
$ ./brainfuck -t 100000 program.bf     # 100,000 cells instead of 30,000
```

---

## Architecture

The interpreter operates in two phases:

### 1. Compilation Phase

The source text is scanned linearly. Every character not in `><+-.,[]` is discarded. Brackets are matched using a stack, and a **jump table** is built that maps each `[` to its corresponding `]` and vice versa. Unmatched brackets cause a compile-time error.

```
Source: "++[>++<-]"
               ↓
Filtered: "++[>++<-]"
Jump table: [2]→[6], [6]→[2]
```

### 2. Execution Phase

The filtered instruction stream is executed in a tight loop:

| Component | Implementation |
|---|---|
| **Tape** | `std::vector<uint8_t>` (contiguous, cache-friendly) |
| **Instruction dispatch** | `switch` statement on the 8 command characters |
| **Bracket jumps** | $O(1)$ via the precomputed jump table |
| **Input** | From a user-provided string or `getchar()` from stdin |
| **Output** | Direct `putchar()`; buffered and flushed at end in debug mode |

### Safety

A configurable instruction limit (`BFOptions::max_instructions`, default 100 M) prevents infinite loops from hanging the interpreter. In benchmark mode the limit is disabled.

---

## Performance Benchmarks

Tested on **Apple M5 (3.40 GHz)**, single-threaded, `-O3` optimisation.

### Instruction Throughput

| Program | Instructions | Time | Throughput |
|---|---|---|---|
| 3-level nest (200 × 200 × 200) | 8,241,204 | 13.23 ms | **623 MIPS** |
| 4-level nest (59⁴) | 4,363,503,478 | 4.91 s | **888 MIPS** |

MIPS = Millions of Instructions Per Second.

### Scaling Observations

- The interpreter sustains **~600–900 MIPS** on the M5, bottlenecked by the `switch` dispatch and bounds checks on the tape pointer. Peak throughput is reached with short programs that fit in the instruction cache.
- Performance is linear in instruction count — the interpreter adds no overhead for longer programs beyond the $O(N)$ compile pass.
- The 4-level nest (59⁴) demonstrates the interpreter handling **4.36 billion instructions** in under 5 seconds, confirming O(1) bracket jumps and efficient bytecode dispatch.
- The compiler produces a newline at the end of output if none is present.

---

## Build System

```bash
cd projects/brainfuck
make && ./brainfuck program.bf
```

| Target | Description |
|---|---|
| `all` | Build the `brainfuck` binary |
| `clean` | Remove object files and binary |

### Dependencies

- C++17 standard library (`<vector>`, `<stack>`, `<chrono>`, `<string>`)
- No external libraries, no GPU, no graphics

---

## Tested Configuration

| Component | Detail |
|---|---|
| **CPU** | Apple M5 (10-core, 3.40 GHz) |
| **RAM** | 24 GB unified |
| **OS** | macOS (arm64) |
| **Compiler** | Apple Clang 21, C++17 |
| **Optimisation** | `-O3 -Wall -Wextra` |

---

## Example Programs

### Cat Program

Reads input and echoes it back verbatim until EOF:

```brainfuck
,[.,]
```

### Infinite Loop (demonstrates limit)

```brainfuck
+[]
```

Will terminate after 100,000,000 instructions with:
```
error: exceeded instruction limit (100000000)
```
