#include "semantic_analyzer.h"
#include <algorithm>
#include <stdexcept>

namespace {

std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}

void collectParagraphNames(ProgramNode *prog, std::set<std::string> &names) {
    for (const auto &stmt : prog->statements) {
        if (auto *para = dynamic_cast<ParagraphNode *>(stmt.get())) {
            names.insert(toUpper(para->name));
        }
    }
}

void validateParagraphTarget(const std::string &target,
                             const std::set<std::string> &paragraphNames,
                             const char *kind) {
    std::string upper = toUpper(target);
    if (paragraphNames.find(upper) == paragraphNames.end()) {
        throw std::runtime_error(std::string("Semantic error: ") + kind +
                                 " target paragraph '" + target +
                                 "' is not defined");
    }
}

void checkStatements(const std::vector<std::unique_ptr<ASTNode>> &stmts,
                     const SemanticAnalyzer *self,
                     ProgramNode *prog,
                     const std::set<std::string> &paragraphNames);

void checkOneStatement(const ASTNode *stmt,
                       const SemanticAnalyzer *self,
                       ProgramNode *prog,
                       const std::set<std::string> &paragraphNames) {
    if (auto *move = dynamic_cast<const MoveNode *>(stmt)) {
        if (!move->sourceIsLiteral) {
            if (!self->isDeclared(move->source)) {
                bool isFile = false;
                for (const auto &fd : prog->fileDescriptions) {
                    if (auto *f = dynamic_cast<FileDescriptionNode *>(fd.get())) {
                        if (toUpper(f->name) == toUpper(move->source)) {
                            isFile = true;
                            break;
                        }
                    }
                }
                if (!isFile) {
                    for (const auto &fc : prog->fileControls) {
                        if (auto *f = dynamic_cast<FileControlNode *>(fc.get())) {
                            if (toUpper(f->selectName) == toUpper(move->source)) {
                                isFile = true;
                                break;
                            }
                        }
                    }
                }
                if (isFile) {
                    throw std::runtime_error(
                        "Semantic error: cannot MOVE a file name '" +
                        move->source + "'");
                }
                throw std::runtime_error(
                    "Semantic error: undefined variable '" + move->source + "'");
            }
        }
        if (!self->isDeclared(move->dest)) {
            bool isFile = false;
            for (const auto &fd : prog->fileDescriptions) {
                if (auto *f = dynamic_cast<FileDescriptionNode *>(fd.get())) {
                    if (toUpper(f->name) == toUpper(move->dest)) {
                        isFile = true;
                        break;
                    }
                }
            }
            if (!isFile) {
                for (const auto &fc : prog->fileControls) {
                    if (auto *f = dynamic_cast<FileControlNode *>(fc.get())) {
                        if (toUpper(f->selectName) == toUpper(move->dest)) {
                            isFile = true;
                            break;
                        }
                    }
                }
            }
            if (isFile) {
                throw std::runtime_error(
                    "Semantic error: cannot MOVE to a file name '" +
                    move->dest + "'");
            }
            throw std::runtime_error(
                "Semantic error: undefined variable '" + move->dest + "'");
        }
    } else if (auto *start = dynamic_cast<const StartNode *>(stmt)) {
        if (!start->keyVar.empty() && !self->isDeclared(start->keyVar)) {
            throw std::runtime_error(
                "Undefined variable in START KEY: " + start->keyVar);
        }
        checkStatements(start->invalidKeyStatements, self, prog, paragraphNames);
    } else if (auto *perf = dynamic_cast<const PerformParagraphNode *>(stmt)) {
        validateParagraphTarget(perf->target, paragraphNames, "PERFORM");
    } else if (auto *go = dynamic_cast<const GoToNode *>(stmt)) {
        validateParagraphTarget(go->target, paragraphNames, "GO TO");
    } else if (auto *ifNode = dynamic_cast<const IfNode *>(stmt)) {
        checkStatements(ifNode->thenStatements, self, prog, paragraphNames);
        checkStatements(ifNode->elseStatements, self, prog, paragraphNames);
    } else if (auto *pu = dynamic_cast<const PerformUntilNode *>(stmt)) {
        checkStatements(pu->body, self, prog, paragraphNames);
    } else if (auto *pv = dynamic_cast<const PerformVaryingNode *>(stmt)) {
        checkStatements(pv->body, self, prog, paragraphNames);
    } else if (auto *pt = dynamic_cast<const PerformTimesNode *>(stmt)) {
        checkStatements(pt->body, self, prog, paragraphNames);
    } else if (auto *read = dynamic_cast<const ReadNode *>(stmt)) {
        checkStatements(read->atEndStatements, self, prog, paragraphNames);
        checkStatements(read->notAtEndStatements, self, prog, paragraphNames);
    } else if (auto *evaluate = dynamic_cast<const EvaluateNode *>(stmt)) {
        if (!evaluate->subjectIsTrue && !evaluate->subject.empty() && !evaluate->subjectIsLiteral) {
            if (!self->isDeclared(evaluate->subject)) {
                throw std::runtime_error(
                    "Semantic error: undefined variable '" + evaluate->subject + "' in EVALUATE");
            }
        }
        for (const auto &wc : evaluate->whenClauses) {
            for (size_t i = 0; i < wc->subjects.size(); ++i) {
                if (!wc->subjectIsLiteral[i] && !self->isDeclared(wc->subjects[i])) {
                    throw std::runtime_error(
                        "Semantic error: undefined variable '" + wc->subjects[i] + "' in EVALUATE WHEN");
                }
            }
            for (const auto &cond : wc->conditions) {
                if (!cond->leftIsLiteral && !self->isDeclared(cond->left)) {
                    throw std::runtime_error(
                        "Semantic error: undefined variable '" + cond->left + "' in EVALUATE condition");
                }
                if (!cond->rightIsLiteral && !self->isDeclared(cond->right)) {
                    throw std::runtime_error(
                        "Semantic error: undefined variable '" + cond->right + "' in EVALUATE condition");
                }
            }
            checkStatements(wc->body, self, prog, paragraphNames);
        }
    } else if (auto *str = dynamic_cast<const StringNode *>(stmt)) {
        for (const auto &src : str->sources) {
            if (!src.sourceIsLiteral && !self->isDeclared(src.source)) {
                throw std::runtime_error(
                    "Semantic error: undefined variable '" + src.source + "' in STRING");
            }
        }
        if (!self->isDeclared(str->dest)) {
            throw std::runtime_error(
                "Semantic error: undefined variable '" + str->dest + "' in STRING INTO");
        }
        if (str->hasPointer && !self->isDeclared(str->pointerVar)) {
            throw std::runtime_error(
                "Semantic error: undefined variable '" + str->pointerVar + "' in STRING POINTER");
        }
        checkStatements(str->overflowBody, self, prog, paragraphNames);
        checkStatements(str->notOverflowBody, self, prog, paragraphNames);
    } else if (auto *uns = dynamic_cast<const UnstringNode *>(stmt)) {
        if (!self->isDeclared(uns->source)) {
            throw std::runtime_error(
                "Semantic error: undefined variable '" + uns->source + "' in UNSTRING");
        }
        if (!uns->delimiter.empty() && !uns->delimiterIsLiteral && !self->isDeclared(uns->delimiter)) {
            throw std::runtime_error(
                "Semantic error: undefined variable '" + uns->delimiter + "' in UNSTRING DELIMITED BY");
        }
        for (const auto &ic : uns->intoClauses) {
            if (!self->isDeclared(ic.dest)) {
                throw std::runtime_error(
                    "Semantic error: undefined variable '" + ic.dest + "' in UNSTRING INTO");
            }
        }
        if (uns->hasPointer && !self->isDeclared(uns->pointerVar)) {
            throw std::runtime_error(
                "Semantic error: undefined variable '" + uns->pointerVar + "' in UNSTRING POINTER");
        }
        if (uns->hasTally && !self->isDeclared(uns->tallyVar)) {
            throw std::runtime_error(
                "Semantic error: undefined variable '" + uns->tallyVar + "' in UNSTRING TALLYING");
        }
    }
}

void checkStatements(const std::vector<std::unique_ptr<ASTNode>> &stmts,
                     const SemanticAnalyzer *self,
                     ProgramNode *prog,
                     const std::set<std::string> &paragraphNames) {
    for (const auto &stmt : stmts) {
        checkOneStatement(stmt.get(), self, prog, paragraphNames);
    }
}

} // namespace

void SemanticAnalyzer::analyze(ProgramNode *prog) {
    if (!prog)
        throw std::runtime_error("Cannot analyze null program");

    // Validate that the program has some meaningful content.
    // If all of these are empty, the source likely has a structural issue
    // (e.g., a misspelled IDENTIFICATION DIVISION header causing everything to be skipped).
    bool hasContent = !prog->programName.empty()
                   || !prog->fileControls.empty()
                   || !prog->fileDescriptions.empty()
                   || !prog->dataItems.empty()
                   || !prog->statements.empty();
    if (!hasContent) {
        throw std::runtime_error(
            "Semantic error: program has no content — "
            "the source may have a structural issue (e.g., misspelled division headers "
            "or unsupported compiler directives at the start of the file)");
    }

    analyzeDataDivision(prog);
    collectParagraphNames(prog, paragraphNames);
    analyzeStatements(prog);
}

void SemanticAnalyzer::analyzeDataDivision(ProgramNode *prog) {
    for (auto &item : prog->dataItems) {
        if (auto dataItem = dynamic_cast<DataItemNode *>(item.get())) {
            std::string nameUpper = toUpper(dataItem->name);
            if (!dataItem->redefines.empty()) {
                std::string nameUpper = toUpper(dataItem->name);
                std::string redefinesUpper = toUpper(dataItem->redefines);
                if (nameUpper == redefinesUpper) {
                    throw std::runtime_error(
                        "Semantic error: item '" + dataItem->name +
                        "' cannot REDEFINES itself");
                }
                if (dataDict.find(redefinesUpper) == dataDict.end()) {
                    throw std::runtime_error(
                        "Semantic error: REDEFINES target '" + dataItem->redefines +
                        "' is not defined");
                }
            }
            dataDict[nameUpper] = {
                dataItem->name,
                dataItem->pic,
                dataItem->picDesc,
                dataItem->level,
                "WORKING-STORAGE",
                dataItem->value,
                dataItem->redefines
            };
            if (!dataItem->indexedBy.empty())
            {
                PicDescriptor idxPic;
                idxPic.isNumeric = true;
                idxPic.displaySize = 1;
                idxPic.storageSize = 1;
                dataDict[dataItem->indexedBy] = {
                    dataItem->indexedBy,
                    "9",
                    idxPic,
                    77,
                    "WORKING-STORAGE",
                    "",
                    ""
                };
            }
        }
    }

    for (auto &fd : prog->fileDescriptions) {
        if (auto fileDesc = dynamic_cast<FileDescriptionNode *>(fd.get())) {
            for (auto &rec : fileDesc->records) {
                if (auto dataItem = dynamic_cast<DataItemNode *>(rec.get())) {
                    std::string nameUpper = toUpper(dataItem->name);
                    dataDict[nameUpper] = {
                        dataItem->name,
                        dataItem->pic,
                        dataItem->picDesc,
                        dataItem->level,
                        "FILE",
                        dataItem->value,
                        dataItem->redefines
                    };
                }
            }
        }
    }
}

void SemanticAnalyzer::analyzeStatements(ProgramNode *prog) {
    checkStatements(prog->statements, this, prog, paragraphNames);
}

bool SemanticAnalyzer::isDeclared(const std::string &name) const {
    std::string baseName = name;
    // Strip subscript if present: WS-KEY(1) -> WS-KEY
    size_t parenPos = baseName.find('(');
    if (parenPos != std::string::npos)
        baseName = baseName.substr(0, parenPos);
    return dataDict.find(toUpper(baseName)) != dataDict.end();
}

DataInfo SemanticAnalyzer::getDataInfo(const std::string &name) const {
    auto it = dataDict.find(toUpper(name));
    if (it == dataDict.end())
        throw std::runtime_error("Undefined variable: " + name);
    return it->second;
}
