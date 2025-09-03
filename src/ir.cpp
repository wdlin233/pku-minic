#include "../include/ir.hpp"

void Program::Dump(std::ostream& os) const {
    for (const auto& func : functions) {
        func->Dump(os);
        os << "\n";
    }
}

void Function::Dump(std::ostream& os) const {
    os << "fun " << name << "(): i32 {\n";
    for (const auto& basic_block : blocks) {
        basic_block->Dump(os);
    }
    os << "}\n";
}

void BasicBlock::Dump(std::ostream& os) const {
    os << name << ":\n";
    for (const auto& inst : insts) {
        os << " ";
        inst->Dump(os);
        os << "\n";
    }
}

void Value::Dump(std::ostream& os) const {
    std::visit([&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, Integer>) {
            os << arg.value;
        } else if constexpr (std::is_same_v<T, Return>) {
            os << "ret ";
            if (arg.value) {
                arg.value->Dump(os);
            }
        }
    }, kind);
}