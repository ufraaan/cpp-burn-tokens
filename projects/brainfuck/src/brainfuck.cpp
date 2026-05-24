#include "brainfuck.hpp"
#include <cstdio>
#include <stack>
#include <chrono>
#include <algorithm>

Brainfuck::Brainfuck(BFOptions opts) : opts_(std::move(opts)) {}

bool Brainfuck::compile(const std::string& source, Program& out) {
    out.code.clear();
    out.jump.clear();

    // Filter valid commands and build jump table
    std::string valid = "><+-.,[]";
    std::vector<size_t> stack;

    for (size_t i = 0; i < source.size(); ++i) {
        char c = source[i];
        if (valid.find(c) == std::string::npos)
            continue;

        if (c == '[') {
            stack.push_back(out.code.size());
        } else if (c == ']') {
            if (stack.empty()) {
                out.code.clear();
                out.jump.clear();
                return false; // unmatched ]
            }
            stack.pop_back();
        }

        out.code.push_back(c);
    }

    if (!stack.empty()) {
        out.code.clear();
        out.jump.clear();
        return false; // unmatched [
    }

    // Build jump table
    out.jump.assign(out.code.size(), -1);
    stack.clear();

    for (size_t i = 0; i < out.code.size(); ++i) {
        if (out.code[i] == '[') {
            stack.push_back(i);
        } else if (out.code[i] == ']') {
            size_t open = stack.back();
            stack.pop_back();
            out.jump[open] = i;
            out.jump[i] = open;
        }
    }

    return true;
}

void Brainfuck::dump_state(const Program& prog, size_t ip, size_t ptr,
                            const std::vector<uint8_t>& tape,
                            uint64_t inst_count,
                            const std::string& output) const
{
    fprintf(stderr, "\n--- [%llu] IP=%zu PTR=%zu ---\n",
            (unsigned long long)inst_count, ip, ptr);

    if (ip < prog.code.size())
        fprintf(stderr, "  instr: '%c'\n", prog.code[ip]);

    // Show relevant tape region (non-zero ± neighbours)
    const int W = 5; // neighbours to show around current cell
    size_t start = (ptr > (size_t)W) ? ptr - W : 0;
    size_t end = std::min(ptr + W + 1, tape.size());

    fprintf(stderr, "  tape[%zu..%zu]: ", start, end);
    for (size_t i = start; i < end; ++i) {
        if (i == ptr)
            fprintf(stderr, "[%3u] ", tape[i]);
        else
            fprintf(stderr, " %3u  ", tape[i]);
    }
    fprintf(stderr, "\n");

    if (!output.empty()) {
        size_t show = std::min(output.size(), (size_t)40);
        std::string last = output.substr(output.size() - show);
        // Replace non-printable
        for (char& c : last)
            if (c < 32 || c > 126) c = '.';
        fprintf(stderr, "  output(%zu): \"%s\"\n", output.size(), last.c_str());
    }

    fflush(stderr);
}

BFResult Brainfuck::execute(const std::string& source, const std::string& input) {
    Program prog;
    if (!compile(source, prog)) {
        return {{}, 0, false, "error: unmatched brackets"};
    }

    if (prog.code.empty()) {
        return {{}, 0, true, ""};
    }

    std::vector<uint8_t> tape(opts_.tape_size, 0);
    size_t ptr = 0;
    size_t ip = 0;
    size_t input_pos = 0;
    uint64_t inst_count = 0;
    std::string output;
    std::string input_buf = input;

    auto t0 = std::chrono::high_resolution_clock::now();

    while (ip < prog.code.size()) {
        // Only enforce instruction limit in non-benchmark mode
        if (!opts_.benchmark && inst_count >= opts_.max_instructions) {
            return {output, inst_count, false,
                    "error: exceeded instruction limit (" +
                    std::to_string(opts_.max_instructions) + ")"};
        }

        char cmd = prog.code[ip];

        if (opts_.debug || opts_.step) {
            dump_state(prog, ip, ptr, tape, inst_count, output);
            if (opts_.step) {
                fprintf(stderr, "  [press Enter] ");
                fflush(stderr);
                int ch;
                do { ch = getchar(); } while (ch != '\n' && ch != EOF);
            }
        }

        switch (cmd) {
            case '>':
                if (ptr < tape.size() - 1) ++ptr;
                break;
            case '<':
                if (ptr > 0) --ptr;
                break;
            case '+':
                ++tape[ptr];
                break;
            case '-':
                --tape[ptr];
                break;
            case '.':
                output += (char)tape[ptr];
                if (!opts_.debug && !opts_.step)
                    putchar(tape[ptr]);
                break;
            case ',':
                if (input_pos < input_buf.size()) {
                    tape[ptr] = (uint8_t)input_buf[input_pos++];
                } else {
                    int ch = getchar();
                    tape[ptr] = (ch == EOF) ? 0 : (uint8_t)ch;
                }
                break;
            case '[':
                if (tape[ptr] == 0)
                    ip = prog.jump[ip];
                break;
            case ']':
                if (tape[ptr] != 0)
                    ip = prog.jump[ip];
                break;
        }

        ++ip;
        ++inst_count;
    }

    if (opts_.benchmark) {
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double ips = (ms > 0) ? (double)inst_count / ms * 1000.0 : 0.0;
        fprintf(stderr, "\n[benchmark] %llu instructions in %.2f ms  (%.0f IPS)\n",
                (unsigned long long)inst_count, ms, ips);
    }

    return {output, inst_count, true, ""};
}

std::string Brainfuck::version() {
    return "brainfuck 1.0 — Brainfuck Interpreter (C++17, Apple Clang)";
}

std::string Brainfuck::help(const char* prog) {
    std::string h;
    h += "Usage:\n";
    h += "  " + std::string(prog) + " <file.bf>              Execute Brainfuck program\n";
    h += "  " + std::string(prog) + " -d <file.bf>            Debug mode\n";
    h += "  " + std::string(prog) + " -s <file.bf>            Step mode\n";
    h += "  " + std::string(prog) + " -b <file.bf>            Benchmark mode\n";
    h += "  " + std::string(prog) + " -t <size> <file.bf>     Set tape size (default 30000)\n";
    h += "\n";
    h += "Input is read from stdin after the program starts.\n";
    h += "All non-command characters in source are treated as comments.\n";
    return h;
}
