#pragma once

#include <string>
#include <memory>
#include <iostream>

class BaseAST {
    public:
        virtual ~BaseAST() = default;

        // operator << 也可行
        virtual void Dump() const = 0;
};

class FuncDefAST;
class FuncTypeAST;
class BlockAST;
class StmtAST;
class NumberAST;

class CompUnitAST : public BaseAST {
    public:
        std::unique_ptr<FuncDefAST> func_def;
        void Dump() const override;
};

class FuncDefAST : public BaseAST {
    public:
        std::unique_ptr<FuncTypeAST> func_type;
        std::string ident;
        std::unique_ptr<BlockAST> block;

        void Dump() const override;
};

class FuncTypeAST : public BaseAST {
    public:
        enum Type {
            TYPE_INT
        };
        Type type;
        explicit FuncTypeAST(Type t);

        void Dump() const override;
    };

class BlockAST : public BaseAST {
    public:
        std::unique_ptr<StmtAST> stmt;

        void Dump() const override;
};

class ExprAST : public BaseAST {};

class NumberAST : public ExprAST {
    public:
        int val;
        explicit NumberAST(int val);

        void Dump() const override;
};

class BinaryExprAST : public ExprAST {
    public:
        char op;
        std::unique_ptr<ExprAST> lhs;
        std::unique_ptr<ExprAST> rhs;

        void Dump() const override;
};

// UnaryOp and UnaryExpr
class UnaryExprAST : public ExprAST {
    public:
        char op;
        std::unique_ptr<ExprAST> operand;

        void Dump() const override;
};

class StmtAST : public BaseAST {
    public:
        std::unique_ptr<ExprAST> expression;

        void Dump() const override;
};
