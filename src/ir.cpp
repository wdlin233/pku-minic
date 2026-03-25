#include "../include/ir.hpp"
#include <cassert>
#include <cstddef>
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
        
        case Value::Binary::AND:
            return "and";
        case Value::Binary::OR:
            return "or";

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
    os << "fun " << name << "(";
    for (size_t i = 0; i < params.size(); ++i) {
        os << params[i]->name << ": i32";
        if (i != params.size() - 1) {
            os << ", ";
        }
    }
    os << ")";

    if (ret_type && ret_type->kind != Type::VOID) {
        os << ": i32";
    }

    os << " {\n";
    // output basic blocks in reverse order to keep %entry in front
    for (auto basic_block = blocks.rbegin(); basic_block != blocks.rend(); basic_block++) {
        (*basic_block)->Dump(os);
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

bool BasicBlock::has_terminator() const {
    if (insts.empty()) return false;
    const auto& last_kind = insts.back()->kind;
    return std::holds_alternative<Value::Return>(last_kind) ||
        std::holds_alternative<Value::Branch>(last_kind) ||
        std::holds_alternative<Value::Jump>(last_kind);
}

void Value::Dump(std::ostream& os) const {
    // check `kind` type, making `name` output
    bool assignment = false;
    if (std::holds_alternative<Binary>(kind) ||
        std::holds_alternative<Alloc>(kind) ||
        std::holds_alternative<Load>(kind)
    ) {
        assignment = true;
    } 
    else if (const auto* call_info = std::get_if<Call>(&kind)) {
        if (call_info->ret_type && 
            call_info->ret_type->kind != Type::VOID
        ) assignment = true;
    }
    if (!name.empty() && assignment) {
        os << name << " = ";
    }

    std::visit([&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, Integer>) {
            os << arg.value;
        } else if constexpr (std::is_same_v<T, Return>) {
            os << "ret ";
            if (!arg.value.has_value()) {
                return;
            }
            if (!arg.value.value()->name.empty() && !std::holds_alternative<Integer>(arg.value.value()->kind)) {
                // e.g. ret %2
                os << arg.value.value()->name;
            } else {
                arg.value.value()->Dump(os);
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
        } else if constexpr (std::is_same_v<T, Alloc>) {
            os << "alloc i32";
        } else if constexpr (std::is_same_v<T, Load>) {
            os << "load " << arg.ptr->name; 
        } else if constexpr (std::is_same_v<T, Store>) {
            os << "store ";
            arg.value->Dump(os);
            os << " ," << arg.dest->name;
        } else if constexpr (std::is_same_v<T, Branch>) {
            os << "br ";
            arg.cond->Dump(os); 
            os << ", " << arg.true_bb->name; // not Dump whole BasicBlock
            os << ", " << arg.false_bb->name;
        } else if constexpr (std::is_same_v<T, Jump>) {
            os << "jump " << arg.target_bb->name;
        } else if constexpr (std::is_same_v<T, Call>) {
            os << "call " << arg.ident << "(";
            for (size_t i = 0; i < arg.args.size(); ++i) {
                arg.args[i]->Dump(os);
                if (i < arg.args.size() - 1) {
                    os << ", ";
                }
            }
            os << ")";
        } else if constexpr (std::is_same_v<T, Argument>) {
            os << name;
        }
    }, kind);
}