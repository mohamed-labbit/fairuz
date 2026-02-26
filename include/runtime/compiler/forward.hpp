#pragma once

namespace mylang {

namespace ast {

struct ASTNode;
struct Stmt;
struct Expr;
struct BinaryExpr;
struct UnaryExpr;
struct LiteralExpr;
struct NameExpr;
struct AssignmentExpr;
struct CallExpr;
struct ListExpr;
struct BlockStmt;
struct ExprStmt;
struct AssignmentStmt;
struct IfStmt;
struct WhileStmt;
struct ForStmt;
struct FunctionDef;
struct ReturnStmt;

}

namespace runtime {

struct ObjHeader;
struct ObjString;
struct ObjList;
struct ObjFunction;
struct ObjClosure;
struct ObjNative;

}

}
