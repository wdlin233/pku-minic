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

class StmtAST : public BaseAST {
    public:
        std::unique_ptr<NumberAST> number;

        void Dump() const override;
};

// 可以使用 ExprAST
class NumberAST : public BaseAST {
    public:
        int val;
        explicit NumberAST(int val);

        void Dump() const override;
};