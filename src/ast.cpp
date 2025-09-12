#include "../include/ast.hpp"

FuncTypeAST::FuncTypeAST(FuncTypeAST::Type t) : type(t) {}

void FuncTypeAST::Dump() const {
    std::cout << "FuncTypeAST { ";
    switch (type) {
        case TYPE_INT:
            std::cout << "int";
            break;
    }
    std::cout << " }";
}

NumberAST::NumberAST(int val) : val(val) {}

void NumberAST::Dump() const {
    std::cout << "NumberAST { " << val << " }";
}

void StmtAST::Dump() const {
    std::cout << "StmtAST { ";
    expression->Dump();
    std::cout << " }";
}

void BlockAST::Dump() const {
    std::cout << "BlockAST { ";
    stmt->Dump();
    std::cout << " }";
}

void FuncDefAST::Dump() const {
    std::cout << "FuncDefAST { ";
    func_type->Dump();
    std::cout << ", " << ident << ", ";
    block->Dump();
    std::cout << " }";
}

void CompUnitAST::Dump() const {
    std::cout << "CompUnitAST { ";
    func_def->Dump();
    std::cout << "}";
}

void BinaryExprAST::Dump() const {
    std::cout << "BinaryExprAST { }";
}

void UnaryExprAST::Dump() const {
    std::cout << "UnaryExprAST { }";
}