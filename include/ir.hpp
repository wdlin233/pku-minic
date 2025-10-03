#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>
#include <variant>

struct Value;
struct BasicBlock;
struct Function;
struct Program;
struct Binary;

struct Type {
    enum Kind {
        INTEGER,      
    };
    Kind kind;
};

// also instruction
struct Value {
    std::string name; // temp name, %name
    std::unique_ptr<Type> type;

    struct Integer { 
        int32_t value; 
        Integer(int32_t v) : value(v) {}
    };
    struct Return { 
        std::unique_ptr<Value> value; 
        Return(std::unique_ptr<Value> v) : value(std::move(v)) {}
    };

    struct Binary {
        enum Op {
            EQ, NE, GT, LT, GE, LE,
            ADD, SUB, MUL, DIV, MOD,
            AND, OR, XOR,
            LAND, LOR,
            SHL, SHR, SAR,
        };

        Op op;
        
        std::unique_ptr<Value> lhs;
        std::unique_ptr<Value> rhs;
    };

    struct SymbolRef {
        const Value* ptr;
    };

    struct Alloc { /* type info in Value::type*/ };
    struct Load { const Value* ptr; };
    struct Store {
        std::unique_ptr<Value> value;
        const Value* dest;
    };

    struct Branch {
        std::unique_ptr<Value> cond;
        const BasicBlock* true_bb;
        const BasicBlock* false_bb;
    };
    struct Jump {
        const BasicBlock* target_bb;
    };

    using Kind = std::variant<
        Integer,
        Return,
        Binary,
        SymbolRef,
        Alloc,
        Load,
        Store,
        Branch,
        Jump
    >;
    Kind kind;

    Value(Integer integer) : kind(std::move(integer)) {}
    Value(Return ret) : kind(std::move(ret)) {}
    Value(Binary binary) : kind(std::move(binary)) {}
    Value(SymbolRef ref) : kind(std::move(ref)) {}
    Value(Alloc alloc) : kind(std::move(alloc)) {}
    Value(Load load) : kind(std::move(load)) {}
    Value(Store store) : kind(std::move(store)) {}
    Value(Branch branch) : kind(std::move(branch)) {}
    Value(Jump jump) : kind(std::move(jump)) {}

    explicit Value(int val) : kind(Integer{val}) {
        type = std::make_unique<Type>();
        type->kind = Type::INTEGER;
    }
    int get_int_value() const {
        assert(std::holds_alternative<Integer>(kind) && "Not an integer value");
        return std::get<Integer>(kind).value;
    } 

    void Dump(std::ostream& os) const;
};

struct BasicBlock {
    std::string name;
    std::vector<std::unique_ptr<Value>> insts;

    void Dump(std::ostream& os) const;
    bool has_terminator() const;
};

struct Function {
    std::string name;
    std::unique_ptr<Type> ret_type;
    std::vector<std::unique_ptr<BasicBlock>> blocks;

    void Dump(std::ostream& os) const;
};

struct Program {
    std::vector<std::unique_ptr<Function>> functions;

    void Dump(std::ostream& os) const;
};
