#include <functional>
#include <vector>

#include "name_resolve.h"


static void add_declarations_to_block_attr(S_Block::AttrType &attr, const S_DeclareList &decls) {
    decls.attr.start_index = static_cast<int>(attr.local_info.size());
    for (const auto &pair : decls.decls) {
        auto it = attr.name_to_local_index.find(pair.name);
        if (it != attr.name_to_local_index.end()) {
            throw DuplicatedLocalName("Duplicated local name: " + u8_encode(pair.name));
        }

        attr.name_to_local_index.emplace(pair.name, attr.local_info.size());
        attr.local_info.emplace_back(pair.name);
    }
}


S_Block::AttrType::NonLocalInfo resolve_from_block(S_Block *block, const ustring &name) {
    for (; block != nullptr; block = block->attr.parent) {
        auto it = block->attr.name_to_local_index.find(name);
        if (it != block->attr.name_to_local_index.end()) {
            return {block, it->second};
        }
    }

    throw NoSuchName("No such name: " + u8_encode(name));
}


static int add_nonlocal_to_block_attr(S_Block::AttrType &attr, const ustring &name, S_Block *start)
{
    auto it = attr.name_to_nonlocal_index.find(name);
    if (it == attr.name_to_nonlocal_index.end()) {
        int index = static_cast<int>(attr.nonlocal_indexes.size());
        attr.name_to_nonlocal_index.emplace(name, index);
        attr.nonlocal_indexes.emplace_back(resolve_from_block(start, name));
        return index;
    } else {
        return it->second;
    }
}


struct RestoreOnExit {
    RestoreOnExit(S_Block **location, S_Block *block)
        : location(location), block(block)
    {}
    ~RestoreOnExit() {
        *this->location = this->block;
    }

    S_Block **location;
    S_Block *block;
};


class Resolver : public NodeVistor {
public:
    Resolver(S_Block *cur_block) : cur_block(cur_block) {}

    virtual void visit_block(S_Block &block) {
        auto _ = this->enter(block);

        for (Node::Ptr &stmt : block.stmts) {
            if (S_DeclareList *decls = dynamic_cast<S_DeclareList *>(stmt.get())) {
                add_declarations_to_block_attr(block.attr, *decls);
            }
        }

        for (Node::Ptr &stmt : block.stmts) {
            stmt->accept(*this);
        }
    }

    virtual void visit_program(Program &prog) {
        this->visit_block(prog);
    }

    virtual void visit_declare_list(S_DeclareList &decls) {
        for (const auto &pair : decls.decls) {
            if (pair.initial) {
                pair.initial->accept(*this);
            }
        }
    }

    virtual void visit_condition(S_Condition &cond) {
        cond.condition->accept(*this);
        cond.then_block->accept(*this);
        if (cond.else_block) {
            cond.else_block->accept(*this);
        }
    }

    virtual void visit_while(S_While &wh) {
        wh.condition->accept(*this);
        wh.block->accept(*this);
    }

    virtual void visit_return(S_Return &ret) {
        if (ret.value) {
            ret.value->accept(*this);
        }
    }

    virtual void visit_stmt_exp(S_Exp &stmt) {
        stmt.value->accept(*this);
    }

    virtual void visit_op(E_Op &exp) {
        for (Node::Ptr &arg : exp.args) {
            arg->accept(*this);
        }
    }

    virtual void visit_var(E_Var &var) {
        assert(this->cur_block);
        S_Block::AttrType &attr = this->cur_block->attr;
        auto it = attr.name_to_local_index.find(var.name);
        if (it != attr.name_to_local_index.end()) {
            var.attr.is_local = true;
            var.attr.index = it->second;
        } else {
            var.attr.is_local = false;
            var.attr.index = add_nonlocal_to_block_attr(attr, var.name, attr.parent);
        }
    }

    virtual void visit_func(E_Func &func) {
        S_Block &func_block = static_cast<S_Block &>(*func.block);

        if (func.args) {
            auto _ = this->enter(func_block);
            func.args->accept(*this);
            add_declarations_to_block_attr(
                func_block.attr, static_cast<S_DeclareList &>(*func.args)
            );
        }

        func_block.accept(*this);
    }

    virtual void visit_list(E_List &list) {
        for (Node::Ptr &item : list.value) {
            item->accept(*this);
        }
    }

private:
    RestoreOnExit enter(S_Block &block) {
        S_Block *origin = this->cur_block;
        this->cur_block = &block;
        block.attr.parent = origin;
        return RestoreOnExit(&this->cur_block, origin);
    }

    S_Block *cur_block = nullptr;
};


void resolve_names_in_block(S_Block &block, Node &node) {
    if (S_DeclareList *decls = dynamic_cast<S_DeclareList *>(&node)) {
        add_declarations_to_block_attr(block.attr, *decls);
    }
    Resolver res(&block);
    node.accept(res);
}


void resolve_names(S_Block &block) {
    Resolver(nullptr).visit_block(block);
}
