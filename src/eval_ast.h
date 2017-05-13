#ifndef JIAOBENSCRIPT_EVAL_AST_H
#define JIAOBENSCRIPT_EVAL_AST_H

#include <functional>
#include <utility>
#include <vector>
#include <stack>

#include "allocator.h"
#include "exceptions.h"
#include "builtins.h"
#include "jbobject.h"
#include "node.h"


struct Signal {};


class BreakSignal : public Signal {};


class ContinueSignal : public Signal {};


class ReturnSignal : public Signal {
public:
    ReturnSignal(JBValue &value) : value(value) {}
    JBValue &value;
};


class Frame : public JBObject {
public:
    Frame *parent = nullptr;
    S_Block *block = nullptr;
    std::vector<JBValue *> vars;

    virtual void each_ref(std::function<void (JBObject &)> callback) override;
};


template<class S>
class StackPoper {
public:
    StackPoper(S &stk) : stk(stk) {}
    ~StackPoper() {
        assert(!this->stk.empty());
        this->stk.pop();
    }
    S &stk;
};


class AstInterpreter : public NodeVistor {
public:
    AstInterpreter() : allocator(), builtins(allocator) {}

    void eval_incomplete_raw_block(S_Block &block);
    void eval_raw_decl_list(S_DeclareList &decls);
    JBValue &eval_raw_exp(Node &exp);
    void eval_raw_stmt(Node &node);

    virtual void visit_block(S_Block &block);
    virtual void visit_program(Program &prog);
    virtual void visit_declare_list(S_DeclareList &decls);
    virtual void visit_condition(S_Condition &cond);
    virtual void visit_while(S_While &wh);
    virtual void visit_return(S_Return &ret);
    virtual void visit_break(S_Break &brk);
    virtual void visit_continue(S_Continue &cont);
    virtual void visit_stmt_exp(S_Exp &stmt);
    virtual void visit_stmt_empty(S_Empty &stmt);
    virtual void visit_op(E_Op &op);
    virtual void visit_var(E_Var &var);
    virtual void visit_func(E_Func &func);
    virtual void visit_bool(E_Bool &bool_node);
    virtual void visit_int(E_Int &int_node);
    virtual void visit_float(E_Float &float_node);
    virtual void visit_string(E_String &str);
    virtual void visit_list(E_List &list);
    virtual void visit_null(E_Null &nil);

private:
    template<class T, class ...Args>
    T &create(Args &&...args) {
        return *this->allocator.construct<T>(std::forward<Args>(args)...);
    }
    Frame &create_frame(Frame *parent, S_Block &block);

    void return_value(JBValue &value);
    StackPoper<std::stack<Frame *>> enter(S_Block &block, Frame *parent_frame = nullptr);
    JBValue &eval_exp(Node &node);
    JBValue **resolve_var(const E_Var &var);
    void resolve_names_current_block(Node &node);

    using UnaryFunc = std::function<JBValue &(JBValue &)>;
    using BinaryFunc = std::function<JBValue &(JBValue &, JBValue &)>;

    void handle_unary_or_binary_op(E_Op &exp, UnaryFunc unary_func, BinaryFunc binary_func);
    void handle_binary_op(E_Op &exp, BinaryFunc binary_func);
    void handle_unary_op(E_Op &exp, UnaryFunc unary_func);
    void handle_logic_and(E_Op &exp);
    void handle_logic_or(E_Op &exp);
    void handle_assign(E_Op &exp);
    void do_assign(Node &lhs, JBValue &value);
    void handle_binop_assign(E_Op &exp, BinaryFunc binary_func);
    void handle_call(E_Op &call);
    void handle_func_body(S_Block &block);
    void handle_getitem(E_Op &exp);
    void handle_explist(E_Op &exp);
    void handle_block(S_Block &block);

    std::stack<Frame *> frames;
    std::stack<JBValue *> values;

    Allocator allocator;
    Builtins builtins;
};


#endif //JIAOBENSCRIPT_EVAL_AST_H
