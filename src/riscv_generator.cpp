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
    
    // Prologue
    if (current_stack_offset < 0) {
        if (current_stack_offset >= -2048) {
            os << "\taddi sp, sp, " << current_stack_offset << "\n";
        } else {
            os << "\tli t2, " << current_stack_offset << "\n";
            os << "\tadd sp, sp, t2\n";
        }
    }

    for (auto bb_it = func.blocks.rbegin(); bb_it != func.blocks.rend(); ++bb_it) {
        const auto& bb_ptr = *bb_it;

        if (bb_it != func.blocks.rbegin()) {
            os << bb_ptr->name.substr(1) << ":\n";
        }

        // visit(const BasicBlock& bb, std::ostream& os)
        for (const auto& inst_ptr : bb_ptr->insts) {
            visit(*inst_ptr, os);
        }
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
            // Epilogue
            if (current_stack_offset < 0) {
                int stack_size = -current_stack_offset;
                if (stack_size <= 2047) {
                    os << "\taddi sp, sp, " << stack_size << "\n";
                } else {
                    os << "\tli t2, " << stack_size << "\n";
                    os << "\tadd sp, sp, t2\n";
                }
                
            }
            os << "\tret\n";
        }
        else if constexpr (std::is_same_v<T, Value::Binary>) {
            load_value_to_reg(arg.lhs.get(), "t0", os);
            load_value_to_reg(arg.rhs.get(), "t1", os);

            switch (arg.op) {
                case Value::Binary::EQ:
                    os << "\tsub t0, t0, t1\n";
                    os << "\tseqz t0, t0\n";  // Set if Equal to Zero
                    break;
                case Value::Binary::NE:
                    os << "\tsub t0, t0, t1\n";
                    os << "\tsnez t0, t0\n"; // Set if Not Equal to Zero
                    break;
                case Value::Binary::LT:
                    os << "\tslt t0, t0, t1\n"; // Set if Less Than
                    break;
                case Value::Binary::GT:
                    os << "\tsgt t0, t0, t1\n"; // Set if Greater Than (pseudo-instruction)
                    break;
                case Value::Binary::LE:
                    os << "\tsgt t0, t0, t1\n"; // t0 = t0 > t1
                    os << "\txori t0, t0, 1\n"; // t0 = !(t0 > t1)  which is t0 <= t1
                    break;
                case Value::Binary::GE:
                    os << "\tslt t0, t0, t1\n"; // t0 = t0 < t1
                    os << "\txori t0, t0, 1\n"; // t0 = !(t0 < t1) which is t0 >= t1
                    break;

                case Value::Binary::ADD:
                    os << "\tadd t0, t0, t1\n";
                    break;
                case Value::Binary::SUB:
                    os << "\tsub t0, t0, t1\n";
                    break;
                case Value::Binary::MUL:
                    os << "\tmul t0, t0, t1\n";
                    break;
                case Value::Binary::DIV:
                    os << "\tdiv t0, t0, t1\n";
                    break;
                case Value::Binary::MOD:
                    os << "\trem t0, t0, t1\n";
                    break;

                case Value::Binary::AND:
                    os << "\tand t0, t0, t1\n";
                    break;
                case Value::Binary::OR:
                    os << "\tor t0, t0, t1\n";
                    break;
                    
                default:
                    assert(false && "Unspported binary operation");
            }

            int result_offset = stack_frame.at(self);
            os << "\tsw t0, " << result_offset << "(sp)\n";
            
        }
        else if constexpr (std::is_same_v<T, Value::Alloc>) {
            // already implemented in allocate_stack
        }
        else if constexpr (std::is_same_v<T, Value::Load>) {
            int src_offset = stack_frame.at(arg.ptr); // offset of @x
            os << "\tlw t0, " << src_offset << "(sp)\n";
            int res_offset = stack_frame.at(self); // offset of %0
            os << "\tsw t0, " << res_offset << "(sp)\n";
        }
        else if constexpr (std::is_same_v<T, Value::Store>) {
            load_value_to_reg(arg.value.get(), "t0", os);
            int dest_offset = stack_frame.at(arg.dest);
            os << "\tsw t0, " << dest_offset << "(sp)\n";
        }
        else if constexpr (std::is_same_v<T, Value::Branch>) {
            load_value_to_reg(arg.cond.get(), "t0", os);
            os << "\tbnez t0, " << arg.true_bb->name.substr(1) << "\n";
            os << "\tj " << arg.false_bb->name.substr(1) << "\n";
        }
        else if constexpr (std::is_same_v<T, Value::Jump>) {
            os << "\tj " << arg.target_bb->name.substr(1) << "\n";
        }
    }, value.kind);
}

void RISCVGenerator::allocate_stack(const Function& func) {
    stack_frame.clear();
    current_stack_offset = 0;
    for (const auto& bb : func.blocks) {
        for (const auto& inst : bb->insts) {
            // store inst doesnt generate new value
            if (std::holds_alternative<Value::Binary>(inst->kind) ||
                std::holds_alternative<Value::Alloc>(inst->kind) ||
                std::holds_alternative<Value::Load>(inst->kind)) {
                current_stack_offset -= 4;
                stack_frame[inst.get()] = current_stack_offset;
            }
        }
    }
    current_stack_offset &= ~15; // & ~15(1111_0000)
}

void RISCVGenerator::load_value_to_reg(const Value* val, const std::string& reg, std::ostream& os) {
    std::visit([&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, Value::Integer>) {
            os << "\tli " << reg << ", " << arg.value << "\n";
        } else if constexpr (std::is_same_v<T, Value::SymbolRef>) {
            // load the referenced value
            load_value_to_reg(arg.ptr, reg, os);
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
