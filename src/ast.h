#ifndef AST_H
#define AST_H

#include <string>
#include <vector>
#include <memory>
#include "pic_descriptor.h"
#include "token.h"

// ============================================================
// AST Node Definitions
// ============================================================

struct ASTNode
{
    virtual ~ASTNode() = default;
};

struct ProgramNode : ASTNode
{
    std::string programName;
    std::vector<std::unique_ptr<ASTNode>> fileControls;
    std::vector<std::unique_ptr<ASTNode>> fileDescriptions;
    std::vector<std::unique_ptr<ASTNode>> dataItems;
    std::vector<std::unique_ptr<ASTNode>> statements;
};

struct DataItemNode : ASTNode
{
    std::string name;
    int level;
    std::string pic;
    PicDescriptor picDesc;
    std::string value;
    bool isComp = false;
    int occursCount = 0;
    std::string indexedBy;
    std::string redefines;
};

struct FileControlNode : ASTNode
{
    std::string selectName;
    std::string assignName;
    bool assignIsVariable = false;  // true if ASSIGN TO is a variable name (not a string literal)

    enum class Organization { SEQUENTIAL, RELATIVE, INDEXED, LINE_SEQUENTIAL } organization = Organization::SEQUENTIAL;
    enum class AccessMode { SEQUENTIAL, RANDOM, DYNAMIC, KEYED, LINE_SEQUENTIAL } accessMode = AccessMode::SEQUENTIAL;

    std::string recordKeyName;
    std::string alternateRecordKey;
    bool alternateWithDuplicates = false;
    std::string relativeKeyName;
    std::string fileStatusVar;
    std::string lockMode; // "MANUAL" or "AUTOMATIC"
};

struct FileDescriptionNode : ASTNode
{
    std::string name;
    std::vector<std::unique_ptr<ASTNode>> records;
};

struct ConditionNode
{
    enum Op { EQ, NE, GT, LT, GE, LE } op;
    std::string left;
    std::string right;
    bool leftIsLiteral = false;
    bool rightIsLiteral = false;
    int line = 0;
};

struct MoveNode : ASTNode
{
    std::string source;
    std::string dest;
    bool sourceIsLiteral = false;
};

struct AddNode : ASTNode
{
    std::string left;
    std::string right;
    std::string dest;
    std::string remainderDest;
    bool leftIsLiteral = false;
    bool rightIsLiteral = false;
};

struct MultiplyNode : ASTNode
{
    std::string left;
    std::string right;
    std::string dest;
    std::string remainderDest;
    bool leftIsLiteral = false;
    bool rightIsLiteral = false;
};

struct SubtractNode : ASTNode
{
    std::string left;
    std::string right;
    std::string dest;
    std::string remainderDest;
    bool leftIsLiteral = false;
    bool rightIsLiteral = false;
};

struct DivideNode : ASTNode
{
    std::string left;
    std::string right;
    std::string dest;
    std::string remainderDest;
    bool leftIsLiteral = false;
    bool rightIsLiteral = false;
};

struct DisplayNode : ASTNode
{
    std::vector<std::string> operands;
    std::vector<bool> isLiteral;
};

struct PrintNode : ASTNode
{
    std::vector<std::string> operands;
    std::vector<bool> isLiteral;
};

struct StopRunNode : ASTNode {};

struct IfNode : ASTNode
{
    std::vector<std::unique_ptr<ConditionNode>> conditions;
    std::vector<TokenType> condOps;
    std::vector<std::unique_ptr<ASTNode>> thenStatements;
    std::vector<std::unique_ptr<ASTNode>> elseStatements;
};

struct PerformUntilNode : ASTNode
{
    std::unique_ptr<ConditionNode> condition;
    std::vector<std::unique_ptr<ASTNode>> body;
    bool testBefore = true;
};

struct PerformVaryingNode : ASTNode
{
    std::string counter;
    std::string from;
    std::string by;
    std::unique_ptr<ConditionNode> untilCondition;
    std::vector<std::unique_ptr<ASTNode>> body;
    bool fromIsLiteral = false;
    bool byIsLiteral = false;
    bool testBefore = true;
};

struct PerformTimesNode : ASTNode
{
    std::string count;
    bool countIsLiteral = false;
    std::vector<std::unique_ptr<ASTNode>> body;
};

struct PerformParagraphNode : ASTNode
{
    std::string target;
    std::unique_ptr<ConditionNode> untilCondition;
    bool testBefore = true;
};
struct GoToNode : ASTNode { std::string target; };
struct ParagraphNode : ASTNode { std::string name; };

struct SearchNode : ASTNode {
    std::string tableName;
    bool isSearchAll = false;
    bool hasAtEnd = false;
    bool hasNotAtEnd = false;
    std::vector<std::unique_ptr<ASTNode>> atEndStatements;
    std::vector<std::unique_ptr<ASTNode>> notAtEndStatements;
};

struct SetIndexNode : ASTNode {
    std::string indexName;
    int direction = 0;  // 1=UP, -1=DOWN, 0=TO (absolute)
    std::string amount;
    bool amountIsLiteral = false;
};

struct InspectNode : ASTNode
{
    std::string target;
    std::string oldValue;
    std::string newValue;
    bool oldIsFigurative = false;
    bool newIsFigurative = false;
};

struct OpenNode : ASTNode
{
    std::string fileName;
    std::string mode;
};

struct CloseNode : ASTNode { std::string fileName; };

struct StartNode : ASTNode
{
    std::string fileName;
    std::string keyVar;
    std::string comp;
    std::vector<std::unique_ptr<ASTNode>> invalidKeyStatements;
    std::vector<std::unique_ptr<ASTNode>> notInvalidKeyStatements;
};

struct ReadNode : ASTNode
{
    std::string fileName;
    std::string intoVar;
    std::string keyVar;
    bool nextRecord = false;
    bool hasAtEnd = false;
    std::vector<std::unique_ptr<ASTNode>> atEndStatements;
    bool hasNotAtEnd = false;
    std::vector<std::unique_ptr<ASTNode>> notAtEndStatements;
    bool hasInvalidKey = false;
    std::vector<std::unique_ptr<ASTNode>> invalidKeyStatements;
    bool hasNotInvalidKey = false;
    std::vector<std::unique_ptr<ASTNode>> notInvalidKeyStatements;
};

struct WriteNode : ASTNode
{
    std::string recordName;
    std::string fileName;
    bool hasInvalidKey = false;
    std::vector<std::unique_ptr<ASTNode>> invalidKeyStatements;
    bool hasNotInvalidKey = false;
    std::vector<std::unique_ptr<ASTNode>> notInvalidKeyStatements;
};

struct ExprNode { virtual ~ExprNode() = default; };
struct LiteralExpr : ExprNode { std::string value; };
struct VariableExpr : ExprNode { std::string name; };

struct BinaryExpr : ExprNode
{
    enum Op { ADD, SUB, MUL, DIV } op;
    std::unique_ptr<ExprNode> left;
    std::unique_ptr<ExprNode> right;
};

struct ComputeNode : ASTNode
{
    std::string dest;
    std::unique_ptr<ExprNode> expr;
};

struct AcceptNode : ASTNode
{
    std::string dest;
    std::string fromType;
};

struct EvaluateWhenClause
{
    std::vector<std::string> subjects;
    std::vector<bool> subjectIsLiteral;
    std::vector<std::unique_ptr<ConditionNode>> conditions;
    bool hasCondition = false;
    bool isOther = false;
    bool hasThru = false;
    std::string thruValue;
    bool thruValueIsLiteral = false;
    std::vector<std::unique_ptr<ASTNode>> body;
};

struct EvaluateNode : ASTNode
{
    std::string subject;
    bool subjectIsLiteral = false;
    bool subjectIsTrue = false;
    std::string testType;
    std::vector<std::unique_ptr<EvaluateWhenClause>> whenClauses;
};

struct StringSourceClause
{
    std::string source;
    bool sourceIsLiteral = false;
    bool delimitedBySize = false;
    char delimiterChar = 0;
    std::string delimiterVar;
    bool delimiterIsVariable = false;
};

struct StringNode : ASTNode
{
    std::vector<StringSourceClause> sources;
    std::string dest;
    std::string pointerVar;
    bool hasPointer = false;
    bool hasOverflow = false;
    bool hasNotOverflow = false;
    std::vector<std::unique_ptr<ASTNode>> overflowBody;
    std::vector<std::unique_ptr<ASTNode>> notOverflowBody;
};

struct UnstringIntoClause
{
    std::string dest;
    bool destIsLiteral = false;
};

struct UnstringNode : ASTNode
{
    std::string source;
    std::vector<UnstringIntoClause> intoClauses;
    std::string delimiter;
    bool delimiterIsLiteral = false;
    bool delimiterAll = false;
    std::string pointerVar;
    bool hasPointer = false;
    std::string tallyVar;
    bool hasTally = false;
};

#endif
