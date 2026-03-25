#pragma once

#include "ir.hpp"
#include <ostream>
#include <unordered_map>

class FrameAllocator {
    public:
        void analyze(const Function& func);

        int frame_size() const { return frame_size_; }
        bool save_ra() const { return save_ra_; }
        int ra_offset() const { return ra_offset_; }

        int outgoing_arg_offset(int arg_index) const;
        int incoming_arg_offset(int arg_index) const;
        int value_offset(const Value* v) const;
        bool has_value_slot(const Value* v) const;

    private:
        std::unordered_map<const Value*, int> local_slot_offsets_;
        int frame_size_ = 0;
        int outgoing_size_ = 0; // 分配在栈的最底部，用于存放当前函数调用其他函数超过 8 个时，存放传递给子函数的第 9, 10 个参数
        int locals_base_ = 0; // 局部变量区在栈上的基址偏移，相对于 sp 指针
        int ra_offset_ = -1;
        bool save_ra_ = false;
};

class RISCVGenerator {
    public:
        void GenerateRISCV(const Program& koopa_program, std::ostream& os);
    private:
        std::unordered_map<const Value*, int> param_index;
        FrameAllocator frame_allocator;

        void visit(const Function& func, std::ostream& os);
        void visit(const Value& value, std::ostream& os);
        
        void load_value_to_reg(const Value* val, const std::string& reg, std::ostream& os);
};