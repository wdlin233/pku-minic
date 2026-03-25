#include "../include/riscv_generator.hpp"
#include "../include/ir.hpp"
#include <algorithm>
#include <ostream>
#include <string>
#include <type_traits>
#include <variant>
#include <cassert>

void FrameAllocator::analyze(const Function& func) {
    local_slot_offsets_.clear();
    frame_size_ = 0;
    outgoing_size_ = 0;
    locals_base_ = 0;
    ra_offset_ = -1;
    save_ra_ = false;

    /*
    高地址 (栈底, 原来的 sp)
    +-------------------------+ 
    | 补齐对齐的 padding      | <- 这里加上对齐的字节，得到最终的 frame_size_
    +-------------------------+ 
    | 保存的寄存器 (如 ra)    | <- 占用 4 字节，起始偏移为 ra_offset_
    +-------------------------+ <- 此时的 offset 为 total = outgoing_size_ + local_size
    |                         | 
    | 局部变量与临时值存放区  | <- 总大小为 local_size
    | (Local Slots)           | 
    +-------------------------+ <- 起始偏移为 locals_base_ (= outgoing_size_)
    | 调用其他函数使用的参数区|
    | (Outgoing Arguments)    | <- 总大小为 outgoing_size_
    +-------------------------+ <- 最低地址 (此刻的 sp, offset = 0)
    低地址 (栈顶)
    */

    int local_size = 0; // 统计所有 Koopa 指令所需要的临时存储空间的总和
    for (const auto& bb : func.blocks) {
        for (const auto& inst : bb->insts) {
            if (std::holds_alternative<Value::Call>(inst->kind)) {
                save_ra_ = true;
                const auto& call = std::get<Value::Call>(inst->kind);
                if (call.args.size() > 8) {
                    int need = static_cast<int>(call.args.size() - 8) * 4;
                    outgoing_size_ = std::max(outgoing_size_, need);
                }
            }

            // Determine if the instruction produces a value that needs to be stored on the stack
            // 如果我将函数的参数保存在局部变量区，会把十个变量写在一起。
            // 但是如果并不保存局部变量，8个参数在寄存器，剩下两个参数在调用者的栈帧底部。
            // 保存局部变量区并不是必须的，甚至栈帧也不是必要的
            // 当前寄存器的策略是为每一条指令的结果都 alloc 局部变量空间，spill-all 策略
            // 不过，当前的后端没有把函数参数主动保存在 local_slot 中
            // 但是，对于变量 @x 的 store @x, %0 的 IR，还是 sw 到 %0 的 local_slot 里，所以还是写进了 local_slot
            bool need_slot = std::holds_alternative<Value::Binary>(inst->kind) ||
                             std::holds_alternative<Value::Alloc>(inst->kind) ||
                             std::holds_alternative<Value::Load>(inst->kind) ||
                             (std::holds_alternative<Value::Call>(inst->kind) &&
                              std::get<Value::Call>(inst->kind).ret_type &&
                              std::get<Value::Call>(inst->kind).ret_type->kind != Type::VOID);
            if (need_slot) {
                // Assign a relative slot offset for this instruction's result
                local_slot_offsets_[inst.get()] = local_size;
                local_size += 4;
            }
        }
    }

    locals_base_ = outgoing_size_;
    int total = outgoing_size_ + local_size;
    if (save_ra_) {
        ra_offset_ = total;
        total += 4;
    }

    frame_size_ = (total + 15) & ~15;
}

// outgoing args offset relative to sp pointer, sp+0, sp+4, ...
int FrameAllocator::outgoing_arg_offset(int arg_index) const {
    assert(arg_index >= 8);
    return (arg_index - 8) * 4;
}

// incoming args offset relative to sp pointer,
// NOT in the same stackframe of current function
// refer to the previous function(Caller) stackframe
// please see: https://pku-minic.github.io/online-doc/lv8-func-n-global/call-with-10-args.png
int FrameAllocator::incoming_arg_offset(int arg_index) const {
    assert(arg_index >= 8);
    return frame_size_ + (arg_index - 8) * 4;
}

int FrameAllocator::value_offset(const Value* v) const {
    return locals_base_ + local_slot_offsets_.at(v);
}

bool FrameAllocator::has_value_slot(const Value* v) const {
    return local_slot_offsets_.find(v) != local_slot_offsets_.end();
}

void RISCVGenerator::GenerateRISCV(const Program& koopa_program, std::ostream& os) {
    os << "\t.text\n";
    for (const auto& func_ptr : koopa_program.functions) {
        os << "\t.globl " << func_ptr->name.substr(1) << "\n";  // name is @ident
        visit(*func_ptr, os);
    }
}

void RISCVGenerator::visit(const Function& func, std::ostream& os) {
    frame_allocator.analyze(func);
    param_index.clear();
    for (size_t i = 0; i < func.params.size(); ++i) {
        param_index[func.params[i].get()] = static_cast<int>(i);
    }
    
    os << func.name.substr(1) << ":\n";
    
    // Prologue
    int frame_size = frame_allocator.frame_size();
    if (frame_size > 0) {
        if (frame_size <= 2048) {
            os << "\taddi sp, sp, -" << frame_size << "\n";
        } else {
            os << "\tli t2, -" << frame_size << "\n";
            os << "\tadd sp, sp, t2\n";
        }
    }

    if (frame_allocator.save_ra()) {
        os << "\tsw ra, " << frame_allocator.ra_offset() << "(sp)\n";
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
                load_value_to_reg(arg.value.value().get(), "a0", os);
            }

            if (frame_allocator.save_ra()) {
                os << "\tlw ra, " << frame_allocator.ra_offset() << "(sp)\n";
            }

            // Epilogue
            int frame_size = frame_allocator.frame_size();
            if (frame_size > 0) {
                if (frame_size <= 2047) {
                    os << "\taddi sp, sp, " << frame_size << "\n";
                } else {
                    os << "\tli t2, " << frame_size << "\n";
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

            int result_offset = frame_allocator.value_offset(self);
            os << "\tsw t0, " << result_offset << "(sp)\n";
            
        }
        else if constexpr (std::is_same_v<T, Value::Alloc>) {
            // already implemented in allocate_stack
        }
        else if constexpr (std::is_same_v<T, Value::Load>) {
            assert(arg.ptr->type && arg.ptr->type->kind == Type::POINTER && "Load source must be an address");
            int src_offset = frame_allocator.value_offset(arg.ptr); // offset of @x
            os << "\tlw t0, " << src_offset << "(sp)\n";
            int res_offset = frame_allocator.value_offset(self); // offset of %0
            os << "\tsw t0, " << res_offset << "(sp)\n";
        }
        else if constexpr (std::is_same_v<T, Value::Store>) {
            assert(arg.dest->type && arg.dest->type->kind == Type::POINTER && "Store destination must be an address");
            load_value_to_reg(arg.value.get(), "t0", os);
            int dest_offset = frame_allocator.value_offset(arg.dest);
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
        else if constexpr (std::is_same_v<T, Value::Call>) {
            // According to RISC-V ABI, first 8 args go to a0-a7,
            // the rest are placed in caller stack argument area.
            for (size_t i = 0; i < arg.args.size(); ++i) {
                if (i < 8) {
                    load_value_to_reg(arg.args[i].get(), "a" + std::to_string(i), os);
                } else {
                    load_value_to_reg(arg.args[i].get(), "t0", os);
                    int offset = frame_allocator.outgoing_arg_offset(static_cast<int>(i));
                    os << "\tsw t0, " << offset << "(sp)\n";
                }
            }

            std::string callee = arg.ident;
            if (!callee.empty() && callee[0] == '@') {
                callee = callee.substr(1);
            }
            os << "\tcall " << callee << "\n";

            if (arg.ret_type && arg.ret_type->kind != Type::VOID) {
                int result_offset = frame_allocator.value_offset(self);
                os << "\tsw a0, " << result_offset << "(sp)\n";
            }
        }
    }, value.kind);
}

void RISCVGenerator::load_value_to_reg(const Value* val, const std::string& reg, std::ostream& os) {
    std::visit([&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, Value::Integer>) {
            os << "\tli " << reg << ", " << arg.value << "\n";
        } else if constexpr (std::is_same_v<T, Value::Argument>) {
            auto it = param_index.find(val);
            if (it == param_index.end()) {
                assert(false && "Function argument is not mapped");
            }
            int idx = it->second;
            if (idx < 8) {
                os << "\tmv " << reg << ", a" << idx << "\n";
            } else {
                int incoming_offset = frame_allocator.incoming_arg_offset(idx);
                os << "\tlw " << reg << ", " << incoming_offset << "(sp)\n";
            }
        } else if constexpr (std::is_same_v<T, Value::SymbolRef>) {
            // load the referenced value
            load_value_to_reg(arg.ptr, reg, os);
        } else {
            if (frame_allocator.has_value_slot(val)) {
                int offset = frame_allocator.value_offset(val);
                os << "\tlw " << reg << ", " << offset << "(sp)\n";
            } else {
                std::cerr << "ERROR: Value cannot be loaded!\n"
                    << "  val kind  = " << val->kind.index() << "\n"
                    << "  reg       = " << reg << "\n"
                    << "  val ptr   = " << val << "\n";               
                assert(false && "Value cannot be loaded");
            }
        }
    } , val->kind);
}
