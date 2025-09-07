#include "../include/riscv_generator.hpp"
#include "../include/ir.hpp"
#include <ostream>
#include <type_traits>
#include <variant>
#include <cassert>

void RISCVGenerator::GenerateRISCV(const Program& koopa_program, std::ostream& os) {
    os << "\t.text\n";
    for (const auto& func_ptr : koopa_program.functions) {
        os << "\t.globl " << func_ptr->name.substr(1) << "\n";  // name is @ident
        visit(*func_ptr, os);
    }
}

void RISCVGenerator::visit(const Function& func, std::ostream& os) {
    allocate_stack(func);
    
    os << func.name.substr(1) << ":\n";
    
    if (current_stask_offset < 0) {
        os << "\taddi sp, sp, " << current_stask_offset << "\n";
    }

    for (const auto& bb_ptr : func.blocks) {
        // os << bb_ptr->name.substr(1) << ":\n"; // name is %ident
        visit(*bb_ptr, os);
    }
}

void RISCVGenerator::visit(const BasicBlock& bb, std::ostream& os) {
    for (const auto& inst_ptr : bb.insts) {
        visit(*inst_ptr, os);
    }
}

void RISCVGenerator::visit(const Value& value, std::ostream& os) {
    std::visit([this, &os, self = &value](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, Value::Integer>) {
            os << "\t# Koopa IR Integer: " << arg.value << "\n";
        } 
        else if constexpr (std::is_same_v<T, Value::Return>) {
            if (arg.value) {
                load_value_to_reg(arg.value.get(), "a0", os);
            }
            if (current_stask_offset < 0) {
                os << "\taddi sp, sp, " << -current_stask_offset << "\n";
            }
            os << "\tret ";
        }
        else if constexpr (std::is_same_v<T, Value::Binary>) {
            load_value_to_reg(arg.lhs.get(), "t0", os);
            load_value_to_reg(arg.rhs.get(), "t1", os);

            switch (arg.op) {
                case Value::Binary::SUB:
                    os << "\tsub t0, t0, t1\n";
                    break;
                case Value::Binary::EQ:
                    os << "\tsub t0, t0, t1\n";
                    os << "\tseqz t0, t0\n";
                    break;
                default:
                    assert(false && "Unspported binary operation");
            }

            int result_offset = stack_frame.at(self);
            os << "\tsw t0, " << result_offset << "(sp)\n";
            
        }
    }, value.kind);
}

void RISCVGenerator::allocate_stack(const Function& func) {
    stack_frame.clear();
    current_stask_offset = 0;
    for (const auto& bb : func.blocks) {
        for (const auto& inst : bb->insts) {
            if (std::holds_alternative<Value::Binary>(inst->kind)) {
                current_stask_offset -= 4;
                stack_frame[inst.get()] = current_stask_offset;
            }
        }
    }
}

void RISCVGenerator::load_value_to_reg(const Value* val, const std::string& reg, std::ostream& os) {
    std::visit([&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, Value::Integer>) {
            os << "\tli " << reg << ", " << arg.value << "\n";
        } else {
            if (stack_frame.count(val)) {
                int offset = stack_frame.at(val);
                os << "\tlw " << reg << ", " << offset << "(sp)\n";
            } else {
                assert(false && "Value cannot be loaded");
            }
        }
    } , val->kind);
}
