#include "code_generator.h"
#include "ast.h"
#include <sstream>
#include <map>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <fstream>

class CodeGenerator
{
    std::ostringstream dataSection;
    std::ostringstream bssSection;
    std::ostringstream textSection;
    std::ostringstream helpers;
    std::map<std::string, std::string> variables;
    std::map<std::string, int> fieldSizes;
    std::map<std::string, bool> fieldSigned;
    std::map<std::string, bool> fieldNumeric;
    std::map<std::string, std::string> fileAssignments;
    std::map<std::string, std::string> fileFdNames;
    std::map<std::string, int> fileRecordSizes;
    std::map<std::string, std::string> fileRecordVars;
    std::map<std::string, std::string> fileRecordKeyVars;
    std::map<std::string, std::string> fileRelativeKeyVars;
    std::map<std::string, std::string> fileStatusVars;
    std::map<std::string, std::string> filePrefetchFlagNames;
    std::map<std::string, std::string> filePrefetchKeyTemps;
    int labelCounter = 0;
    std::string programName;
    bool needsDecimalInc = false;
    bool needsAsciiHelpers = false;
    bool programEndsWithStopRun = false;

    std::string newLabel(const std::string &prefix)
    {
        return prefix + "_" + std::to_string(labelCounter++);
    }

    std::string getAsmName(const std::string &cobolName)
    {
        std::string name = cobolName;
        for (char &c : name)
            if (c == '-')
                c = '_';
        return name;
    }

    int CodeGenerator::resolveSize(const std::string &cobolName)
    {
        // 1. Check cache
        auto it = fieldSizes.find(cobolName);
        if (it != fieldSizes.end())
            return it->second;

        // 2. Search in WORKING-STORAGE
        size_t idx = 0;
        for (const auto& node : programAst->dataItems) {
            if (auto* d = dynamic_cast<DataItemNode*>(node.get())) {
                if (d->name == cobolName) {
                    auto [size, count] = computeGroupSize(programAst->dataItems, idx);
                    fieldSizes[cobolName] = size;   // cache it
                    return size;
                }
            }
            ++idx;
        }

        // 3. Search in FILE SECTION records
        for (const auto& fd : programAst->fileDescriptions) {
            if (auto* f = dynamic_cast<FileDescriptionNode*>(fd.get())) {
                size_t idx2 = 0;
                for (const auto& rec : f->records) {
                    if (auto* d = dynamic_cast<DataItemNode*>(rec.get())) {
                        if (d->name == cobolName) {
                            auto [size, count] = computeGroupSize(f->records, idx2);
                            fieldSizes[cobolName] = size;
                            return size;
                        }
                    }
                    ++idx2;
                }
            }
        }

        // 4. Fallback
        return 1;
    }

    void ensurePrefetchBuffers(const std::string &fileName, int keySize)
    {
        if (filePrefetchFlagNames.find(fileName) != filePrefetchFlagNames.end())
            return;
        std::string flag = newLabel("prefetch_flag_" + getAsmName(fileName));
        std::string temp = newLabel("prefetch_key_" + getAsmName(fileName));
        filePrefetchFlagNames[fileName] = flag;
        filePrefetchKeyTemps[fileName] = temp;
        bssSection << "    " << flag << ": resb 1\n";
        bssSection << "    " << temp << ": resb " << keySize << "\n";
    }

    std::pair<int, size_t> computeGroupSize(const std::vector<std::unique_ptr<ASTNode>> &items, size_t index) const
    {
        auto *item = dynamic_cast<DataItemNode *>(items[index].get());
        if (!item)
            return {0, 1};
        if (item->picDesc.storageSize > 0)
            return {item->picDesc.storageSize, 1};

        int total = 0;
        size_t i = index + 1;
        while (i < items.size())
        {
            auto *next = dynamic_cast<DataItemNode *>(items[i].get());
            if (!next)
            {
                ++i;
                continue;
            }
            if (next->level <= item->level)
                break;
            auto [size, count] = computeGroupSize(items, i);
            total += size;
            i += count;
        }
        return {total, i - index};
    }

    void emitLiteral(const std::string &label, const std::string &value)
    {
        dataSection << "    " << label << ": db ";
        // Build string portion and a list of raw byte values for non-printable chars
        std::string strPart;
        std::vector<unsigned char> rawBytes;
        for (unsigned char c : value)
        {
            if (c == '"')
            {
                strPart += "\\\"";
            }
            else if (c == '\\')
            {
                strPart += "\\\\";
            }
            else if (c == '\n')
            {
                if (!strPart.empty())
                {
                    dataSection << "\"" << strPart << "\",";
                    strPart.clear();
                }
                rawBytes.push_back(c);
                continue;
            }
            else if (c == '\r')
            {
                if (!strPart.empty())
                {
                    dataSection << "\"" << strPart << "\",";
                    strPart.clear();
                }
                rawBytes.push_back(c);
                continue;
            }
            else if (c == '\t')
            {
                strPart += "\\t";
            }
            else if (c >= 0x20 && c <= 0x7E)
            {
                strPart += static_cast<char>(c);
            }
            else
            {
                // Non-printable or control char: flush current string part
                if (!strPart.empty())
                {
                    dataSection << "\"" << strPart << "\",";
                    strPart.clear();
                }
                rawBytes.push_back(c);
            }
        }
        if (!strPart.empty())
        {
            dataSection << "\"" << strPart << "\"";
        }
        if (!rawBytes.empty())
        {
            if (!strPart.empty())
                dataSection << ",";
            for (size_t i = 0; i < rawBytes.size(); ++i)
            {
                if (i > 0) dataSection << ",";
                dataSection << " 0x" << std::hex << std::uppercase << static_cast<int>(rawBytes[i]);
            }
            dataSection << std::dec;
        }
        dataSection << "\n";
        dataSection << "    " << label << "_len equ $ - " << label << "\n";
    }

    void emitCString(const std::string &label, const std::string &value)
    {
        dataSection << "    " << label << ": db ";
        std::string strPart;
        std::vector<unsigned char> rawBytes;
        for (unsigned char c : value)
        {
            if (c == '"')
            {
                strPart += "\\\"";
            }
            else if (c == '\\')
            {
                strPart += "\\\\";
            }
            else if (c == '\n')
            {
                if (!strPart.empty())
                {
                    dataSection << "\"" << strPart << "\",";
                    strPart.clear();
                }
                rawBytes.push_back(c);
                continue;
            }
            else if (c == '\r')
            {
                if (!strPart.empty())
                {
                    dataSection << "\"" << strPart << "\",";
                    strPart.clear();
                }
                rawBytes.push_back(c);
                continue;
            }
            else if (c == '\t')
            {
                strPart += "\\t";
            }
            else if (c >= 0x20 && c <= 0x7E)
            {
                strPart += static_cast<char>(c);
            }
            else
            {
                if (!strPart.empty())
                {
                    dataSection << "\"" << strPart << "\",";
                    strPart.clear();
                }
                rawBytes.push_back(c);
            }
        }
        if (!strPart.empty())
        {
            dataSection << "\"" << strPart << "\",";
        }
        if (!rawBytes.empty())
        {
            for (size_t i = 0; i < rawBytes.size(); ++i)
            {
                if (i > 0) dataSection << ",";
                dataSection << " 0x" << std::hex << std::uppercase << static_cast<int>(rawBytes[i]);
            }
            dataSection << std::dec;
        }
        // Append null terminator
        dataSection << ", 0\n";
        dataSection << "    " << label << "_len equ $ - " << label << "\n";
    }

    std::string emitPaddedLiteral(const std::string &value, int targetSize, const std::string &prefix)
    {
        std::string padded = value;
        if (targetSize > 0)
        {
            bool isNegative = false;
            bool hasSign = false;
            if (!padded.empty() && (padded[0] == '-' || padded[0] == '+'))
            {
                isNegative = (padded[0] == '-');
                hasSign = true;
                padded = padded.substr(1);
            }
            int digitCount = hasSign ? targetSize - 1 : targetSize;
            while ((int)padded.length() < digitCount)
            {
                padded = "0" + padded;
            }
            if (hasSign)
            {
                padded = (isNegative ? "-" : "+") + padded;
            }
        }
        std::string label = newLabel(prefix);
        emitLiteral(label, padded);
        return label;
    }

    void generateLexCompare(const std::string &leftLabel, const std::string &rightLabel, int len)
    {
        std::string doneLabel = newLabel("cmp_done");
        for (int i = 0; i < len; i++)
        {
            textSection << "    mov al, [" << leftLabel << "+" << i << "]\n";
            textSection << "    cmp al, [" << rightLabel << "+" << i << "]\n";
            if (i < len - 1)
            {
                textSection << "    jne " << doneLabel << "\n";
            }
        }
        textSection << doneLabel << ":\n";
    }

    bool isNumericLiteral(const std::string &s)
    {
        if (s.empty())
            return false;
        size_t i = (s[0] == '-' || s[0] == '+') ? 1 : 0;
        for (; i < s.size(); ++i)
            if (!std::isdigit(s[i]))
                return false;
        return true;
    }

    bool isNumericOperand(const std::string &name, bool isLiteral)
    {
        if (isLiteral)
            return isNumericLiteral(name);
        auto it = fieldNumeric.find(name);
        return (it != fieldNumeric.end() && it->second);
    }

    void generateNumericComparison(const ConditionNode *cond)
    {
        needsAsciiHelpers = true;
        if (cond->leftIsLiteral)
        {
            generateOperandToInt(cond->left, true, 0, false);
        }
        else
        {
            generateOperandToInt(cond->left, false, 0, fieldSigned[cond->left]);
        }
        textSection << "    push rax\n";

        if (cond->rightIsLiteral)
        {
            generateOperandToInt(cond->right, true, 0, false);
        }
        else
        {
            generateOperandToInt(cond->right, false, 0, fieldSigned[cond->right]);
        }
        textSection << "    mov rbx, rax\n";
        textSection << "    pop rax\n";
        textSection << "    cmp rax, rbx\n";
    }

    void generateComparisonSetup(const ConditionNode *cond)
    {
        int leftSize = 1;
        if (!cond->leftIsLiteral)
        {
            auto it = fieldSizes.find(cond->left);
            if (it != fieldSizes.end())
                leftSize = it->second;
        }

        std::string leftLabel;
        if (cond->leftIsLiteral)
        {
            leftLabel = emitPaddedLiteral(cond->left, leftSize, "left_lit");
        }
        else
        {
            leftLabel = getAsmName(cond->left);
        }

        std::string rightLabel;
        if (cond->rightIsLiteral)
        {
            rightLabel = emitPaddedLiteral(cond->right, leftSize, "right_lit");
        }
        else
        {
            rightLabel = getAsmName(cond->right);
            int rightSize = 1;
            auto it = fieldSizes.find(cond->right);
            if (it != fieldSizes.end())
                rightSize = it->second;
            leftSize = std::max(leftSize, rightSize);
        }

        generateLexCompare(leftLabel, rightLabel, leftSize);
    }

    void generateConditionJumpFalse(const ConditionNode *cond, const std::string &label)
    {
        if (isNumericOperand(cond->left, cond->leftIsLiteral) || isNumericOperand(cond->right, cond->rightIsLiteral))
        {
            generateNumericComparison(cond);
            switch (cond->op)
            {
            case ConditionNode::EQ:
                textSection << "    jne near " << label << "\n";
                break;
            case ConditionNode::NE:
                textSection << "    je near " << label << "\n";
                break;
            case ConditionNode::GT:
                textSection << "    jle near " << label << "\n";
                break;
            case ConditionNode::LT:
                textSection << "    jge near " << label << "\n";
                break;
            case ConditionNode::GE:
                textSection << "    jl near " << label << "\n";
                break;
            case ConditionNode::LE:
                textSection << "    jg near " << label << "\n";
                break;
            }
        }
        else
        {
            generateComparisonSetup(cond);
            switch (cond->op)
            {
            case ConditionNode::EQ:
                textSection << "    jne near " << label << "\n";
                break;
            case ConditionNode::NE:
                textSection << "    je near " << label << "\n";
                break;
            case ConditionNode::GT:
                textSection << "    jle near " << label << "\n";
                break;
            case ConditionNode::LT:
                textSection << "    jge near " << label << "\n";
                break;
            case ConditionNode::GE:
                textSection << "    jl near " << label << "\n";
                break;
            case ConditionNode::LE:
                textSection << "    jg near " << label << "\n";
                break;
            }
        }
    }

    void generateConditionJumpTrue(const ConditionNode *cond, const std::string &label)
    {
        if (isNumericOperand(cond->left, cond->leftIsLiteral) || isNumericOperand(cond->right, cond->rightIsLiteral))
        {
            generateNumericComparison(cond);
            switch (cond->op)
            {
            case ConditionNode::EQ:
                textSection << "    je near " << label << "\n";
                break;
            case ConditionNode::NE:
                textSection << "    jne near " << label << "\n";
                break;
            case ConditionNode::GT:
                textSection << "    jg near " << label << "\n";
                break;
            case ConditionNode::LT:
                textSection << "    jl near " << label << "\n";
                break;
            case ConditionNode::GE:
                textSection << "    jge near " << label << "\n";
                break;
            case ConditionNode::LE:
                textSection << "    jle near " << label << "\n";
                break;
            }
        }
        else
        {
            generateComparisonSetup(cond);
            switch (cond->op)
            {
            case ConditionNode::EQ:
                textSection << "    je near " << label << "\n";
                break;
            case ConditionNode::NE:
                textSection << "    jne near " << label << "\n";
                break;
            case ConditionNode::GT:
                textSection << "    jg near " << label << "\n";
                break;
            case ConditionNode::LT:
                textSection << "    jl near " << label << "\n";
                break;
            case ConditionNode::GE:
                textSection << "    jge near " << label << "\n";
                break;
            case ConditionNode::LE:
                textSection << "    jle near " << label << "\n";
                break;
            }
        }
    }

    void emitDecimalIncHelper()
    {
        helpers << "\n; decimal_inc: increment ASCII decimal string at [rdi], length rcx\n";
        helpers << "decimal_inc:\n";
        helpers << "    push rdi\n";
        helpers << "    push rcx\n";
        helpers << "    add rdi, rcx\n";
        helpers << "    dec rdi\n";
        helpers << ".dec_loop:\n";
        helpers << "    mov al, [rdi]\n";
        helpers << "    cmp al, '9'\n";
        helpers << "    je .carry\n";
        helpers << "    inc al\n";
        helpers << "    mov [rdi], al\n";
        helpers << "    pop rcx\n";
        helpers << "    pop rdi\n";
        helpers << "    ret\n";
        helpers << ".carry:\n";
        helpers << "    mov byte [rdi], '0'\n";
        helpers << "    dec rdi\n";
        helpers << "    dec rcx\n";
        helpers << "    jnz .dec_loop\n";
        helpers << "    pop rcx\n";
        helpers << "    pop rdi\n";
        helpers << "    ret\n";
    }

    void emitAsciiHelpers()
    {
        helpers << "\n; ascii_to_int: convert ASCII decimal at [rsi], length rcx -> rax\n";
        helpers << "; Handles optional leading + or - sign\n";
        helpers << "ascii_to_int:\n";
        helpers << "    push rbx\n";
        helpers << "    push rdx\n";
        helpers << "    xor rax, rax\n";
        helpers << "    xor rdx, rdx\n";
        helpers << "    mov bl, [rsi]\n";
        helpers << "    cmp bl, '-'\n";
        helpers << "    jne .check_plus\n";
        helpers << "    mov rdx, 1\n";
        helpers << "    inc rsi\n";
        helpers << "    dec rcx\n";
        helpers << "    jmp .convert\n";
        helpers << ".check_plus:\n";
        helpers << "    cmp bl, '+'\n";
        helpers << "    jne .convert\n";
        helpers << "    inc rsi\n";
        helpers << "    dec rcx\n";
        helpers << ".convert:\n";
        helpers << "    xor rbx, rbx\n";
        helpers << ".loop:\n";
        helpers << "    mov bl, [rsi]\n";
        helpers << "    sub bl, '0'\n";
        helpers << "    imul rax, 10\n";
        helpers << "    add rax, rbx\n";
        helpers << "    inc rsi\n";
        helpers << "    dec rcx\n";
        helpers << "    jnz .loop\n";
        helpers << "    test rdx, rdx\n";
        helpers << "    jz .done\n";
        helpers << "    neg rax\n";
        helpers << ".done:\n";
        helpers << "    pop rdx\n";
        helpers << "    pop rbx\n";
        helpers << "    ret\n";

        helpers << "\n; int_to_ascii: convert rax to ASCII decimal at [rdi], length rcx\n";
        helpers << "int_to_ascii:\n";
        helpers << "    push rdi\n";
        helpers << "    push rcx\n";
        helpers << "    push rax\n";
        helpers << "    push rbx\n";
        helpers << "    push rdx\n";
        helpers << "    push rax\n";
        helpers << "    push rdi\n";
        helpers << "    push rcx\n";
        helpers << "    mov al, '0'\n";
        helpers << "    cld\n";
        helpers << "    rep stosb\n";
        helpers << "    pop rcx\n";
        helpers << "    pop rdi\n";
        helpers << "    pop rax\n";
        helpers << "    cmp rax, 0\n";
        helpers << "    jge .convert\n";
        helpers << "    neg rax\n";
        helpers << ".convert:\n";
        helpers << "    add rdi, rcx\n";
        helpers << "    dec rdi\n";
        helpers << "    mov rbx, 10\n";
        helpers << ".loop:\n";
        helpers << "    test rax, rax\n";
        helpers << "    jz .done\n";
        helpers << "    xor rdx, rdx\n";
        helpers << "    div rbx\n";
        helpers << "    add dl, '0'\n";
        helpers << "    mov [rdi], dl\n";
        helpers << "    dec rdi\n";
        helpers << "    dec rcx\n";
        helpers << "    jnz .loop\n";
        helpers << ".done:\n";
        helpers << "    pop rdx\n";
        helpers << "    pop rbx\n";
        helpers << "    pop rax\n";
        helpers << "    pop rcx\n";
        helpers << "    pop rdi\n";
        helpers << "    ret\n";

        helpers << "\n; int_to_ascii_signed: convert rax to signed ASCII decimal at [rdi], length rcx\n";
        helpers << "int_to_ascii_signed:\n";
        helpers << "    push rdi\n";
        helpers << "    push rcx\n";
        helpers << "    push rax\n";
        helpers << "    push rbx\n";
        helpers << "    push rdx\n";
        helpers << "    push rax\n";
        helpers << "    push rdi\n";
        helpers << "    push rcx\n";
        helpers << "    mov al, '0'\n";
        helpers << "    cld\n";
        helpers << "    rep stosb\n";
        helpers << "    pop rcx\n";
        helpers << "    pop rdi\n";
        helpers << "    pop rax\n";
        helpers << "    cmp rax, 0\n";
        helpers << "    jge .positive\n";
        helpers << "    neg rax\n";
        helpers << "    mov byte [rdi], '-'\n";
        helpers << "    jmp .convert\n";
        helpers << ".positive:\n";
        helpers << "    mov byte [rdi], '+'\n";
        helpers << ".convert:\n";
        helpers << "    inc rdi\n";
        helpers << "    dec rcx\n";
        helpers << "    add rdi, rcx\n";
        helpers << "    dec rdi\n";
        helpers << "    mov rbx, 10\n";
        helpers << ".loop:\n";
        helpers << "    test rax, rax\n";
        helpers << "    jz .done\n";
        helpers << "    xor rdx, rdx\n";
        helpers << "    div rbx\n";
        helpers << "    add dl, '0'\n";
        helpers << "    mov [rdi], dl\n";
        helpers << "    dec rdi\n";
        helpers << "    dec rcx\n";
        helpers << "    jnz .loop\n";
        helpers << ".done:\n";
        helpers << "    pop rdx\n";
        helpers << "    pop rbx\n";
        helpers << "    pop rax\n";
        helpers << "    pop rcx\n";
        helpers << "    pop rdi\n";
        helpers << "    ret\n";
    }

    void generateOperandToInt(const std::string &value, bool isLiteral, int padSize = 0, bool isSigned = false)
    {
        if (isLiteral)
        {
            std::string litLabel = newLabel("lit");
            std::string padded = value;
            if (padSize > 0)
            {
                int digitCount = isSigned ? padSize - 1 : padSize;
                bool isNegative = false;
                bool hasSign = false;
                if (!padded.empty() && (padded[0] == '-' || padded[0] == '+'))
                {
                    isNegative = (padded[0] == '-');
                    hasSign = true;
                    padded = padded.substr(1);
                }
                while ((int)padded.length() < digitCount)
                {
                    padded = "0" + padded;
                }
                if (isSigned)
                {
                    padded = (isNegative ? "-" : "+") + padded;
                }
                else if (hasSign && isNegative)
                {
                    padded = "-" + padded;
                }
            }
            emitLiteral(litLabel, padded);
            textSection << "    lea rsi, [rel " << litLabel << "]\n";
            textSection << "    mov rcx, " << (padSize > 0 ? padSize : (int)padded.length()) << "\n";
        }
        else
        {
            textSection << "    lea rsi, [rel " << getAsmName(value) << "]\n";
            textSection << "    mov rcx, " << getAsmName(value) << "_len\n";
        }
        textSection << "    call ascii_to_int\n";
    }

    void generateIntToDest(const std::string &dest)
    {
        int destSize = 1;
        auto it = fieldSizes.find(dest);
        if (it != fieldSizes.end())
            destSize = it->second;
        bool isSigned = false;
        auto sit = fieldSigned.find(dest);
        if (sit != fieldSigned.end())
            isSigned = sit->second;

        textSection << "    lea rdi, [rel " << getAsmName(dest) << "]\n";
        textSection << "    mov rcx, " << destSize << "\n";
        if (isSigned)
        {
            textSection << "    call int_to_ascii_signed\n";
        }
        else
        {
            textSection << "    call int_to_ascii\n";
        }
    }

    void generateExpr(const ExprNode *expr, int padSize = 0, bool isSigned = false)
    {
        if (auto *lit = dynamic_cast<const LiteralExpr *>(expr))
        {
            generateOperandToInt(lit->value, true, padSize, isSigned);
        }
        else if (auto *var = dynamic_cast<const VariableExpr *>(expr))
        {
            generateOperandToInt(var->name, false, 0, isSigned);
        }
        else if (auto *bin = dynamic_cast<const BinaryExpr *>(expr))
        {
            generateExpr(bin->left.get(), padSize, isSigned);
            textSection << "    push rax\n";
            generateExpr(bin->right.get(), padSize, isSigned);
            textSection << "    mov rbx, rax\n";
            textSection << "    pop rax\n";
            switch (bin->op)
            {
            case BinaryExpr::ADD:
                textSection << "    add rax, rbx\n";
                break;
            case BinaryExpr::SUB:
                textSection << "    sub rax, rbx\n";
                break;
            case BinaryExpr::MUL:
                textSection << "    imul rax, rbx\n";
                break;
            case BinaryExpr::DIV:
                textSection << "    xor rdx, rdx\n";
                textSection << "    div rbx\n";
                break;
            }
        }
    }

public:
     void CodeGenerator::generateProgram(const ProgramNode *program)
        {
        programAst = program;   // <-- add this line
        programName = program->programName;
        programEndsWithStopRun = false;

        // Process file controls
        for (const auto &fc : program->fileControls)
        {
            if (auto *f = dynamic_cast<FileControlNode *>(fc.get()))
            {
                fileAssignments[f->selectName] = f->assignName;
                std::string fdLabel = newLabel("fd_" + getAsmName(f->selectName));
                fileFdNames[f->selectName] = fdLabel;
                bssSection << "    " << fdLabel << ": resq 1\n";

                std::string fnameLabel = newLabel("fname");
                emitCString(fnameLabel, f->assignName);
                fileAssignments[f->selectName + "_label"] = fnameLabel;

                if (!f->recordKeyName.empty() &&
                    f->organization == FileControlNode::Organization::INDEXED)
                {
                    fileRecordKeyVars[f->selectName] = f->recordKeyName;
                }

                // Track file status variable if declared
                if (!f->fileStatusVar.empty())
                {
                    fileStatusVars[f->selectName] = f->fileStatusVar;
                }

                // Track relative key variable for RELATIVE organization files
                if (!f->relativeKeyName.empty() &&
                    f->organization == FileControlNode::Organization::RELATIVE)
                {
                    fileRelativeKeyVars[f->selectName] = f->relativeKeyName;
                }
            }
        }

        // Process file descriptions
        for (const auto &fd : program->fileDescriptions)
        {
            if (auto *f = dynamic_cast<FileDescriptionNode *>(fd.get()))
            {
                // If no file control entry, create one implicitly
                if (fileFdNames.find(f->name) == fileFdNames.end())
                {
                    std::string fdLabel = newLabel("fd_" + getAsmName(f->name));
                    fileFdNames[f->name] = fdLabel;
                    bssSection << "    " << fdLabel << ": resq 1\n";

                    std::string fnameLabel = newLabel("fname");
                    emitCString(fnameLabel, f->name);
                    fileAssignments[f->name + "_label"] = fnameLabel;
                }

                int totalSize = 0;
                size_t index = 0;
                while (index < f->records.size())
                {
                    if (auto *d = dynamic_cast<DataItemNode *>(f->records[index].get()))
                    {
                        auto [size, count] = computeGroupSize(f->records, index);
                        generateDataItemTree(f->records, index);
                        totalSize += fieldSizes[d->name];
                        index += count;
                        continue;
                    }
                    ++index;
                }
                fileRecordSizes[f->name] = totalSize;
                if (!f->records.empty())
                {
                    if (auto *first = dynamic_cast<DataItemNode *>(f->records[0].get()))
                    {
                        fileRecordVars[f->name] = first->name;
                    }
                }
            }
        }

        size_t dataIndex = 0;
        while (dataIndex < program->dataItems.size())
        {
            if (auto *data = dynamic_cast<DataItemNode *>(program->dataItems[dataIndex].get()))
            {
                auto [size, count] = computeGroupSize(program->dataItems, dataIndex);
                generateDataItemTree(program->dataItems, dataIndex);
                dataIndex += count;
                continue;
            }
            ++dataIndex;
        }

        textSection << "section .text\n";
        textSection << "global _start\n";
        textSection << "_start:\n";
        textSection << "    cld\n";

        for (const auto &stmt : program->statements)
        {
            generateStatement(stmt.get());
        }

        if (!programEndsWithStopRun)
        {
            textSection << "    mov rax, 60\n";
            textSection << "    xor rdi, rdi\n";
            textSection << "    syscall\n";
        }

        if (needsDecimalInc)
        {
            emitDecimalIncHelper();
        }
        if (needsAsciiHelpers)
        {
            emitAsciiHelpers();
        }
    }

    bool hasNestedDataItems(const std::vector<std::unique_ptr<ASTNode>> &items, size_t index)
    {
        auto *item = dynamic_cast<DataItemNode *>(items[index].get());
        if (!item || index + 1 >= items.size())
            return false;
        auto *next = dynamic_cast<DataItemNode *>(items[index + 1].get());
        return next && next->level > item->level;
    }

    void emitAlias(const std::string &alias, const std::string &base, int offset)
    {
        variables[alias] = alias;
        fieldSizes[alias] = fieldSizes[alias];
        bssSection << "    " << alias << " equ " << base;
        if (offset > 0)
            bssSection << " + " << offset;
        bssSection << "\n";
    }

    void generateNestedAliases(const std::vector<std::unique_ptr<ASTNode>> &items,
                               size_t start, size_t end,
                               int parentLevel,
                               const std::string &baseAsmName,
                               int baseOffset)
    {
        size_t i = start + 1;
        int offset = 0;
        while (i < end)
        {
            auto *item = dynamic_cast<DataItemNode *>(items[i].get());
            if (!item)
            {
                ++i;
                continue;
            }
            if (item->level <= parentLevel)
                break;

            auto [itemSize, itemCount] = computeGroupSize(items, i);
            std::string asmName = getAsmName(item->name);
            variables[item->name] = asmName;
            fieldSizes[item->name] = itemSize;
            fieldSigned[item->name] = item->picDesc.isSigned;
            fieldNumeric[item->name] = item->picDesc.isNumeric;
            bssSection << "    " << asmName << " equ " << baseAsmName;
            if (baseOffset + offset > 0)
                bssSection << " + " << (baseOffset + offset);
            bssSection << "\n";

            if (hasNestedDataItems(items, i))
            {
                generateNestedAliases(items, i, i + itemCount, item->level, baseAsmName, baseOffset + offset);
            }

            offset += itemSize;
            i += itemCount;
        }
    }

    void generateDataItem(const DataItemNode *item, int overrideSize = 0)
    {
        std::string asmName = getAsmName(item->name);
        variables[item->name] = asmName;
        int size = overrideSize > 0 ? overrideSize : item->picDesc.storageSize;
        if (size == 0)
            size = 1;
        fieldSizes[item->name] = size;
        fieldSigned[item->name] = item->picDesc.isSigned;
        fieldNumeric[item->name] = item->picDesc.isNumeric;

        if (!item->value.empty())
        {
            std::string val = item->value;
            if (item->picDesc.isNumeric)
            {
                if (item->picDesc.isSigned)
                {
                    int digitCount = size - 1;
                    bool isNegative = (val[0] == '-');
                    if (isNegative)
                        val = val.substr(1);
                    while ((int)val.length() < digitCount)
                    {
                        val = "0" + val;
                    }
                    val = (isNegative ? "-" : "+") + val;
                }
                else
                {
                    while ((int)val.length() < size)
                    {
                        val = "0" + val;
                    }
                }
            }
            else if (item->picDesc.isAlpha)
            {
                while ((int)val.length() < size)
                {
                    val += ' ';
                }
            }
            emitLiteral(asmName, val);
        }
        else
        {
            bssSection << "    " << asmName << ": resb " << size << "\n";
            bssSection << "    " << asmName << "_len equ " << size << "\n";
        }
    }

    void generateDataItemTree(const std::vector<std::unique_ptr<ASTNode>> &items, size_t index)
    {
        auto *item = dynamic_cast<DataItemNode *>(items[index].get());
        if (!item)
            return;

        if (!hasNestedDataItems(items, index))
        {
            generateDataItem(item);
            return;
        }

        auto [size, count] = computeGroupSize(items, index);
        std::string asmName = getAsmName(item->name);
        variables[item->name] = asmName;
        fieldSizes[item->name] = size;
        fieldSigned[item->name] = item->picDesc.isSigned;
        fieldNumeric[item->name] = item->picDesc.isNumeric;

        bssSection << "    " << asmName << ": resb " << size << "\n";
        bssSection << "    " << asmName << "_len equ " << size << "\n";
        generateNestedAliases(items, index, index + count, item->level, asmName, 0);
    }

    void generateStatement(const ASTNode *stmt)
    {
        if (auto *move = dynamic_cast<const MoveNode *>(stmt))
            generateMove(move);
        else if (auto *add = dynamic_cast<const AddNode *>(stmt))
            generateAdd(add);
        else if (auto *mul = dynamic_cast<const MultiplyNode *>(stmt))
            generateMultiply(mul);
        else if (auto *sub = dynamic_cast<const SubtractNode *>(stmt))
            generateSubtract(sub);
        else if (auto *div = dynamic_cast<const DivideNode *>(stmt))
            generateDivide(div);
        else if (auto *comp = dynamic_cast<const ComputeNode *>(stmt))
            generateCompute(comp);
        else if (auto *disp = dynamic_cast<const DisplayNode *>(stmt))
            generateDisplay(disp);
        else if (dynamic_cast<const StopRunNode *>(stmt))
            generateStopRun();
        else if (auto *ifNode = dynamic_cast<const IfNode *>(stmt))
            generateIf(ifNode);
        else if (auto *perfUntil = dynamic_cast<const PerformUntilNode *>(stmt))
            generatePerformUntil(perfUntil);
        else if (auto *perfVary = dynamic_cast<const PerformVaryingNode *>(stmt))
            generatePerformVarying(perfVary);
        else if (auto *perfTimes = dynamic_cast<const PerformTimesNode *>(stmt))
            generatePerformTimes(perfTimes);
        else if (auto *perfPara = dynamic_cast<const PerformParagraphNode *>(stmt))
            generatePerformParagraph(perfPara);
        else if (auto *go = dynamic_cast<const GoToNode *>(stmt))
            generateGoTo(go);
        else if (auto *para = dynamic_cast<const ParagraphNode *>(stmt))
            generateParagraph(para);
        else if (auto *inspect = dynamic_cast<const InspectNode *>(stmt))
            generateInspect(inspect);
        else if (auto *open = dynamic_cast<const OpenNode *>(stmt))
            generateOpen(open);
        else if (auto *close = dynamic_cast<const CloseNode *>(stmt))
            generateClose(close);
        else if (auto *read = dynamic_cast<const ReadNode *>(stmt))
            generateRead(read);
        else if (auto *write = dynamic_cast<const WriteNode *>(stmt))
            generateWrite(write);
        else if (auto *start = dynamic_cast<const StartNode *>(stmt))
            generateStart(start);
    }

    void generateMove(const MoveNode *move)
    {
        std::string dest = resolveName(move->dest);
        int destSize = resolveSize(move->dest);
        bool isSigned = false;
        auto sit = fieldSigned.find(move->dest);
        if (sit != fieldSigned.end())
            isSigned = sit->second;

        if (move->sourceIsLiteral)
        {
            std::string litLabel = newLabel("lit");
            std::string padded = move->source;
            if (isSigned)
            {
                int digitCount = destSize - 1;
                bool isNegative = (!padded.empty() && padded[0] == '-');
                if (isNegative)
                    padded = padded.substr(1);
                while ((int)padded.length() < digitCount)
                {
                    padded = "0" + padded;
                }
                padded = (isNegative ? "-" : "+") + padded;
            }
            else
            {
                while ((int)padded.length() < destSize)
                {
                    padded = "0" + padded;
                }
            }
            emitLiteral(litLabel, padded);
            textSection << "    ; MOVE '" << move->source << "' TO " << move->dest << "\n";
            textSection << "    mov rsi, " << litLabel << "\n";
            textSection << "    mov rcx, " << destSize << "\n";
            textSection << "    mov rdi, " << dest << "\n";
            textSection << "    rep movsb\n";
        }
        else
        {
            std::string src = resolveName(move->source);
            int srcSize = resolveSize(move->source);
            int copySize = std::min(destSize, srcSize);
            textSection << "    ; MOVE " << move->source << " TO " << move->dest << "\n";
            textSection << "    mov rsi, " << src << "\n";
            textSection << "    mov rcx, " << copySize << "\n";
            textSection << "    mov rdi, " << dest << "\n";
            textSection << "    rep movsb\n";
            if (destSize > copySize)
            {
                char pad = isSigned ? '0' : (fieldNumeric[move->dest] ? '0' : ' ');
                textSection << "    lea rdi, [rel " << dest << " + " << copySize << "]\n";
                textSection << "    mov rcx, " << (destSize - copySize) << "\n";
                textSection << "    mov al, '" << pad << "'\n";
                textSection << "    rep stosb\n";
            }
        }
    }

    void generateAdd(const AddNode *add)
    {
        int destSize = 1;
        auto it = fieldSizes.find(add->dest);
        if (it != fieldSizes.end())
            destSize = it->second;
        bool isSigned = false;
        auto sit = fieldSigned.find(add->dest);
        if (sit != fieldSigned.end())
            isSigned = sit->second;

        if (destSize == 1 && !isSigned)
        {
            textSection << "    ; ADD " << add->left << " TO " << add->right << "\n";
            textSection << "    xor rax, rax\n";
            if (add->leftIsLiteral)
            {
                textSection << "    mov al, " << add->left << "\n";
            }
            else
            {
                textSection << "    mov al, [" << getAsmName(add->left) << "]\n";
                textSection << "    sub al, '0'\n";
            }
            if (add->rightIsLiteral)
            {
                textSection << "    add al, " << add->right << "\n";
            }
            else
            {
                textSection << "    add al, [" << getAsmName(add->right) << "]\n";
                textSection << "    sub al, '0'\n";
            }
            textSection << "    aam\n";
            textSection << "    add al, '0'\n";
            textSection << "    mov [" << getAsmName(add->dest) << "], al\n";
        }
        else
        {
            needsAsciiHelpers = true;
            textSection << "    ; ADD " << add->left << " TO " << add->right;
            if (add->dest != add->right)
                textSection << " GIVING " << add->dest;
            textSection << "\n";

            if (add->dest != add->right)
            {
                if (add->rightIsLiteral)
                {
                    generateOperandToInt(add->right, true, destSize, isSigned);
                }
                else
                {
                    generateOperandToInt(add->right, false, 0, isSigned);
                }
                generateIntToDest(add->dest);
            }

            if (add->leftIsLiteral)
            {
                generateOperandToInt(add->left, true, destSize, isSigned);
            }
            else
            {
                generateOperandToInt(add->left, false, 0, isSigned);
            }
            textSection << "    push rax\n";

            generateOperandToInt(add->dest, false, 0, isSigned);
            textSection << "    mov rbx, rax\n";
            textSection << "    pop rax\n";
            textSection << "    add rax, rbx\n";
            generateIntToDest(add->dest);
        }
    }

    void generateMultiply(const MultiplyNode *mul)
    {
        needsAsciiHelpers = true;
        int destSize = 1;
        auto it = fieldSizes.find(mul->dest);
        if (it != fieldSizes.end())
            destSize = it->second;
        bool isSigned = false;
        auto sit = fieldSigned.find(mul->dest);
        if (sit != fieldSigned.end())
            isSigned = sit->second;

        textSection << "    ; MULTIPLY " << mul->left << " BY " << mul->right;
        if (mul->dest != mul->right)
            textSection << " GIVING " << mul->dest;
        textSection << "\n";

        generateOperandToInt(mul->left, mul->leftIsLiteral, destSize, isSigned);
        textSection << "    push rax\n";
        generateOperandToInt(mul->right, mul->rightIsLiteral, destSize, isSigned);
        textSection << "    mov rbx, rax\n";
        textSection << "    pop rax\n";
        textSection << "    imul rax, rbx\n";
        generateIntToDest(mul->dest);
    }

    void generateSubtract(const SubtractNode *sub)
    {
        needsAsciiHelpers = true;
        int destSize = 1;
        auto it = fieldSizes.find(sub->dest);
        if (it != fieldSizes.end())
            destSize = it->second;
        bool isSigned = false;
        auto sit = fieldSigned.find(sub->dest);
        if (sit != fieldSigned.end())
            isSigned = sit->second;

        textSection << "    ; SUBTRACT " << sub->left << " FROM " << sub->right;
        if (sub->dest != sub->right)
            textSection << " GIVING " << sub->dest;
        textSection << "\n";

        generateOperandToInt(sub->right, sub->rightIsLiteral, destSize, isSigned);
        textSection << "    push rax\n";
        generateOperandToInt(sub->left, sub->leftIsLiteral, destSize, isSigned);
        textSection << "    mov rbx, rax\n";
        textSection << "    pop rax\n";
        textSection << "    sub rax, rbx\n";
        generateIntToDest(sub->dest);
    }

    void generateDivide(const DivideNode *div)
    {
        needsAsciiHelpers = true;
        int destSize = 1;
        auto it = fieldSizes.find(div->dest);
        if (it != fieldSizes.end())
            destSize = it->second;
        bool isSigned = false;
        auto sit = fieldSigned.find(div->dest);
        if (sit != fieldSigned.end())
            isSigned = sit->second;

        textSection << "    ; DIVIDE " << div->left << " INTO " << div->right;
        if (div->dest != div->right)
            textSection << " GIVING " << div->dest;
        textSection << "\n";

        generateOperandToInt(div->right, div->rightIsLiteral, destSize, isSigned);
        textSection << "    push rax\n";
        generateOperandToInt(div->left, div->leftIsLiteral, destSize, isSigned);
        textSection << "    mov rbx, rax\n";
        textSection << "    pop rax\n";
        textSection << "    xor rdx, rdx\n";
        textSection << "    div rbx\n";
        generateIntToDest(div->dest);

        if (!div->remainderDest.empty())
        {
            textSection << "    mov rax, rdx\n";
            generateIntToDest(div->remainderDest);
        }
    }

    void generateCompute(const ComputeNode *comp)
    {
        needsAsciiHelpers = true;
        int destSize = 1;
        auto it = fieldSizes.find(comp->dest);
        if (it != fieldSizes.end())
            destSize = it->second;
        bool isSigned = false;
        auto sit = fieldSigned.find(comp->dest);
        if (sit != fieldSigned.end())
            isSigned = sit->second;

        textSection << "    ; COMPUTE " << comp->dest << " = ...\n";
        generateExpr(comp->expr.get(), destSize, isSigned);
        generateIntToDest(comp->dest);
    }

    void generateDisplay(const DisplayNode *disp)
    {
        for (size_t i = 0; i < disp->operands.size(); ++i)
        {
            if (disp->isLiteral[i])
            {
                std::string litLabel = newLabel("disp_lit");
                emitLiteral(litLabel, disp->operands[i]);
                textSection << "    mov rax, 1\n";
                textSection << "    mov rdi, 1\n";
                textSection << "    mov rsi, " << litLabel << "\n";
                textSection << "    mov rdx, " << litLabel << "_len\n";
                textSection << "    syscall\n";
            }
            else
            {
                std::string var = resolveName(disp->operands[i]);
                int varSize = resolveSize(disp->operands[i]);
                textSection << "    mov rax, 1\n";
                textSection << "    mov rdi, 1\n";
                textSection << "    mov rsi, " << var << "\n";
                textSection << "    mov rdx, " << varSize << "\n";
                textSection << "    syscall\n";
            }
        }
        std::string nlLabel = newLabel("nl");
        dataSection << "    " << nlLabel << ": db 10\n";
        textSection << "    mov rax, 1\n";
        textSection << "    mov rdi, 1\n";
        textSection << "    mov rsi, " << nlLabel << "\n";
        textSection << "    mov rdx, 1\n";
        textSection << "    syscall\n";
    }

    void generateStopRun()
    {
        programEndsWithStopRun = true;
        textSection << "    ; STOP RUN\n";
        textSection << "    mov rax, 60\n";
        textSection << "    xor rdi, rdi\n";
        textSection << "    syscall\n";
    }

    void generateIf(const IfNode *ifNode)
    {
        std::string elseLabel = newLabel("else");
        std::string endifLabel = newLabel("endif");

        textSection << "    ; IF\n";
        // If all connectors are AND (or single condition), then any false -> else
        bool hasOr = false;
        for (auto &op : ifNode->condOps)
            if (op == TokenType::OR)
                hasOr = true;

        if (!hasOr)
        {
            for (const auto &c : ifNode->conditions)
                generateConditionJumpFalse(c.get(), elseLabel);

            for (const auto &stmt : ifNode->thenStatements)
                generateStatement(stmt.get());
            textSection << "    jmp near " << endifLabel << "\n";
        }
        else
        {
            // OR-chain: if any condition true then execute thenStatements
            std::string trueLabel = newLabel("if_true");
            for (const auto &c : ifNode->conditions)
                generateConditionJumpTrue(c.get(), trueLabel);

            // none true -> else
            textSection << "    jmp near " << elseLabel << "\n";

            textSection << trueLabel << ":\n";
            for (const auto &stmt : ifNode->thenStatements)
                generateStatement(stmt.get());
            textSection << "    jmp near " << endifLabel << "\n";
        }

        textSection << elseLabel << ":\n";
        for (const auto &stmt : ifNode->elseStatements)
            generateStatement(stmt.get());

        textSection << endifLabel << ":\n";
    }

    void generatePerformUntil(const PerformUntilNode *perf)
    {
        std::string startLabel = newLabel("loop_start");
        std::string endLabel = newLabel("loop_end");

        textSection << "    ; PERFORM UNTIL\n";
        textSection << startLabel << ":\n";
        generateConditionJumpTrue(perf->condition.get(), endLabel);

        for (const auto &stmt : perf->body)
        {
            generateStatement(stmt.get());
        }
        textSection << "    jmp near " << startLabel << "\n";
        textSection << endLabel << ":\n";
    }

    void generatePerformVarying(const PerformVaryingNode *perf)
    {
        std::string startLabel = newLabel("loop_start");
        std::string endLabel = newLabel("loop_end");
        std::string counterName = getAsmName(perf->counter);
        int counterSize = 1;
        auto it = fieldSizes.find(perf->counter);
        if (it != fieldSizes.end())
            counterSize = it->second;

        textSection << "    ; PERFORM VARYING\n";

        if (perf->fromIsLiteral)
        {
            std::string litLabel = emitPaddedLiteral(perf->from, counterSize, "from_lit");
            textSection << "    mov rsi, " << litLabel << "\n";
            textSection << "    mov rcx, " << counterSize << "\n";
            textSection << "    mov rdi, " << counterName << "\n";
            textSection << "    rep movsb\n";
        }
        else
        {
            int fromSize = 1;
            auto fit = fieldSizes.find(perf->from);
            if (fit != fieldSizes.end())
                fromSize = fit->second;
            textSection << "    mov rsi, " << getAsmName(perf->from) << "\n";
            textSection << "    mov rcx, " << fromSize << "\n";
            textSection << "    mov rdi, " << counterName << "\n";
            textSection << "    rep movsb\n";
            if (counterSize > fromSize)
            {
                textSection << "    lea rdi, [rel " << counterName << " + " << fromSize << "]\n";
                textSection << "    mov rcx, " << (counterSize - fromSize) << "\n";
                textSection << "    mov al, '0'\n";
                textSection << "    rep stosb\n";
            }
        }

        textSection << startLabel << ":\n";
        generateConditionJumpTrue(perf->untilCondition.get(), endLabel);

        for (const auto &stmt : perf->body)
        {
            generateStatement(stmt.get());
        }

        if (perf->byIsLiteral)
        {
            int byVal = std::stoi(perf->by);
            if (counterSize == 1)
            {
                textSection << "    add byte [" << counterName << "], " << byVal << "\n";
            }
            else
            {
                needsDecimalInc = true;
                for (int i = 0; i < byVal; ++i)
                {
                    textSection << "    mov rdi, " << counterName << "\n";
                    textSection << "    mov rcx, " << counterSize << "\n";
                    textSection << "    call decimal_inc\n";
                }
            }
        }
        else
        {
            textSection << "    mov al, [" << getAsmName(perf->by) << "]\n";
            textSection << "    sub al, '0'\n";
            textSection << "    add [" << counterName << "], al\n";
        }

        textSection << "    jmp near " << startLabel << "\n";
        textSection << endLabel << ":\n";
    }

    void generatePerformTimes(const PerformTimesNode *perf)
    {
        std::string counterLabel = newLabel("perf_count");
        std::string startLabel = newLabel("loop_start");
        std::string endLabel = newLabel("loop_end");

        bssSection << "    " << counterLabel << ": resb 1\n";

        textSection << "    ; PERFORM TIMES\n";
        needsAsciiHelpers = true;
        if (perf->countIsLiteral)
        {
            int count = 0;
            try
            {
                count = std::stoi(perf->count);
            }
            catch (...)
            {
                count = 0;
            }
            textSection << "    mov byte [" << counterLabel << "], " << count << "\n";
        }
        else
        {
            generateOperandToInt(perf->count, false, 0, false);
            textSection << "    mov byte [" << counterLabel << "], al\n";
        }

        textSection << startLabel << ":\n";
        textSection << "    cmp byte [" << counterLabel << "], 0\n";
        textSection << "    je near " << endLabel << "\n";

        for (const auto &stmt : perf->body)
        {
            generateStatement(stmt.get());
        }

        textSection << "    dec byte [" << counterLabel << "]\n";
        textSection << "    jmp near " << startLabel << "\n";
        textSection << endLabel << ":\n";
    }

    void CodeGenerator::generateStart(const StartNode *start)
    {
        textSection << "    ; START " << start->fileName << " KEY " << start->comp << " " << start->keyVar << "\n";

        // ----- get record variable and its size -----
        std::string recordVar;
        int recordSize = 0;
        if (fileRecordVars.find(start->fileName) != fileRecordVars.end()) {
            recordVar = fileRecordVars[start->fileName];
            recordSize = fileRecordSizes[start->fileName];
        } else {
            throw std::runtime_error("Code generation: no record variable for file " + start->fileName);
        }

        // ----- determine key field and its size -----
        std::string keyField = start->keyVar;
        std::string keyAsmName = getAsmName(keyField);
        if (variables.find(keyField) != variables.end())
            keyAsmName = variables[keyField];

        int keySize = resolveSize(keyField);   // now works for all items
        if (keySize == 1) {
            auto it = fieldSizes.find(keyField);
            if (it != fieldSizes.end())
                keySize = it->second;
        }

        // ----- prefetch buffer for target key -----
        std::string prefetchFlag, prefetchKeyTemp;
        ensurePrefetchBuffers(start->fileName, keySize);
        prefetchFlag = filePrefetchFlagNames[start->fileName];
        prefetchKeyTemp = filePrefetchKeyTemps[start->fileName];

        // Copy target key into prefetch buffer (use the full key size)
        textSection << "    mov byte [" << prefetchFlag << "], 0\n";
        int keyVarSize = resolveSize(start->keyVar);
        int copyLen = std::min(keyVarSize, keySize);
        textSection << "    mov rsi, " << keyAsmName << "\n";
        textSection << "    mov rdi, " << prefetchKeyTemp << "\n";
        textSection << "    mov rcx, " << copyLen << "\n";
        textSection << "    rep movsb\n";
        if (copyLen < keySize) {
            char pad = (fieldNumeric.count(start->keyVar) && fieldNumeric[start->keyVar]) ? '0' : ' ';
            textSection << "    mov al, '" << pad << "'\n";
            textSection << "    mov rcx, " << (keySize - copyLen) << "\n";
            textSection << "    rep stosb\n";
        }

        // ----- determine if numeric comparison is needed -----
        bool numericKey = fieldNumeric.count(start->keyVar) && fieldNumeric[start->keyVar];

        // ----- record key field (from FILE-CONTROL RECORD KEY or fallback) -----
        std::string recordKeyField;
        if (fileRecordKeyVars.find(start->fileName) != fileRecordKeyVars.end())
            recordKeyField = fileRecordKeyVars[start->fileName];
        else
            recordKeyField = start->keyVar;
        std::string recordKeyAsm = getAsmName(recordKeyField);
        if (variables.find(recordKeyField) != variables.end())
            recordKeyAsm = variables[recordKeyField];

        // ----- labels -----
        std::string startLoop = newLabel("start_loop");
        std::string startInvalid = newLabel("start_invalid");
        std::string startFound = newLabel("start_found");

        // ----- main loop: read each record -----
        textSection << startLoop << ":\n";
        textSection << "    mov rax, 0\n";
        textSection << "    mov rdi, [" << fileFdNames[start->fileName] << "]\n";
        textSection << "    mov rsi, " << getAsmName(recordVar) << "\n";
        textSection << "    mov rdx, " << recordSize << "\n";
        textSection << "    syscall\n";
        textSection << "    cmp rax, 0\n";
        textSection << "    jle near " << startInvalid << "\n";

        // ----- compare record key with target key -----
        if (numericKey) {
            needsAsciiHelpers = true;
            // convert record key -> rax
            textSection << "    mov rsi, " << recordKeyAsm << "\n";
            textSection << "    mov rcx, " << keySize << "\n";
            textSection << "    call ascii_to_int\n";
            textSection << "    push rax\n";

            // convert target key -> rbx
            textSection << "    mov rsi, " << prefetchKeyTemp << "\n";
            textSection << "    mov rcx, " << keySize << "\n";
            textSection << "    call ascii_to_int\n";
            textSection << "    mov rbx, rax\n";
            textSection << "    pop rax\n";

            // Now we want to compare record (rax) with target (rbx).
            // To fix the inversion, we swap the operands: cmp rbx, rax
            // and adjust the jump conditions accordingly.
            textSection << "    cmp rbx, rax\n";   // target : record

            // Branch based on comp (using swapped operands)
            if (start->comp == "LESS_THAN")            // record < target  => target > record
                textSection << "    jg near " << startFound << "\n";
            else if (start->comp == "LESS_THAN_OR_EQUAL") // record <= target => target >= record
                textSection << "    jge near " << startFound << "\n";
            else if (start->comp == "GREATER_THAN")    // record > target  => target < record
                textSection << "    jl near " << startFound << "\n";
            else if (start->comp == "GREATER_THAN_OR_EQUAL") // record >= target => target <= record
                textSection << "    jle near " << startFound << "\n";
            else if (start->comp == "EQUAL")
                textSection << "    je near " << startFound << "\n";
            else if (start->comp == "NOT_EQUAL")
                textSection << "    jne near " << startFound << "\n";
            else {
                // fallback
                textSection << "    jmp near " << startLoop << "\n";
            }
            textSection << "    jmp near " << startLoop << "\n";
        } else {
            // ----- lexicographic comparison (byte-by-byte) -----
            textSection << "    mov rcx, " << keySize << "\n";
            textSection << "    mov rsi, " << prefetchKeyTemp << "\n";  // target
            textSection << "    mov rdi, " << recordKeyAsm << "\n";     // record

            std::string cmpLoop = newLabel("key_cmp_loop");
            std::string cmpDone = newLabel("key_cmp_done");
            std::string cmpLess = newLabel("key_less");
            std::string cmpGreater = newLabel("key_greater");
            std::string cmpEqual = newLabel("key_equal");

            // Compare bytes: we want to set flags for record vs target.
            // We'll compare target (rsi) with record (rdi) and use the same swapped logic.
            textSection << cmpLoop << ":\n";
            textSection << "    cmp rcx, 0\n";
            textSection << "    je near " << cmpEqual << "\n";
            textSection << "    mov al, [rsi]\n";   // target byte
            textSection << "    mov bl, [rdi]\n";   // record byte
            textSection << "    cmp al, bl\n";      // compare target vs record
            textSection << "    jne near " << cmpDone << "\n";
            textSection << "    inc rsi\n";
            textSection << "    inc rdi\n";
            textSection << "    dec rcx\n";
            textSection << "    jmp near " << cmpLoop << "\n";

            textSection << cmpEqual << ":\n";
            textSection << "    mov al, 0\n";
            textSection << "    mov bl, 0\n";
            textSection << "    jmp near " << cmpDone << "\n";

            textSection << cmpDone << ":\n";
            // After cmp al, bl, flags reflect target vs record.
            // So jl means target < record  => record > target  => GREATER_THAN
            // jg means target > record  => record < target  => LESS_THAN
            textSection << "    cmp al, bl\n";
            textSection << "    jl near " << cmpLess << "\n";    // target < record
            textSection << "    jg near " << cmpGreater << "\n"; // target > record

            // Equal case
            if (start->comp == "EQUAL" || start->comp == "LESS_THAN_OR_EQUAL" || start->comp == "GREATER_THAN_OR_EQUAL")
                textSection << "    jmp near " << startFound << "\n";
            else
                textSection << "    jmp near " << startLoop << "\n";

            // target < record  => record > target
            textSection << cmpLess << ":\n";
            if (start->comp == "GREATER_THAN" || start->comp == "GREATER_THAN_OR_EQUAL")
                textSection << "    jmp near " << startFound << "\n";
            else
                textSection << "    jmp near " << startLoop << "\n";

            // target > record  => record < target
            textSection << cmpGreater << ":\n";
            if (start->comp == "LESS_THAN" || start->comp == "LESS_THAN_OR_EQUAL")
                textSection << "    jmp near " << startFound << "\n";
            else
                textSection << "    jmp near " << startLoop << "\n";
        }

        textSection << startInvalid << ":\n";
        textSection << "    mov byte [" << prefetchFlag << "], 0\n";
        for (const auto &stmt : start->invalidKeyStatements)
            generateStatement(stmt.get());

        textSection << startFound << ":\n";
        textSection << "    mov byte [" << prefetchFlag << "], 1\n";
    }
    void generatePerformParagraph(const PerformParagraphNode *perf)
    {
        textSection << "    ; PERFORM " << perf->target << "\n";
        textSection << "    call " << getAsmName(perf->target) << "\n";
    }

    void generateGoTo(const GoToNode *go)
    {
        textSection << "    jmp near " << getAsmName(go->target) << "\n";
    }

    void generateParagraph(const ParagraphNode *para)
    {
        textSection << getAsmName(para->name) << ":\n";
    }

    void generateInspect(const InspectNode *inspect)
    {
        std::string var = getAsmName(inspect->target);
        int size = 1;
        auto it = fieldSizes.find(inspect->target);
        if (it != fieldSizes.end())
            size = it->second;

        char oldChar = inspect->oldValue.empty() ? 0 : inspect->oldValue[0];
        char newChar = inspect->newValue.empty() ? 0 : inspect->newValue[0];

        std::string loopLabel = newLabel("inspect_loop");
        std::string nextLabel = newLabel("inspect_next");
        std::string endLabel = newLabel("inspect_end");

        textSection << "    ; INSPECT " << inspect->target << " REPLACING ALL " << inspect->oldValue << " BY " << inspect->newValue << "\n";
        textSection << "    mov rcx, " << size << "\n";
        textSection << "    mov rsi, " << var << "\n";
        textSection << loopLabel << ":\n";
        textSection << "    cmp rcx, 0\n";
        textSection << "    je near " << endLabel << "\n";
        textSection << "    cmp byte [rsi], " << (int)(unsigned char)oldChar << "\n";
        textSection << "    jne near " << nextLabel << "\n";
        textSection << "    mov byte [rsi], " << (int)(unsigned char)newChar << "\n";
        textSection << nextLabel << ":\n";
        textSection << "    inc rsi\n";
        textSection << "    dec rcx\n";
        textSection << "    jmp near " << loopLabel << "\n";
        textSection << endLabel << ":\n";
    }

    void generateOpen(const OpenNode *open)
    {
        // Ensure fd variable exists for this file
        if (fileFdNames.find(open->fileName) == fileFdNames.end())
        {
            std::string fdLabel = newLabel("fd_" + getAsmName(open->fileName));
            fileFdNames[open->fileName] = fdLabel;
            bssSection << "    " << fdLabel << ": resq 1\n";
        }
        if (fileFdNames.find(open->fileName) == fileFdNames.end())
        {
            throw std::runtime_error("Code generation error: file descriptor for '" + open->fileName + "' not defined");
        }
        std::string fdVar = fileFdNames[open->fileName];

        // Determine filename pointer: prefer SELECT/ASSIGN emitted C-string label,
        // otherwise if ASSIGN TO is a working-storage variable, copy it to a temp buffer.
        std::string fnameLabel;
        if (fileAssignments.find(open->fileName + "_label") != fileAssignments.end())
        {
            fnameLabel = fileAssignments[open->fileName + "_label"];
        }
        else if (fileAssignments[open->fileName + "_isVar"] == "1")
        {
            // ASSIGN TO variable: copy the variable's runtime content into a temp buffer
            std::string varName = fileAssignments[open->fileName];
            auto it = fieldSizes.find(varName);
            if (it == fieldSizes.end())
            {
                throw std::runtime_error(
                    "Code generation error: variable '" + varName +
                    "' (used in ASSIGN TO) not found in working storage");
            }
            int len = it->second;
            std::string src = getAsmName(varName);
            std::string tmp = newLabel("fname_tmp");
            bssSection << "    " << tmp << ": resb " << (len + 1) << "\n";

            textSection << "    ; Copy ASSIGN TO variable '" << varName
                        << "' to filename buffer for open(" << open->fileName << ")\n";
            textSection << "    mov rsi, " << src << "\n";
            textSection << "    mov rdi, " << tmp << "\n";
            textSection << "    mov rcx, " << len << "\n";
            textSection << "    rep movsb\n";
            textSection << "    mov byte [" << tmp << " + " << len << "], 0\n";
            fnameLabel = tmp;
        }
        else if (fieldSizes.find(open->fileName) != fieldSizes.end())
        {
            // Fallback: file name itself is a working-storage variable
            int len = fieldSizes[open->fileName];
            std::string src = getAsmName(open->fileName);
            std::string tmp = newLabel("fname_tmp");
            bssSection << "    " << tmp << ": resb " << (len + 1) << "\n";

            // copy working-storage filename into tmp and append NUL
            textSection << "    ; Prepare filename buffer for open(" << open->fileName << ")\n";
            textSection << "    mov rsi, " << src << "\n";
            textSection << "    mov rdi, " << tmp << "\n";
            textSection << "    mov rcx, " << len << "\n";
            textSection << "    rep movsb\n";
            textSection << "    mov byte [" << tmp << " + " << len << "], 0\n";
            fnameLabel = tmp;
        }
        else
        {
            // Fallback: no filename known; use empty string label
            std::string tmp = newLabel("fname_empty");
            emitCString(tmp, "");
            fnameLabel = tmp;
        }

        textSection << "    ; OPEN " << open->mode << " " << open->fileName << "\n";

        // sys_open: rax=2, rdi=filename, rsi=flags, rdx=mode
        textSection << "    mov rax, 2\n";
        textSection << "    lea rdi, [rel " << fnameLabel << "]\n";

        if (open->mode == "INPUT")
        {
            textSection << "    mov rsi, 0\n"; // O_RDONLY
        }
        else if (open->mode == "OUTPUT")
        {
            textSection << "    mov rsi, 577\n"; // O_WRONLY | O_CREAT | O_TRUNC = 1 | 64 | 512
        }
        else if (open->mode == "I-O")
        {
            textSection << "    mov rsi, 66\n"; // O_RDWR | O_CREAT = 2 | 64
        }
        else if (open->mode == "EXTEND")
        {
            textSection << "    mov rsi, 1089\n"; // O_WRONLY | O_CREAT | O_APPEND = 1 | 64 | 1024
        }
        else
        {
            textSection << "    mov rsi, 0\n";
        }

        textSection << "    mov rdx, 0644o\n";
        textSection << "    syscall\n";
        textSection << "    mov [" << fdVar << "], rax\n";
    }

    void generateClose(const CloseNode *close)
    {
        if (fileFdNames.find(close->fileName) == fileFdNames.end())
        {
            std::string fdLabel = newLabel("fd_" + getAsmName(close->fileName));
            fileFdNames[close->fileName] = fdLabel;
            bssSection << "    " << fdLabel << ": resq 1\n";
        }
        if (fileFdNames.find(close->fileName) == fileFdNames.end())
        {
            throw std::runtime_error("Code generation error: file descriptor for '" + close->fileName + "' not defined");
        }
        std::string fdVar = fileFdNames[close->fileName];
        textSection << "    ; CLOSE " << close->fileName << "\n";
        textSection << "    mov rax, 3\n";
        textSection << "    mov rdi, [" << fdVar << "]\n";
        textSection << "    syscall\n";
    }

    void generateRead(const ReadNode *read)
    {
        if (fileFdNames.find(read->fileName) == fileFdNames.end())
        {
            std::string fdLabel = newLabel("fd_" + getAsmName(read->fileName));
            fileFdNames[read->fileName] = fdLabel;
            bssSection << "    " << fdLabel << ": resq 1\n";
        }
        if (fileFdNames.find(read->fileName) == fileFdNames.end())
        {
            throw std::runtime_error("Code generation error: file descriptor for '" + read->fileName + "' not defined");
        }
        std::string fdVar = fileFdNames[read->fileName];
        std::string recordVar;
        std::string fileRecordVar;
        int recordSize = 0;
        bool directInto = false;

        if (fileRecordVars.find(read->fileName) != fileRecordVars.end())
        {
            fileRecordVar = fileRecordVars[read->fileName];
        }

        if (!read->intoVar.empty())
        {
            recordVar = read->intoVar;
            directInto = true;
            auto it = fieldSizes.find(read->intoVar);
            if (it != fieldSizes.end())
                recordSize = it->second;
        }
        else if (!fileRecordVar.empty())
        {
            recordVar = fileRecordVar;
            recordSize = fileRecordSizes[read->fileName];
        }

        bool canUsePrefetch = !read->nextRecord && !fileRecordVar.empty() &&
                              filePrefetchFlagNames.find(read->fileName) != filePrefetchFlagNames.end();
        std::string useSyscallLabel = canUsePrefetch ? newLabel("read_syscall") : "";
        std::string afterReadLabel = canUsePrefetch ? newLabel("read_after") : "";

        textSection << "    ; READ " << read->fileName;
        if (read->nextRecord)
            textSection << " NEXT RECORD";
        textSection << "\n";

        if (canUsePrefetch)
        {
            textSection << "    mov al, [" << filePrefetchFlagNames[read->fileName] << "]\n";
            textSection << "    cmp al, 1\n";
            textSection << "    jne near " << useSyscallLabel << "\n";
            textSection << "    mov byte [" << filePrefetchFlagNames[read->fileName] << "], 0\n";
            if (!read->intoVar.empty() && !fileRecordVar.empty() && read->intoVar != fileRecordVar)
            {
                int copySize = 1;
                auto it = fieldSizes.find(read->intoVar);
                if (it != fieldSizes.end())
                    copySize = std::min(copySize, it->second);
                auto fit = fieldSizes.find(fileRecordVar);
                if (fit != fieldSizes.end())
                    copySize = std::min(copySize, fit->second);
                textSection << "    mov rsi, " << getAsmName(fileRecordVar) << "\n";
                textSection << "    mov rdi, " << getAsmName(read->intoVar) << "\n";
                textSection << "    mov rcx, " << copySize << "\n";
                textSection << "    rep movsb\n";
            }
            textSection << "    mov rax, 1\n";
            textSection << "    jmp near " << afterReadLabel << "\n";
            textSection << useSyscallLabel << ":\n";
        }

        textSection << "    mov rax, 0\n";
        textSection << "    mov rdi, [" << fdVar << "]\n";
        if (!recordVar.empty())
        {
            textSection << "    mov rsi, " << getAsmName(recordVar) << "\n";
        }
        else
        {
            textSection << "    mov rsi, 0\n";
        }
        textSection << "    mov rdx, " << recordSize << "\n";
        textSection << "    syscall\n";

        if (canUsePrefetch)
        {
            textSection << afterReadLabel << ":\n";
        }

        bool needsBranch = read->hasAtEnd || read->hasNotAtEnd || !read->intoVar.empty();

        if (needsBranch)
        {
            std::string atEndLabel = newLabel("at_end");
            std::string readDoneLabel = newLabel("read_done");

            textSection << "    cmp rax, 0\n";
            textSection << "    jle near " << atEndLabel << "\n";

            if (!read->intoVar.empty() && !directInto && !recordVar.empty())
            {
                int intoSize = 1;
                auto it = fieldSizes.find(read->intoVar);
                if (it != fieldSizes.end())
                    intoSize = it->second;
                int copySize = std::min(recordSize, intoSize);
                textSection << "    mov rsi, " << getAsmName(recordVar) << "\n";
                textSection << "    mov rdi, " << getAsmName(read->intoVar) << "\n";
                textSection << "    mov rcx, " << copySize << "\n";
                textSection << "    rep movsb\n";
            }

            for (const auto &stmt : read->notAtEndStatements)
            {
                generateStatement(stmt.get());
            }

            textSection << "    jmp near " << readDoneLabel << "\n";

            textSection << atEndLabel << ":\n";
            for (const auto &stmt : read->atEndStatements)
            {
                generateStatement(stmt.get());
            }

            textSection << readDoneLabel << ":\n";
        }
    }

    void generateWrite(const WriteNode *write)
    {
        // Determine target file: explicit TO overrides inferred mapping
        std::string fileName = write->fileName;
        if (fileName.empty())
        {
            if (fileRecordVars.find(write->recordName) != fileRecordVars.end())
            {
                fileName = write->recordName;
            }
            else
            {
                for (const auto &kv : fileRecordVars)
                {
                    if (kv.second == write->recordName)
                    {
                        fileName = kv.first;
                        break;
                    }
                }
            }
        }

        std::string var = getAsmName(write->recordName);
        int size = 1;
        auto it = fieldSizes.find(write->recordName);
        if (it != fieldSizes.end())
            size = it->second;

        if (!fileName.empty() && fileRecordVars.find(fileName) != fileRecordVars.end())
        {
            if (fieldSizes.find(write->recordName) == fieldSizes.end())
            {
                var = getAsmName(fileRecordVars[fileName]);
            }
            if (fieldSizes.find(write->recordName) == fieldSizes.end() && fileRecordSizes.find(fileName) != fileRecordSizes.end())
            {
                size = fileRecordSizes[fileName];
            }
        }

        if (fileName.empty())
        {
            // Just write the variable content to stdout as fallback
            textSection << "    ; WRITE " << write->recordName << "\n";
            textSection << "    mov rax, 1\n";
            textSection << "    mov rdi, 1\n";
            textSection << "    mov rsi, " << var << "\n";
            textSection << "    mov rdx, " << size << "\n";
            textSection << "    syscall\n";
        }
        else
        {
            std::string fdVar = fileFdNames[fileName];
            if (fileRecordSizes.find(fileName) != fileRecordSizes.end())
                size = fileRecordSizes[fileName];

            textSection << "    ; WRITE " << write->recordName << "\n";
            textSection << "    mov rax, 1\n";
            textSection << "    mov rdi, [" << fdVar << "]\n";
            textSection << "    mov rsi, " << var << "\n";
            textSection << "    mov rdx, " << size << "\n";
            textSection << "    syscall\n";
        }
    }

    std::string getOutput() const
    {
        std::string out;
        if (!dataSection.str().empty())
        {
            out += "section .data\n" + dataSection.str() + "\n";
        }
        if (!bssSection.str().empty())
        {
            out += "section .bss\n" + bssSection.str() + "\n";
        }
        out += textSection.str();
        if (!helpers.str().empty())
        {
            out += helpers.str();
        }
        return out;
    }
    int CodeGenerator::getDataItemSize(const std::string& name) const
    {
        // Check fieldSizes first
        auto it = fieldSizes.find(name);
        if (it != fieldSizes.end())
            return it->second;

        // Search in dataItems
        size_t index = 0;
        for (const auto& node : programAst->dataItems) {
            if (auto* d = dynamic_cast<DataItemNode*>(node.get())) {
                if (d->name == name) {
                    // Now computeGroupSize is const, so we can call it directly.
                    auto [size, count] = computeGroupSize(programAst->dataItems, index);
                    return size;
                }
            }
            ++index;
        }
        return 1;
    }
