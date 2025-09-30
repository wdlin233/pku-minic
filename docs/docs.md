# 遇到的问题/思考记录

### IR 的数据结构是如何定义的？为什么这么定义？

我的理解是 IR 就表示了整个程序经过词法和语法分析后得到的中间表示的结构，是 `Program`, `Function` 和 `BasicBlock` 三个层的嵌套，在最底层或者说最基础的表示都归于 `Value` 这个结构，里面就区分了 `Integer` 和 `Return` 这些我们在 `IRGenerator::vist(const NumberAST& number)` 中解析出来的类型.

那么一个正常的疑问就是，IR 的数据结构表示和 IRGenerator 之间的联系是什么？

例如下面这两个函数

```cpp
void IRGenerator::visit(const FuncDefAST& func_def) {
    auto func = std::make_unique<Function>();
    func->name = "@" + func_def.ident;
    func->ret_type = std::make_unique<Type>();
    func->ret_type->kind = Type::INTEGER;
    
    auto bb = std::make_unique<BasicBlock>();
    bb->name = "%entry";

    current_function = func.get();
    current_bb = bb.get();

    visit(*func_def.block->stmt);

    func->blocks.push_back(std::move(bb)); // push basic block into basic blocks
    program->functions.push_back(std::move(func));
}

std::unique_ptr<Value> IRGenerator::visit(const NumberAST& number) {
    auto val = std::make_unique<Value>(
        Value::Integer{ std::move(number.val) }
    );
    val->name = new_temp_var_name();
    val->type = std::make_unique<Type>();
    val->type->kind = Type::INTEGER;

    return val;
}
```

我们实际上是在一直递归 `vist` 遍历这个 AST 结构，在每个对应的结构处都为其建立对应的 IR 数据结构.

`func->blocks.push_back(std::move(bb))` 以支持后续打印相关的指令.

### IR 和 RISCV 汇编的生成

AST 的解析由 `extern int yyparse(unique_ptr<BaseAST> &ast)` 完成，得到的 AST 再经由类型转换得到 `CompUnitAST* comp_unit_ast`.

`IRGenerator` 会遍历递归 AST 的结构，从中构建 IR 相关的数据结构，例如 `Program`, `Function`, `BasicBlock` 和 `Value` 等.

然后对于 `-koopa`，直接解析 IR 存在内存中的内容，以 Koopa IR 指定的格式输出即可.

对于 `-riscv`，引入 `RISCVGenerator`，在解析 Koopa IR 之外需要做一些分配栈帧和寄存器的操作.

### 关于添加新的表达式解析

添加新的表达式就是实现新的语法分析，构建其他类型的 AST，然后在生成 IR 时做对应的处理.

语法分析在 `ast.h` 和 `sysy.y` 中. 构建 AST 实际上是在 `sysy.y` 中作语法分析，`ast.cpp` 还保留着原本的 `Dump()` 解析文本输出，感觉不是很优雅.

到构建 IR 不会太复杂，但是生成汇编时需要一个寄存器分配的方法，目前还是让其固定分配.

### 解决 heap-use-after-free 错误

这个错误就意味着一个指针指向的内存被 delete 了，但是另一段代码试图通过指针去访问这块内存.

Freed Call Stack:

```shell
freed by thread T0 here:
...
#1 0x4d5ab3 in std::default_delete<Value>::operator()(Value*) const ...
#2 0x4d4050 in std::unique_ptr<Value, ...>::~unique_ptr() ...
...
#7 0x4d94a6 in std::vector<std::unique_ptr<Value, ...> >::~vector() ...
#8 0x4d940c in BasicBlock::~BasicBlock() /root/compiler/include/ir.hpp:66:8
...
```

从中看出先释放 `BasicBlock`，然后是 `vector` 和 `unique_ptr`，也就是释放 `BasicBlock` 中的成员，是正常的生命周期结束.

Crashed Call Stack:

```shell
READ of size 1 at ... thread T0
...
#0 0x4d5eec in std::variant<Value::Integer, Value::Return, Value::Binary>::index() const
...
#11 0x4d5aec in Value::~Value() ...
#12 0x4d5aaa in std::default_delete<Value>::operator()(Value*) ...
#13 0x4d4050 in std::unique_ptr<Value, ...>::~unique_ptr() ...
#14 0x4d3fb4 in Value::Return::~Return() ...
...
#39 0x4d94a6 in std::vector<std::unique_ptr<Value, ...> >::~vector() ...
#40 0x4d940c in BasicBlock::~BasicBlock() ...
```

从中可以看出是销毁 `Return` 指令内部的 `unique_ptr` 时发生的.

也就是说被释放的对象是 `BasicBlock` 中的 `inst` vector 的一个元素，而再次释放（崩溃点）的对象是 `Value::Return` 的 `value` 成员.

解决方法是：

```cpp
// origin
current_bb->insts.push_back(std::move(sub_inst));
return std::unique_ptr<Value>(current_bb->insts.back().get());

//modified
Value* result_ptr = sub_inst.get();
current_bb->insts.push_back(std::move(sub_inst));
return std::make_unique<Value>(Value::SymbolRef{result_ptr});
```

`unique_ptr` 的 `get()` 方法获取其原始指针，不转移所有权.

意味着会对同一个地址进行二次释放.

对于包含一个裸指针的 `SymbolRef`，析构函数不会二次 `delete`. 实际上是从日志里推出编译器设计的问题.

### 添加比较和逻辑表达式

在 Bison 的语法规则中，一个符号只能代表一个 Token，不修改 Flex 是不能识别诸如 "<=" 这样的表达式的.

也就是说，对于

```bison
// RelExp ::= AddExp | RelExp ("<" | ">" | "<=" | ">=") AddExp;
RelExp
  : AddExp { $$ = $1; }
  | RelExp '<' AddExp { }
  | RelExp '>' AddExp { }
  | RelExp "<=" AddExp { }
  | RelExp ">=" AddExp { }
  ;
```

不添加

```flex
"<="             { return T_LE; }
```

的 Parser 只会收到 RelExp, '<', '=', AddExp. 则无法匹配.

另外，Flex 会优先匹配更长的规则.

Yash 这个插件很不错，可以及时发现一些错误.

### 关于 NBFS

自己设计的话，必然要考虑优先级.

做实验的同学只要实现功能就好了，MaxXing 考虑的就多了.

### RISCV 只支持小于指令 slt、按位运算与逻辑运算

对于这段程序：

```c
int f(int a, int b) {
    return (a > b);
}
```

其汇编为

```asm
f:
    slt     a0, a1, a0
    ret
```

如果将大于换成小于，生成 `slt a0, a0, a1`. 所以实际上大于 `sgt` 指令就是对不同操作数的小于指令.

实际上这个问题是对处理 `LAND` 和 `LOR` 的一个例子，RISCV 汇编只支持按位运算而不支持逻辑运算.

对于这段代码：

```c
int f(int a, int b) {
    return (a && b);
}
```

经过 `-O3` 优化的 RISCV 汇编：

```asm
f:
    snez    a0, a0
    snez    a1, a1
    and     a0, a0, a1
    ret
```

`snez` 做的是将操作数“规范化”为布尔值的操作.

逻辑或运算也可以用类似的方法得到，所以说 Compiler Explorer 是神.

对于生成的 Koopa IR 也需要类似的中间表示，否则不能通过 `-koopa` 测试：

```koopa
fun @main(): i32 {
%entry:
    %2 = ne 2, 0
    %3 = ne 4, 0
    %4 = and %2, %3
    ret %4
}
```

### 设计新的 AST

考虑如下语法规范：

```ebnf
Decl          ::= ConstDecl;
ConstDecl     ::= "const" BType ConstDef {"," ConstDef} ";";
BType         ::= "int";
ConstDef      ::= IDENT "=" ConstInitVal;
ConstInitVal  ::= ConstExp;

Block         ::= "{" {BlockItem} "}";
BlockItem     ::= Decl | Stmt;

ConstExp      ::= Exp;
```

`BlockItem` 是最顶层的，以 `Block` 的成员 `std::vector<std::unique_ptr<BlockItemAST>> items;` 存储. `Decl` 和 `Stmt` 来自 `BlockItem`，AST 也继承 `BlockItemAST`.

`ConstDef` 推导出终结符，可以直接继承 `BaseAST`. 而 `ConstDecl` 由非终结符 `Decl` 推导而来，继承 `DeclAST`.

问 `ConstInitVal` 怎么处理，由于其产生式的右侧都很固定，最后得到的是 `Exp`，在一定程度上简化可以直接在 `ConstDefAST` 中存下 `ExprAST`.

### Bison 生命周期管理

`std::vector` 是一个复杂的类，由自己的构造函数、析构函数和拷贝/移动操作，所以不能直接放入 `union` 中，可以使用裸指针. 然后在 Bison 的动作代码中手动 `new` 和 `delete` 它.

一个很自然的问题是，为什么 `BaseAST *ast_val` 和 `std::string *str_val` 不需要手动 `delete` 呢？

实际上 Bison 有两种生命周期管理机制：

- Bison `%destructor` 自动清理：当一个符号（token 或 non-terminal）被移出解析栈且不再需要时，Bison 会调用为该符号类型定义的 `%destructor`. 默认的析构器会尝试 delete 指针.
- 手动管理：例如使用 `unique_ptr` 接管，或者手动调用 `delete`.

目前来说，`std::string` 以 `new string()` 生成，然后其所有权转移给 `unqiue_ptr<string()>.` `BASEAST` 也是类似的.

内存管理的责任都被委托到 `std::unique_ptr` 和 RAII 机制中.

### 遍历 AST 时计算 ConstExp

在遇到常量声明语句时, 你应该遍历 AST, 直接算出语句右侧的 `ConstExp` 的值, 得到一个 32 位整数, 然后把这个常量定义插入到符号表中.

根据 C 语言的语义要求，`const` 变量的初始化表达式必须是编译时常量. 而且在生成 IR 时计算也会导致效率问题.

```c
// 错误的做法
const int x = 1 + 2 * 3 - 4;
%0 = add 1, 6    // 2 * 3 = 6
%1 = sub %0, 4   // 7 - 4 = 3
```

所以应当是：

```c
// 正确的做法：编译时求值
const int x = 1 + 2 * 3 - 4;
// 符号表：symbol_table["x"] = 3
// 后续使用x时，直接替换为3
return x;  // 生成IR: ret 3
```

通过对左值的访问：

```cpp
if (auto lval = dynamic_cast<const LValAST*>(&expr)) {
    assert(symbol_table.count(lval->ident) && "Undefined constant variable");
    int const_val = symbol_table.at(lval->ident);

    return std::make_unique<Value>(Value::Integer{const_val});
}
```

### closure

`ir.cpp` 中的 `[&](auto&& arg) { ... }` 是一个 [Lambda 表达式](https://en.cppreference.com/w/cpp/language/lambda.html)，而对于 `std::visit()`：

```cpp
template<typename _Visitor, typename... _Variants>
visit(_Visitor&& __visitor, _Variants&&... __variants)
```

即是以 `visitor` 为可调用对象，接收 `variants` 为参数.

Lambda 可以参考[一文深入了解C++ lambda（C++17）](
https://zhuanlan.zhihu.com/p/582664524)，编译器会根据 Lambda 表达式构造一个等价的仿函数.

`[&]` 说明是引用类型全捕获，此处捕获了 `os`. `auto&& arg` 是一个转发引用或者说万能引用，可以对左值和右值分别初始化.

对于 `[this, &os, self = &value](auto&& arg) { ... }` 其中 `this` 用于访问 `RISCVGenerator` 类的成员变量诸如 `stack_frame` 和 `current_stack_offset`，`self = &value` 是以 `&value` 来初始化 `self` 变量.

### 指针、引用与所有权

一般用 `std::unique` 来管理所有权，裸指针 `*` 用于临时的、非所有权的访问，当然 `.get()` 也是取裸指针的操作.

```cpp
const Value* var_ptr = alloc_inst.get();
```

`std::move(alloc_inst)` 就意味着所有权的转移.

类型检查：

```cpp
void IRGenerator::visit(const BlockItemAST& item) {
    if (auto stmt = dynamic_cast<const StmtAST*>(&item)) {
        visit(*stmt);
        return;
    }
    ...
}
```

首先对引用 `item` 进行取地址操作 `&item`，得到被引用对象的原始指针. 然后将这个**基类指针**尝试向下转型为派生类 `StmtAST` 的**指针**，转型成功后 `*stmt` 对指针解引用，以引用类型传入 `visit` 的参数表.

解引用指针得到了一个对象，将对象以引用的方式传递给 `visit` 函数，由此避免了对 `StmtAST` 的任何拷贝.