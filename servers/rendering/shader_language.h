/*************************************************************************/
/*  shader_language.h                                                    */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#pragma once
 
#include "core/list.h"
#include "core/map.h"
#include "core/script_language.h"
#include "core/set.h"
#include "core/string_name.h"
#include "core/typedefs.h"
#include "core/variant.h"

class ShaderLanguage {

public:
    struct TkPos {
        int char_idx;
        int tk_line;
    };
    enum TokenType {
        TK_EMPTY,
        TK_IDENTIFIER,
        TK_TRUE,
        TK_FALSE,
        TK_REAL_CONSTANT,
        TK_INT_CONSTANT,
        TK_UINT_CONSTANT,
        TK_TYPE_VOID,
        TK_TYPE_BOOL,
        TK_TYPE_BVEC2,
        TK_TYPE_BVEC3,
        TK_TYPE_BVEC4,
        TK_TYPE_INT,
        TK_TYPE_IVEC2,
        TK_TYPE_IVEC3,
        TK_TYPE_IVEC4,
        TK_TYPE_UINT,
        TK_TYPE_UVEC2,
        TK_TYPE_UVEC3,
        TK_TYPE_UVEC4,
        TK_TYPE_FLOAT,
        TK_TYPE_VEC2,
        TK_TYPE_VEC3,
        TK_TYPE_VEC4,
        TK_TYPE_MAT2,
        TK_TYPE_MAT3,
        TK_TYPE_MAT4,
        TK_TYPE_SAMPLER2D,
        TK_TYPE_ISAMPLER2D,
        TK_TYPE_USAMPLER2D,
        TK_TYPE_SAMPLER2DARRAY,
        TK_TYPE_ISAMPLER2DARRAY,
        TK_TYPE_USAMPLER2DARRAY,
        TK_TYPE_SAMPLER3D,
        TK_TYPE_ISAMPLER3D,
        TK_TYPE_USAMPLER3D,
        TK_TYPE_SAMPLERCUBE,
        TK_TYPE_SAMPLEREXT,
        TK_INTERPOLATION_FLAT,
        TK_INTERPOLATION_SMOOTH,
        TK_CONST,
        TK_STRUCT,
        TK_PRECISION_LOW,
        TK_PRECISION_MID,
        TK_PRECISION_HIGH,
        TK_OP_EQUAL,
        TK_OP_NOT_EQUAL,
        TK_OP_LESS,
        TK_OP_LESS_EQUAL,
        TK_OP_GREATER,
        TK_OP_GREATER_EQUAL,
        TK_OP_AND,
        TK_OP_OR,
        TK_OP_NOT,
        TK_OP_ADD,
        TK_OP_SUB,
        TK_OP_MUL,
        TK_OP_DIV,
        TK_OP_MOD,
        TK_OP_SHIFT_LEFT,
        TK_OP_SHIFT_RIGHT,
        TK_OP_ASSIGN,
        TK_OP_ASSIGN_ADD,
        TK_OP_ASSIGN_SUB,
        TK_OP_ASSIGN_MUL,
        TK_OP_ASSIGN_DIV,
        TK_OP_ASSIGN_MOD,
        TK_OP_ASSIGN_SHIFT_LEFT,
        TK_OP_ASSIGN_SHIFT_RIGHT,
        TK_OP_ASSIGN_BIT_AND,
        TK_OP_ASSIGN_BIT_OR,
        TK_OP_ASSIGN_BIT_XOR,
        TK_OP_BIT_AND,
        TK_OP_BIT_OR,
        TK_OP_BIT_XOR,
        TK_OP_BIT_INVERT,
        TK_OP_INCREMENT,
        TK_OP_DECREMENT,
        TK_CF_IF,
        TK_CF_ELSE,
        TK_CF_FOR,
        TK_CF_WHILE,
        TK_CF_DO,
        TK_CF_SWITCH,
        TK_CF_CASE,
        TK_CF_DEFAULT,
        TK_CF_BREAK,
        TK_CF_CONTINUE,
        TK_CF_RETURN,
        TK_CF_DISCARD,
        TK_BRACKET_OPEN,
        TK_BRACKET_CLOSE,
        TK_CURLY_BRACKET_OPEN,
        TK_CURLY_BRACKET_CLOSE,
        TK_PARENTHESIS_OPEN,
        TK_PARENTHESIS_CLOSE,
        TK_QUESTION,
        TK_COMMA,
        TK_COLON,
        TK_SEMICOLON,
        TK_PERIOD,
        TK_UNIFORM,
        TK_INSTANCE,
        TK_GLOBAL,
        TK_VARYING,
        TK_ARG_IN,
        TK_ARG_OUT,
        TK_ARG_INOUT,
        TK_RENDER_MODE,
        TK_HINT_WHITE_TEXTURE,
        TK_HINT_BLACK_TEXTURE,
        TK_HINT_TRANSPARENT_TEXTURE,
        TK_HINT_NORMAL_TEXTURE,
        TK_HINT_ROUGHNESS_NORMAL_TEXTURE,
        TK_HINT_ROUGHNESS_R,
        TK_HINT_ROUGHNESS_G,
        TK_HINT_ROUGHNESS_B,
        TK_HINT_ROUGHNESS_A,
        TK_HINT_ROUGHNESS_GRAY,
        TK_HINT_ANISO_TEXTURE,
        TK_HINT_ALBEDO_TEXTURE,
        TK_HINT_BLACK_ALBEDO_TEXTURE,
        TK_HINT_COLOR,
        TK_HINT_RANGE,
        TK_HINT_INSTANCE_INDEX,
        TK_FILTER_NEAREST,
        TK_FILTER_LINEAR,
        TK_FILTER_NEAREST_MIPMAP,
        TK_FILTER_LINEAR_MIPMAP,
        TK_FILTER_NEAREST_MIPMAP_ANISO,
        TK_FILTER_LINEAR_MIPMAP_ANISO,
        TK_REPEAT_ENABLE,
        TK_REPEAT_DISABLE,
        TK_SHADER_TYPE,
        TK_CURSOR,
        TK_ERROR,
        TK_EOF,
        TK_MAX
    };

/* COMPILER */

// lame work around to Apple defining this as a macro in 10.12 SDK
#ifdef TYPE_BOOL
#undef TYPE_BOOL
#endif

    enum DataType : int8_t {
        TYPE_VOID,
        TYPE_BOOL,
        TYPE_BVEC2,
        TYPE_BVEC3,
        TYPE_BVEC4,
        TYPE_INT,
        TYPE_IVEC2,
        TYPE_IVEC3,
        TYPE_IVEC4,
        TYPE_UINT,
        TYPE_UVEC2,
        TYPE_UVEC3,
        TYPE_UVEC4,
        TYPE_FLOAT,
        TYPE_VEC2,
        TYPE_VEC3,
        TYPE_VEC4,
        TYPE_MAT2,
        TYPE_MAT3,
        TYPE_MAT4,
        TYPE_SAMPLER2D,
        TYPE_ISAMPLER2D,
        TYPE_USAMPLER2D,
        TYPE_SAMPLER2DARRAY,
        TYPE_ISAMPLER2DARRAY,
        TYPE_USAMPLER2DARRAY,
        TYPE_SAMPLER3D,
        TYPE_ISAMPLER3D,
        TYPE_USAMPLER3D,
        TYPE_SAMPLERCUBE,
        TYPE_SAMPLEREXT, //TYPE_SAMPLERCUBEARRAY,
        TYPE_STRUCT,
        TYPE_MAX
    };

    enum DataPrecision : int8_t {
        PRECISION_LOWP,
        PRECISION_MEDIUMP,
        PRECISION_HIGHP,
        PRECISION_DEFAULT,
    };

    enum DataInterpolation : int8_t {
        INTERPOLATION_FLAT,
        INTERPOLATION_SMOOTH,
    };

    enum Operator : int8_t {
        OP_EQUAL,
        OP_NOT_EQUAL,
        OP_LESS,
        OP_LESS_EQUAL,
        OP_GREATER,
        OP_GREATER_EQUAL,
        OP_AND,
        OP_OR,
        OP_NOT,
        OP_NEGATE,
        OP_ADD,
        OP_SUB,
        OP_MUL,
        OP_DIV,
        OP_MOD,
        OP_SHIFT_LEFT,
        OP_SHIFT_RIGHT,
        OP_ASSIGN,
        OP_ASSIGN_ADD,
        OP_ASSIGN_SUB,
        OP_ASSIGN_MUL,
        OP_ASSIGN_DIV,
        OP_ASSIGN_MOD,
        OP_ASSIGN_SHIFT_LEFT,
        OP_ASSIGN_SHIFT_RIGHT,
        OP_ASSIGN_BIT_AND,
        OP_ASSIGN_BIT_OR,
        OP_ASSIGN_BIT_XOR,
        OP_BIT_AND,
        OP_BIT_OR,
        OP_BIT_XOR,
        OP_BIT_INVERT,
        OP_INCREMENT,
        OP_DECREMENT,
        OP_SELECT_IF,
        OP_SELECT_ELSE, //used only internally, then only IF appears with 3 arguments
        OP_POST_INCREMENT,
        OP_POST_DECREMENT,
        OP_CALL,
        OP_CONSTRUCT,
        OP_STRUCT,
        OP_INDEX,
        OP_MAX
    };

    enum FlowOperation : int8_t {
        FLOW_OP_IF,
        FLOW_OP_RETURN,
        FLOW_OP_FOR,
        FLOW_OP_WHILE,
        FLOW_OP_DO,
        FLOW_OP_BREAK,
        FLOW_OP_SWITCH,
        FLOW_OP_CASE,
        FLOW_OP_DEFAULT,
        FLOW_OP_CONTINUE,
        FLOW_OP_DISCARD
    };

    enum ArgumentQualifier : int8_t {
        ARGUMENT_QUALIFIER_IN,
        ARGUMENT_QUALIFIER_OUT,
        ARGUMENT_QUALIFIER_INOUT,
    };

    enum SubClassTag  : int8_t{
        TAG_GLOBAL,
        TAG_ARRAY,
    };
    struct VaryingFunctionNames {
        StringName fragment;
        StringName vertex;
        StringName light;
        VaryingFunctionNames() :
            fragment("fragment"),
            vertex("vertex"),
            light("light")
        {
        }
    };

    struct Node {
        enum Type : int8_t {
            TYPE_SHADER,
            TYPE_FUNCTION,
            TYPE_BLOCK,
            TYPE_VARIABLE,
            TYPE_VARIABLE_DECLARATION,
            TYPE_CONSTANT,
            TYPE_OPERATOR,
            TYPE_CONTROL_FLOW,
            TYPE_MEMBER,
            TYPE_ARRAY,
            TYPE_ARRAY_DECLARATION,
            TYPE_ARRAY_CONSTRUCT,
            TYPE_STRUCT,
        };

        Node *next = nullptr;
        Type type;

        virtual DataType get_datatype() const { return TYPE_VOID; }
        virtual StringName get_datatype_name() const { return StringName(); }
        Node(Type t) : type(t) {}
        virtual ~Node() = default;
    };

    template <class T>
    T *alloc_node() {
        T *node = memnew(T);
        node->next = nodes;
        nodes = node;
        return node;
    }

    Node *nodes=nullptr;

    struct OperatorNode : public Node {
        DataType return_cache = TYPE_VOID;
        DataPrecision return_precision_cache=PRECISION_DEFAULT;
        Operator op=OP_EQUAL;
        Vector<Node *> arguments;
        StringName struct_name;

        DataType get_datatype() const override { return return_cache; }

        StringName get_datatype_name() const override { return struct_name; }
        OperatorNode() :
                Node(TYPE_OPERATOR) {}
    };

    struct VariableNode : public Node {
        DataType datatype_cache = TYPE_VOID;
        StringName name;
        StringName struct_name;
        bool is_const = false;
        bool is_local = false;

        DataType get_datatype() const override { return datatype_cache; }
        StringName get_datatype_name() const override { return struct_name; }

        VariableNode() : Node(TYPE_VARIABLE) {}
    };

    struct VariableDeclarationNode : public Node {
        StringName struct_name;
        DataPrecision precision=PRECISION_DEFAULT;
        DataType datatype = TYPE_VOID;
        bool is_const=false;

        struct Declaration {
            StringName name;
            Node *initializer;
        };

        Vector<Declaration> declarations;
        DataType get_datatype() const override { return datatype; }

        VariableDeclarationNode() :
                Node(TYPE_VARIABLE_DECLARATION) {}
    };

    struct ArrayNode : public Node {
        DataType datatype_cache=TYPE_VOID;
        StringName struct_name;
        StringName name;
        Node *index_expression=nullptr;
        Node *call_expression=nullptr;
        Node *assign_expression=nullptr;
        bool is_const=false;
        bool is_local;

        DataType get_datatype() const override { return datatype_cache; }
        StringName get_datatype_name() const override { return struct_name; }

        ArrayNode() :
                Node(TYPE_ARRAY) {}
    };
    struct ArrayConstructNode : public Node {
        DataType datatype = TYPE_VOID;
        StringName struct_name;
        Vector<Node *> initializer;

        ArrayConstructNode() :
                Node(TYPE_ARRAY_CONSTRUCT) {
        }
    };
    struct ArrayDeclarationNode : public Node {
        StringName struct_name;
        DataPrecision precision=PRECISION_DEFAULT;
        DataType datatype=TYPE_VOID;
        bool is_const=false;

        struct Declaration {
            StringName name;
            uint32_t size;
            Vector<Node *> initializer;
        };

        Vector<Declaration> declarations;
        DataType get_datatype() const override { return datatype; }

        ArrayDeclarationNode() :
                Node(TYPE_ARRAY_DECLARATION) {}
    };

    struct ConstantNode : public Node {
        DataType datatype=TYPE_VOID;
        StringName struct_name;
        int array_size = 0;

        union Value {
            bool boolean;
            float real;
            int32_t sint;
            uint32_t uint;
        };

        Vector<Value> values;
        Vector<ArrayDeclarationNode::Declaration> array_declarations;
        DataType get_datatype() const override { return datatype; }
        StringName get_datatype_name() const override { return struct_name; }

        ConstantNode() : Node(TYPE_CONSTANT) {}
    };

    struct FunctionNode;

    struct BlockNode : public Node {
        enum BlockType {
            BLOCK_TYPE_STANDART,
            BLOCK_TYPE_FOR,
            BLOCK_TYPE_SWITCH,
            BLOCK_TYPE_CASE,
            BLOCK_TYPE_DEFAULT,
        };

        struct Variable {
            StringName struct_name;
            int line; //for completion
            int array_size;
            DataType type;
            DataPrecision precision;
            bool is_const;
        };

        FunctionNode *parent_function=nullptr;
        BlockNode *parent_block=nullptr;
        HashMap<StringName, Variable> variables;
        Vector<Node *> statements;
        int block_type=BLOCK_TYPE_STANDART;
        SubClassTag block_tag=SubClassTag::TAG_GLOBAL;
        bool single_statement=false;

        BlockNode() :
                Node(TYPE_BLOCK) {}
    };

    struct ControlFlowNode : public Node {
        FlowOperation flow_op=FLOW_OP_IF;
        Vector<Node *> expressions;
        Vector<BlockNode *> blocks;

        ControlFlowNode() :
                Node(TYPE_CONTROL_FLOW) {}
    };

    struct MemberNode : public Node {
        StringName base_struct_name;
        StringName struct_name;
        StringName name;
        DataType basetype=TYPE_VOID;
        DataPrecision precision;
        DataType datatype=TYPE_VOID;
        int array_size=0;
        Node *owner = nullptr;

        Node *index_expression= nullptr;
        Node *assign_expression= nullptr;
        bool has_swizzling_duplicates=false;
        bool basetype_const = false;
        DataType get_datatype() const override { return datatype; }

        StringName get_datatype_name() const override { return struct_name; }
        MemberNode() : Node(TYPE_MEMBER) {}
    };

    struct StructNode : public Node {
        Vector<MemberNode *> members;
        StructNode() :
                Node(TYPE_STRUCT) {}
    };
    struct FunctionNode : public Node {
        struct Argument {
            StringName name;
            StringName type_str;
            ArgumentQualifier qualifier;
            DataType type;
            DataPrecision precision;
            bool is_const;
        };

        StringName name;
        StringName return_struct_name;
        DataType return_type;
        DataPrecision return_precision;
        Vector<Argument> arguments;
        BlockNode *body;
        bool can_discard;

        FunctionNode() :
                Node(TYPE_FUNCTION),
                return_type(TYPE_VOID),
                return_precision(PRECISION_DEFAULT),
                body(nullptr),
                can_discard(false) {}
    };

    struct ShaderNode : public Node {

        struct Constant {
            StringName name;
            StringName type_str;
            ConstantNode *initializer;
            DataType type;
            DataPrecision precision;
            int array_size;
        };

        struct Function {
            StringName name;
            FunctionNode *function;
            HashSet<StringName> uses_function {};
            bool callable;
        };
        struct Struct {
            StringName name;
            StructNode *shader_struct;
        };
        struct Varying {
            enum Stage {
                STAGE_UNKNOWN,
                STAGE_VERTEX, // transition stage to STAGE_VERTEX_TO_FRAGMENT_LIGHT, emits warning if it's not used
                STAGE_FRAGMENT, // transition stage to STAGE_FRAGMENT_TO_LIGHT, emits warning if it's not used
                STAGE_VERTEX_TO_FRAGMENT_LIGHT,
                STAGE_FRAGMENT_TO_LIGHT,
        };

            Stage stage = STAGE_UNKNOWN;
            DataType type=TYPE_VOID;
            DataInterpolation interpolation=INTERPOLATION_FLAT;
            DataPrecision precision=PRECISION_DEFAULT;
            int array_size=0;
            TkPos tkpos;
        };

        struct Uniform {
            enum Hint {
                HINT_NONE,
                HINT_COLOR,
                HINT_RANGE,
                HINT_ALBEDO,
                HINT_BLACK_ALBEDO,
                HINT_NORMAL,
                HINT_ROUGHNESS_NORMAL,
                HINT_ROUGHNESS_R,
                HINT_ROUGHNESS_G,
                HINT_ROUGHNESS_B,
                HINT_ROUGHNESS_A,
                HINT_ROUGHNESS_GRAY,
                HINT_BLACK,
                HINT_WHITE,
                HINT_TRANSPARENT,
                HINT_ANISO,
                HINT_MAX
            };

            int order=0;
            int texture_order=0;
            DataType type=TYPE_VOID;
            DataPrecision precision=PRECISION_DEFAULT;
            Vector<ConstantNode::Value> default_value;
            Hint hint=HINT_NONE;
            float hint_range[3];

            Uniform() {
                hint_range[0] = 0.0f;
                hint_range[1] = 1.0f;
                hint_range[2] = 0.001f;
            }
        };

        HashMap<StringName, Constant> constants;
        HashMap<StringName, Varying> varyings;
        HashMap<StringName, Uniform> uniforms;
        HashMap<StringName, Struct> structs;
        Vector<StringName> render_modes;

        Vector<Function> functions;
        Vector<Constant> vconstants;
        Vector<Struct> vstructs;

        ShaderNode() :
                Node(TYPE_SHADER) {}
    };

    struct Expression {
        bool is_op;
        union {
            Operator op;
            Node *node;
        };
    };

    struct VarInfo {
        StringName name;
        DataType type;
    };

    enum CompletionType {
        COMPLETION_NONE,
        COMPLETION_SHADER_TYPE,
        COMPLETION_RENDER_MODE,
        COMPLETION_MAIN_FUNCTION,
        COMPLETION_IDENTIFIER,
        COMPLETION_FUNCTION_CALL,
        COMPLETION_CALL_ARGUMENTS,
        COMPLETION_INDEX,
        COMPLETION_STRUCT,
        COMPLETION_HINT,
    };

    struct Token {
        StringName text;
        double constant;
        uint16_t line;
        TokenType type;
        bool is_integer_constant() const {
            return type == TK_INT_CONSTANT || type == TK_UINT_CONSTANT;
        }
    };

    static const char *get_operator_text(Operator p_op);
    static String get_token_text(const Token& p_token);

    static bool is_token_datatype(TokenType p_type);
    static bool is_token_variable_datatype(TokenType p_type);
    static DataType get_token_datatype(TokenType p_type);
    static bool is_token_interpolation(TokenType p_type);
    static DataInterpolation get_token_interpolation(TokenType p_type);
    static bool is_token_precision(TokenType p_type);
    static DataPrecision get_token_precision(TokenType p_type);
    static const char *get_precision_name(DataPrecision p_type);
    static const char *get_datatype_name(DataType p_type);
    static bool is_token_nonvoid_datatype(TokenType p_type);
    static bool is_token_operator(TokenType p_type);
    static bool is_token_operator_assign(TokenType p_type);
    static bool is_token_hint(TokenType p_type);

    static bool convert_constant(ConstantNode *p_constant, DataType p_to_type, ConstantNode::Value *p_value = nullptr);
    static DataType get_scalar_type(DataType p_type);
    static int get_cardinality(DataType p_type);
    static bool is_scalar_type(DataType p_type);
    static bool is_sampler_type(DataType p_type);
    static Variant constant_value_to_variant(const Vector<ConstantNode::Value> &p_value, DataType p_type, ShaderLanguage::ShaderNode::Uniform::Hint p_hint = ShaderLanguage::ShaderNode::Uniform::HINT_NONE);

    static void get_keyword_list(Vector<StringView> *r_keywords);
    static bool is_control_flow_keyword(StringView p_keyword);
    static void get_builtin_funcs(Vector<String> *r_keywords);

    struct BuiltInInfo {
        DataType type;
        bool constant;

        BuiltInInfo() :
                type(TYPE_VOID),
                constant(false) {}

        BuiltInInfo(DataType p_type, bool p_constant = false) :
                type(p_type),
                constant(p_constant) {}
    };

    struct FunctionInfo {
        HashMap<StringName, BuiltInInfo> built_ins;
        bool can_discard = false;
        bool main_function = false;
    };
    static bool has_builtin(const HashMap<StringName, FunctionInfo> &p_functions, const StringName &p_name);

private:
    struct KeyWord {
        TokenType token;
        const char *text;
    };

    static const KeyWord keyword_list[];

    bool error_set;
    String error_str;
    int error_line;

    String code;
    int char_idx;
    int tk_line;

    StringName current_function;
    bool last_const = false;

    VaryingFunctionNames varying_function_names;

    TkPos _get_tkpos() {
        TkPos tkp;
        tkp.char_idx = char_idx;
        tkp.tk_line = tk_line;
        return tkp;
    }

    void _set_tkpos(TkPos p_pos) {
        char_idx = p_pos.char_idx;
        tk_line = p_pos.tk_line;
    }

    void _set_error_ui(const UIString &p_str);
    void _set_error(StringView p_str);

    static const char *token_names[TK_MAX];

    Token _make_token(TokenType p_type, const StringName &p_text = StringName());
    Token _get_token();

    ShaderNode *shader;

    enum IdentifierType {
        IDENTIFIER_FUNCTION,
        IDENTIFIER_UNIFORM,
        IDENTIFIER_VARYING,
        IDENTIFIER_FUNCTION_ARGUMENT,
        IDENTIFIER_LOCAL_VAR,
        IDENTIFIER_BUILTIN_VAR,
        IDENTIFIER_CONSTANT,
    };

    bool _find_identifier(const BlockNode *p_block, const HashMap<StringName, BuiltInInfo> &p_builtin_types, const StringName &p_identifier, DataType *r_data_type = nullptr, IdentifierType *r_type = nullptr, bool *r_is_const = nullptr, int *r_array_size = nullptr, StringName *r_struct_name = nullptr, ConstantNode::Value *r_constant_value = nullptr);
    bool _is_operator_assign(Operator p_op) const;
    bool _validate_assign(Node *p_node, const HashMap<StringName, BuiltInInfo> &p_builtin_types, String *r_message = nullptr);
    bool _validate_operator(OperatorNode *p_op, DataType *r_ret_type = nullptr);

    struct BuiltinFuncDef {
        enum { MAX_ARGS = 5 };
        const char *name;
        DataType rettype;
        const DataType args[MAX_ARGS];
        SubClassTag tag;
        bool high_end;
    };

    struct BuiltinFuncOutArgs { //arguments used as out in built in functions
        const char *name;
        int argument;
    };

    CompletionType completion_type;
    int completion_line;
    BlockNode *completion_block;
    DataType completion_base;
    SubClassTag completion_class = TAG_GLOBAL;
    StringName completion_function;
    StringName completion_struct;
    int completion_argument;

    bool _get_completable_identifier(BlockNode *p_block, CompletionType p_type, StringName &identifier);
    static const BuiltinFuncDef builtin_func_defs[];
    static const BuiltinFuncOutArgs builtin_func_out_args[];

    static bool is_const_suffix_lut_initialized;

    bool _compare_datatypes_in_nodes(Node *a, Node *b) const;

    bool _validate_function_call(BlockNode *p_block, OperatorNode *p_func, DataType *r_ret_type, StringName *r_ret_type_str);
    bool _parse_function_arguments(BlockNode *p_block, const HashMap<StringName, BuiltInInfo> &p_builtin_types, OperatorNode *p_func, int *r_complete_arg = nullptr);
    bool _validate_varying_assign(ShaderNode::Varying &p_varying, String *r_message);

    Node *_parse_expression(BlockNode *p_block, const HashMap<StringName, BuiltInInfo> &p_builtin_types);
    Error _parse_array_size(BlockNode *p_block, int *r_array_size);
    Node *_parse_array_constructor(BlockNode *p_block, const HashMap<StringName, BuiltInInfo> &p_builtin_types, DataType p_type, const StringName &p_struct_name, int p_array_size);
    ShaderLanguage::Node *_reduce_expression(BlockNode *p_block, ShaderLanguage::Node *p_node);

    Node *_parse_and_reduce_expression(BlockNode *p_block, const HashMap<StringName, BuiltInInfo> &p_builtin_types);
    Error _parse_block(BlockNode *p_block, const HashMap<StringName, BuiltInInfo> &p_builtin_types, bool p_just_one = false, bool p_can_break = false, bool p_can_continue = false);
    Error _parse_shader(const HashMap<StringName, FunctionInfo> &p_functions, const Vector<StringName> &p_render_modes, const HashSet<StringName> &p_shader_types);

    Error _find_last_flow_op_in_block(BlockNode *p_block, FlowOperation p_op);
    Error _find_last_flow_op_in_op(ControlFlowNode *p_flow, FlowOperation p_op);
public:
    //static void get_keyword_list(ShaderType p_type,List<String> *p_keywords);

    void clear();

    static String get_shader_type(const String &p_code);
    Error compile(const String &p_code, const HashMap<StringName, FunctionInfo> &p_functions, const Vector<StringName> &p_render_modes, const HashSet<StringName> &p_shader_types);
    Error complete(const String &p_code, const HashMap<StringName, FunctionInfo> &p_functions, const Vector<StringName> &p_render_modes, const HashSet<StringName> &p_shader_types, Vector
            <ScriptCodeCompletionOption> *r_options, String &r_call_hint);

    const String &get_error_text();
    int get_error_line();

    ShaderNode *get_shader();

    String token_debug(const String &p_code);

    ShaderLanguage();
    ~ShaderLanguage();
protected:
    String _get_shader_type_list(const HashSet<StringName> &p_shader_types) const;
};
