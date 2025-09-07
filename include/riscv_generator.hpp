#pragma once

#include "ir.hpp"
#include <ostream>
#include <unordered_map>

class RISCVGenerator {
    public:
        void GenerateRISCV(const Program& koopa_program, std::ostream& os);
    private:
        std::unordered_map<const Value*, int> stack_frame;
        int current_stask_offset = 0;

        void visit(const Function& func, std::ostream& os);
        void visit(const BasicBlock& bb, std::ostream& os);
        void visit(const Value& value, std::ostream& os);
        
        void allocate_stack(const Function& func);
        void load_value_to_reg(const Value* val, const std::string& reg, std::ostream& os);
};