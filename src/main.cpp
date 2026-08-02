#include <cctype>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "lexer.h"
#include "parser.h"
#include "semantic_analyzer.h"
#include "code_generator.h"

int main(int argc, char *argv[])
{
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <cobol-file> [output-file]\n";
        return 1;
    }

    std::string inputFile = argv[1];
    std::string outputFile = (argc > 2) ? argv[2] : "output.asm";

    // Read source file line by line
    std::ifstream file(inputFile);
    if (!file) {
        std::cerr << "Error: Could not open file " << inputFile << "\n";
        return 1;
    }

    std::string source;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        source += line + "\n";
    }
    file.close();

    try {
        // Tokenize
        Lexer lexer(source);
        std::vector<Token> tokens = lexer.tokenize();

        // Parse
        Parser parser(tokens);
        auto program = parser.parse();

        // Semantic analysis
        SemanticAnalyzer analyzer;
        analyzer.analyze(program.get());

        // Generate assembly
        CodeGenerator gen;
        gen.generateProgram(program.get());
        std::string asmOut = gen.getOutput();

        // Write output
        std::ofstream out(outputFile);
        if (!out) {
            std::cerr << "Error: Could not write to " << outputFile << "\n";
            return 1;
        }
        out << asmOut;
        out.close();

        // std::cout << "Compilation successful! \n";
        return 0;

    } catch (const std::exception &e) {
        std::cerr << "Compilation error: " << e.what() << "\n";
        return 1;
    }
}
