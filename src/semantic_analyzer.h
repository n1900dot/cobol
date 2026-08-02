#ifndef SEMANTIC_ANALYZER_H
#define SEMANTIC_ANALYZER_H

#include "ast.h"
#include <memory>
#include <map>
#include <string>
#include <set>

struct DataInfo
{
    std::string name;
    std::string picClause;
    PicDescriptor picDesc;
    int level;
    std::string section;
    std::string value;
    std::string redefines;
};

class SemanticAnalyzer
{
private:
    std::map<std::string, DataInfo> dataDict;
    std::set<std::string> fileNames;
    std::set<std::string> paragraphNames;

public:
    void analyze(ProgramNode *prog);
    void analyzeDataDivision(ProgramNode *prog);
    void analyzeStatements(ProgramNode *prog);
    
    bool isDeclared(const std::string &name) const;
    DataInfo getDataInfo(const std::string &name) const;
};

#endif
