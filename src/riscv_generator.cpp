#include "../include/riscv_generator.hpp"
#include "../include/ir.hpp"
#include <ostream>
#include <type_traits>
#include <variant>

void RISCVGenerator::GenerateRISCV(const Program& koopa_program, std::ostream& os) {
    os << "\t.text\n";
    for (const auto& func_ptr : koopa_program.functions) {
        os << "\t.globl " << func_ptr->name.substr(1) << "\n";  // name is @ident
        visit(*func_ptr, os);
    }
}

void RISCVGenerator::visit(const Function& func, std::ostream& os) {
    os << func.name.substr(1) << ":\n";
    for (const auto& bb_ptr : func.blocks) {
        os << bb_ptr->name.substr(1) << ":\n"; // name is %ident
        visit(*bb_ptr, os);
    }
}

void RISCVGenerator::visit(const BasicBlock& bb, std::ostream& os) {
    for (const auto& inst_ptr : bb.insts) {
        visit(*inst_ptr, os);
    }
}

void RISCVGenerator::visit(const Value& value, std::ostream& os) {
    std::visit([&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, Value::Integer>) {
            os << "\t# Koopa IR Integer: " << arg.value << "\n";
        } else if constexpr (std::is_same_v<T, Value::Return>) {
            if (arg.value) {
                std::visit([&](auto&& ret_arg) {
                    using RetT = std::decay_t<decltype(ret_arg)>;
                    if constexpr (std::is_same_v<RetT, Value::Integer>) {
                        os << "\tli a0, " << ret_arg.value << "\n";
                    }
                }, arg.value->kind);
            }
            os << "ret ";
        }
    }, value.kind);
}
