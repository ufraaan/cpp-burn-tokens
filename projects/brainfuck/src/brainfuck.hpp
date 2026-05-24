#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

struct BFOptions {
    size_t tape_size = 30000;
    bool debug = false;
    bool step = false;
    bool benchmark = false;
    uint64_t max_instructions = 100'000'000;
};

struct BFResult {
    std::string output;
    uint64_t instructions;
    bool success;
    std::string error;
};

class Brainfuck {
public:
    explicit Brainfuck(BFOptions opts = {});

    BFResult execute(const std::string& source, const std::string& input = "");

    static std::string version();
    static std::string help(const char* prog);

private:
    BFOptions opts_;

    struct Program {
        std::string code;
        std::vector<size_t> jump;
    };

    bool compile(const std::string& source, Program& out);
    void dump_state(const Program& prog, size_t ip, size_t ptr,
                    const std::vector<uint8_t>& tape, uint64_t inst_count,
                    const std::string& output) const;
};
