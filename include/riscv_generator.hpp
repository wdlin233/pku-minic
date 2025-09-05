#pragma once

#include "ir.hpp"
#include <ostream>

class RISCVGenerator {
    public:
        void GenerateRISCV(const Program& koopa_program, std::ostream& os);
    private:
        void visit(const Function& func, std::ostream& os);
        void visit(const BasicBlock& bb, std::ostream& os);
        void visit(const Value& value, std::ostream& os);
};