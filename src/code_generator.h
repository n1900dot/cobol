#ifndef CODE_GENERATOR_H
#define CODE_GENERATOR_H
#include "ast.h"
#include <sstream>
#include <map>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <fstream>
#include <unordered_set>
class CodeGenerator
{
private:
    std::ostringstream dataSection;
    std::ostringstream bssSection;
    std::ostringstream textSection;
    std::ostringstream helpers;
    std::map<std::string, std::string> variables;
    std::map<std::string, int> fieldSizes;
    std::map<std::string, bool> fieldSigned;
    std::map<std::string, bool> fieldNumeric;
    std::map<std::string, int> fieldDecimalPlaces;  // count of 9's after V in PIC clause
    std::map<std::string, bool> fieldJustified;
    std::map<std::string, bool> fieldJustifyLeft;
    std::map<std::string, int> tableOccurs;
    std::map<std::string, std::string> fileAssignments;
    std::map<std::string, std::string> fileFdNames;
    std::map<std::string, int> fileRecordSizes;
    std::map<std::string, std::string> fileRecordVars;
    std::map<std::string, std::string> fileRecordKeyVars;
    std::map<std::string, std::string> fileRelativeKeyVars;
    std::map<std::string, FileControlNode::Organization> fileOrganizations;
    std::map<std::string, FileControlNode::AccessMode> fileAccessModes;
    std::map<std::string, std::string> fileStatusVars;
    std::map<std::string, std::string> filePrefetchFlagNames;
    std::map<std::string, std::string> filePrefetchKeyTemps;
    const ProgramNode* programAst = nullptr;   // Store AST for size computations
    int labelCounter = 0;
    std::string programName;
    bool needsDecimalInc = false;
    bool needsAsciiHelpers = false;
    bool formatVHelperEmitted = false;
    bool dateHelperEmitted = false;
    bool tzOffsetHelperEmitted = false;
    bool programEndsWithStopRun = false;
    bool performRuntimeReady = false;
    bool insideParagraph = false;
    int getDataItemSize(const std::string& name) const;

    void ensurePerformRuntime()
    {
        if (performRuntimeReady)
            return;
        // Depth counter for nested PERFORM; return addresses live on the CPU stack.
        bssSection << "    perf_depth: resq 1\n";
        performRuntimeReady = true;
    }

    // At paragraph boundaries: if entered via PERFORM, return; else fall through.
    void emitParagraphEpilogue()
    {
        if (!insideParagraph)
            return;
        ensurePerformRuntime();
        std::string cont = newLabel("para_cont");
        textSection << "    ; end of paragraph (return if PERFORMed)\n";
        textSection << "    cmp qword [perf_depth], 0\n";
        textSection << "    je " << cont << "\n";
        textSection << "    sub qword [perf_depth], 1\n";
        textSection << "    pop rax\n";
        textSection << "    jmp rax\n";
        textSection << cont << ":\n";
        insideParagraph = false;
    }

    // Single PERFORM of a paragraph (push return addr, jump to label).
    void emitPerformOnce(const std::string &target)
    {
        ensurePerformRuntime();
        std::string retLabel = newLabel("perf_ret");
        textSection << "    lea rax, [rel " << retLabel << "]\n";
        textSection << "    push rax\n";
        textSection << "    add qword [perf_depth], 1\n";
        textSection << "    jmp " << getAsmName(target) << "\n";
        textSection << retLabel << ":\n";
    }

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
        static const std::unordered_set<std::string> reserved = {
            "AL", "AH", "BL", "BH", "CL", "CH", "DL", "DH",
            "AX", "BX", "CX", "DX", "SP", "BP", "SI", "DI", "IP",
            "EAX", "EBX", "ECX", "EDX", "ESP", "EBP", "ESI", "EDI", "EIP",
            "RAX", "RBX", "RCX", "RDX", "RSP", "RBP", "RSI", "RDI", "RIP",
            "R8", "R9", "R10", "R11", "R12", "R13", "R14", "R15",
            "CS", "DS", "ES", "FS", "GS", "SS",
            "XMM0", "XMM1", "XMM2", "XMM3", "XMM4", "XMM5", "XMM6", "XMM7",
            "XMM8", "XMM9", "XMM10", "XMM11", "XMM12", "XMM13", "XMM14", "XMM15",
            "YMM0", "YMM1", "YMM2", "YMM3", "YMM4", "YMM5", "YMM6", "YMM7",
            "ST", "ST0", "ST1", "ST2", "ST3", "ST4", "ST5", "ST6", "ST7",
            "MM0", "MM1", "MM2", "MM3", "MM4", "MM5", "MM6", "MM7",
            "CR0", "CR1", "CR2", "CR3", "CR4", "CR8",
            "DR0", "DR1", "DR2", "DR3", "DR4", "DR5", "DR6", "DR7",
            "EQU", "DB", "DW", "DD", "DQ", "DT", "DO", "DY", "DZ",
            "RESB", "RESW", "RESD", "RESQ", "REST",
            "SECTION", "GLOBAL", "EXTERN", "BITS", "ABSOLUTE", "EXPORT", "IMPORT",
            "DEFAULT", "REL", "SEGMENT", "ENDS", "ALIGN", "ALIGNB", "BITS",
            "USE16", "USE32", "USE64",
            "BYTE", "WORD", "DWORD", "QWORD", "TBYTE", "OWORD", "YWORD", "ZWORD",
        };
        std::string upper = name;
        for (char &c : upper)
            c = std::toupper(c);
        if (reserved.count(upper))
            name = "_" + name;
        return name;
    }

    std::string resolveName(const std::string &cobolName)
    {
        // If the name is a file name, resolve to its record variable
        auto it = fileRecordVars.find(cobolName);
        if (it != fileRecordVars.end())
            return getAsmName(it->second);
        return getAsmName(cobolName);
    }

    int resolveSize(const std::string &cobolName)
    {
        auto it = fieldSizes.find(cobolName);
        if (it != fieldSizes.end())
            return it->second;
        auto fit = fileRecordSizes.find(cobolName);
        if (fit != fileRecordSizes.end())
            return fit->second;
        return 1;
    }

    // Size used for EVALUATE / condition comparisons.
    // Prefers the recorded field size so subordinate items of a record
    // (and items that share storage via REDEFINES) compare correctly.
    int elementarySize(const std::string &name)
    {
        auto it = fieldSizes.find(name);
        if (it != fieldSizes.end())
            return it->second;
        return resolveSize(name);
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
        {
            int totalSize = item->picDesc.storageSize;
            if (item->occursCount > 0)
                totalSize *= item->occursCount;
            return {totalSize, 1};
        }

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
            if (!next->redefines.empty())
            {
                auto [dummy, childCount] = computeGroupSize(items, i);
                i += childCount;
                continue;
            }
            auto [size, count] = computeGroupSize(items, i);
            total += size;
            i += count;
        }
        if (item->occursCount > 0)
            total *= item->occursCount;
        return {total, i - index};
    }

    void emitLiteral(const std::string &label, const std::string &value)
    {
        dataSection << "    " << label << ": db ";
        // Emit string segments separated by explicit byte values for non-printable chars.
        // NASM supports \\\" (double quote), \\\\\\\\ (backslash), \\n (newline),
        // \\r (CR), \\t (tab) escapes inside db strings.
        // For non-printable bytes that NASM can't handle safely in literals (e.g. null, hex 0xA0+),
        // emit them as explicit numeric byte values instead.
        std::string segment;
        bool haveOutput = false;
        auto emitComma = [&]() {
            if (haveOutput) dataSection << ", ";
        };
        auto flushSegment = [&]() {
            if (segment.empty()) return;
            emitComma();
            dataSection << "\"";
            for (char c : segment)
            {
                if (c == '"')
                    dataSection << "\\\"";
                else if (c == '\\')
                    dataSection << "\\\\";
                else
                    dataSection << c;
            }
            dataSection << "\"";
            segment.clear();
            haveOutput = true;
        };
        auto emitHexByte = [&](unsigned char uc) {
            emitComma();
            dataSection << "0x" << std::hex << std::uppercase << static_cast<int>(uc);
            haveOutput = true;
        };
        for (unsigned char uc : value)
        {
            if (uc == '\n' || uc == '\r' || uc == '\t')
            {
                flushSegment();
                emitComma();
                dataSection << static_cast<int>(uc);
                haveOutput = true;
            }
            else if (uc >= 0x20 && uc <= 0x7E)
                segment += static_cast<char>(uc);
            else
            {
                flushSegment();
                emitHexByte(uc);
            }
        }
        flushSegment();
        dataSection << "\n";
        dataSection << "    " << label << "_len equ $ - " << label << "\n";
    }

    void emitCString(const std::string &label, const std::string &value)
    {
        dataSection << "    " << label << ": db ";
        std::string segment;
        bool haveOutput = false;
        auto emitComma = [&]() {
            if (haveOutput) dataSection << ", ";
        };
        auto flushSegment = [&]() {
            if (segment.empty()) return;
            emitComma();
            dataSection << "\"";
            for (char c : segment)
            {
                if (c == '"')
                    dataSection << "\\\"";
                else if (c == '\\')
                    dataSection << "\\\\";
                else
                    dataSection << c;
            }
            dataSection << "\"";
            segment.clear();
            haveOutput = true;
        };
        auto emitHexByte = [&](unsigned char uc) {
            emitComma();
            dataSection << "0x" << std::hex << std::uppercase << static_cast<int>(uc);
            haveOutput = true;
        };
        for (unsigned char uc : value)
        {
            if (uc == '\n' || uc == '\r' || uc == '\t')
            {
                flushSegment();
                emitComma();
                dataSection << static_cast<int>(uc);
                haveOutput = true;
            }
            else if (uc >= 0x20 && uc <= 0x7E)
                segment += static_cast<char>(uc);
            else
            {
                flushSegment();
                emitHexByte(uc);
            }
        }
        flushSegment();
        dataSection << ", 0\n";
    }

    // Emit a COBOL FILE STATUS update after a file operation.
    // rax should contain the POSIX syscall return value on entry.
    void emitFileStatusUpdate(const std::string &fileName)
    {
        auto it = fileStatusVars.find(fileName);
        if (it == fileStatusVars.end())
            return;
        const std::string &statusVar = it->second;
        std::string okLabel = newLabel("fstat_ok");
        std::string endLabel = newLabel("fstat_end");
        // If rax >= 0, operation succeeded (00); else permanent error (30)
        textSection << "    ; FILE STATUS update for " << fileName << "\n";
        textSection << "    cmp rax, 0\n";
        textSection << "    jge " << okLabel << "\n";
        textSection << "    mov word [" << getAsmName(statusVar) << "], '30'\n";
        textSection << "    jmp " << endLabel << "\n";
        textSection << okLabel << ":\n";
        textSection << "    mov word [" << getAsmName(statusVar) << "], '00'\n";
        textSection << endLabel << ":\n";
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
        // Handle subscripted variables: check the base name
        std::string baseName = name;
        if (variableHasSubscript(name))
            baseName = baseVariableName(name);
        auto it = fieldNumeric.find(baseName);
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
        needsAsciiHelpers = true;
        int leftSize = 1;
        std::string leftLabel;

        if (cond->leftIsLiteral)
        {
            leftSize = cond->left.length();
            leftLabel = emitPaddedLiteral(cond->left, leftSize, "left_lit");
        }
        else if (variableHasSubscript(cond->left))
        {
            std::string base = baseVariableName(cond->left);
            int elemSize = resolveElementSize(base);
            leftSize = elemSize;
            const std::string baseAsm = getAsmName(base);
            std::string subExpr = subscriptExpression(cond->left);
            std::string tempLabel = newLabel("cmp_left");
            bssSection << "    " << tempLabel << ": resb " << elemSize << "\n";

            textSection << "    lea rax, [rel " << baseAsm << "]\n";
            if (isNumericLiteral(subExpr))
            {
                int subVal = std::stoi(subExpr);
                textSection << "    mov rcx, " << (subVal - 1) << "\n";
            }
            else
            {
                textSection << "    lea rsi, [rel " << getAsmName(subExpr) << "]\n";
                textSection << "    mov rcx, " << resolveSize(subExpr) << "\n";
                textSection << "    call ascii_to_int\n";
                textSection << "    dec rax\n";
                textSection << "    mov rcx, rax\n";
            }
            if (elemSize > 1)
            {
                textSection << "    mov rbx, " << elemSize << "\n";
                textSection << "    imul rcx, rbx\n";
            }
            textSection << "    add rax, rcx\n";
            // Load the VALUE at the element address, not the address itself
            if (elemSize == 1)
            {
                textSection << "    movzx eax, byte [rax]\n";
                textSection << "    mov [" << tempLabel << "], al\n";
            }
            else
            {
                textSection << "    lea rdi, [rel " << tempLabel << "]\n";
                textSection << "    mov rcx, " << elemSize << "\n";
                textSection << "    rep movsb\n";
            }
            leftLabel = tempLabel;
        }
        else
        {
            leftLabel = getAsmName(cond->left);
            auto it = fieldSizes.find(cond->left);
            if (it != fieldSizes.end())
                leftSize = it->second;
        }

        std::string rightLabel;
        if (cond->rightIsLiteral)
        {
            int rightSize = cond->right.length();
            rightLabel = emitPaddedLiteral(cond->right, std::max(leftSize, rightSize), "right_lit");
            leftSize = std::max(leftSize, rightSize);
        }
        else if (variableHasSubscript(cond->right))
        {
            std::string base = baseVariableName(cond->right);
            int elemSize = resolveElementSize(base);
            leftSize = std::max(leftSize, elemSize);
            const std::string baseAsm = getAsmName(base);
            std::string subExpr = subscriptExpression(cond->right);
            std::string tempLabel = newLabel("cmp_right");
            bssSection << "    " << tempLabel << ": resb " << elemSize << "\n";

            textSection << "    lea rax, [rel " << baseAsm << "]\n";
            if (isNumericLiteral(subExpr))
            {
                int subVal = std::stoi(subExpr);
                textSection << "    mov rcx, " << (subVal - 1) << "\n";
            }
            else
            {
                textSection << "    lea rsi, [rel " << getAsmName(subExpr) << "]\n";
                textSection << "    mov rcx, " << resolveSize(subExpr) << "\n";
                textSection << "    call ascii_to_int\n";
                textSection << "    dec rax\n";
                textSection << "    mov rcx, rax\n";
            }
            if (elemSize > 1)
            {
                textSection << "    mov rbx, " << elemSize << "\n";
                textSection << "    imul rcx, rbx\n";
            }
            textSection << "    add rax, rcx\n";
            // Load the VALUE at the element address, not the address itself
            if (elemSize == 1)
            {
                textSection << "    movzx eax, byte [rax]\n";
                textSection << "    mov [" << tempLabel << "], al\n";
            }
            else
            {
                textSection << "    lea rdi, [rel " << tempLabel << "]\n";
                textSection << "    mov rcx, " << elemSize << "\n";
                textSection << "    rep movsb\n";
            }
            rightLabel = tempLabel;
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
                    textSection << "    jne " << label << "\n";
                    break;
                case ConditionNode::NE:
                    textSection << "    je " << label << "\n";
                    break;
                case ConditionNode::GT:
                    textSection << "    jle " << label << "\n";
                    break;
                case ConditionNode::LT:
                    textSection << "    jge " << label << "\n";
                    break;
                case ConditionNode::GE:
                    textSection << "    jl " << label << "\n";
                    break;
                case ConditionNode::LE:
                    textSection << "    jg " << label << "\n";
                    break;
            }
        }
        else
        {
            generateComparisonSetup(cond);
            switch (cond->op)
            {
                case ConditionNode::EQ:
                    textSection << "    jne " << label << "\n";
                    break;
                case ConditionNode::NE:
                    textSection << "    je " << label << "\n";
                    break;
                case ConditionNode::GT:
                    textSection << "    jle " << label << "\n";
                    break;
                case ConditionNode::LT:
                    textSection << "    jge " << label << "\n";
                    break;
                case ConditionNode::GE:
                    textSection << "    jl " << label << "\n";
                    break;
                case ConditionNode::LE:
                    textSection << "    jg " << label << "\n";
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
                    textSection << "    je " << label << "\n";
                    break;
                case ConditionNode::NE:
                    textSection << "    jne " << label << "\n";
                    break;
                case ConditionNode::GT:
                    textSection << "    jg " << label << "\n";
                    break;
                case ConditionNode::LT:
                    textSection << "    jl " << label << "\n";
                    break;
                case ConditionNode::GE:
                    textSection << "    jge " << label << "\n";
                    break;
                case ConditionNode::LE:
                    textSection << "    jle " << label << "\n";
                    break;
            }
        }
        else
        {
            generateComparisonSetup(cond);
            switch (cond->op)
            {
                case ConditionNode::EQ:
                    textSection << "    je " << label << "\n";
                    break;
                case ConditionNode::NE:
                    textSection << "    jne " << label << "\n";
                    break;
                case ConditionNode::GT:
                    textSection << "    jg " << label << "\n";
                    break;
                case ConditionNode::LT:
                    textSection << "    jl " << label << "\n";
                    break;
                case ConditionNode::GE:
                    textSection << "    jge " << label << "\n";
                    break;
                case ConditionNode::LE:
                    textSection << "    jle " << label << "\n";
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
        helpers << "; Handles optional leading + or - sign and decimal point\n";
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
        helpers << "    cmp bl, '.'\n";
        helpers << "    je .skip_dot\n";
        helpers << "    sub bl, '0'\n";
        helpers << "    imul rax, 10\n";
        helpers << "    add rax, rbx\n";
        helpers << "    jmp .next\n";
        helpers << ".skip_dot:\n";
        helpers << "    inc rsi\n";
        helpers << "    dec rcx\n";
        helpers << "    jnz .loop\n";
        helpers << "    jmp .done\n";
        helpers << ".next:\n";
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

    void emitTzOffsetHelper()
    {
        if (tzOffsetHelperEmitted)
            return;
        tzOffsetHelperEmitted = true;
        // Stub that returns 0.  The previous TZif parser produced an incorrect
        // offset (often ~9 minutes).  DATE/TIME now rely on the kernel's
        // clock_gettime seconds directly, eliminating the drift.
        helpers << "\n; get_tz_offset – stub (returns 0). Local conversion is\n";
        helpers << "; performed by the DATE/TIME helpers via clock_gettime.\n";
        helpers << "get_tz_offset:\n";
        helpers << "    xor rax, rax\n";
        helpers << "    ret\n";
    }

    void emitFormatVOutputHelper()
    {
        if (formatVHelperEmitted)
            return;
        formatVHelperEmitted = true;
        helpers << "\n; format_v_output: insert decimal point for implied-decimal fields\n";
        helpers << "; rsi=src, rcx=total_len, r8=decimal_pos, rdi=dest_buf\n";
        helpers << "; writes prefix + '.' + suffix  (total_len+1 bytes)\n";
        helpers << "; clobbers: rax, rcx, rdx, r8, r9, r10, r11\n";
        helpers << "; preserves: rbx, rbp, r12-r15\n";
        helpers << "format_v_output:\n";
        helpers << "    push rbx\n";
        helpers << "    push rcx\n";
        helpers << "    mov rbx, r8\n";
        helpers << "    mov rcx, rbx\n";
        helpers << "    rep movsb\n";
        helpers << "    mov byte [rdi], '.'\n";
        helpers << "    inc rdi\n";
        helpers << "    pop rcx\n";
        helpers << "    sub rcx, rbx\n";
        helpers << "    rep movsb\n";
        helpers << "    pop rbx\n";
        helpers << "    ret\n";
    }

    void emitDateHelper()
    {
        if (dateHelperEmitted)
            return;
        dateHelperEmitted = true;
        helpers << "\n; format_date_yyyymmdd: convert epoch seconds to YYYYMMDD ASCII\n";
        helpers << "; Input:  rax = seconds since epoch\n";
        helpers << ";         rdi = dest buffer (8 bytes)\n";
        helpers << "; Clobbers: rax, rcx, rdx, r8, r9, r10, rsi\n";
        helpers << "; Preserves: rbx, rbp, r12-r15\n";
        helpers << "format_date_yyyymmdd:\n";
        helpers << "    push rbx\n";
        helpers << "    mov rbx, rax\n";
        helpers << "    mov rax, rbx\n";
        helpers << "    mov rcx, 86400\n";
        helpers << "    xor rdx, rdx\n";
        helpers << "    div rcx\n";
        helpers << "    mov rbx, rax\n";
        helpers << "    mov rcx, 1970\n";
        helpers << ".find_year:\n";
        helpers << "    mov rax, rcx\n";
        helpers << "    mov rdx, 0\n";
        helpers << "    mov r8, 4\n";
        helpers << "    div r8\n";
        helpers << "    test rdx, rdx\n";
        helpers << "    jnz .not_leap_y\n";
        helpers << "    mov rax, rcx\n";
        helpers << "    mov rdx, 0\n";
        helpers << "    mov r8, 100\n";
        helpers << "    div r8\n";
        helpers << "    test rdx, rdx\n";
        helpers << "    jnz .is_leap_y\n";
        helpers << "    mov rax, rcx\n";
        helpers << "    mov rdx, 0\n";
        helpers << "    mov r8, 400\n";
        helpers << "    div r8\n";
        helpers << "    test rdx, rdx\n";
        helpers << "    jz .is_leap_y\n";
        helpers << "    jmp .not_leap_y\n";
        helpers << ".is_leap_y:\n";
        helpers << "    mov rdx, 366\n";
        helpers << "    jmp .check_days\n";
        helpers << ".not_leap_y:\n";
        helpers << "    mov rdx, 365\n";
        helpers << ".check_days:\n";
        helpers << "    cmp rbx, rdx\n";
        helpers << "    jb .year_done\n";
        helpers << "    sub rbx, rdx\n";
        helpers << "    inc rcx\n";
        helpers << "    jmp .find_year\n";
        helpers << ".year_done:\n";
        helpers << "    mov rax, rcx\n";
        helpers << "    mov rdx, 0\n";
        helpers << "    mov r8, 4\n";
        helpers << "    div r8\n";
        helpers << "    test rdx, rdx\n";
        helpers << "    jnz .not_leap_f\n";
        helpers << "    mov rax, rcx\n";
        helpers << "    mov rdx, 0\n";
        helpers << "    mov r8, 100\n";
        helpers << "    div r8\n";
        helpers << "    test rdx, rdx\n";
        helpers << "    jnz .is_leap_f\n";
        helpers << "    mov rax, rcx\n";
        helpers << "    mov rdx, 0\n";
        helpers << "    mov r8, 400\n";
        helpers << "    div r8\n";
        helpers << "    test rdx, rdx\n";
        helpers << "    jz .is_leap_f\n";
        helpers << "    jmp .not_leap_f\n";
        helpers << ".is_leap_f:\n";
        helpers << "    mov r9, 29\n";
        helpers << "    jmp .month_loop\n";
        helpers << ".not_leap_f:\n";
        helpers << "    mov r9, 28\n";
        helpers << ".month_loop:\n";
        helpers << "    mov rsi, 1\n";
        helpers << "    mov r8, rbx\n";
        helpers << ".ml:\n";
        helpers << "    cmp rsi, 1\n";
        helpers << "    jne .chk2\n";
        helpers << "    mov rax, 31\n";
        helpers << "    jmp .chk_days\n";
        helpers << ".chk2:\n";
        helpers << "    cmp rsi, 2\n";
        helpers << "    jne .chk3\n";
        helpers << "    mov rax, r9\n";
        helpers << "    jmp .chk_days\n";
        helpers << ".chk3:\n";
        helpers << "    cmp rsi, 3\n";
        helpers << "    jne .chk4\n";
        helpers << "    mov rax, 31\n";
        helpers << "    jmp .chk_days\n";
        helpers << ".chk4:\n";
        helpers << "    cmp rsi, 4\n";
        helpers << "    jne .chk5\n";
        helpers << "    mov rax, 30\n";
        helpers << "    jmp .chk_days\n";
        helpers << ".chk5:\n";
        helpers << "    cmp rsi, 5\n";
        helpers << "    jne .chk6\n";
        helpers << "    mov rax, 31\n";
        helpers << "    jmp .chk_days\n";
        helpers << ".chk6:\n";
        helpers << "    cmp rsi, 6\n";
        helpers << "    jne .chk7\n";
        helpers << "    mov rax, 30\n";
        helpers << "    jmp .chk_days\n";
        helpers << ".chk7:\n";
        helpers << "    cmp rsi, 7\n";
        helpers << "    jne .chk8\n";
        helpers << "    mov rax, 31\n";
        helpers << "    jmp .chk_days\n";
        helpers << ".chk8:\n";
        helpers << "    cmp rsi, 8\n";
        helpers << "    jne .chk9\n";
        helpers << "    mov rax, 31\n";
        helpers << "    jmp .chk_days\n";
        helpers << ".chk9:\n";
        helpers << "    cmp rsi, 9\n";
        helpers << "    jne .chk10\n";
        helpers << "    mov rax, 30\n";
        helpers << "    jmp .chk_days\n";
        helpers << ".chk10:\n";
        helpers << "    cmp rsi, 10\n";
        helpers << "    jne .chk11\n";
        helpers << "    mov rax, 31\n";
        helpers << "    jmp .chk_days\n";
        helpers << ".chk11:\n";
        helpers << "    cmp rsi, 11\n";
        helpers << "    jne .chk12\n";
        helpers << "    mov rax, 30\n";
        helpers << "    jmp .chk_days\n";
        helpers << ".chk12:\n";
        helpers << "    mov rax, 31\n";
        helpers << ".chk_days:\n";
        helpers << "    cmp r8, rax\n";
        helpers << "    jb .done_month\n";
        helpers << "    sub r8, rax\n";
        helpers << "    inc rsi\n";
        helpers << "    cmp rsi, 12\n";
        helpers << "    jle .ml\n";
        helpers << ".done_month:\n";
        helpers << "    inc r8\n";
        helpers << "    ; write YYYY (4 digits)\n";
        helpers << "    mov rax, rcx\n";
        helpers << "    mov rcx, 4\n";
        helpers << "    push rdi\n";
        helpers << "    call int_to_ascii\n";
        helpers << "    pop rdi\n";
        helpers << "    add rdi, 4\n";
        helpers << "    ; write MM (2 digits)\n";
        helpers << "    mov rax, rsi\n";
        helpers << "    mov rcx, 2\n";
        helpers << "    push rdi\n";
        helpers << "    call int_to_ascii\n";
        helpers << "    pop rdi\n";
        helpers << "    add rdi, 2\n";
        helpers << "    ; write DD (2 digits)\n";
        helpers << "    mov rax, r8\n";
        helpers << "    mov rcx, 2\n";
        helpers << "    call int_to_ascii\n";
        helpers << "    pop rbx\n";
        helpers << "    ret\n";
    }

    void generateOperandToInt(const std::string &value, bool isLiteral, int padSize = 0, bool isSigned = false)
    {
        needsAsciiHelpers = true;
        int postDivCount = 0;
        bool postDivSigned = false;
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
                // Only pad up to digitCount; never truncate a longer literal.
                if ((int)padded.length() < digitCount)
                {
                    padded = std::string(digitCount - padded.length(), '0') + padded;
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
            textSection << "    mov rcx, " << (int)padded.length() << "\n";
            // If literal contains a decimal point, remember to divide by 10^dp
            // after ascii_to_int so the result is the true integer value.
            size_t dotPos = value.find('.');
            if (dotPos != std::string::npos)
            {
                std::string src = value;
                if (!src.empty() && (src[0] == '-' || src[0] == '+')) src = src.substr(1);
                size_t srcDot = src.find('.');
                postDivCount = (srcDot != std::string::npos) ? (int)(src.size() - srcDot - 1) : 0;
                postDivSigned = true;
            }
        }
        else if (variableHasSubscript(value))
        {
            // Subscripted variable: compute address at runtime
            std::string base = baseVariableName(value);
            std::string subExpr = subscriptExpression(value);
            int elemSize = resolveElementSize(base);

            textSection << "    lea rsi, [rel " << getAsmName(base) << "]\n";
            if (isNumericLiteral(subExpr))
            {
                int subVal = std::stoi(subExpr);
                textSection << "    mov rcx, " << (subVal - 1) << "\n";
            }
            else
            {
                textSection << "    lea rsi, [rel " << getAsmName(subExpr) << "]\n";
                textSection << "    mov rcx, " << resolveSize(subExpr) << "\n";
                textSection << "    call ascii_to_int\n";
                textSection << "    dec rax\n";
                textSection << "    mov rcx, rax\n";
            }
            if (elemSize > 1)
            {
                textSection << "    mov rbx, " << elemSize << "\n";
                textSection << "    imul rcx, rbx\n";
            }
            textSection << "    add rsi, rcx\n";

            // Now rsi points to the element; convert the string to int
            textSection << "    mov rcx, " << elemSize << "\n";
        }
        else
        {
            textSection << "    lea rsi, [rel " << getAsmName(value) << "]\n";
            textSection << "    mov rcx, " << getAsmName(value) << "_len\n";
        }
        textSection << "    call ascii_to_int\n";
        // Scale the integer result for fields with an implied decimal (V).
        // ascii_to_int reads the raw digit string (e.g. '2230' for 99V99 VALUE 22.30)
        // as a plain integer; divide by 10^decimalPlaces to get the true value.
        if (!isLiteral)
        {
            std::string srcBase = variableHasSubscript(value) ? baseVariableName(value) : value;
            auto dpit = fieldDecimalPlaces.find(srcBase);
            if (dpit != fieldDecimalPlaces.end() && dpit->second > 0)
            {
                int p = dpit->second;
                for (int i = 0; i < p; ++i)
                {
                    textSection << "    ; divide by 10 for implied decimal\n";
                    textSection << "    mov rbx, 10\n";
                    textSection << "    xor rdx, rdx\n";
                    textSection << "    div rbx\n";
                }
            }
        }
        if (postDivCount > 0)
        {
            for (int i = 0; i < postDivCount; ++i)
            {
                textSection << "    ; divide by 10 for literal decimal\n";
                textSection << "    mov rbx, 10\n";
                textSection << "    cqo\n";
                textSection << "    idiv rbx\n";
            }
        }
    }

    void generateIntToDest(const std::string &dest)
    {
        needsAsciiHelpers = true;
        int destSize = 1;
        bool isSigned = false;
        auto sit = fieldSigned.find(dest);
        if (sit != fieldSigned.end())
            isSigned = sit->second;

        textSection << "    ; INT_TO_DEST " << dest << "\n";

        if (variableHasSubscript(dest))
        {
            std::string base = baseVariableName(dest);
            std::string subExpr = subscriptExpression(dest);
            destSize = resolveElementSize(base);
            const std::string baseAsm = getAsmName(base);

            textSection << "    lea rdi, [rel " << baseAsm << "]\n";
            if (isNumericLiteral(subExpr))
            {
                int subVal = std::stoi(subExpr);
                textSection << "    mov rcx, " << (subVal - 1) << "\n";
            }
            else
            {
                textSection << "    lea rsi, [rel " << getAsmName(subExpr) << "]\n";
                textSection << "    mov rcx, " << resolveSize(subExpr) << "\n";
                textSection << "    call ascii_to_int\n";
                textSection << "    dec rax\n";
                textSection << "    mov rcx, rax\n";
            }
            if (destSize > 1)
            {
                textSection << "    mov rbx, " << destSize << "\n";
                textSection << "    imul rcx, rbx\n";
            }
            textSection << "    add rdi, rcx\n";
            textSection << "    mov rcx, " << destSize << "\n";
        }
        else
        {
            auto it = fieldSizes.find(dest);
            if (it != fieldSizes.end())
                destSize = it->second;

            textSection << "    lea rdi, [rel " << getAsmName(dest) << "]\n";
            textSection << "    mov rcx, " << destSize << "\n";
        }

        // Scale rax by 10^decimalPlaces before writing to implied-decimal destination.
        // int_to_ascii writes only digits; e.g. 44.60 -> store 4460 in 99V99.
        {
            std::string destBase = variableHasSubscript(dest) ? baseVariableName(dest) : dest;
            auto dpit = fieldDecimalPlaces.find(destBase);
            if (dpit != fieldDecimalPlaces.end() && dpit->second > 0)
            {
                int p = dpit->second;
                for (int i = 0; i < p; ++i)
                {
                    textSection << "    ; multiply by 10 for implied decimal dest\n";
                    textSection << "    mov rbx, 10\n";
                    textSection << "    imul rax, rbx\n";
                }
            }
        }

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
    void generateProgram(const ProgramNode *program)
    {
        const ProgramNode* programAst = nullptr;
        programName = program->programName;
        programEndsWithStopRun = false;

        dataSection << "    tzfile_path: db \"/etc/localtime\", 0\n";

        // Process file controls
        for (const auto &fc : program->fileControls)
        {
            if (auto *f = dynamic_cast<FileControlNode *>(fc.get()))
            {
                fileAssignments[f->selectName] = f->assignName;
                fileAssignments[f->selectName + "_isVar"] =
                    f->assignIsVariable ? "1" : "0";
                std::string fdLabel = newLabel("fd_" + getAsmName(f->selectName));
                fileFdNames[f->selectName] = fdLabel;
                bssSection << "    " << fdLabel << ": resq 1\n";

                if (!f->assignIsVariable)
                {
                    // Literal string: pre-create a C string label
                    std::string fnameLabel = newLabel("fname");
                    emitCString(fnameLabel, f->assignName);
                    fileAssignments[f->selectName + "_label"] = fnameLabel;
                }

                if (!f->recordKeyName.empty() &&
                    f->organization == FileControlNode::Organization::INDEXED)
                {
                    fileRecordKeyVars[f->selectName] = f->recordKeyName;
                }

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

                // Track organization and access mode for this file
                fileOrganizations[f->selectName] = f->organization;
                fileAccessModes[f->selectName] = f->accessMode;
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

        textSection << "bits 64\n";
        textSection << "section .text\n";
        textSection << "global _start\n";
        textSection << "_start:\n";
        textSection << "    cld\n";

        for (const auto &stmt : program->statements)
        {
            generateStatement(stmt.get());
        }

        // Close the final paragraph (PERFORM return or fall-through).
        emitParagraphEpilogue();

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
        size_t nextIdx = index + 1;
        while (nextIdx < items.size()) {
            auto *next = dynamic_cast<DataItemNode *>(items[nextIdx].get());
            if (!next)
            {
                ++nextIdx;
                continue;
            }
            if (!next->redefines.empty())
            {
                ++nextIdx;
                continue;
            }
            return next->level > item->level;
        }
        return false;
    }

    void applyRedefinesFieldInfo(const std::string &itemName, const std::string &redefinesTarget)
    {
        std::string upper = redefinesTarget;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

        // Ensure target has at least a default size so the redefining item
        // always gets usable metadata (and shares the target's VALUE).
        if (!fieldSizes.count(upper))
            fieldSizes[upper] = 1;

        fieldSizes[itemName]         = fieldSizes[upper];
        fieldSigned[itemName]        = fieldSigned.count(upper) ? fieldSigned[upper] : false;
        fieldNumeric[itemName]       = fieldNumeric.count(upper) ? fieldNumeric[upper] : false;
        fieldDecimalPlaces[itemName] = fieldDecimalPlaces.count(upper) ? fieldDecimalPlaces[upper] : 0;
        fieldJustified[itemName]     = fieldJustified.count(upper) ? fieldJustified[upper] : false;
        fieldJustifyLeft[itemName]   = fieldJustifyLeft.count(upper) ? fieldJustifyLeft[upper] : false;
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
            int occursMultiply = item->occursCount > 0 ? item->occursCount : 1;
            std::string asmName = getAsmName(item->name);
            variables[item->name] = asmName;
            fieldSizes[item->name] = itemSize;
            fieldSigned[item->name] = item->picDesc.isSigned;
            fieldNumeric[item->name] = item->picDesc.isNumeric;

            fieldDecimalPlaces[item->name] = item->picDesc.decimalPlaces;
            fieldJustified[item->name] = item->picDesc.isJustified;
            fieldJustifyLeft[item->name] = item->picDesc.justifyLeft;

            if (!item->redefines.empty())
            {
                applyRedefinesFieldInfo(item->name, item->redefines);
                std::string redefinesAsm = getAsmName(item->redefines);
                bssSection << "    " << asmName << " equ " << redefinesAsm << "\n";
                bssSection << "    " << asmName << "_len equ " << itemSize << "\n";
                if (hasNestedDataItems(items, i))
                {
                    generateNestedAliases(items, i, i + itemCount, item->level, redefinesAsm, 0);
                }
            }
            else
            {
                int totalItemSize = itemSize * occursMultiply;
                bssSection << "    " << asmName << " equ " << baseAsmName;
                if (baseOffset + offset > 0)
                    bssSection << " + " << (baseOffset + offset);
                bssSection << "\n";
                bssSection << "    " << asmName << "_len equ " << itemSize << "\n";

                if (!item->indexedBy.empty())
                {
                    std::string idxAsm = getAsmName(item->indexedBy);
                    bssSection << "    " << idxAsm << ": resb 1\n";
                }

                if (hasNestedDataItems(items, i))
                {
                    generateNestedAliases(items, i, i + itemCount, item->level, asmName, 0);
                }

                offset += totalItemSize;
            }
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
        if (item->occursCount > 0)
            size *= item->occursCount;
        fieldSizes[item->name] = size;
        fieldSigned[item->name] = item->picDesc.isSigned;
        fieldNumeric[item->name] = item->picDesc.isNumeric;
        fieldDecimalPlaces[item->name] = item->picDesc.decimalPlaces;
        fieldJustified[item->name] = item->picDesc.isJustified;
        fieldJustifyLeft[item->name] = item->picDesc.justifyLeft;

        // Track OCCURS count for element size computation
        if (item->occursCount > 0)
            tableOccurs[item->name] = item->occursCount;

        // Allocate BSS for index variable if this is a table
        if (!item->indexedBy.empty())
        {
            std::string idxAsm = getAsmName(item->indexedBy);
            bssSection << "    " << idxAsm << ": resb 1\n";
        }

        if (!item->redefines.empty())
        {
            applyRedefinesFieldInfo(item->name, item->redefines);
            std::string redefinesAsm = getAsmName(item->redefines);
            bssSection << "    " << asmName << " equ " << redefinesAsm << "\n";
            // Emit length so later DISPLAY / MOVE / arithmetic can resolve it
            int sz = 1;
            auto it = fieldSizes.find(item->name);
            if (it != fieldSizes.end())
                sz = it->second;
            bssSection << "    " << asmName << "_len equ " << sz << "\n";
            return;
        }

        if (!item->value.empty())
        {
            std::string val = item->value;
            if (item->picDesc.isNumeric && item->picDesc.decimalPlaces > 0)
            {
                // Implied-decimal (V): split at '.', left-pad integer part and
                // right-pad fractional part so digits align with the PIC positions.
                // Example: PIC 9(6)V9(4) VALUE 22.30 -> store "0000223000".
                std::string intPart, fracPart;
                size_t dotPos2 = val.find('.');
                if (dotPos2 != std::string::npos)
                {
                    intPart = val.substr(0, dotPos2);
                    fracPart = val.substr(dotPos2 + 1);
                }
                else
                {
                    intPart = val;
                    fracPart = "";
                }
                bool isNegative = false;
                if (!intPart.empty() && (intPart[0] == '-' || intPart[0] == '+'))
                {
                    isNegative = (intPart[0] == '-');
                    intPart = intPart.substr(1);
                }
                int intSize = size - item->picDesc.decimalPlaces;
                while ((int)intPart.length() < intSize)
                    intPart = "0" + intPart;
                while ((int)fracPart.length() < item->picDesc.decimalPlaces)
                    fracPart += "0";
                if (isNegative && item->picDesc.isSigned)
                    val = "-" + intPart + fracPart;
                else
                    val = intPart + fracPart;
            }
            else if (item->picDesc.isNumeric)
            {
                if (item->picDesc.isJustified && item->picDesc.justifyLeft)
                {
                    int digitCount = item->picDesc.isSigned ? size - 1 : size;
                    bool isNegative = false;
                    if (!val.empty() && (val[0] == '-' || val[0] == '+'))
                    {
                        isNegative = (val[0] == '-');
                        val = val.substr(1);
                    }
                    while ((int)val.length() < digitCount)
                        val += ' ';
                    if (item->picDesc.isSigned)
                        val = (isNegative ? "-" : "+") + val;
                }
                else if (item->picDesc.isSigned)
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
                else if (item->picDesc.isJustified && !item->picDesc.justifyLeft)
                {
                    while ((int)val.length() < size)
                        val = ' ' + val;
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
                if (item->picDesc.isJustified && !item->picDesc.justifyLeft)
                {
                    while ((int)val.length() < size)
                        val = ' ' + val;
                }
                else
                {
                    while ((int)val.length() < size)
                    {
                        val += ' ';
                    }
                }
            }
            emitLiteral(asmName, val);
            bssSection << "    " << asmName << "_len equ " << size << "\n";
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

        if (!item->redefines.empty())
        {
            std::string asmName = getAsmName(item->name);
            variables[item->name] = asmName;
            applyRedefinesFieldInfo(item->name, item->redefines);
            std::string redefinesAsm = getAsmName(item->redefines);
            bssSection << "    " << asmName << " equ " << redefinesAsm << "\n";
            auto [size, count] = computeGroupSize(items, index);
            fieldSizes[item->name] = size;   // ensure group size is recorded
            bssSection << "    " << asmName << "_len equ " << size << "\n";
            generateNestedAliases(items, index, index + count, item->level, redefinesAsm, 0);
            return;
        }

        auto [size, count] = computeGroupSize(items, index);
        std::string asmName = getAsmName(item->name);
        variables[item->name] = asmName;

        if (item->occursCount > 0)
        {
            // For OCCURS items: fieldSizes = total allocation, tableOccurs = OCCURS count
            fieldSizes[item->name] = size;
            tableOccurs[item->name] = item->occursCount;
        }
        else
        {
            fieldSizes[item->name] = size;
        }

        fieldSigned[item->name] = item->picDesc.isSigned;
        fieldNumeric[item->name] = item->picDesc.isNumeric;
        fieldDecimalPlaces[item->name] = item->picDesc.decimalPlaces;
        fieldJustified[item->name] = item->picDesc.isJustified;
        fieldJustifyLeft[item->name] = item->picDesc.justifyLeft;

        if (!hasNestedDataItems(items, index))
        {
            generateDataItem(item);
            return;
        }

        bssSection << "    " << asmName << ": resb " << size << "\n";
        bssSection << "    " << asmName << "_len equ " << size << "\n";
        if (!item->indexedBy.empty())
        {
            std::string idxAsm = getAsmName(item->indexedBy);
            bssSection << "    " << idxAsm << ": resb 1\n";
        }
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
        else if (auto *print = dynamic_cast<const PrintNode *>(stmt))
            generatePrint(print);
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
        else if (auto *search = dynamic_cast<const SearchNode *>(stmt))
            generateSearch(search);
        else if (auto *set = dynamic_cast<const SetIndexNode *>(stmt))
            generateSet(set);
        else if (auto *acc = dynamic_cast<const AcceptNode *>(stmt))
            generateAccept(acc);
        else if (auto *evaluate = dynamic_cast<const EvaluateNode *>(stmt))
            generateEvaluate(evaluate);
        else if (auto *str = dynamic_cast<const StringNode *>(stmt))
            generateString(str);
        else if (auto *uns = dynamic_cast<const UnstringNode *>(stmt))
            generateUnstring(uns);
    }

    void generateMove(const MoveNode *move)
    {
        needsAsciiHelpers = true;
        bool srcHasSub = variableHasSubscript(move->source);
        bool destHasSub = variableHasSubscript(move->dest);
        std::string src, dest;
        int destSize = resolveSize(move->dest);
        int srcSize = 1;
        int copySize;
        bool isSigned = false;
        auto sit = fieldSigned.find(move->dest);
        if (sit != fieldSigned.end())
            isSigned = sit->second;

        // Sanitize source for comment: escape non-printable bytes to avoid breaking asm output
        {
            std::string safeSource;
            for (unsigned char c : move->source)
            {
                if (c >= 0x20 && c <= 0x7E)
                    safeSource += static_cast<char>(c);
                else
                {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\x%02X", c);
                    safeSource += buf;
                }
            }
            textSection << "    ; MOVE " << safeSource << " TO " << move->dest << "\n";
        }

        // For subscripted dest, compute element size first
        int moveDestSize = destSize;
        if (destHasSub)
        {
            std::string destBase = baseVariableName(move->dest);
            moveDestSize = resolveElementSize(destBase);
        }

        // Cross-type literal: if literal has a decimal point and dest has no V,
        // convert through integer helpers so '22.30' -> integer 22 (not '2230').
        {
            std::string destKey = destHasSub ? baseVariableName(move->dest) : move->dest;
            auto destDpit3 = fieldDecimalPlaces.find(destKey);
            int destDP3 = (destDpit3 != fieldDecimalPlaces.end()) ? destDpit3->second : 0;
            if (move->sourceIsLiteral && move->source.find('.') != std::string::npos && destDP3 == 0)
            {
                generateOperandToInt(move->source, true, 0, isSigned);
                generateIntToDest(move->dest);
                return;
            }
        }

        if (move->sourceIsLiteral)
        {
            // If dest has V and literal has no decimal point, interpret as integer
            // and scale to destination precision via generateIntToDest.
            auto destDpitLit = fieldDecimalPlaces.find(move->dest);
            int destDPLit = (destDpitLit != fieldDecimalPlaces.end()) ? destDpitLit->second : 0;
            if (destDPLit > 0 && move->source.find('.') == std::string::npos)
            {
                generateOperandToInt(move->source, true, 0, isSigned);
                generateIntToDest(move->dest);
                return;
            }

            std::string litLabel = newLabel("lit");
            std::string padded = move->source;
            bool destJustified = false;
            bool destJustifyLeft = false;
            auto fit = fieldJustified.find(move->dest);
            if (fit != fieldJustified.end()) {
                destJustified = fit->second;
                destJustifyLeft = fieldJustifyLeft[move->dest];
            }
            if (isSigned)
            {
                int digitCount = moveDestSize - 1;
                bool isNegative = (!padded.empty() && padded[0] == '-');
                if (isNegative)
                    padded = padded.substr(1);
                if (destJustified && destJustifyLeft)
                {
                    while ((int)padded.length() < digitCount)
                        padded += ' ';
                }
                else
                {
                    while ((int)padded.length() < digitCount)
                        padded = "0" + padded;
                }
                padded = (isNegative ? "-" : "+") + padded;
            }
            else
            {
                // Strip '.' from numeric literal so it fits the byte count
                if (!isSigned)
                {
                    std::string noDot;
                    for (char c : padded)
                        if (c != '.')
                            noDot += c;
                    padded = noDot;
                }
                if (destJustified && destJustifyLeft)
                {
                    while ((int)padded.length() < moveDestSize)
                        padded += ' ';
                }
                else if (destJustified && !destJustifyLeft)
                {
                    while ((int)padded.length() < moveDestSize)
                        padded = ' ' + padded;
                }
                else
                {
                    char padChar = fieldNumeric[move->dest] ? '0' : ' ';
                    while ((int)padded.length() < moveDestSize)
                        padded = padChar + padded;
                }
            }
            emitLiteral(litLabel, padded);
            textSection << "    mov rsi, " << litLabel << "\n";
            textSection << "    mov rcx, " << moveDestSize << "\n";

            if (destHasSub)
            {
                // Compute dest element address in rdi
                std::string base = baseVariableName(move->dest);
                std::string subExpr = subscriptExpression(move->dest);
                const std::string baseAsm = getAsmName(base);

                textSection << "    lea rdi, [rel " << baseAsm << "]\n";
                if (isNumericLiteral(subExpr))
                {
                    int subVal = std::stoi(subExpr);
                    textSection << "    mov rdx, " << (subVal - 1) << "\n";
                }
                else
                {
                    textSection << "    push rsi\n";
                    textSection << "    lea rsi, [rel " << getAsmName(subExpr) << "]\n";
                    textSection << "    mov rcx, " << resolveSize(subExpr) << "\n";
                    textSection << "    call ascii_to_int\n";
                    textSection << "    pop rsi\n";
                    textSection << "    mov rcx, " << moveDestSize << "\n";
                    textSection << "    dec rax\n";
                    textSection << "    mov rdx, rax\n";
                }
                if (moveDestSize > 1)
                {
                    textSection << "    mov rbx, " << moveDestSize << "\n";
                    textSection << "    imul rdx, rbx\n";
                }
                textSection << "    add rdi, rdx\n";
            }
            else
            {
                dest = resolveName(move->dest);
                textSection << "    mov rdi, " << dest << "\n";
            }
            textSection << "    rep movsb\n";
            return;
        }

        // Source: handle subscript
        if (srcHasSub)
        {
            std::string base = baseVariableName(move->source);
            std::string subExpr = subscriptExpression(move->source);
            srcSize = resolveElementSize(base);
            const std::string baseAsm = getAsmName(base);

            textSection << "    ; Subscripted source: " << move->source << "\n";
            textSection << "    lea r12, [rel " << baseAsm << "]\n";
            if (isNumericLiteral(subExpr))
            {
                int subVal = std::stoi(subExpr);
                textSection << "    mov rcx, " << (subVal - 1) << "\n";
            }
            else
            {
                textSection << "    lea rsi, [rel " << getAsmName(subExpr) << "]\n";
                textSection << "    mov rcx, " << resolveSize(subExpr) << "\n";
                textSection << "    call ascii_to_int\n";
                textSection << "    dec rax\n";
                textSection << "    mov rcx, rax\n";
            }
            if (srcSize > 1)
            {
                textSection << "    mov rbx, " << srcSize << "\n";
                textSection << "    imul rcx, rbx\n";
            }
            textSection << "    add r12, rcx\n";
            textSection << "    mov rsi, r12\n";
        }
        else
        {
            src = resolveName(move->source);
            srcSize = resolveSize(move->source);
        }

        copySize = std::min(moveDestSize, srcSize);

        dest = resolveName(move->dest);

        // Destination: handle subscript
        if (destHasSub)
        {
            std::string base = baseVariableName(move->dest);
            std::string subExpr = subscriptExpression(move->dest);
            const std::string baseAsm = getAsmName(base);

            // For subscripted dest, use element size for copy
            int finalDestSize = destSize;
            if (destHasSub)
                finalDestSize = resolveElementSize(base);

            copySize = std::min(finalDestSize, srcSize);
            textSection << "    ; Subscripted dest: " << move->dest << "\n";

            // Compute dest address in rdi
            textSection << "    lea rdi, [rel " << baseAsm << "]\n";
            if (isNumericLiteral(subExpr))
            {
                int subVal = std::stoi(subExpr);
                textSection << "    mov rcx, " << (subVal - 1) << "\n";
            }
            else
            {
                textSection << "    lea rsi, [rel " << getAsmName(subExpr) << "]\n";
                textSection << "    mov rcx, " << resolveSize(subExpr) << "\n";
                textSection << "    call ascii_to_int\n";
                textSection << "    dec rax\n";
                textSection << "    mov rcx, rax\n";
            }
            if (finalDestSize > 1)
            {
                textSection << "    mov rbx, " << finalDestSize << "\n";
                textSection << "    imul rcx, rbx\n";
            }
            textSection << "    add rdi, rcx\n";

            if (srcHasSub)
            {
                // Source is already in rax (address)
                textSection << "    mov rsi, rax\n";
            }
            else
            {
                textSection << "    mov rsi, " << src << "\n";
            }
            // When source has V and dest does not: convert through integer helpers
            {
                std::string srcBase = srcHasSub ? baseVariableName(move->source) : move->source;
                auto srcDpit = fieldDecimalPlaces.find(srcBase);
                std::string destBase2 = destHasSub ? baseVariableName(move->dest) : move->dest;
                auto destDpit2 = fieldDecimalPlaces.find(destBase2);
                int srcDP = (srcDpit != fieldDecimalPlaces.end()) ? srcDpit->second : 0;
                int destDP = (destDpit2 != fieldDecimalPlaces.end()) ? destDpit2->second : 0;
                if (srcDP > 0 && destDP == 0)
                {
                    if (move->sourceIsLiteral)
                        generateOperandToInt(move->source, true, 0, isSigned);
                    else
                        generateOperandToInt(move->source, false, 0, isSigned);
                    generateIntToDest(move->dest);
                    return;
                }
            }

            textSection << "    mov rcx, " << copySize << "\n";
            textSection << "    rep movsb\n";
            return;
        }

        // When source field has an implied decimal (V) and dest does not, the
        // stored bytes are a scaled representation (e.g. '2230' means 22.30 for
        // PIC 99V99).  Convert through the integer helpers: generateOperandToInt
        // divides by 10^srcDP and generateIntToDest writes only the integer part.
        {
            std::string srcBase = srcHasSub ? baseVariableName(move->source) : move->source;
            auto srcDpit = fieldDecimalPlaces.find(srcBase);
            auto destDpit2 = fieldDecimalPlaces.find(move->dest);
            int srcDP = (srcDpit != fieldDecimalPlaces.end()) ? srcDpit->second : 0;
            int destDP = (destDpit2 != fieldDecimalPlaces.end()) ? destDpit2->second : 0;
            if (srcDP > 0 && destDP == 0)
            {
                if (move->sourceIsLiteral)
                    generateOperandToInt(move->source, true, 0, isSigned);
                else if (srcHasSub)
                    generateOperandToInt(move->source, false, 0, isSigned);
                else
                    generateOperandToInt(move->source, false, 0, isSigned);
                generateIntToDest(move->dest);
                return;
            }
        }

        // Default: raw byte copy
        textSection << "    mov rsi, " << (srcHasSub ? "rax" : src) << "\n";
        textSection << "    mov rcx, " << copySize << "\n";
        bool destJustified = false;
        bool destJustifyLeft = false;
        auto fit = fieldJustified.find(move->dest);
        if (fit != fieldJustified.end()) {
            destJustified = fit->second;
            destJustifyLeft = fieldJustifyLeft[move->dest];
        }
        if (destJustified && !destJustifyLeft && destSize > copySize)
        {
            int padCount = destSize - copySize;
            textSection << "    lea rdi, [rel " << dest << "]\n";
            textSection << "    mov rcx, " << padCount << "\n";
            textSection << "    mov al, ' '\n";
            textSection << "    rep stosb\n";
            textSection << "    lea rdi, [rel " << dest << " + " << padCount << "]\n";
            textSection << "    mov rcx, " << copySize << "\n";
        }
        else
        {
            textSection << "    mov rdi, " << dest << "\n";
        }
        textSection << "    rep movsb\n";
        if (destSize > copySize)
        {
            if (destJustified && destJustifyLeft)
            {
                textSection << "    lea rdi, [rel " << dest << " + " << copySize << "]\n";
                textSection << "    mov rcx, " << (destSize - copySize) << "\n";
                textSection << "    mov al, ' '\n";
                textSection << "    rep stosb\n";
            }
            else if (destJustified && !destJustifyLeft)
            {
                // Already positioned correctly; no extra padding needed
            }
            else
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
        needsAsciiHelpers = true;
        int destSize = 1;
        auto it = fieldSizes.find(add->dest);
        if (it != fieldSizes.end())
            destSize = it->second;
        bool isSigned = false;
        auto sit = fieldSigned.find(add->dest);
        if (sit != fieldSigned.end())
            isSigned = sit->second;

        textSection << "    ; ADD " << add->left << " TO " << add->right;
        if (add->dest != add->right)
            textSection << " GIVING " << add->dest;
        textSection << "\n";

        // Determine effective decimal places for each operand
        int leftDP = 0, rightDP = 0, destDP = 0;
        int leftLitDP = 0, rightLitDP = 0;
        if (add->leftIsLiteral)
        {
            std::string src = add->left;
            if (!src.empty() && (src[0] == '-' || src[0] == '+')) src = src.substr(1);
            size_t dotPos = src.find('.');
            if (dotPos != std::string::npos)
                leftLitDP = (int)(src.size() - dotPos - 1);
        }
        else
        {
            std::string leftBase = add->left;
            auto ldpit = fieldDecimalPlaces.find(leftBase);
            if (ldpit != fieldDecimalPlaces.end())
                leftDP = ldpit->second;
        }
        if (add->rightIsLiteral)
        {
            std::string src = add->right;
            if (!src.empty() && (src[0] == '-' || src[0] == '+')) src = src.substr(1);
            size_t dotPos = src.find('.');
            if (dotPos != std::string::npos)
                rightLitDP = (int)(src.size() - dotPos - 1);
        }
        else
        {
            std::string rightBase = add->right;
            auto rdpit = fieldDecimalPlaces.find(rightBase);
            if (rdpit != fieldDecimalPlaces.end())
                rightDP = rdpit->second;
        }
        {
            auto dpit = fieldDecimalPlaces.find(add->dest);
            if (dpit != fieldDecimalPlaces.end())
                destDP = dpit->second;
        }

        int leftEffectiveDP = add->leftIsLiteral ? leftLitDP : leftDP;
        int rightEffectiveDP = add->rightIsLiteral ? rightLitDP : rightDP;
        int maxDP = std::max(leftEffectiveDP, rightEffectiveDP);

        // Helper lambda: load operand as raw scaled integer at maxDP
        auto loadRawScaled = [&](const std::string &operand, bool isLit, int operandDP, int litDP)
        {
            if (isLit)
            {
                std::string stripped;
                for (char c : operand)
                    if (c != '.') stripped += c;
                std::string litLabel = newLabel("lit");
                emitLiteral(litLabel, stripped);
                textSection << "    lea rsi, [rel " << litLabel << "]\n";
                textSection << "    mov rcx, " << stripped.length() << "\n";
                textSection << "    call ascii_to_int\n";
                for (int i = 0; i < maxDP - litDP; ++i)
                {
                    textSection << "    mov rbx, 10\n";
                    textSection << "    imul rax, rbx\n";
                }
            }
            else if (variableHasSubscript(operand))
            {
                std::string base = baseVariableName(operand);
                std::string subExpr = subscriptExpression(operand);
                int elemSize = resolveElementSize(base);
                textSection << "    lea rsi, [rel " << getAsmName(base) << "]\n";
                if (isNumericLiteral(subExpr))
                {
                    int subVal = std::stoi(subExpr);
                    textSection << "    mov rcx, " << (subVal - 1) << "\n";
                }
                else
                {
                    textSection << "    lea rsi, [rel " << getAsmName(subExpr) << "]\n";
                    textSection << "    mov rcx, " << resolveSize(subExpr) << "\n";
                    textSection << "    call ascii_to_int\n";
                    textSection << "    dec rax\n";
                    textSection << "    mov rcx, rax\n";
                }
                if (elemSize > 1)
                {
                    textSection << "    mov rbx, " << elemSize << "\n";
                    textSection << "    imul rcx, rbx\n";
                }
                textSection << "    add rsi, rcx\n";
                textSection << "    mov rcx, " << elemSize << "\n";
                textSection << "    call ascii_to_int\n";
                for (int i = 0; i < maxDP - operandDP; ++i)
                {
                    textSection << "    mov rbx, 10\n";
                    textSection << "    imul rax, rbx\n";
                }
            }
            else
            {
                textSection << "    lea rsi, [rel " << getAsmName(operand) << "]\n";
                textSection << "    mov rcx, " << getAsmName(operand) << "_len\n";
                textSection << "    call ascii_to_int\n";
                for (int i = 0; i < maxDP - operandDP; ++i)
                {
                    textSection << "    mov rbx, 10\n";
                    textSection << "    imul rax, rbx\n";
                }
            }
        };

        // Load left operand
        loadRawScaled(add->left, add->leftIsLiteral, leftDP, leftLitDP);
        textSection << "    push rax\n";

        // Load right operand
        loadRawScaled(add->right, add->rightIsLiteral, rightDP, rightLitDP);
        textSection << "    mov rbx, rax\n";
        textSection << "    pop rax\n";
        textSection << "    add rax, rbx\n";

        // Scale result down to destDP and store
        for (int i = 0; i < maxDP - destDP; ++i)
        {
            textSection << "    mov rbx, 10\n";
            textSection << "    cqo\n";
            textSection << "    idiv rbx\n";
        }

        // Store to destination
        if (variableHasSubscript(add->dest))
        {
            std::string base = baseVariableName(add->dest);
            std::string subExpr = subscriptExpression(add->dest);
            int elemSize = resolveElementSize(base);
            textSection << "    lea rdi, [rel " << getAsmName(base) << "]\n";
            if (isNumericLiteral(subExpr))
            {
                int subVal = std::stoi(subExpr);
                textSection << "    mov rcx, " << (subVal - 1) << "\n";
            }
            else
            {
                textSection << "    lea rsi, [rel " << getAsmName(subExpr) << "]\n";
                textSection << "    mov rcx, " << resolveSize(subExpr) << "\n";
                textSection << "    call ascii_to_int\n";
                textSection << "    dec rax\n";
                textSection << "    mov rcx, rax\n";
            }
            if (elemSize > 1)
            {
                textSection << "    mov rbx, " << elemSize << "\n";
                textSection << "    imul rcx, rbx\n";
            }
            textSection << "    add rdi, rcx\n";
            textSection << "    mov rcx, " << elemSize << "\n";
        }
        else
        {
            textSection << "    lea rdi, [rel " << getAsmName(add->dest) << "]\n";
            textSection << "    mov rcx, " << destSize << "\n";
        }
        if (isSigned)
            textSection << "    call int_to_ascii_signed\n";
        else
            textSection << "    call int_to_ascii\n";
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

        // Determine effective decimal places for each operand
        int leftDP = 0, rightDP = 0, destDP = 0;
        int leftLitDP = 0, rightLitDP = 0;
        if (sub->leftIsLiteral)
        {
            std::string src = sub->left;
            if (!src.empty() && (src[0] == '-' || src[0] == '+')) src = src.substr(1);
            size_t dotPos = src.find('.');
            if (dotPos != std::string::npos)
                leftLitDP = (int)(src.size() - dotPos - 1);
        }
        else
        {
            std::string leftBase = sub->left;
            auto ldpit = fieldDecimalPlaces.find(leftBase);
            if (ldpit != fieldDecimalPlaces.end())
                leftDP = ldpit->second;
        }
        if (sub->rightIsLiteral)
        {
            std::string src = sub->right;
            if (!src.empty() && (src[0] == '-' || src[0] == '+')) src = src.substr(1);
            size_t dotPos = src.find('.');
            if (dotPos != std::string::npos)
                rightLitDP = (int)(src.size() - dotPos - 1);
        }
        else
        {
            std::string rightBase = sub->right;
            auto rdpit = fieldDecimalPlaces.find(rightBase);
            if (rdpit != fieldDecimalPlaces.end())
                rightDP = rdpit->second;
        }
        {
            auto dpit = fieldDecimalPlaces.find(sub->dest);
            if (dpit != fieldDecimalPlaces.end())
                destDP = dpit->second;
        }

        int leftEffectiveDP = sub->leftIsLiteral ? leftLitDP : leftDP;
        int rightEffectiveDP = sub->rightIsLiteral ? rightLitDP : rightDP;
        int maxDP = std::max(leftEffectiveDP, rightEffectiveDP);

        // Helper lambda: load operand as raw scaled integer at maxDP
        auto loadRawScaled = [&](const std::string &operand, bool isLit, int operandDP, int litDP)
        {
            if (isLit)
            {
                std::string stripped;
                for (char c : operand)
                    if (c != '.') stripped += c;
                std::string litLabel = newLabel("lit");
                emitLiteral(litLabel, stripped);
                textSection << "    lea rsi, [rel " << litLabel << "]\n";
                textSection << "    mov rcx, " << stripped.length() << "\n";
                textSection << "    call ascii_to_int\n";
                for (int i = 0; i < maxDP - litDP; ++i)
                {
                    textSection << "    mov rbx, 10\n";
                    textSection << "    imul rax, rbx\n";
                }
            }
            else if (variableHasSubscript(operand))
            {
                std::string base = baseVariableName(operand);
                std::string subExpr = subscriptExpression(operand);
                int elemSize = resolveElementSize(base);
                textSection << "    lea rsi, [rel " << getAsmName(base) << "]\n";
                if (isNumericLiteral(subExpr))
                {
                    int subVal = std::stoi(subExpr);
                    textSection << "    mov rcx, " << (subVal - 1) << "\n";
                }
                else
                {
                    textSection << "    lea rsi, [rel " << getAsmName(subExpr) << "]\n";
                    textSection << "    mov rcx, " << resolveSize(subExpr) << "\n";
                    textSection << "    call ascii_to_int\n";
                    textSection << "    dec rax\n";
                    textSection << "    mov rcx, rax\n";
                }
                if (elemSize > 1)
                {
                    textSection << "    mov rbx, " << elemSize << "\n";
                    textSection << "    imul rcx, rbx\n";
                }
                textSection << "    add rsi, rcx\n";
                textSection << "    mov rcx, " << elemSize << "\n";
                textSection << "    call ascii_to_int\n";
                for (int i = 0; i < maxDP - operandDP; ++i)
                {
                    textSection << "    mov rbx, 10\n";
                    textSection << "    imul rax, rbx\n";
                }
            }
            else
            {
                textSection << "    lea rsi, [rel " << getAsmName(operand) << "]\n";
                textSection << "    mov rcx, " << getAsmName(operand) << "_len\n";
                textSection << "    call ascii_to_int\n";
                for (int i = 0; i < maxDP - operandDP; ++i)
                {
                    textSection << "    mov rbx, 10\n";
                    textSection << "    imul rax, rbx\n";
                }
            }
        };

        // Load right operand (minuend)
        loadRawScaled(sub->right, sub->rightIsLiteral, rightDP, rightLitDP);
        textSection << "    push rax\n";

        // Load left operand (subtrahend)
        loadRawScaled(sub->left, sub->leftIsLiteral, leftDP, leftLitDP);
        textSection << "    mov rbx, rax\n";
        textSection << "    pop rax\n";
        textSection << "    sub rax, rbx\n";

        // Scale result down to destDP and store
        for (int i = 0; i < maxDP - destDP; ++i)
        {
            textSection << "    mov rbx, 10\n";
            textSection << "    cqo\n";
            textSection << "    idiv rbx\n";
        }

        // Store to destination
        if (variableHasSubscript(sub->dest))
        {
            std::string base = baseVariableName(sub->dest);
            std::string subExpr = subscriptExpression(sub->dest);
            int elemSize = resolveElementSize(base);
            textSection << "    lea rdi, [rel " << getAsmName(base) << "]\n";
            if (isNumericLiteral(subExpr))
            {
                int subVal = std::stoi(subExpr);
                textSection << "    mov rcx, " << (subVal - 1) << "\n";
            }
            else
            {
                textSection << "    lea rsi, [rel " << getAsmName(subExpr) << "]\n";
                textSection << "    mov rcx, " << resolveSize(subExpr) << "\n";
                textSection << "    call ascii_to_int\n";
                textSection << "    dec rax\n";
                textSection << "    mov rcx, rax\n";
            }
            if (elemSize > 1)
            {
                textSection << "    mov rbx, " << elemSize << "\n";
                textSection << "    imul rcx, rbx\n";
            }
            textSection << "    add rdi, rcx\n";
            textSection << "    mov rcx, " << elemSize << "\n";
        }
        else
        {
            textSection << "    lea rdi, [rel " << getAsmName(sub->dest) << "]\n";
            textSection << "    mov rcx, " << destSize << "\n";
        }
        if (isSigned)
            textSection << "    call int_to_ascii_signed\n";
        else
            textSection << "    call int_to_ascii\n";
    }

    void generateDivide(const DivideNode *div)
    {
        needsAsciiHelpers = true;
        int destSize = 1;
        auto sizeIt = fieldSizes.find(div->dest);
        if (sizeIt != fieldSizes.end())
            destSize = sizeIt->second;
        bool isSigned = false;
        auto sit = fieldSigned.find(div->dest);
        if (sit != fieldSigned.end())
            isSigned = sit->second;

        textSection << "    ; DIVIDE " << div->left << " INTO " << div->right;
        if (div->dest != div->right)
            textSection << " GIVING " << div->dest;
        if (!div->remainderDest.empty())
            textSection << " REMAINDER " << div->remainderDest;
        textSection << "\n";

        // Determine effective decimal places for each operand
        int leftDP = 0, rightDP = 0, destDP = 0, remDP = 0;
        int leftLitDP = 0, rightLitDP = 0;
        if (div->leftIsLiteral)
        {
            std::string src = div->left;
            if (!src.empty() && (src[0] == '-' || src[0] == '+')) src = src.substr(1);
            size_t dotPos = src.find('.');
            if (dotPos != std::string::npos)
                leftLitDP = (int)(src.size() - dotPos - 1);
        }
        else
        {
            std::string leftBase = variableHasSubscript(div->left) ? baseVariableName(div->left) : div->left;
            auto ldpit = fieldDecimalPlaces.find(leftBase);
            if (ldpit != fieldDecimalPlaces.end())
                leftDP = ldpit->second;
        }
        if (div->rightIsLiteral)
        {
            std::string src = div->right;
            if (!src.empty() && (src[0] == '-' || src[0] == '+')) src = src.substr(1);
            size_t dotPos = src.find('.');
            if (dotPos != std::string::npos)
                rightLitDP = (int)(src.size() - dotPos - 1);
        }
        else
        {
            std::string rightBase = variableHasSubscript(div->right) ? baseVariableName(div->right) : div->right;
            auto rdpit = fieldDecimalPlaces.find(rightBase);
            if (rdpit != fieldDecimalPlaces.end())
                rightDP = rdpit->second;
        }
        {
            auto dpit = fieldDecimalPlaces.find(div->dest);
            if (dpit != fieldDecimalPlaces.end())
                destDP = dpit->second;
        }
        if (!div->remainderDest.empty())
        {
            auto rdpit2 = fieldDecimalPlaces.find(div->remainderDest);
            if (rdpit2 != fieldDecimalPlaces.end())
                remDP = rdpit2->second;
        }

        int leftEffectiveDP = div->leftIsLiteral ? leftLitDP : leftDP;
        int rightEffectiveDP = div->rightIsLiteral ? rightLitDP : rightDP;
        int maxDP = std::max({leftEffectiveDP, rightEffectiveDP});

        // Helper: load operand as raw scaled integer, scaling UP to maxDP
        auto loadRawScaled = [&](const std::string &operand, bool isLit, int operandDP, int litDP)
        {
            if (isLit)
            {
                std::string stripped;
                for (char c : operand)
                    if (c != '.') stripped += c;
                std::string litLabel = newLabel("lit");
                emitLiteral(litLabel, stripped);
                textSection << "    lea rsi, [rel " << litLabel << "]\n";
                textSection << "    mov rcx, " << stripped.length() << "\n";
                textSection << "    call ascii_to_int\n";
                for (int i = 0; i < maxDP - litDP; ++i)
                {
                    textSection << "    mov rbx, 10\n";
                    textSection << "    imul rax, rbx\n";
                }
            }
            else if (variableHasSubscript(operand))
            {
                std::string base = baseVariableName(operand);
                std::string subExpr = subscriptExpression(operand);
                int elemSize = resolveElementSize(base);
                textSection << "    lea rsi, [rel " << getAsmName(base) << "]\n";
                if (isNumericLiteral(subExpr))
                {
                    int subVal = std::stoi(subExpr);
                    textSection << "    mov rcx, " << (subVal - 1) << "\n";
                }
                else
                {
                    textSection << "    lea rsi, [rel " << getAsmName(subExpr) << "]\n";
                    textSection << "    mov rcx, " << resolveSize(subExpr) << "\n";
                    textSection << "    call ascii_to_int\n";
                    textSection << "    dec rax\n";
                    textSection << "    mov rcx, rax\n";
                }
                if (elemSize > 1)
                {
                    textSection << "    mov rbx, " << elemSize << "\n";
                    textSection << "    imul rcx, rbx\n";
                }
                textSection << "    add rsi, rcx\n";
                textSection << "    mov rcx, " << elemSize << "\n";
                textSection << "    call ascii_to_int\n";
                for (int i = 0; i < maxDP - operandDP; ++i)
                {
                    textSection << "    mov rbx, 10\n";
                    textSection << "    imul rax, rbx\n";
                }
            }
            else
            {
                textSection << "    lea rsi, [rel " << getAsmName(operand) << "]\n";
                textSection << "    mov rcx, " << getAsmName(operand) << "_len\n";
                textSection << "    call ascii_to_int\n";
                for (int i = 0; i < maxDP - operandDP; ++i)
                {
                    textSection << "    mov rbx, 10\n";
                    textSection << "    imul rax, rbx\n";
                }
            }
        };

        // Load dividend (left operand)
        loadRawScaled(div->left, div->leftIsLiteral, leftDP, leftLitDP);
        textSection << "    push rax\n";

        // Load divisor (right operand)
        loadRawScaled(div->right, div->rightIsLiteral, rightDP, rightLitDP);
        textSection << "    mov rbx, rax\n";
        textSection << "    pop rax\n";
        textSection << "    cqo\n";
        textSection << "    idiv rbx\n"; // rax=quotient, rdx=remainder (both at maxDP scale)
        textSection << "    push rdx\n"; // save remainder at maxDP for later

        // Scale quotient to destDP
        for (int i = 0; i < destDP; ++i)
        {
            textSection << "    mov rbx, 10\n";
            textSection << "    imul rax, rbx\n";
        }
        // Store quotient to dest
        if (variableHasSubscript(div->dest))
        {
            std::string base = baseVariableName(div->dest);
            std::string subExpr = subscriptExpression(div->dest);
            int elemSize = resolveElementSize(base);
            textSection << "    lea rdi, [rel " << getAsmName(base) << "]\n";
            if (isNumericLiteral(subExpr))
            {
                int subVal = std::stoi(subExpr);
                textSection << "    mov rcx, " << (subVal - 1) << "\n";
            }
            else
            {
                textSection << "    lea rsi, [rel " << getAsmName(subExpr) << "]\n";
                textSection << "    mov rcx, " << resolveSize(subExpr) << "\n";
                textSection << "    call ascii_to_int\n";
                textSection << "    dec rax\n";
                textSection << "    mov rcx, rax\n";
            }
            if (elemSize > 1)
            {
                textSection << "    mov rbx, " << elemSize << "\n";
                textSection << "    imul rcx, rbx\n";
            }
            textSection << "    add rdi, rcx\n";
            textSection << "    mov rcx, " << elemSize << "\n";
        }
        else
        {
            textSection << "    lea rdi, [rel " << getAsmName(div->dest) << "]\n";
            textSection << "    mov rcx, " << destSize << "\n";
        }
        if (isSigned)
            textSection << "    call int_to_ascii_signed\n";
        else
            textSection << "    call int_to_ascii\n";

        // Handle REMAINDER for DIVIDE
        if (!div->remainderDest.empty())
        {
            textSection << "    ; DIVIDE REMAINDER\n";
            textSection << "    pop rax\n"; // load remainder at maxDP from stack
            // rdx currently has remainder at maxDP scale (from idiv above)
            // Scale remainder to remDP
            if (remDP > maxDP)
            {
                for (int i = 0; i < remDP - maxDP; ++i)
                {
                    textSection << "    mov rbx, 10\n";
                    textSection << "    imul rax, rbx\n";
                }
            }
            else if (remDP < maxDP)
            {
                for (int i = 0; i < maxDP - remDP; ++i)
                {
                    textSection << "    mov rbx, 10\n";
                    textSection << "    cqo\n";
                    textSection << "    idiv rbx\n";
                }
            }
            // Store remainder to remainderDest
            if (variableHasSubscript(div->remainderDest))
            {
                std::string rbase = baseVariableName(div->remainderDest);
                std::string rsubExpr = subscriptExpression(div->remainderDest);
                int relemSize = resolveElementSize(rbase);
                textSection << "    lea rdi, [rel " << getAsmName(rbase) << "]\n";
                if (isNumericLiteral(rsubExpr))
                {
                    int rsubVal = std::stoi(rsubExpr);
                    textSection << "    mov rcx, " << (rsubVal - 1) << "\n";
                }
                else
                {
                    textSection << "    lea rsi, [rel " << getAsmName(rsubExpr) << "]\n";
                    textSection << "    mov rcx, " << resolveSize(rsubExpr) << "\n";
                    textSection << "    call ascii_to_int\n";
                    textSection << "    dec rax\n";
                    textSection << "    mov rcx, rax\n";
                }
                if (relemSize > 1)
                {
                    textSection << "    mov rbx, " << relemSize << "\n";
                    textSection << "    imul rcx, rbx\n";
                }
                textSection << "    add rdi, rcx\n";
                textSection << "    mov rcx, " << relemSize << "\n";
            }
            else
            {
                int remSize = 1;
                auto rsit = fieldSizes.find(div->remainderDest);
                if (rsit != fieldSizes.end())
                    remSize = rsit->second;
                textSection << "    lea rdi, [rel " << getAsmName(div->remainderDest) << "]\n";
                textSection << "    mov rcx, " << remSize << "\n";
            }
            bool remSigned = false;
            auto rsm = fieldSigned.find(div->remainderDest);
            if (rsm != fieldSigned.end())
                remSigned = rsm->second;
            if (remSigned)
                textSection << "    call int_to_ascii_signed\n";
            else
                textSection << "    call int_to_ascii\n";
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
        needsAsciiHelpers = true;
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
                bool hasSub = variableHasSubscript(disp->operands[i]);
                int varSize;
                if (hasSub)
                {
                    std::string base = baseVariableName(disp->operands[i]);
                    std::string subExpr = subscriptExpression(disp->operands[i]);
                    varSize = resolveElementSize(base);
                    const std::string baseAsm = getAsmName(base);

                    textSection << "    lea rsi, [rel " << baseAsm << "]\n";
                    if (isNumericLiteral(subExpr))
                    {
                        int subVal = std::stoi(subExpr);
                        textSection << "    mov rcx, " << (subVal - 1) << "\n";
                    }
                    else
                    {
                        textSection << "    push rsi\n";
                        textSection << "    lea rsi, [rel " << getAsmName(subExpr) << "]\n";
                        textSection << "    mov rcx, " << resolveSize(subExpr) << "\n";
                        textSection << "    call ascii_to_int\n";
                        textSection << "    pop rsi\n";
                        textSection << "    dec rax\n";
                        textSection << "    mov rcx, rax\n";
                    }
                    if (varSize > 1)
                    {
                        textSection << "    mov rbx, " << varSize << "\n";
                        textSection << "    imul rcx, rbx\n";
                    }
                    textSection << "    add rsi, rcx\n";

                    auto dpIt2 = fieldDecimalPlaces.find(base);
                    int dp2 = (dpIt2 != fieldDecimalPlaces.end()) ? dpIt2->second : 0;
                    if (dp2 > 0)
                    {
                        emitFormatVOutputHelper();
                        std::string fmtBuf = newLabel("fmt_buf");
                        bssSection << "    " << fmtBuf << ": resb " << (varSize + 1) << "\n";
                        textSection << "    mov rcx, " << varSize << "\n";
                        textSection << "    mov r8, " << (varSize - dp2) << "\n";
                        textSection << "    lea rdi, [rel " << fmtBuf << "]\n";
                        textSection << "    call format_v_output\n";
                        textSection << "    mov rax, 1\n";
                        textSection << "    mov rdi, 1\n";
                        textSection << "    mov rsi, " << fmtBuf << "\n";
                        textSection << "    mov rdx, " << (varSize + 1) << "\n";
                        textSection << "    syscall\n";
                    }
                    else
                    {
                        textSection << "    mov rax, 1\n";
                        textSection << "    mov rdi, 1\n";
                        textSection << "    mov rdx, " << varSize << "\n";
                        textSection << "    syscall\n";
                    }
                }
                else
                {
                    std::string var = resolveName(disp->operands[i]);
                    varSize = resolveSize(disp->operands[i]);
                    auto dpIt = fieldDecimalPlaces.find(var);
                    int dp = (dpIt != fieldDecimalPlaces.end()) ? dpIt->second : 0;
                    if (dp > 0)
                    {
                        emitFormatVOutputHelper();
                        std::string fmtBuf = newLabel("fmt_buf");
                        bssSection << "    " << fmtBuf << ": resb " << (varSize + 1) << "\n";
                        textSection << "    mov rsi, " << var << "\n";
                        textSection << "    mov rcx, " << varSize << "\n";
                        textSection << "    mov r8, " << (varSize - dp) << "\n";
                        textSection << "    lea rdi, [rel " << fmtBuf << "]\n";
                        textSection << "    call format_v_output\n";
                        textSection << "    mov rax, 1\n";
                        textSection << "    mov rdi, 1\n";
                        textSection << "    mov rsi, " << fmtBuf << "\n";
                        textSection << "    mov rdx, " << (varSize + 1) << "\n";
                        textSection << "    syscall\n";
                    }
                    else
                    {
                        textSection << "    mov rax, 1\n";
                        textSection << "    mov rdi, 1\n";
                        textSection << "    mov rsi, " << var << "\n";
                        textSection << "    mov rdx, " << varSize << "\n";
                        textSection << "    syscall\n";
                    }
                }
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

    void generatePrint(const PrintNode *print)
    {
        for (size_t i = 0; i < print->operands.size(); ++i)
        {
            if (print->isLiteral[i])
            {
                std::string litLabel = newLabel("print_lit");
                emitLiteral(litLabel, print->operands[i]);
                textSection << "    mov rax, 1\n";
                textSection << "    mov rdi, 1\n";
                textSection << "    mov rsi, " << litLabel << "\n";
                textSection << "    mov rdx, " << litLabel << "_len\n";
                textSection << "    syscall\n";
            }
            else
            {
                bool hasSub = variableHasSubscript(print->operands[i]);
                int varSize;
                if (hasSub)
                {
                    std::string base = baseVariableName(print->operands[i]);
                    std::string subExpr = subscriptExpression(print->operands[i]);
                    varSize = resolveElementSize(base);
                    const std::string baseAsm = getAsmName(base);

                    textSection << "    lea rsi, [rel " << baseAsm << "]\n";
                    if (isNumericLiteral(subExpr))
                    {
                        int subVal = std::stoi(subExpr);
                        textSection << "    mov rcx, " << (subVal - 1) << "\n";
                    }
                    else
                    {
                        textSection << "    push rsi\n";
                        textSection << "    lea rsi, [rel " << getAsmName(subExpr) << "]\n";
                        textSection << "    mov rcx, " << resolveSize(subExpr) << "\n";
                        textSection << "    call ascii_to_int\n";
                        textSection << "    pop rsi\n";
                        textSection << "    dec rax\n";
                        textSection << "    mov rcx, rax\n";
                    }
                    if (varSize > 1)
                    {
                        textSection << "    mov rbx, " << varSize << "\n";
                        textSection << "    imul rcx, rbx\n";
                    }
                    textSection << "    add rsi, rcx\n";

                    auto dpIt2 = fieldDecimalPlaces.find(base);
                    int dp2 = (dpIt2 != fieldDecimalPlaces.end()) ? dpIt2->second : 0;
                    if (dp2 > 0)
                    {
                        emitFormatVOutputHelper();
                        std::string fmtBuf = newLabel("fmt_buf");
                        bssSection << "    " << fmtBuf << ": resb " << (varSize + 1) << "\n";
                        textSection << "    mov rcx, " << varSize << "\n";
                        textSection << "    mov r8, " << (varSize - dp2) << "\n";
                        textSection << "    lea rdi, [rel " << fmtBuf << "]\n";
                        textSection << "    call format_v_output\n";
                        textSection << "    mov rax, 1\n";
                        textSection << "    mov rdi, 1\n";
                        textSection << "    mov rsi, " << fmtBuf << "\n";
                        textSection << "    mov rdx, " << (varSize + 1) << "\n";
                        textSection << "    syscall\n";
                    }
                    else
                    {
                        textSection << "    mov rax, 1\n";
                        textSection << "    mov rdi, 1\n";
                        textSection << "    mov rdx, " << varSize << "\n";
                        textSection << "    syscall\n";
                    }
                }
                else
                {
                    std::string var = resolveName(print->operands[i]);
                    varSize = resolveSize(print->operands[i]);
                    auto dpIt = fieldDecimalPlaces.find(var);
                    int dp = (dpIt != fieldDecimalPlaces.end()) ? dpIt->second : 0;
                    if (dp > 0)
                    {
                        emitFormatVOutputHelper();
                        std::string fmtBuf = newLabel("fmt_buf");
                        bssSection << "    " << fmtBuf << ": resb " << (varSize + 1) << "\n";
                        textSection << "    mov rsi, " << var << "\n";
                        textSection << "    mov rcx, " << varSize << "\n";
                        textSection << "    mov r8, " << (varSize - dp) << "\n";
                        textSection << "    lea rdi, [rel " << fmtBuf << "]\n";
                        textSection << "    call format_v_output\n";
                        textSection << "    mov rax, 1\n";
                        textSection << "    mov rdi, 1\n";
                        textSection << "    mov rsi, " << fmtBuf << "\n";
                        textSection << "    mov rdx, " << (varSize + 1) << "\n";
                        textSection << "    syscall\n";
                    }
                    else
                    {
                        textSection << "    mov rax, 1\n";
                        textSection << "    mov rdi, 1\n";
                        textSection << "    mov rsi, " << var << "\n";
                        textSection << "    mov rdx, " << varSize << "\n";
                        textSection << "    syscall\n";
                    }
                }
            }
        }
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
            textSection << "    jmp " << endifLabel << "\n";
        }
        else
        {
            // OR-chain: if any condition true then execute thenStatements
            std::string trueLabel = newLabel("if_true");
            for (const auto &c : ifNode->conditions)
                generateConditionJumpTrue(c.get(), trueLabel);

            // none true -> else
            textSection << "    jmp " << elseLabel << "\n";

            textSection << trueLabel << ":\n";
            for (const auto &stmt : ifNode->thenStatements)
                generateStatement(stmt.get());
            textSection << "    jmp " << endifLabel << "\n";
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

        if (perf->testBefore)
        {
            textSection << startLabel << ":\n";
            generateConditionJumpTrue(perf->condition.get(), endLabel);
            for (const auto &stmt : perf->body)
            {
                generateStatement(stmt.get());
            }
            textSection << "    jmp " << startLabel << "\n";
        }
        else
        {
            textSection << startLabel << ":\n";
            for (const auto &stmt : perf->body)
            {
                generateStatement(stmt.get());
            }
            generateConditionJumpTrue(perf->condition.get(), endLabel);
            textSection << "    jmp " << startLabel << "\n";
        }
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

        if (perf->testBefore)
        {
            generateConditionJumpTrue(perf->untilCondition.get(), endLabel);
        }

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

        if (!perf->testBefore)
        {
            generateConditionJumpTrue(perf->untilCondition.get(), endLabel);
        }

        textSection << "    jmp " << startLabel << "\n";
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
        textSection << "    je " << endLabel << "\n";

        for (const auto &stmt : perf->body)
        {
            generateStatement(stmt.get());
        }

        textSection << "    dec byte [" << counterLabel << "]\n";
        textSection << "    jmp " << startLabel << "\n";
        textSection << endLabel << ":\n";
    }

    void generateStart(const StartNode *start)
    {
        textSection << "    ; START " << start->fileName << " KEY " << start->comp << " " << start->keyVar << "\n";
        std::string recordVar;
        int recordSize = 0;
        if (fileRecordVars.find(start->fileName) != fileRecordVars.end())
        {
            recordVar = fileRecordVars[start->fileName];
            recordSize = fileRecordSizes[start->fileName];
        }

        std::string keyField = start->keyVar;
        std::string keyAsmName = getAsmName(keyField);
        if (variables.find(keyField) != variables.end())
            keyAsmName = variables[keyField];
        int keySize = 1;
        auto keyIt = fieldSizes.find(keyField);
        if (keyIt != fieldSizes.end())
            keySize = keyIt->second;

        std::string prefetchFlag;
        std::string prefetchKeyTemp;
        if (!recordVar.empty())
        {
            ensurePrefetchBuffers(start->fileName, keySize);
            prefetchFlag = filePrefetchFlagNames[start->fileName];
            prefetchKeyTemp = filePrefetchKeyTemps[start->fileName];
        }

        std::string startLoop = newLabel("start_loop");
        std::string startInvalid = newLabel("start_invalid");
        std::string startFound = newLabel("start_found");

        if (!prefetchFlag.empty())
        {
            textSection << "    mov byte [" << prefetchFlag << "], 0\n";
            int keyVarSize = 1;
            auto keyVarIt = fieldSizes.find(start->keyVar);
            if (keyVarIt != fieldSizes.end())
                keyVarSize = keyVarIt->second;
            int copyLen = std::min(keyVarSize, keySize);
            textSection << "    mov rsi, " << getAsmName(start->keyVar) << "\n";
            textSection << "    mov rdi, " << prefetchKeyTemp << "\n";
            textSection << "    mov rcx, " << copyLen << "\n";
            textSection << "    rep movsb\n";
            if (copyLen < keySize)
            {
                char pad = fieldNumeric[start->keyVar] ? '0' : ' ';
                textSection << "    mov al, '" << pad << "'\n";
                textSection << "    mov rcx, " << (keySize - copyLen) << "\n";
                textSection << "    rep stosb\n";
            }
        }

        if (fileFdNames.find(start->fileName) == fileFdNames.end())
        {
            throw std::runtime_error("Code generation error: file descriptor for '" + start->fileName + "' not defined");
        }

        // ===== Rewind file to beginning =====
        textSection << "    ; Rewind file to beginning\n";
        textSection << "    mov rax, 8\n";          // sys_lseek
        textSection << "    mov rdi, [" << fileFdNames[start->fileName] << "]\n";
        textSection << "    xor rsi, rsi\n";       // offset = 0
        textSection << "    xor rdx, rdx\n";       // whence = SEEK_SET (0)
        textSection << "    syscall\n";

        textSection << startLoop << ":\n";
        textSection << "    mov rax, 0\n";
        textSection << "    mov rdi, [" << fileFdNames[start->fileName] << "]\n";
        textSection << "    mov rsi, " << getAsmName(recordVar) << "\n";
        textSection << "    mov rdx, " << recordSize << "\n";
        textSection << "    syscall\n";
        emitFileStatusUpdate(start->fileName);
        textSection << "    cmp rax, 0\n";
        textSection << "    jle " << startInvalid << "\n";

        // ===== Compare record key (rsi) with target key (rdi) =====
        textSection << "    mov rcx, " << keySize << "\n";
        textSection << "    mov rsi, " << keyAsmName << "\n";      // record key
        textSection << "    mov rdi, " << prefetchKeyTemp << "\n";  // target key

        std::string cmpLoop = newLabel("key_cmp_loop");
        std::string cmpEqual = newLabel("key_cmp_equal");
        std::string cmpDone = newLabel("key_cmp_done");
        std::string cmpLess = newLabel("key_cmp_less");
        std::string cmpGreater = newLabel("key_cmp_greater");

        textSection << cmpLoop << ":\n";
        textSection << "    cmp rcx, 0\n";
        textSection << "    je " << cmpEqual << "\n";
        textSection << "    mov al, [rsi]\n";   // record byte
        textSection << "    mov bl, [rdi]\n";   // target byte
        textSection << "    cmp al, bl\n";      // record vs target
        textSection << "    jne " << cmpDone << "\n";
        textSection << "    inc rsi\n";
        textSection << "    inc rdi\n";
        textSection << "    dec rcx\n";
        textSection << "    jmp " << cmpLoop << "\n";

        textSection << cmpEqual << ":\n";
        textSection << "    xor al, al\n";
        textSection << "    xor bl, bl\n";
        textSection << "    jmp " << cmpDone << "\n";

        textSection << cmpDone << ":\n";
        // After cmp al, bl: flags = record - target
        // jl = record < target, jg = record > target, je = record == target
        textSection << "    cmp al, bl\n";
        textSection << "    jl " << cmpLess << "\n";       // record < target
        textSection << "    jg " << cmpGreater << "\n";    // record > target
        // Fallthrough: record == target

        // ===== EQUAL case (record == target) =====
        if (start->comp == "EQUAL" || start->comp == "LESS_THAN_OR_EQUAL" || start->comp == "GREATER_THAN_OR_EQUAL")
            textSection << "    jmp " << startFound << "\n";
        else
            textSection << "    jmp " << startLoop << "\n";

        // ===== LESS case (record < target) =====
        textSection << cmpLess << ":\n";
        if (start->comp == "LESS_THAN" || start->comp == "LESS_THAN_OR_EQUAL" || start->comp == "NOT_EQUAL")
            textSection << "    jmp " << startFound << "\n";
        else
            textSection << "    jmp " << startLoop << "\n";

        // ===== GREATER case (record > target) =====
        textSection << cmpGreater << ":\n";
        if (start->comp == "GREATER_THAN" || start->comp == "GREATER_THAN_OR_EQUAL" || start->comp == "NOT_EQUAL")
            textSection << "    jmp " << startFound << "\n";
        else
            textSection << "    jmp " << startLoop << "\n";

        std::string startDone = newLabel("start_done");

        textSection << startInvalid << ":\n";
        textSection << "    mov byte [" << prefetchFlag << "], 0\n";
        for (const auto &stmt : start->invalidKeyStatements)
            generateStatement(stmt.get());
        textSection << "    jmp " << startDone << "\n";

        textSection << startFound << ":\n";
        textSection << "    mov byte [" << prefetchFlag << "], 1\n";
        for (const auto &stmt : start->notInvalidKeyStatements)
            generateStatement(stmt.get());

        textSection << startDone << ":\n";
    }
    void generatePerformParagraph(const PerformParagraphNode *perf)
    {
        if (perf->untilCondition)
        {
            if (perf->testBefore)
            {
                // PERFORM para UNTIL cond — TEST BEFORE (skip body while cond true)
                std::string loopLabel = newLabel("perf_until_loop");
                std::string endLabel = newLabel("perf_until_end");
                textSection << "    ; PERFORM " << perf->target << " UNTIL ...\n";
                textSection << loopLabel << ":\n";
                generateConditionJumpTrue(perf->untilCondition.get(), endLabel);
                emitPerformOnce(perf->target);
                textSection << "    jmp " << loopLabel << "\n";
                textSection << endLabel << ":\n";
            }
            else
            {
                // PERFORM para UNTIL cond — TEST AFTER (execute body, then check)
                std::string loopLabel = newLabel("perf_after_loop");
                std::string endLabel = newLabel("perf_after_end");
                textSection << "    ; PERFORM " << perf->target << " UNTIL ... TEST AFTER\n";
                textSection << loopLabel << ":\n";
                emitPerformOnce(perf->target);
                generateConditionJumpTrue(perf->untilCondition.get(), endLabel);
                textSection << "    jmp " << loopLabel << "\n";
                textSection << endLabel << ":\n";
            }
        }
        else
        {
            textSection << "    ; PERFORM " << perf->target << "\n";
            emitPerformOnce(perf->target);
        }
    }

    void generateGoTo(const GoToNode *go)
    {
        textSection << "    jmp " << getAsmName(go->target) << "\n";
    }

    void generateParagraph(const ParagraphNode *para)
    {
        // Finish previous paragraph before opening a new label.
        emitParagraphEpilogue();
        ensurePerformRuntime();
        insideParagraph = true;
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
        textSection << "    je " << endLabel << "\n";
        textSection << "    cmp byte [rsi], " << (int)(unsigned char)oldChar << "\n";
        textSection << "    jne " << nextLabel << "\n";
        textSection << "    mov byte [rsi], " << (int)(unsigned char)newChar << "\n";
        textSection << nextLabel << ":\n";
        textSection << "    inc rsi\n";
        textSection << "    dec rcx\n";
        textSection << "    jmp " << loopLabel << "\n";
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
        emitFileStatusUpdate(open->fileName);
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
        emitFileStatusUpdate(close->fileName);
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
        std::string readDoneLabel = newLabel("read_done");

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

        // ----- Keyed READ: READ file-name KEY IS key-var -----
        bool isKeyedRead = !read->keyVar.empty();
        std::string keyReadLoop, keyReadFound, keyReadInvalid;
        std::string recordKeyVar;
        int recordKeySize = 0;

        if (isKeyedRead) {
            if (fileRecordKeyVars.find(read->fileName) != fileRecordKeyVars.end()) {
                recordKeyVar = fileRecordKeyVars[read->fileName];
                auto kit = fieldSizes.find(recordKeyVar);
                if (kit != fieldSizes.end())
                    recordKeySize = kit->second;
            }

            keyReadLoop    = newLabel("read_key_loop");
            keyReadFound   = newLabel("read_key_found");
            keyReadInvalid = newLabel("read_key_invalid");

            textSection << "    ; Keyed READ: search for record key\n";
            textSection << "    mov rax, 8\n";               // sys_lseek
            textSection << "    mov rdi, [" << fdVar << "]\n";
            textSection << "    xor rsi, rsi\n";             // offset 0
            textSection << "    xor rdx, rdx\n";             // SEEK_SET
            textSection << "    syscall\n";

            textSection << keyReadLoop << ":\n";
            textSection << "    mov rax, 0\n";               // sys_read
            textSection << "    mov rdi, [" << fdVar << "]\n";
            if (!fileRecordVar.empty()) {
                textSection << "    mov rsi, " << getAsmName(fileRecordVar) << "\n";
            } else {
                textSection << "    mov rsi, 0\n";
            }
            textSection << "    mov rdx, " << recordSize << "\n";
            textSection << "    syscall\n";
            emitFileStatusUpdate(read->fileName);
            textSection << "    cmp rax, 0\n";
            textSection << "    jle " << keyReadInvalid << "\n";

            if (!recordKeyVar.empty() && recordKeySize > 0) {
                textSection << "    mov rcx, " << recordKeySize << "\n";
                textSection << "    mov rsi, " << getAsmName(recordKeyVar) << "\n";
                textSection << "    mov rdi, " << getAsmName(read->keyVar) << "\n";
                textSection << "    repe cmpsb\n";
                textSection << "    je " << keyReadFound << "\n";
            }
            textSection << "    jmp " << keyReadLoop << "\n";

            textSection << keyReadFound << ":\n";
            if (!read->intoVar.empty() && !fileRecordVar.empty() && read->intoVar != fileRecordVar) {
                int intoSize = 1;
                auto it = fieldSizes.find(read->intoVar);
                if (it != fieldSizes.end())
                    intoSize = it->second;
                int copySize = std::min(recordSize, intoSize);
                textSection << "    mov rsi, " << getAsmName(fileRecordVar) << "\n";
                textSection << "    mov rdi, " << getAsmName(read->intoVar) << "\n";
                textSection << "    mov rcx, " << copySize << "\n";
                textSection << "    rep movsb\n";
            }
            // Skip the normal read path — we already have the record
            textSection << "    jmp " << readDoneLabel << "\n";

            textSection << keyReadInvalid << ":\n";
            // Fall through to normal failure handling (INVALID KEY / AT END)
        }

        if (canUsePrefetch)
        {
            textSection << "    mov al, [" << filePrefetchFlagNames[read->fileName] << "]\n";
            textSection << "    cmp al, 1\n";
            textSection << "    jne " << useSyscallLabel << "\n";
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
            textSection << "    jmp " << afterReadLabel << "\n";
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
        emitFileStatusUpdate(read->fileName);

        if (canUsePrefetch)
        {
            textSection << afterReadLabel << ":\n";
        }

        bool needsBranch = read->hasAtEnd || read->hasNotAtEnd ||
                           read->hasInvalidKey || read->hasNotInvalidKey ||
                           !read->intoVar.empty();

        if (needsBranch)
        {
            std::string invalidKeyLabel = newLabel("invalid_key");
            std::string atEndLabel = newLabel("at_end");
            bool hasInvalidKeyBranch = read->hasInvalidKey || read->hasNotInvalidKey;
            bool hasAtEndBranch = read->hasAtEnd || read->hasNotAtEnd;
            std::string failLabel = hasInvalidKeyBranch ? invalidKeyLabel : atEndLabel;

            textSection << "    cmp rax, 0\n";
            textSection << "    jle " << failLabel << "\n";

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

            if (hasInvalidKeyBranch && read->hasNotInvalidKey)
            {
                for (const auto &stmt : read->notInvalidKeyStatements)
                {
                    generateStatement(stmt.get());
                }
            }
            else if (hasAtEndBranch && read->hasNotAtEnd)
            {
                for (const auto &stmt : read->notAtEndStatements)
                {
                    generateStatement(stmt.get());
                }
            }

            textSection << "    jmp " << readDoneLabel << "\n";

            if (hasInvalidKeyBranch)
            {
                textSection << invalidKeyLabel << ":\n";
                for (const auto &stmt : read->invalidKeyStatements)
                {
                    generateStatement(stmt.get());
                }
                if (hasAtEndBranch)
                {
                    textSection << "    jmp " << readDoneLabel << "\n";
                }
            }
            if (hasAtEndBranch)
            {
                textSection << atEndLabel << ":\n";
                for (const auto &stmt : read->atEndStatements)
                {
                    generateStatement(stmt.get());
                }
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
            auto dpIt = fieldDecimalPlaces.find(write->recordName);
            int dp = (dpIt != fieldDecimalPlaces.end()) ? dpIt->second : 0;
            if (dp > 0)
            {
                emitFormatVOutputHelper();
                std::string fmtBuf = newLabel("fmt_buf");
                bssSection << "    " << fmtBuf << ": resb " << (size + 1) << "\n";
                textSection << "    mov rsi, " << var << "\n";
                textSection << "    mov rcx, " << size << "\n";
                textSection << "    mov r8, " << (size - dp) << "\n";
                textSection << "    lea rdi, [rel " << fmtBuf << "]\n";
                textSection << "    call format_v_output\n";
                textSection << "    mov rax, 1\n";
                textSection << "    mov rdi, 1\n";
                textSection << "    mov rsi, " << fmtBuf << "\n";
                textSection << "    mov rdx, " << (size + 1) << "\n";
                textSection << "    syscall\n";
            }
            else
            {
                textSection << "    mov rax, 1\n";
                textSection << "    mov rdi, 1\n";
                textSection << "    mov rsi, " << var << "\n";
                textSection << "    mov rdx, " << size << "\n";
                textSection << "    syscall\n";
            }
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
            emitFileStatusUpdate(write->fileName);

            if (write->hasInvalidKey || write->hasNotInvalidKey)
            {
                std::string invalidKeyLabel = newLabel("write_invalid_key");
                std::string writeDoneLabel = newLabel("write_done");

                textSection << "    cmp rax, 0\n";
                textSection << "    jle " << invalidKeyLabel << "\n";

                if (write->hasNotInvalidKey)
                {
                    for (const auto &stmt : write->notInvalidKeyStatements)
                    {
                        generateStatement(stmt.get());
                    }
                }
                textSection << "    jmp " << writeDoneLabel << "\n";

                textSection << invalidKeyLabel << ":\n";
                if (write->hasInvalidKey)
                {
                    for (const auto &stmt : write->invalidKeyStatements)
                    {
                        generateStatement(stmt.get());
                    }
                }
                textSection << writeDoneLabel << ":\n";
            }
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

    void generateSearch(const SearchNode *search)
    {
        std::string tableName = search->tableName;
        std::string tableAsmName = getAsmName(tableName);

        int elementSize = resolveElementSize(tableName);
        int totalSize = 1;
        auto it = fieldSizes.find(tableName);
        if (it != fieldSizes.end())
            totalSize = it->second;

        // Find the indexedBy variable name from the table data item
        std::string indexName;

        textSection << "    ; SEARCH " << (search->isSearchAll ? "ALL " : "") << tableName << "\n";

        if (search->isSearchAll)
        {
            // Binary search (SEARCH ALL) - table must be sorted ascending
            int numElements = (elementSize > 0 && totalSize >= elementSize) ? totalSize / elementSize : 0;
            if (numElements <= 0) numElements = 1;

            std::string loopLabel = newLabel("search_loop");
            std::string foundLabel = newLabel("search_found");
            std::string endLabel = newLabel("search_end");
            std::string notFoundLabel = newLabel("search_not_found");

            textSection << "    mov rax, 1\n";               // low = 1
            textSection << "    mov rcx, " << numElements << "\n"; // high = N

            textSection << loopLabel << ":\n";
            textSection << "    cmp rax, rcx\n";
            textSection << "    jg near " << notFoundLabel << "\n";

            // mid = (low + high) / 2
            textSection << "    mov rdx, rax\n";
            textSection << "    add rdx, rcx\n";
            textSection << "    shr rdx, 1\n";

            // Load element at mid: base + (mid-1) * elementSize
            textSection << "    mov rbx, rdx\n";
            textSection << "    dec rbx\n";
            textSection << "    mov rsi, " << elementSize << "\n";
            textSection << "    imul rbx, rsi\n";
            textSection << "    lea rsi, [rel " << tableAsmName << "]\n";
            textSection << "    add rsi, rbx\n";

            // Compare table[mid] with indexName value byte-by-byte
            if (indexName.empty())
            {
                textSection << "    jmp near " << foundLabel << "\n";
            }
            else
            {
            std::string cmpEnd = newLabel("cmp_end");
            for (int i = 0; i < elementSize; i++)
            {
                textSection << "    mov al, [rsi + " << i << "]\n";
                textSection << "    mov bl, [" << getAsmName(indexName) << " + " << i << "]\n";
                textSection << "    cmp al, bl\n";
                textSection << "    jne near " << cmpEnd << "\n";
            }
            textSection << "    jmp near " << foundLabel << "\n";

            textSection << cmpEnd << ":\n";
            // If table[mid] < key: search right half, low = mid + 1
            textSection << "    mov al, [rsi]\n";
            textSection << "    mov bl, [" << getAsmName(indexName) << "]\n";
            textSection << "    cmp al, bl\n";
            std::string goRight = newLabel("go_right");
            textSection << "    jb near " << goRight << "\n";
            // table[mid] >= key: go left (high = mid - 1)
            textSection << "    mov rcx, rdx\n";
            textSection << "    dec rcx\n";
            textSection << "    jmp near " << loopLabel << "\n";

            textSection << goRight << ":\n";
            // low = mid + 1
            textSection << "    mov rax, rdx\n";
            textSection << "    inc rax\n";
            textSection << "    jmp near " << loopLabel << "\n";
            }

            textSection << notFoundLabel << ":\n";
            for (const auto &stmt : search->atEndStatements)
                generateStatement(stmt.get());
            textSection << "    jmp near " << endLabel << "\n";

            textSection << foundLabel << ":\n";
            for (const auto &stmt : search->notAtEndStatements)
                generateStatement(stmt.get());

            textSection << endLabel << ":\n";
        }
        else
        {
            // Sequential search (SEARCH)
            int numElements = (elementSize > 0 && totalSize >= elementSize) ? totalSize / elementSize : 0;
            if (numElements <= 0) numElements = totalSize;

            std::string loopLabel = newLabel("search_loop");
            std::string foundLabel = newLabel("search_found");
            std::string endLabel = newLabel("search_end");
            std::string notFoundLabel = newLabel("search_not_found");

            textSection << "    mov r8, 1\n";  // index = 1

            textSection << loopLabel << ":\n";
            textSection << "    cmp r8, " << numElements << "\n";
            textSection << "    jg near " << notFoundLabel << "\n";

            // Load element address: base + (index - 1) * elementSize
            textSection << "    mov rcx, r8\n";
            textSection << "    dec rcx\n";
            textSection << "    mov rbx, " << elementSize << "\n";
            textSection << "    imul rcx, rbx\n";
            textSection << "    lea rsi, [rel " << tableAsmName << "]\n";
            textSection << "    add rsi, rcx\n";

            if (indexName.empty())
            {
                // No index variable - just set found on first iteration
                textSection << "    jmp near " << foundLabel << "\n";
            }
            else
            {
            // Compare element with indexName value byte-by-byte
            std::string cmpEnd = newLabel("cmp_end");
            for (int i = 0; i < elementSize; i++)
            {
                textSection << "    mov al, [rsi + " << i << "]\n";
                textSection << "    mov bl, [" << getAsmName(indexName) << " + " << i << "]\n";
                textSection << "    cmp al, bl\n";
                textSection << "    jne near " << cmpEnd << "\n";
            }
            textSection << "    jmp near " << foundLabel << "\n";

            textSection << cmpEnd << ":\n";
            textSection << "    inc r8\n";
            }
            textSection << "    jmp near " << loopLabel << "\n";

            textSection << notFoundLabel << ":\n";
            for (const auto &stmt : search->atEndStatements)
                generateStatement(stmt.get());
            textSection << "    jmp near " << endLabel << "\n";

            textSection << foundLabel << ":\n";
            for (const auto &stmt : search->notAtEndStatements)
                generateStatement(stmt.get());

            textSection << endLabel << ":\n";
        }
    }

    void generateSet(const SetIndexNode *set)
    {
        textSection << "    ; SET " << set->indexName;
        if (set->direction == 1)
            textSection << " UP";
        else if (set->direction == -1)
            textSection << " DOWN";
        else
            textSection << " TO";
        textSection << " BY " << set->amount << "\n";

        std::string idxAsm = getAsmName(set->indexName);

        if (set->direction == 0)
        {
            // SET index TO value
            if (set->amountIsLiteral)
            {
                textSection << "    mov byte [" << idxAsm << "], '" << set->amount << "'\n";
            }
            else
            {
                textSection << "    movzx rax, byte [" << getAsmName(set->amount) << "]\n";
                textSection << "    mov byte [" << idxAsm << "], al\n";
            }
        }
        else if (set->amountIsLiteral)
        {
            int byVal = std::stoi(set->amount);
            if (set->direction == 1)
            {
                textSection << "    add byte [" << idxAsm << "], " << byVal << "\n";
            }
            else
            {
                textSection << "    sub byte [" << idxAsm << "], " << byVal << "\n";
            }
        }
        else
        {
            textSection << "    movzx rax, byte [" << getAsmName(set->amount) << "]\n";
            if (set->direction == 1)
                textSection << "    add byte [" << idxAsm << "], al\n";
            else
                textSection << "    sub byte [" << idxAsm << "], al\n";
        }
    }

    void generateAccept(const AcceptNode *acc)
    {
        std::string var = resolveName(acc->dest);
        std::string varLenLabel = var + "_len";
        textSection << "    ; ACCEPT " << acc->dest;

        if (acc->fromType == "TIME")
        {
            textSection << " FROM TIME\n";
            needsAsciiHelpers = true;
            // No longer call the broken TZif parser – use clock_gettime
            // seconds directly (eliminates the ~9-minute drift).
            std::string timeBuf = newLabel("time_buf");
            bssSection << "    " << timeBuf << ": resb 16\n";
            textSection << "    mov rax, 228\n";          // sys_clock_gettime
            textSection << "    mov rdi, 0\n";            // CLOCK_REALTIME
            textSection << "    lea rsi, [rel " << timeBuf << "]\n";
            textSection << "    syscall\n";
            textSection << "    mov rax, [" << timeBuf << "]\n";  // seconds
            textSection << "    mov rcx, 86400\n";
            textSection << "    xor rdx, rdx\n";
            textSection << "    div rcx\n";
            textSection << "    mov rbx, rdx\n";
            textSection << "    mov rax, rbx\n";
            textSection << "    mov rcx, 3600\n";
            textSection << "    xor rdx, rdx\n";
            textSection << "    div rcx\n";
            textSection << "    mov r8, rax\n";
            textSection << "    mov rax, rdx\n";
            textSection << "    mov rcx, 60\n";
            textSection << "    xor rdx, rdx\n";
            textSection << "    div rcx\n";
            textSection << "    mov r9, rax\n";
            textSection << "    mov r10, rdx\n";
            textSection << "    mov rdi, " << var << "\n";
            textSection << "    mov rcx, 6\n";
            textSection << "    mov al, '0'\n";
            textSection << "    rep stosb\n";
            textSection << "    mov rax, r8\n";
            textSection << "    mov rcx, 2\n";
            textSection << "    mov rdi, " << var << "\n";
            textSection << "    call int_to_ascii\n";
            textSection << "    mov rax, r9\n";
            textSection << "    mov rcx, 2\n";
            textSection << "    mov rdi, " << var << " + 2\n";
            textSection << "    call int_to_ascii\n";
            textSection << "    mov rax, r10\n";
            textSection << "    mov rcx, 2\n";
            textSection << "    mov rdi, " << var << " + 4\n";
            textSection << "    call int_to_ascii\n";
        }
        else if (acc->fromType == "DATE")
        {
            textSection << " FROM DATE\n";
            needsAsciiHelpers = true;
            emitDateHelper();
            std::string timeBuf = newLabel("date_buf");
            bssSection << "    " << timeBuf << ": resb 16\n";
            textSection << "    mov rax, 228\n";
            textSection << "    mov rdi, 0\n";
            textSection << "    lea rsi, [rel " << timeBuf << "]\n";
            textSection << "    syscall\n";
            textSection << "    mov rax, [" << timeBuf << "]\n";
            // No tz-offset addition – use kernel seconds directly.
            // format_date_yyyymmdd now writes 8-byte YYYYMMDD.
            textSection << "    mov rdi, " << var << "\n";
            textSection << "    call format_date_yyyymmdd\n";
        }
        else
        {
            textSection << "\n";
            textSection << "    mov rax, 0\n";
            textSection << "    mov rdi, 0\n";
            textSection << "    mov rsi, " << var << "\n";
            textSection << "    mov rdx, " << varLenLabel << "\n";
            textSection << "    syscall\n";

            std::string scanLabel = newLabel("acc_scan");
            std::string foundNL = newLabel("acc_found_nl");
            std::string doneLabel = newLabel("acc_done");

            textSection << "    cmp rax, 0\n";
            textSection << "    jle " << doneLabel << "\n";
            textSection << "    mov rcx, rax\n";
            textSection << scanLabel << ":\n";
            textSection << "    cmp rcx, 0\n";
            textSection << "    je " << foundNL << "\n";
            textSection << "    cmp byte [" << var << " + rcx - 1], 10\n";
            textSection << "    jne " << foundNL << "\n";
            textSection << "    dec rcx\n";
            textSection << "    jmp " << scanLabel << "\n";
            textSection << foundNL << ":\n";
            textSection << "    mov byte [" << var << " + rcx], 0\n";
            textSection << doneLabel << ":\n";
        }
    }

    void generateEvaluate(const EvaluateNode *eval)
    {
        textSection << "    ; EVALUATE\n";

        bool testAfter = (!eval->testType.empty() && eval->testType == "AFTER");

        // Evaluate subject if not TRUE
        std::string subjectLabel;
        int subjectSize = 1;

        if (!eval->subjectIsTrue) {
            if (eval->subjectIsLiteral) {
                subjectLabel = emitPaddedLiteral(eval->subject, (int)eval->subject.length(), "eval_subj");
                subjectSize = (int)eval->subject.length();
            } else {
                subjectLabel = getAsmName(eval->subject);
                // Use elementarySize so subordinate items of a record
                // (and REDEFINES items) get the correct comparison length.
                subjectSize = elementarySize(eval->subject);
            }
        }

        // Build a list of (whenClause, bodyLabel, nextLabel) for sequential checking
        std::vector<std::pair<std::string, std::string>> clauseLabels;
        for (size_t i = 0; i < eval->whenClauses.size(); ++i) {
            clauseLabels.push_back({newLabel("eval_when_body"), newLabel("eval_when_next")});
        }
        std::string endLabel = newLabel("eval_end");

        for (size_t i = 0; i < eval->whenClauses.size(); ++i) {
            const auto &wc = eval->whenClauses[i];
            const std::string &bodyLabel = clauseLabels[i].first;
            const std::string &nextLabel = clauseLabels[i].second;

            if (testAfter) {
                // TEST AFTER is non-standard for EVALUATE; use TEST BEFORE semantics
                // to ensure correct "break" after each WHEN body.
                if (wc->isOther) {
                    textSection << "    jmp " << bodyLabel << "\n";
                } else if (wc->hasCondition && !eval->subjectIsTrue) {
                    for (const auto &cond : wc->conditions) {
                        auto tempCond = std::make_unique<ConditionNode>();
                        tempCond->op = cond->op;
                        tempCond->left = eval->subject;
                        tempCond->leftIsLiteral = eval->subjectIsLiteral;
                        tempCond->right = cond->right;
                        tempCond->rightIsLiteral = cond->rightIsLiteral;
                        tempCond->line = cond->line;
                        generateConditionJumpTrue(tempCond.get(), bodyLabel);
                    }
                    textSection << "    jmp " << nextLabel << "\n";
                } else if (wc->hasCondition && eval->subjectIsTrue) {
                    for (const auto &cond : wc->conditions) {
                        generateConditionJumpTrue(cond.get(), bodyLabel);
                    }
                    textSection << "    jmp " << nextLabel << "\n";
                } else {
                    for (size_t j = 0; j < wc->subjects.size(); ++j) {
                        const std::string &whenVal = wc->subjects[j];
                        bool whenIsLit = wc->subjectIsLiteral[j];

                        if (eval->subjectIsTrue) {
                            textSection << "    jmp " << bodyLabel << "\n";
                            break;
                        }

                        auto tempCond = std::make_unique<ConditionNode>();
                        tempCond->op = ConditionNode::EQ;
                        tempCond->left = eval->subject;
                        tempCond->leftIsLiteral = eval->subjectIsLiteral;
                        tempCond->right = whenVal;
                        tempCond->rightIsLiteral = whenIsLit;
                        tempCond->line = 0;
                        generateConditionJumpTrue(tempCond.get(), bodyLabel);
                    }
                    if (wc->hasThru) {
                        std::string lowerBound = (!wc->subjects.empty()) ? wc->subjects[0] : wc->thruValue;
                        bool lowerIsLiteral = (!wc->subjects.empty()) ? wc->subjectIsLiteral[0] : wc->thruValueIsLiteral;

                        auto geCond = std::make_unique<ConditionNode>();
                        geCond->op = ConditionNode::GE;
                        geCond->left = eval->subject;
                        geCond->leftIsLiteral = eval->subjectIsLiteral;
                        geCond->right = lowerBound;
                        geCond->rightIsLiteral = lowerIsLiteral;
                        geCond->line = 0;
                        generateConditionJumpFalse(geCond.get(), nextLabel);

                        auto leCond = std::make_unique<ConditionNode>();
                        leCond->op = ConditionNode::LE;
                        leCond->left = eval->subject;
                        leCond->leftIsLiteral = eval->subjectIsLiteral;
                        leCond->right = wc->thruValue;
                        leCond->rightIsLiteral = wc->thruValueIsLiteral;
                        leCond->line = 0;
                        generateConditionJumpFalse(leCond.get(), nextLabel);

                        textSection << "    jmp " << bodyLabel << "\n";
                    }
                    textSection << "    jmp " << nextLabel << "\n";
                }

                textSection << bodyLabel << ":\n";
                for (const auto &stmt : wc->body)
                    generateStatement(stmt.get());
                textSection << "    jmp " << endLabel << "\n";
                textSection << nextLabel << ":\n";
            } else {
                // TEST BEFORE (default): test condition first, then execute body
                if (wc->isOther) {
                    textSection << "    jmp " << bodyLabel << "\n";
                } else if (wc->hasCondition && !eval->subjectIsTrue) {
                    for (const auto &cond : wc->conditions) {
                        auto tempCond = std::make_unique<ConditionNode>();
                        tempCond->op = cond->op;
                        tempCond->left = eval->subject;
                        tempCond->leftIsLiteral = eval->subjectIsLiteral;
                        tempCond->right = cond->right;
                        tempCond->rightIsLiteral = cond->rightIsLiteral;
                        tempCond->line = cond->line;
                        generateConditionJumpTrue(tempCond.get(), bodyLabel);
                    }
                    textSection << "    jmp " << nextLabel << "\n";
                } else if (wc->hasCondition && eval->subjectIsTrue) {
                    for (const auto &cond : wc->conditions) {
                        generateConditionJumpTrue(cond.get(), bodyLabel);
                    }
                    textSection << "    jmp " << nextLabel << "\n";
                } else {
                    for (size_t j = 0; j < wc->subjects.size(); ++j) {
                        const std::string &whenVal = wc->subjects[j];
                        bool whenIsLit = wc->subjectIsLiteral[j];

                        if (eval->subjectIsTrue) {
                            textSection << "    jmp " << bodyLabel << "\n";
                            break;
                        }

                        auto tempCond = std::make_unique<ConditionNode>();
                        tempCond->op = ConditionNode::EQ;
                        tempCond->left = eval->subject;
                        tempCond->leftIsLiteral = eval->subjectIsLiteral;
                        tempCond->right = whenVal;
                        tempCond->rightIsLiteral = whenIsLit;
                        tempCond->line = 0;
                        generateConditionJumpTrue(tempCond.get(), bodyLabel);
                    }
                    if (wc->hasThru) {
                        std::string lowerBound = (!wc->subjects.empty()) ? wc->subjects[0] : wc->thruValue;
                        bool lowerIsLiteral = (!wc->subjects.empty()) ? wc->subjectIsLiteral[0] : wc->thruValueIsLiteral;

                        auto geCond = std::make_unique<ConditionNode>();
                        geCond->op = ConditionNode::GE;
                        geCond->left = eval->subject;
                        geCond->leftIsLiteral = eval->subjectIsLiteral;
                        geCond->right = lowerBound;
                        geCond->rightIsLiteral = lowerIsLiteral;
                        geCond->line = 0;
                        generateConditionJumpFalse(geCond.get(), nextLabel);

                        auto leCond = std::make_unique<ConditionNode>();
                        leCond->op = ConditionNode::LE;
                        leCond->left = eval->subject;
                        leCond->leftIsLiteral = eval->subjectIsLiteral;
                        leCond->right = wc->thruValue;
                        leCond->rightIsLiteral = wc->thruValueIsLiteral;
                        leCond->line = 0;
                        generateConditionJumpFalse(leCond.get(), nextLabel);

                        textSection << "    jmp " << bodyLabel << "\n";
                    }
                    textSection << "    jmp " << nextLabel << "\n";
                }

                textSection << bodyLabel << ":\n";
                for (const auto &stmt : wc->body)
                    generateStatement(stmt.get());
                textSection << "    jmp " << endLabel << "\n";
                textSection << nextLabel << ":\n";
            }
        }

        textSection << endLabel << ":\n";
    }

    void generateString(const StringNode *str)
    {
        needsAsciiHelpers = true;
        textSection << "    ; STRING\n";

        std::string destAsm = getAsmName(str->dest);
        int destSize = resolveSize(str->dest);
        std::string pointerAsm;

        if (str->hasPointer) {
            pointerAsm = getAsmName(str->pointerVar);
            textSection << "    movzx eax, byte [" << pointerAsm << "]\n";
            textSection << "    lea rdi, [rel " << destAsm << "]\n";
            textSection << "    add rdi, rax\n";
        } else {
            textSection << "    lea rdi, [rel " << destAsm << "]\n";
        }

        for (const auto &src : str->sources) {
            std::string srcLabel;
            int srcSize = 1;

            if (src.sourceIsLiteral) {
                srcLabel = emitPaddedLiteral(src.source, (int)src.source.length(), "str_src");
                srcSize = (int)src.source.length();
            } else {
                srcLabel = getAsmName(src.source);
                auto sit = fieldSizes.find(src.source);
                if (sit != fieldSizes.end())
                    srcSize = sit->second;
            }

            if (src.delimitedBySize) {
                textSection << "    mov rsi, " << srcLabel << "\n";
                textSection << "    mov rcx, " << srcSize << "\n";
                textSection << "    rep movsb\n";
                if (str->hasPointer) {
                    textSection << "    add byte [" << pointerAsm << "], " << srcSize << "\n";
                }
            } else {
                char delimChar = src.delimiterChar;
                std::string scanLoop = newLabel("str_scan");
                std::string scanFound = newLabel("str_scan_found");
                std::string scanDone = newLabel("str_scan_done");

                textSection << "    mov rsi, " << srcLabel << "\n";
                textSection << "    mov rcx, " << srcSize << "\n";
                textSection << scanLoop << ":\n";
                textSection << "    cmp rcx, 0\n";
                textSection << "    je " << scanDone << "\n";
                textSection << "    mov al, [rsi]\n";
                textSection << "    cmp al, " << (int)(unsigned char)delimChar << "\n";
                textSection << "    je " << scanFound << "\n";
                textSection << "    mov [rdi], al\n";
                textSection << "    inc rdi\n";
                textSection << "    inc rsi\n";
                if (str->hasPointer) {
                    textSection << "    inc byte [" << pointerAsm << "]\n";
                }
                textSection << "    dec rcx\n";
                textSection << "    jmp " << scanLoop << "\n";
                textSection << scanFound << ":\n";
                textSection << scanDone << ":\n";
            }
        }

        // Check overflow: pointer > destSize
        if (str->hasPointer) {
            std::string overflowLabel = newLabel("str_overflow");
            std::string noOverflowLabel = newLabel("str_no_overflow");
            textSection << "    movzx eax, byte [" << pointerAsm << "]\n";
            textSection << "    cmp eax, " << destSize << "\n";
            textSection << "    ja " << overflowLabel << "\n";
            textSection << "    jmp " << noOverflowLabel << "\n";

            textSection << overflowLabel << ":\n";
            for (const auto &stmt : str->overflowBody)
                generateStatement(stmt.get());

            textSection << noOverflowLabel << ":\n";
        }

        // Execute ON OVERFLOW / NOT ON OVERFLOW bodies
        if (str->hasOverflow || str->hasNotOverflow) {
            std::string overflowLabel = newLabel("str_overflow");
            std::string noOverflowLabel = newLabel("str_no_overflow");

            if (!str->hasPointer) {
                // Compute pointer as (rdi - dest)
                std::string ptrCalc = newLabel("str_ptr_calc");
                textSection << "    lea rax, [" << destAsm << "]\n";
                textSection << "    sub rdi, rax\n";
                textSection << "    mov eax, edi\n";
                textSection << "    cmp eax, " << destSize << "\n";
                textSection << "    ja " << overflowLabel << "\n";
                textSection << "    jmp " << noOverflowLabel << "\n";
            }

            textSection << overflowLabel << ":\n";
            for (const auto &stmt : str->overflowBody)
                generateStatement(stmt.get());

            textSection << noOverflowLabel << ":\n";
            for (const auto &stmt : str->notOverflowBody)
                generateStatement(stmt.get());
        }
    }

    void generateUnstring(const UnstringNode *uns)
    {
        needsAsciiHelpers = true;
        textSection << "    ; UNSTRING\n";

        std::string srcAsm = getAsmName(uns->source);
        std::string pointerAsm;
        int pointerSize = 1;

        std::string srcPtr = newLabel("uns_src_ptr");
        bssSection << "    " << srcPtr << ": resb 1\n";

        if (uns->hasPointer) {
            pointerAsm = getAsmName(uns->pointerVar);
            auto pit = fieldSizes.find(uns->pointerVar);
            if (pit != fieldSizes.end())
                pointerSize = pit->second;
            textSection << "    movzx eax, byte [" << pointerAsm << "]\n";
            textSection << "    mov byte [" << srcPtr << "], al\n";
        } else {
            textSection << "    mov byte [" << srcPtr << "], 0\n";
        }

        if (uns->hasTally) {
            textSection << "    mov byte [" << getAsmName(uns->tallyVar) << "], 0\n";
        }

        char delimByte = 0;
        bool hasDelim = !uns->delimiter.empty();
        if (hasDelim) {
            delimByte = uns->delimiter[0];
        }

        for (size_t i = 0; i < uns->intoClauses.size(); ++i) {
            const auto &ic = uns->intoClauses[i];
            std::string destBase = ic.dest;
            std::string destAsm = getAsmName(destBase);
            int destSize = resolveSize(destBase);
            bool destHasSub = variableHasSubscript(destBase);
            int elemSize = destHasSub ? resolveElementSize(baseVariableName(destBase)) : destSize;

            std::string copyLoop = newLabel("uns_copy_" + std::to_string(i));
            std::string delimFound = newLabel("uns_delim_" + std::to_string(i));
            std::string destFull = newLabel("uns_dfull_" + std::to_string(i));
            std::string destSetup = newLabel("uns_dest_" + std::to_string(i));

            textSection << "    ; UNSTRING field " << (i + 1) << " -> " << ic.dest << "\n";

            // Set up source pointer
            textSection << "    movzx eax, byte [" << srcPtr << "]\n";
            textSection << "    lea rbx, [rel " << srcAsm << "]\n";
            textSection << "    add rbx, rax\n";
            textSection << "    mov rsi, rbx\n";

            // Set up dest pointer (handle subscripts)
            textSection << destSetup << ":\n";
            if (destHasSub) {
                std::string base = baseVariableName(destBase);
                std::string subExpr = subscriptExpression(destBase);
                textSection << "    lea rdi, [rel " << getAsmName(base) << "]\n";
                if (isNumericLiteral(subExpr)) {
                    int subVal = std::stoi(subExpr);
                    textSection << "    mov rcx, " << (subVal - 1) << "\n";
                } else {
                    textSection << "    lea rbx, [rel " << getAsmName(subExpr) << "]\n";
                    textSection << "    mov rcx, " << resolveSize(subExpr) << "\n";
                    textSection << "    call ascii_to_int\n";
                    textSection << "    dec rax\n";
                    textSection << "    mov rcx, rax\n";
                }
                if (elemSize > 1) {
                    textSection << "    mov rbx, " << elemSize << "\n";
                    textSection << "    imul rcx, rbx\n";
                }
                textSection << "    add rdi, rcx\n";
            } else {
                textSection << "    lea rdi, [rel " << destAsm << "]\n";
            }

            textSection << "    mov rcx, " << elemSize << "\n";
            textSection << copyLoop << ":\n";
            textSection << "    cmp rcx, 0\n";
            textSection << "    je " << destFull << "\n";
            textSection << "    mov al, [rsi]\n";
            textSection << "    cmp al, 0\n";
            textSection << "    je " << delimFound << "\n";

            if (hasDelim) {
                textSection << "    cmp al, " << (int)(unsigned char)delimByte << "\n";
                textSection << "    je " << delimFound << "\n";
            }

            textSection << "    mov [rdi], al\n";
            textSection << "    inc rdi\n";
            textSection << "    inc rsi\n";
            textSection << "    inc byte [" << srcPtr << "]\n";
            textSection << "    dec rcx\n";
            textSection << "    jmp " << copyLoop << "\n";

            textSection << destFull << ":\n";
            textSection << "    ; skip past delimiter\n";
            textSection << "    inc rsi\n";
            textSection << "    inc byte [" << srcPtr << "]\n";
            if (uns->hasTally) {
                textSection << "    inc byte [" << getAsmName(uns->tallyVar) << "]\n";
            }
            textSection << delimFound << ":\n";
            if (hasDelim) {
                std::string skipPastDelim = newLabel("uns_skipdl_");
                std::string nextLabel = newLabel("uns_next_");
                textSection << skipPastDelim << ":\n";
                textSection << "    mov al, [rsi]\n";
                textSection << "    cmp al, 0\n";
                textSection << "    je " << nextLabel << "\n";
                textSection << "    cmp al, " << (int)(unsigned char)delimByte << "\n";
                textSection << "    jne " << nextLabel << "\n";
                textSection << "    inc rsi\n";
                textSection << "    inc byte [" << srcPtr << "]\n";
                textSection << "    jmp " << skipPastDelim << "\n";
                textSection << nextLabel << ":\n";
            }
        }

        std::string unsDoneLabel = newLabel("uns_done");
        textSection << unsDoneLabel << ":\n";
    }

    bool variableHasSubscript(const std::string &name) const
    {
        return name.find('(') != std::string::npos;
    }

    std::string baseVariableName(const std::string &name) const
    {
        size_t pos = name.find('(');
        if (pos != std::string::npos)
            return name.substr(0, pos);
        return name;
    }

    std::string subscriptExpression(const std::string &name) const
    {
        size_t open = name.find('(');
        size_t close = name.find(')', open);
        if (open != std::string::npos && close != std::string::npos)
            return name.substr(open + 1, close - open - 1);
        return "";
    }

    int resolveElementSize(const std::string &tableName) const
    {
        auto it = fieldSizes.find(tableName);
        if (it != fieldSizes.end())
        {
            auto oit = tableOccurs.find(tableName);
            if (oit != tableOccurs.end() && oit->second > 1)
                return it->second / oit->second;
            return it->second;
        }
        return 1;
    }

};

// ============================================================
// 7. MAIN DRIVER
// ============================================================

// int main(int argc, char *argv[])
// {
//     if(argc <2) return 0;
//     std::string source;

//         std::ifstream file(argv[1]);
//         if (!file)
//         {
//             std::cerr << "Cannot open: " << argv[1] << "\n";
//             return 1;
//         }
//         source = std::string((std::istreambuf_iterator<char>(file)),
//                              std::istreambuf_iterator<char>());

//     try
//     {
//         Lexer lexer(source);
//         auto tokens = lexer.tokenize();

//         Parser parser(tokens);
//         auto ast = parser.parse();

//         SemanticAnalyzer sem(ast.get());
//         sem.analyze();

//         CodeGenerator gen;
//         gen.generateProgram(ast.get());

//         std::string asmOut = gen.getOutput();
//         std::cout << asmOut << "\n";

//         std::ofstream out("output.asm");
//         out << asmOut;
//         std::cout << "\nAssembly written to output.asm\n";
//         std::cout << "Assemble with: nasm -f elf64 output.asm -o output.o && ld output.o -o output\n";
//     }
//     catch (const std::exception &e)
//     {
//         std::cerr << "Error: " << e.what() << "\n";
//         return 1;
//     }

//     return 0;
// }

// Assuming Lexer, Parser, SemanticAnalyzer, and CodeGenerator are defined elsewhere.

#endif // CODE_GENERATOR_H
