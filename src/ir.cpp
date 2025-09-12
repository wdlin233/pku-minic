#include "../include/ir.hpp"
#include <cassert>
#include <type_traits>
#include <variant>

const char* op_to_string(Value::Binary::Op op) {
    switch (op) {
        case Value::Binary::EQ:
            return "eq";
        case Value::Binary::NE:
            return "ne";
        case Value::Binary::LT:
            return "lt";
         case Value::Binary::GT:
            return "gt";
         case Value::Binary::LE:
            return "le";
         case Value::Binary::GE:
            return "ge";

        case Value::Binary::ADD:
            return "add";
        case Value::Binary::SUB:
            return "sub";
        case Value::Binary::MUL:
            return "mul";
        case Value::Binary::DIV:
            return "div";
        case Value::Binary::MOD:
            return "mod";
        
        default:
            assert(false && "Unsupported op for transforming to string");
            return "";
    }
}

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
    // check `kind` type, making `name` output
    if (!name.empty() && !std::holds_alternative<Return>(kind) && !std::holds_alternative<Integer>(kind)) {
        os << name << " = ";
    }

    std::visit([&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, Integer>) {
            os << arg.value;
        } else if constexpr (std::is_same_v<T, Return>) {
            os << "ret ";
            if (!arg.value->name.empty() && !std::holds_alternative<Integer>(arg.value->kind)) {
                // e.g. ret %2
                os << arg.value->name;
            } else {
                arg.value->Dump(os);
            }
        } else if constexpr (std::is_same_v<T, Binary>) {
            os << op_to_string(arg.op) << " ";
            if (!arg.lhs->name.empty() && !std::holds_alternative<Integer>(arg.lhs->kind)) {
                os << arg.lhs->name;
            } else {
                arg.lhs->Dump(os);
            }
            os << ", ";
            if (!arg.rhs->name.empty() && !std::holds_alternative<Integer>(arg.rhs->kind)) {
                os << arg.rhs->name;
            } else {
                arg.rhs->Dump(os);
            }
        } else if constexpr (std::is_same_v<T, SymbolRef>) {
            os << arg.ptr->name;
        }
    }, kind);
}