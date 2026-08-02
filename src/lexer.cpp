#include "lexer.h"
#include <cctype>
#include <algorithm>
#include <map>

Lexer::Lexer(std::string src) : source(std::move(src)) {}

char Lexer::peek() const 
{ 
    return pos < source.size() ? source[pos] : '\0'; 
}

char Lexer::advance()
{
    char c = source[pos++];
    if (c == '\n') { line++; col = 1; }
    else col++;
    return c;
}

void Lexer::skipWhitespace()
{
    while (peek() == ' ' || peek() == '\t')
        advance();
}

void Lexer::skipComment()
{
    size_t p = pos;
    while (p > 0 && (source[p - 1] == ' ' || source[p - 1] == '\t')) --p;
    if (p == 0 || source[p - 1] == '\n' || source[p - 1] == '\r') {
        if (peek() == '*') {
            while (peek() != '\n' && peek() != '\0') advance();
        }
    }
}

TokenType Lexer::checkKeyword(const std::string &word)
{
    static const std::map<std::string, TokenType> keywords = {
        {"IDENTIFICATION", TokenType::IDENTIFICATION}, {"ENVIRONMENT", TokenType::ENVIRONMENT},
        {"DATA", TokenType::DATA}, {"PROCEDURE", TokenType::PROCEDURE},
        {"DIVISION", TokenType::DIVISION}, {"SECTION", TokenType::SECTION},
        {"PROGRAM-ID", TokenType::PROGRAM_ID}, {"AUTHOR", TokenType::AUTHOR},
        {"DATE-WRITTEN", TokenType::DATE_WRITTEN}, {"PIC", TokenType::PIC},
        {"IS", TokenType::IS}, {"VALUE", TokenType::VALUE}, {"COMP", TokenType::COMP},
        {"DISPLAY", TokenType::DISPLAY}, {"PRINT", TokenType::PRINT}, {"STOP", TokenType::STOP}, {"RUN", TokenType::RUN},
        {"PRINT", TokenType::PRINT},
        {"MOVE", TokenType::MOVE}, {"TO", TokenType::TO}, {"ADD", TokenType::ADD},
        {"GIVING", TokenType::GIVING}, {"SUBTRACT", TokenType::SUBTRACT},
        {"MULTIPLY", TokenType::MULTIPLY}, {"DIVIDE", TokenType::DIVIDE},
        {"COMPUTE", TokenType::COMPUTE}, {"IF", TokenType::IF}, {"ELSE", TokenType::ELSE},
        {"END-IF", TokenType::END_IF}, {"THEN", TokenType::THEN}, {"PERFORM", TokenType::PERFORM},
        {"END-PERFORM", TokenType::END_PERFORM}, {"UNTIL", TokenType::UNTIL}, {"TIMES", TokenType::TIMES},
        {"VARYING", TokenType::VARYING}, {"FROM", TokenType::FROM}, {"BY", TokenType::BY},
        {"INTO", TokenType::INTO}, {"GO", TokenType::GO}, {"NOT", TokenType::NOT},
        {"AND", TokenType::AND}, {"OR", TokenType::OR}, {"EQUAL", TokenType::EQUAL},
        {"GREATER", TokenType::GREATER}, {"LESS", TokenType::LESS}, {"THAN", TokenType::THAN},
        {"THRU", TokenType::THRU}, {"INPUT-OUTPUT", TokenType::INPUT_OUTPUT},
        {"FILE-CONTROL", TokenType::FILE_CONTROL}, {"SELECT", TokenType::SELECT},
        {"ASSIGN", TokenType::ASSIGN}, {"FD", TokenType::FD}, {"SD", TokenType::SD},
        {"RECORD", TokenType::RECORD}, {"INDEXED", TokenType::INDEXED}, {"DYNAMIC", TokenType::DYNAMIC},
        {"LABEL", TokenType::LABEL}, {"STANDARD", TokenType::STANDARD}, {"OMITTED", TokenType::OMITTED},
        {"WORKING-STORAGE", TokenType::WORKING_STORAGE}, {"LOCAL-STORAGE", TokenType::LOCAL_STORAGE},
        {"LINKAGE", TokenType::LINKAGE}, {"FILE", TokenType::FILE_SECTION}, {"INSPECT", TokenType::INSPECT},
        {"REPLACING", TokenType::REPLACING}, {"ALL", TokenType::ALL}, {"ZEROS", TokenType::ZEROS},
        {"ZERO", TokenType::ZEROS}, {"ZEROES", TokenType::ZEROS}, {"SPACES", TokenType::SPACES},
        {"SPACE", TokenType::SPACES}, {"OPEN", TokenType::OPEN}, {"CLOSE", TokenType::CLOSE},
        {"READ", TokenType::READ}, {"WRITE", TokenType::WRITE}, {"INPUT", TokenType::INPUT},
        {"OUTPUT", TokenType::OUTPUT}, {"EXTEND", TokenType::EXTEND}, {"I-O", TokenType::I_O},
        {"AT", TokenType::AT}, {"END", TokenType::END}, {"ORGANIZATION", TokenType::ORGANIZATION},
        {"LINE", TokenType::LINE}, {"SEQUENTIAL", TokenType::SEQUENTIAL}, {"ACCESS", TokenType::ACCESS},
        {"MODE", TokenType::MODE}, {"RECORDS", TokenType::RECORDS}, {"STATUS", TokenType::STATUS},
         {"NEXT", TokenType::NEXT}, {"END-READ", TokenType::END_READ}, {"END-WRITE", TokenType::END_WRITE},
         {"START", TokenType::START},
        {"KEY", TokenType::KEY}, {"INVALID", TokenType::INVALID}, {"END-START", TokenType::END_START},
        {"RELATIVE", TokenType::RELATIVE}, {"RANDOM", TokenType::RANDOM}, {"KEYED", TokenType::KEYED},
        {"ALTERNATE", TokenType::ALTERNATE}, {"WITH", TokenType::WITH}, {"DUPLICATES", TokenType::DUPLICATES},
        {"MANUAL", TokenType::MANUAL}, {"AUTOMATIC", TokenType::AUTOMATIC}, {"LOCK", TokenType::LOCK},
        {"OCCURS", TokenType::OCCURS}, {"TABLE", TokenType::TABLE}, {"SEARCH", TokenType::SEARCH},
        {"SET", TokenType::SET}, {"ASCENDING", TokenType::ASCENDING}, {"DESCENDING", TokenType::DESCENDING},
        {"END-SEARCH", TokenType::END_SEARCH}, {"UP", TokenType::UP}, {"DOWN", TokenType::DOWN},
        {"REMAINDER", TokenType::REMAINDER}, {"REM", TokenType::REMAINDER},
        {"ACCEPT", TokenType::ACCEPT},
        {"JUST", TokenType::JUST}, {"LEFT", TokenType::LEFT}, {"RIGHT", TokenType::RIGHT},
        {"REDEFINES", TokenType::REDEFINES},
        {"EVALUATE", TokenType::EVALUATE}, {"END-EVALUATE", TokenType::END_EVALUATE},
        {"WHEN", TokenType::WHEN}, {"OTHER", TokenType::OTHER}, {"TRUE", TokenType::TRUE},
        {"STRING", TokenType::STRING_OP}, {"END-STRING", TokenType::END_STRING_OP},
        {"UNSTRING", TokenType::UNSTRING}, {"END-UNSTRING", TokenType::END_UNSTRING},
        {"POINTER", TokenType::POINTER}, {"DELIMITED", TokenType::DELIMITED},
         {"SIZE", TokenType::SIZE}, {"OVERFLOW", TokenType::OVERFLOW},
         {"TALLYING", TokenType::TALLYING}, {"IN", TokenType::IN}, {"ON", TokenType::ON},
    };
    auto it = keywords.find(word);
    return (it != keywords.end()) ? it->second : TokenType::IDENTIFIER;
}

std::vector<Token> Lexer::tokenize()
{
    while (peek() != '\0') {
        skipWhitespace();
        skipComment();
        if (peek() == '\0') break;
        skipWhitespace();

        int startLine = line, startCol = col;

        // Recognize \n, \r, or \r\n as newline tokens
        if (peek() == '\n' || peek() == '\r') {
            if (peek() == '\r' && pos + 1 < source.size() && source[pos + 1] == '\n') {
                // \r\n (Windows CRLF): consume both, emit one NEWLINE
                advance(); // consume \r (advances col)
                advance(); // consume \n (increments line, resets col)
                tokens.push_back({TokenType::NEWLINE, "\\r\\n", startLine, startCol});
                continue;
            } else if (peek() == '\r') {
                // standalone \r (old Mac): consume and emit NEWLINE
                advance(); // consume \r (advances col)
                line++; col = 1;  // advance() does not increment line for \r, do it manually
                tokens.push_back({TokenType::NEWLINE, "\\r", startLine, startCol});
                continue;
            } else {
                // standalone \n (Unix)
                advance(); // consumes \n, increments line, resets col
                tokens.push_back({TokenType::NEWLINE, "\\n", startLine, startCol});
                continue;
            }
        }
        if (peek() == '.') { advance(); tokens.push_back({TokenType::DOT, ".", startLine, startCol}); continue; }
        if (peek() == ',') { advance(); tokens.push_back({TokenType::COMMA, ",", startLine, startCol}); continue; }
        if (peek() == '(') { advance(); tokens.push_back({TokenType::LPAREN, "(", startLine, startCol}); continue; }
        if (peek() == ')') { advance(); tokens.push_back({TokenType::RPAREN, ")", startLine, startCol}); continue; }
        if (peek() == '+') { advance(); tokens.push_back({TokenType::PLUS, "+", startLine, startCol}); continue; }
        if (peek() == '-') { advance(); tokens.push_back({TokenType::MINUS, "-", startLine, startCol}); continue; }
        if (peek() == '*') { advance(); tokens.push_back({TokenType::MUL, "*", startLine, startCol}); continue; }
        if (peek() == '/') { advance(); tokens.push_back({TokenType::DIV_OP, "/", startLine, startCol}); continue; }
        if (peek() == '=') { advance(); tokens.push_back({TokenType::EQUALS, "=", startLine, startCol}); continue; }
        if (peek() == '>') { advance(); tokens.push_back({TokenType::GREATER_THAN, ">", startLine, startCol}); continue; }
        if (peek() == '<') { advance(); tokens.push_back({TokenType::LESS_THAN, "<", startLine, startCol}); continue; }
        if (peek() == '$') { advance(); tokens.push_back({TokenType::DOLLAR, "$", startLine, startCol}); continue; }

        if (peek() == '"' || peek() == '\'') {
            char quote = peek(); advance();
            std::string str;
            while (peek() != quote && peek() != '\0') {
                if (peek() == '\\') {
                    advance(); // consume backslash
                    char next = peek();
                    switch (next) {
                        case 'n':  advance(); str += '\n'; break;
                        case 'r':  advance(); str += '\r'; break;
                        case 't':  advance(); str += '\t'; break;
                        case '0':  advance(); str += '\0'; break;
                        case 'a':  advance(); str += '\a'; break;
                        case 'b':  advance(); str += '\b'; break;
                        case 'f':  advance(); str += '\f'; break;
                        case 'v':  advance(); str += '\v'; break;
                        case '\\': advance(); str += '\\'; break;
                        case '"':  advance(); str += '"'; break;
                        case '\'': advance(); str += '\''; break;
                        case 'x': {
                            advance(); // consume 'x'
                            std::string hex;
                            while (std::isxdigit(peek()) && hex.size() < 2) {
                                hex += advance();
                            }
                            if (hex.empty()) str += 'x';
                            else str += static_cast<char>(std::stoi(hex, nullptr, 16));
                            break;
                        }
                        default: str += next; advance(); break;
                    }
                } else {
                    str += advance();
                }
            }
            if (peek() == quote) advance();
            tokens.push_back({TokenType::STRING, str, startLine, startCol});
            continue;
        }

        // Hex literal: X"..." or X'...' (COBOL hexadecimal literal)
        if ((peek() == 'X' || peek() == 'x') && pos + 1 < source.size() && (source[pos + 1] == '"' || source[pos + 1] == '\'')) {
            char hexQuote = source[pos + 1];
            advance(); // consume X/x
            advance(); // consume opening quote
            std::string hex;
            while (peek() != hexQuote && peek() != '\0') hex += advance();
            if (peek() == hexQuote) advance(); // consume closing quote
            if (hex.size() % 2 != 0) hex = "0" + hex; // pad odd-length
            bool hexValid = true;
            for (char c : hex) {
                if (!std::isxdigit(static_cast<unsigned char>(c))) { hexValid = false; break; }
            }
            if (!hexValid) {
                tokens.push_back({TokenType::INVALID, hex, startLine, startCol});
                continue;
            }
            std::string decoded;
            for (size_t i = 0; i + 1 < hex.size(); i += 2) {
                std::string byte = hex.substr(i, 2);
                decoded += static_cast<char>(std::stoi(byte, nullptr, 16));
            }
            tokens.push_back({TokenType::STRING, decoded, startLine, startCol});
            continue;
        }

        // Octal literal: O"..." or O'...' (octal integer literal)
        if ((peek() == 'O' || peek() == 'o') && pos + 1 < source.size() && (source[pos + 1] == '"' || source[pos + 1] == '\'')) {
            char octQuote = source[pos + 1];
            advance(); // consume O/o
            advance(); // consume opening quote
            std::string octal;
            while (peek() != octQuote && peek() != '\0') octal += advance();
            if (peek() == octQuote) advance(); // consume closing quote
            bool octValid = true;
            for (char c : octal) {
                if (c < '0' || c > '7') { octValid = false; break; }
            }
            if (!octValid) {
                tokens.push_back({TokenType::INVALID, octal, startLine, startCol});
                continue;
            }
            unsigned long ival = octal.empty() ? 0 : std::stoul(octal, nullptr, 8);
            std::string decoded;
            decoded += static_cast<char>(ival & 0xFF);
            tokens.push_back({TokenType::STRING, decoded, startLine, startCol});
            continue;
        }

        // Binary literal: B"..." or B'...' (binary integer literal)
        if ((peek() == 'B' || peek() == 'b') && pos + 1 < source.size() && (source[pos + 1] == '"' || source[pos + 1] == '\'')) {
            char binQuote = source[pos + 1];
            advance(); // consume B/b
            advance(); // consume opening quote
            std::string binary;
            while (peek() != binQuote && peek() != '\0') binary += advance();
            if (peek() == binQuote) advance(); // consume closing quote
            bool binValid = true;
            for (char c : binary) {
                if (c != '0' && c != '1') { binValid = false; break; }
            }
            if (!binValid) {
                tokens.push_back({TokenType::INVALID, binary, startLine, startCol});
                continue;
            }
            unsigned long bval = binary.empty() ? 0 : std::stoul(binary, nullptr, 2);
            std::string decoded;
            decoded += static_cast<char>(bval & 0xFF);
            tokens.push_back({TokenType::STRING, decoded, startLine, startCol});
            continue;
        }

        if (std::isdigit(peek())) {
            std::string num;
            while (std::isdigit(peek())) num += advance();
            if (peek() == '.' && pos + 1 < source.size() && std::isdigit(source[pos + 1])) {
                num += advance(); // consume '.'
                while (std::isdigit(peek())) num += advance();
            }
            tokens.push_back({TokenType::NUMBER, num, startLine, startCol});
            continue;
        }

        if (std::isalpha(peek()) || peek() == '-' || peek() == '_') {
            std::string word;
            while (std::isalnum(peek()) || peek() == '-' || peek() == '_') word += advance();
            for (char &c : word) if (c == '_') c = '-';
            std::string upper = word;
            std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
            TokenType t = checkKeyword(upper);
            tokens.push_back({t, word, startLine, startCol});
            continue;
        }

        std::string bad(1, advance());
        tokens.push_back({TokenType::INVALID, bad, startLine, startCol});
    }
    tokens.push_back({TokenType::EOF_TOKEN, "", line, col});
    return tokens;
}
