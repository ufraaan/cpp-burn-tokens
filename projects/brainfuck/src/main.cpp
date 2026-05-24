#include "brainfuck.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static bool read_file(const std::string& path, std::string& out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) { fprintf(stderr, "error: cannot open '%s'\n", path.c_str()); return false; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz < 0) { fclose(f); return false; }
    out.resize((size_t)sz);
    if (sz > 0 && fread(out.data(), 1, (size_t)sz, f) != (size_t)sz) {
        fclose(f); return false;
    }
    fclose(f);
    return true;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("%s\n", Brainfuck::help(argv[0]).c_str());
        return 0;
    }

    BFOptions opts;
    const char* file = nullptr;
    int argi = 1;

    while (argi < argc && argv[argi][0] == '-') {
        std::string flag(argv[argi]);
        if (flag == "-d") opts.debug = true;
        else if (flag == "-s") { opts.debug = true; opts.step = true; }
        else if (flag == "-b") opts.benchmark = true;
        else if (flag == "-t") {
            if (argi + 1 >= argc) {
                fprintf(stderr, "error: -t requires a size argument\n");
                return 1;
            }
            opts.tape_size = (size_t)atol(argv[++argi]);
            if (opts.tape_size < 64) opts.tape_size = 64;
        } else if (flag == "--version" || flag == "-v") {
            printf("%s\n", Brainfuck::version().c_str());
            return 0;
        } else if (flag == "--help" || flag == "-h") {
            printf("%s\n", Brainfuck::help(argv[0]).c_str());
            return 0;
        } else {
            fprintf(stderr, "error: unknown flag %s\n", flag.c_str());
            return 1;
        }
        ++argi;
    }

    if (argi >= argc) {
        fprintf(stderr, "error: missing file argument\n");
        return 1;
    }
    file = argv[argi];

    std::string source;
    if (!read_file(file, source)) return 1;

    // Any remaining args are treated as input for the program
    std::string input;
    for (int i = argi + 1; i < argc; ++i) {
        if (i > argi + 1) input += ' ';
        input += argv[i];
    }

    Brainfuck bf(opts);
    BFResult result = bf.execute(source, input);

    if (!result.success) {
        fprintf(stderr, "\n%s\n", result.error.c_str());
        return 1;
    }

    // In debug/step mode, output was NOT written to stdout,
    // so print it now
    if ((opts.debug || opts.step) && !result.output.empty()) {
        fwrite(result.output.data(), 1, result.output.size(), stdout);
        fflush(stdout);
    }

    return 0;
}
