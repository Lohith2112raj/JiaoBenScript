#ifndef JIAOBENSCRIPT_NODE_H
#define JIAOBENSCRIPT_NODE_H

#include <cassert>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "sourcepos.h"
#include "unicode.h"
#include "string_fmt.hpp"
#include "repr.hpp"


class NodeVisitor;


struct Node {
    typedef std::unique_ptr<Node> Ptr;

    virtual ~Node() {}
    virtual bool operator==(const Node &rhs) const;
    bool operator!=(const Node &rhs) const;
    virtual std::string repr(uint32_t = 0) const {
        return "<Node>";
    }
    virtual void accept(NodeVisitor &vis) = 0;

    SourcePos pos_start;
    SourcePos pos_end;
};


REPR(Node) {
    return value.repr();
}


struct S_Block : Node {
    struct AttrType {
        struct VarInfo {
            VarInfo(const ustring &name) : name(name) {}
            ustring name;

            bool operator==(const VarInfo &rhs) const {
                return this->name == rhs.name;
            }
        };

        struct NonLocalInfo {
            NonLocalInfo(S_Block *parent, int index) : parent(parent), index(index) {}

            S_Block *parent = nullptr;
            int index = -1;

            bool operator==(const NonLocalInfo &rhs) const {
                return this->parent == rhs.parent && this->index == rhs.index;
            }
        };

        S_Block *parent = nullptr;
        std::vector<VarInfo> local_info;
        std::vector<NonLocalInfo> nonlocal_indexes;

        // tmp
        std::map<ustring, int> name_to_local_index;
        std::map<ustring, int> name_to_nonlocal_index;
    };

    std::vector<Node::Ptr> stmts;
    AttrType attr {};

    virtual bool operator==(const Node &rhs) const override;
    virtual std::string repr(uint32_t indent = 0) const override;
    virtual void accept(NodeVisitor &vis) override;
};


struct Program : S_Block {
    virtual void accept(NodeVisitor &vis) override;
};


struct A_DeclareList {
    int start_index = -1;
};


struct S_DeclareList : Node {
    struct PairType {
        PairType(const ustring &name, Node::Ptr initial)
            : name(name), initial(std::move(initial))
        {}

        ustring name;
        Node::Ptr initial;
    };
    std::vector<PairType> decls;
    mutable A_DeclareList attr;

    virtual bool operator==(const Node &rhs) const override;
    virtual std::string repr(uint32_t indent = 0) const override;
    virtual void accept(NodeVisitor &vis) override;
};


struct S_Condition : Node {
    Node::Ptr condition;
    Node::Ptr then_block;
    Node::Ptr else_block;   // optional, S_Block or S_Condition

    virtual bool operator==(const Node &rhs) const override;
    virtual std::string repr(uint32_t indent = 0) const override;
    virtual void accept(NodeVisitor &vis) override;
};


struct S_While : Node {
    Node::Ptr condition;
    Node::Ptr block;

    virtual bool operator==(const Node &rhs) const override;
    virtual std::string repr(uint32_t indent = 0) const override;
    virtual void accept(NodeVisitor &vis) override;
};


// TODO: S_For, S_DoWhile


struct S_Return : Node {
    Node::Ptr value;    // optional

    virtual bool operator==(const Node &rhs) const override;
    virtual std::string repr(uint32_t indent = 0) const override;
    virtual void accept(NodeVisitor &vis) override;
};


struct S_Break : Node {
    virtual std::string repr(uint32_t indent = 0) const override;
    virtual void accept(NodeVisitor &vis) override;
};


struct S_Continue : Node {
    virtual std::string repr(uint32_t indent = 0) const override;
    virtual void accept(NodeVisitor &vis) override;
};


struct S_Exp : Node {
    Node::Ptr value;

    virtual bool operator==(const Node &rhs) const override;
    virtual std::string repr(uint32_t indent = 0) const override;
    virtual void accept(NodeVisitor &vis) override;
};


struct S_Empty : Node {
    virtual std::string repr(uint32_t indent = 0) const override;
    virtual void accept(NodeVisitor &vis) override;
};


enum class OpCode : uint32_t {
    // copied from TokenCode
    PLUS            = '+',
    MINUS           = '-',
    STAR            = '*',
    SLASH           = '/',
    PERCENT         = '%',
    LESS            = '<',
    LESSEQ          = '<=',
    GREAT           = '>',
    GREATEQ         = '>=',
    EQ              = '==',
    NEQ             = '!=',
    NOT             = '!',
    AND             = '&&',
    OR              = '||',
    ASSIGN          = '=',
    PLUS_ASSIGN     = '+=',
    MINUS_ASSIGN    = '-=',
    STAR_ASSIGN     = '*=',
    SLASH_ASSIGN    = '/=',
    PERCENT_ASSIGN  = '%=',

    CALL            = '()',
    SUBSCRIPT       = '[]',
    EXPLIST         = ',',
};


struct E_Op : Node {
    explicit E_Op(OpCode op_code) : op_code(op_code) {}

    OpCode op_code;
    std::vector<Node::Ptr> args;

    virtual bool operator==(const Node &rhs) const override;
    virtual std::string repr(uint32_t indent = 0) const override;
    virtual void accept(NodeVisitor &vis) override;
};


struct E_Var : Node {
    struct AttrType {
        bool is_local;
        int index = -1;
    };

    explicit E_Var(const ustring &name) : name(name) {}

    ustring name;
    AttrType attr {};

    virtual bool operator==(const Node &rhs) const override;
    virtual std::string repr(uint32_t indent = 0) const override;
    virtual void accept(NodeVisitor &vis) override;
};


struct E_Func : Node {
    Node::Ptr args;     // S_DeclareList, optional
    Node::Ptr block;

    virtual bool operator==(const Node &rhs) const override;
    virtual std::string repr(uint32_t indent = 0) const override;
    virtual void accept(NodeVisitor &vis) override;
};


template<class ValueType>
struct _E_Value : Node {
    typedef _E_Value<ValueType> _SelfType;
    explicit _E_Value<ValueType>(const ValueType &value) : value(value) {}

    ValueType value;

    virtual bool operator==(const Node &rhs) const override {
        const _SelfType *other = dynamic_cast<const _SelfType *>(&rhs);
        return other != nullptr && this->value == other->value;
    }
    virtual std::string repr(uint32_t indent = 0) const override;
    virtual void accept(NodeVisitor &vis) override;
};


typedef _E_Value<bool> E_Bool;
typedef _E_Value<int64_t> E_Int;
typedef _E_Value<double> E_Float;
typedef _E_Value<ustring> E_String;


struct E_List : Node {
    std::vector<Node::Ptr> value;

    virtual bool operator==(const Node &rhs) const override;
    virtual std::string repr(uint32_t indent = 0) const override;
    virtual void accept(NodeVisitor &vis) override;
};


struct E_Null : Node {
    virtual std::string repr(uint32_t indent = 0) const override;
    virtual void accept(NodeVisitor &vis) override;
};


#endif //JIAOBENSCRIPT_NODE_H
