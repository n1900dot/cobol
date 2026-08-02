#include "parser.h"
#include <algorithm>
#include <stdexcept>

bool Parser::consume(TokenType t) {
  {
    if (check(t)) {
      advance();
      return true;
    }
    return false;
  }
}

bool Parser::expect(TokenType t, const std::string &msg) {
  {
    if (!check(t)) {
      throw std::runtime_error("Parse error at line " +
                               std::to_string(peek().line) + ": expected " +
                               msg + ", got '" + peek().lexeme + "'");
    }
    advance();
    return true;
  }
}

void Parser::skipNewlines() {
  {
    while (check(TokenType::NEWLINE))
      advance();
  }
}

std::string Parser::parsePictureClause() {
  {
    std::string pic;
    while (true) {
      if (check(TokenType::EOF_TOKEN) || check(TokenType::NEWLINE))
        break;
      if (check(TokenType::VALUE) || check(TokenType::COMP))
        break;
      if (check(TokenType::OCCURS))
        break;
      if (check(TokenType::JUST))
        break;
      if (check(TokenType::DOT)) {
        if (pos + 1 < tokens.size() &&
            tokens[pos + 1].type == TokenType::NUMBER) {
          pic += advance().lexeme;
          continue;
        }
        break;
      }
      pic += advance().lexeme;
    }
    return pic;
  }
}

std::unique_ptr<ConditionNode> Parser::parseCondition() {
  {
    auto cond = std::make_unique<ConditionNode>();
    cond->line = peek().line;
    skipNewlines();

    if (check(TokenType::NUMBER) || check(TokenType::STRING)) {
      cond->left = advance().lexeme;
      cond->leftIsLiteral = true;
    } else if (check(TokenType::MINUS)) {
      advance();
      if (!check(TokenType::NUMBER)) {
        throw std::runtime_error(
            "Expected number after '-' in condition at line " +
            std::to_string(peek().line));
      }
      cond->left = "-" + advance().lexeme;
      cond->leftIsLiteral = true;
    } else if (check(TokenType::IDENTIFIER)) {
      cond->left = readVariable();
    } else {
      throw std::runtime_error(
          "Unexpected token '" + peek().lexeme + "' in condition at line " +
          std::to_string(peek().line) +
          ": expected identifier, number, or string literal");
    }

    skipNewlines();

    bool hasNot = false;
    if (check(TokenType::IS)) {
      advance(); // consume IS
      if (check(TokenType::NOT))
        advance(); // consume NOT
      hasNot = true;
    } else if (check(TokenType::NOT)) {
      advance(); // consume NOT
      hasNot = true;
    }

    skipNewlines();

    if (check(TokenType::EQUAL)) {
      advance();
      consume(TokenType::TO);
      cond->op = hasNot ? ConditionNode::NE : ConditionNode::EQ;
    } else if (check(TokenType::GREATER)) {
      advance();
      if (consume(TokenType::THAN)) {
        if (consume(TokenType::OR)) {
          expect(TokenType::EQUAL, "EQUAL");
          expect(TokenType::TO, "TO");
          cond->op = hasNot ? ConditionNode::LE : ConditionNode::GE;
        } else {
          cond->op = hasNot ? ConditionNode::LE : ConditionNode::GT;
        }
      } else {
        cond->op = hasNot ? ConditionNode::LE : ConditionNode::GT;
      }
    } else if (check(TokenType::LESS)) {
      advance();
      if (consume(TokenType::THAN)) {
        if (consume(TokenType::OR)) {
          expect(TokenType::EQUAL, "EQUAL");
          expect(TokenType::TO, "TO");
          cond->op = hasNot ? ConditionNode::GE : ConditionNode::LE;
        } else {
          cond->op = hasNot ? ConditionNode::GE : ConditionNode::LT;
        }
      } else {
        cond->op = hasNot ? ConditionNode::GE : ConditionNode::LT;
      }
    } else if (check(TokenType::GREATER_THAN)) {
      advance();
      if (check(TokenType::EQUALS)) {
        advance(); // consume =
        cond->op = hasNot ? ConditionNode::LT : ConditionNode::GE;
      } else {
        cond->op = hasNot ? ConditionNode::LE : ConditionNode::GT;
      }
    } else if (check(TokenType::LESS_THAN)) {
      advance();
      if (check(TokenType::EQUALS)) {
        advance(); // consume =
        cond->op = hasNot ? ConditionNode::GT : ConditionNode::LE;
      } else {
        cond->op = hasNot ? ConditionNode::GE : ConditionNode::LT;
      }
    } else if (check(TokenType::EQUALS)) {
      advance();
      cond->op = hasNot ? ConditionNode::NE : ConditionNode::EQ;
    } else {
      throw std::runtime_error(
          "Expected comparison operator in condition at line " +
          std::to_string(peek().line));
    }

    skipNewlines();

    if (check(TokenType::NUMBER) || check(TokenType::STRING)) {
      cond->right = advance().lexeme;
      cond->rightIsLiteral = true;
    } else if (check(TokenType::MINUS)) {
      advance();
      if (!check(TokenType::NUMBER)) {
        throw std::runtime_error(
            "Expected number after '-' in condition at line " +
            std::to_string(peek().line));
      }
      cond->right = "-" + advance().lexeme;
      cond->rightIsLiteral = true;
    } else if (check(TokenType::IDENTIFIER)) {
      cond->right = readVariable();
    } else {
      throw std::runtime_error(
          "Unexpected token '" + peek().lexeme + "' in condition at line " +
          std::to_string(peek().line) +
          ": expected identifier, number, or string literal");
    }

    return cond;
  }
}

std::vector<std::unique_ptr<ASTNode>>
Parser::parseBlock(const std::vector<TokenType> &terminators) {
  {
    std::vector<std::unique_ptr<ASTNode>> stmts;
    skipNewlines();
    while (!check(TokenType::EOF_TOKEN)) {
      bool foundTerm = false;
      for (auto t : terminators) {
        if (check(t)) {
          foundTerm = true;
          break;
        }
      }
      if (foundTerm)
        break;
      if (check(TokenType::NEWLINE)) {
        advance();
        continue;
      }

      auto stmt = parseStatement();
      if (stmt) {
        stmts.push_back(std::move(stmt));
      }
      skipNewlines();
      if (check(TokenType::DOT))
        advance();
    }
    return stmts;
  }
}

std::unique_ptr<ExprNode> Parser::parseExpression() {
  {
    auto left = parseTerm();
    while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
      auto op = advance().type;
      auto right = parseTerm();
      auto bin = std::make_unique<BinaryExpr>();
      bin->op = (op == TokenType::PLUS) ? BinaryExpr::ADD : BinaryExpr::SUB;
      bin->left = std::move(left);
      bin->right = std::move(right);
      left = std::move(bin);
    }
    return left;
  }
}

std::unique_ptr<ExprNode> Parser::parseTerm() {
  {
    auto left = parseFactor();
    while (check(TokenType::MUL) || check(TokenType::DIV_OP)) {
      auto op = advance().type;
      auto right = parseFactor();
      auto bin = std::make_unique<BinaryExpr>();
      bin->op = (op == TokenType::MUL) ? BinaryExpr::MUL : BinaryExpr::DIV;
      bin->left = std::move(left);
      bin->right = std::move(right);
      left = std::move(bin);
    }
    return left;
  }
}

std::unique_ptr<ExprNode> Parser::parseFactor() {
  {
    if (check(TokenType::NUMBER)) {
      auto lit = std::make_unique<LiteralExpr>();
      lit->value = advance().lexeme;
      return lit;
    } else if (check(TokenType::MINUS)) {
      advance();
      if (!check(TokenType::NUMBER)) {
        throw std::runtime_error(
            "Expected number after '-' in expression at line " +
            std::to_string(peek().line));
      }
      auto lit = std::make_unique<LiteralExpr>();
      lit->value = "-" + advance().lexeme;
      return lit;
    } else if (check(TokenType::IDENTIFIER)) {
      auto var = std::make_unique<VariableExpr>();
      var->name = advance().lexeme;
      std::transform(var->name.begin(), var->name.end(), var->name.begin(),
                     ::toupper);
      // Handle subscripted variable access: WS-ITEM(1) or WS-ITEM(IDX)
      if (check(TokenType::LPAREN)) {
        advance();
        var->name += "(";
        while (!check(TokenType::RPAREN) && !check(TokenType::EOF_TOKEN)) {
          var->name += advance().lexeme;
        }
        if (check(TokenType::RPAREN)) {
          var->name += advance().lexeme;
        }
      }
      return var;
    } else if (check(TokenType::LPAREN)) {
      advance();
      auto expr = parseExpression();
      expect(TokenType::RPAREN, ")");
      return expr;
    }
    throw std::runtime_error(
        "Expected number, variable, or '(' in expression at line " +
        std::to_string(peek().line));
  }
}

std::unique_ptr<ProgramNode> Parser::parse() {
  {
    auto program = std::make_unique<ProgramNode>();
    skipNewlines();

    // Detect invalid/unrecognized start of program.
    // If the first meaningful token is not a known division header and
    // not EOF, report an error with line number and offending token.
    if (!check(TokenType::IDENTIFICATION) &&
        !check(TokenType::ENVIRONMENT) &&
        !check(TokenType::DATA) &&
        !check(TokenType::PROCEDURE) &&
        !check(TokenType::EOF_TOKEN)) {
      const Token &bad = peek();
      throw std::runtime_error(
          "Syntax error at line " + std::to_string(bad.line) +
          ", column " + std::to_string(bad.column) +
          ": unrecognized start token '" + bad.lexeme +
          "' — expected IDENTIFICATION, ENVIRONMENT, DATA, or PROCEDURE");
    }

    if (check(TokenType::IDENTIFICATION)) {
      advance();
      expect(TokenType::DIVISION, "DIVISION");
      expect(TokenType::DOT, ".");
      skipNewlines();

      if (check(TokenType::PROGRAM_ID)) {
        advance();
        expect(TokenType::DOT, ".");
        skipNewlines();
        program->programName = advance().lexeme;
        std::transform(program->programName.begin(), program->programName.end(),
                       program->programName.begin(), ::toupper);
        expect(TokenType::DOT, ".");
        skipNewlines();
      }
      while (!check(TokenType::ENVIRONMENT) && !check(TokenType::DATA) &&
             !check(TokenType::PROCEDURE) && !check(TokenType::EOF_TOKEN))
        advance();
    }

    if (check(TokenType::ENVIRONMENT)) {
      advance();
      expect(TokenType::DIVISION, "DIVISION");
      expect(TokenType::DOT, ".");
      skipNewlines();

      // INPUT-OUTPUT SECTION
      if (check(TokenType::INPUT_OUTPUT)) {
        advance();
        expect(TokenType::SECTION, "SECTION");
        expect(TokenType::DOT, ".");
        skipNewlines();

        if (check(TokenType::FILE_CONTROL)) {
          advance();
          expect(TokenType::DOT, ".");
          skipNewlines();

          while (!check(TokenType::DATA) && !check(TokenType::PROCEDURE) &&
                 !check(TokenType::EOF_TOKEN)) {
            if (check(TokenType::SELECT)) {
              program->fileControls.push_back(parseFileControlEntry());
            } else if (check(TokenType::IDENTIFIER)) {
              // Allow SELECT-less file control entries
              auto node = std::make_unique<FileControlNode>();
              node->selectName = advance().lexeme;
              std::transform(node->selectName.begin(), node->selectName.end(),
                             node->selectName.begin(), ::toupper);
              skipNewlines();
              expect(TokenType::ASSIGN, "ASSIGN");
              expect(TokenType::TO, "TO");
              skipNewlines();
              if (check(TokenType::STRING)) {
                node->assignName = advance().lexeme;
              } else {
                node->assignName = advance().lexeme;
              }
              skipNewlines();
              while (!check(TokenType::DOT) && !check(TokenType::EOF_TOKEN) &&
                     !check(TokenType::NEWLINE)) {
                advance();
              }
              expect(TokenType::DOT, ".");
              program->fileControls.push_back(std::move(node));
            } else {
              break;
            }
            skipNewlines();
          }
        }
      }

      while (!check(TokenType::DATA) && !check(TokenType::PROCEDURE) &&
             !check(TokenType::EOF_TOKEN))
        advance();
    }

    if (check(TokenType::DATA)) {
      advance();
      expect(TokenType::DIVISION, "DIVISION");
      expect(TokenType::DOT, ".");
      skipNewlines();

      // FILE SECTION
      if (check(TokenType::FILE_SECTION)) {
        advance();
        expect(TokenType::SECTION, "SECTION");
        expect(TokenType::DOT, ".");
        skipNewlines();
        while (!check(TokenType::WORKING_STORAGE) &&
               !check(TokenType::PROCEDURE) && !check(TokenType::EOF_TOKEN) &&
               check(TokenType::FD)) {
          program->fileDescriptions.push_back(parseFileDescription());
          skipNewlines();
        }
      }

      if (check(TokenType::WORKING_STORAGE)) {
        advance();
        expect(TokenType::SECTION, "SECTION");
        expect(TokenType::DOT, ".");
        skipNewlines();
        while (!check(TokenType::PROCEDURE) && !check(TokenType::EOF_TOKEN) &&
               (check(TokenType::NUMBER) || check(TokenType::IDENTIFIER))) {
          program->dataItems.push_back(parseDataItem());
          skipNewlines();
        }
      }
    }

    if (check(TokenType::PROCEDURE)) {
      advance();
      expect(TokenType::DIVISION, "DIVISION");
      expect(TokenType::DOT, ".");
      skipNewlines();
      while (!check(TokenType::EOF_TOKEN)) {
        if (check(TokenType::NEWLINE)) {
          advance();
          continue;
        }

        // Paragraph header: <name> .
        // Name may be an IDENTIFIER or a keyword (e.g. START.) used as a label.
        if (!check(TokenType::DOT) && !check(TokenType::EOF_TOKEN) &&
            !check(TokenType::NEWLINE)) {
          size_t save = pos;
          std::string name = peek().lexeme;
          advance();
          skipNewlines();
          if (check(TokenType::DOT)) {
            auto para = std::make_unique<ParagraphNode>();
            para->name = name;
            std::transform(para->name.begin(), para->name.end(),
                           para->name.begin(), ::toupper);
            program->statements.push_back(std::move(para));
            advance(); // consume DOT
            skipNewlines();
            continue;
          }
          pos = save;
        }

        auto stmt = parseStatement();
        if (stmt) {
          program->statements.push_back(std::move(stmt));
          if (check(TokenType::DOT))
            advance();
        }
        skipNewlines();
      }
    }

    return program;
  }
}

std::string Parser::readVariable() {
  {
    std::string name = advance().lexeme;
    std::transform(name.begin(), name.end(), name.begin(), ::toupper);
    if (check(TokenType::LPAREN)) {
      name += "(";
      advance();
      // Read the subscript expression until ')'
      while (!check(TokenType::RPAREN) && !check(TokenType::EOF_TOKEN)) {
        name += advance().lexeme;
      }
      if (check(TokenType::RPAREN)) {
        name += ")";
        advance();
      }
    }
    return name;
  }
}

std::unique_ptr<FileControlNode> Parser::parseFileControlEntry() {
  {
    auto node = std::make_unique<FileControlNode>();
    expect(TokenType::SELECT, "SELECT");
    skipNewlines();
    node->selectName = advance().lexeme;
    std::transform(node->selectName.begin(), node->selectName.end(),
                   node->selectName.begin(), ::toupper);
    skipNewlines();
    expect(TokenType::ASSIGN, "ASSIGN");
    expect(TokenType::TO, "TO");
    skipNewlines();
    if (check(TokenType::STRING)) {
      node->assignName = advance().lexeme;
      node->assignIsVariable = false;
    } else {
      node->assignName = advance().lexeme;
      node->assignIsVariable = true;
    }
    skipNewlines();

    while (!check(TokenType::DOT) && !check(TokenType::EOF_TOKEN)) {
      if (consume(TokenType::ORGANIZATION)) {
        skipNewlines();
        expect(TokenType::IS, "IS");
        skipNewlines();
        if (consume(TokenType::INDEXED)) {
          node->organization = FileControlNode::Organization::INDEXED;
        } else if (consume(TokenType::RELATIVE)) {
          node->organization = FileControlNode::Organization::RELATIVE;
        } else if (consume(TokenType::LINE)) {
          skipNewlines();
          if (consume(TokenType::SEQUENTIAL)) {
            node->organization = FileControlNode::Organization::LINE_SEQUENTIAL;
            node->accessMode = FileControlNode::AccessMode::LINE_SEQUENTIAL;
          }
        } else if (consume(TokenType::SEQUENTIAL)) {
          node->organization = FileControlNode::Organization::SEQUENTIAL;
        }
      } else if (consume(TokenType::ACCESS)) {
        skipNewlines();
        expect(TokenType::MODE, "MODE");
        skipNewlines();
        expect(TokenType::IS, "IS");
        skipNewlines();
        if (consume(TokenType::SEQUENTIAL)) {
          node->accessMode = FileControlNode::AccessMode::SEQUENTIAL;
        } else if (consume(TokenType::RANDOM)) {
          node->accessMode = FileControlNode::AccessMode::RANDOM;
        } else if (consume(TokenType::DYNAMIC)) {
          node->accessMode = FileControlNode::AccessMode::DYNAMIC;
        } else if (consume(TokenType::KEYED)) {
          node->accessMode = FileControlNode::AccessMode::KEYED;
        } else if (consume(TokenType::LINE)) {
          skipNewlines();
          if (consume(TokenType::SEQUENTIAL)) {
            node->accessMode = FileControlNode::AccessMode::LINE_SEQUENTIAL;
          }
        }
      } else if (consume(TokenType::RECORD)) {
        skipNewlines();
        expect(TokenType::KEY, "KEY");
        skipNewlines();
        expect(TokenType::IS, "IS");
        skipNewlines();
        node->recordKeyName = advance().lexeme;
        std::transform(node->recordKeyName.begin(), node->recordKeyName.end(),
                       node->recordKeyName.begin(), ::toupper);
        skipNewlines();
      } else if (consume(TokenType::RELATIVE)) {
        skipNewlines();
        expect(TokenType::KEY, "KEY");
        skipNewlines();
        expect(TokenType::IS, "IS");
        skipNewlines();
        node->relativeKeyName = advance().lexeme;
        std::transform(node->relativeKeyName.begin(), node->relativeKeyName.end(),
                       node->relativeKeyName.begin(), ::toupper);
        skipNewlines();
      } else if (consume(TokenType::ALTERNATE)) {
        skipNewlines();
        if (consume(TokenType::RECORD)) skipNewlines();
        expect(TokenType::KEY, "KEY");
        skipNewlines();
        expect(TokenType::IS, "IS");
        skipNewlines();
        node->alternateRecordKey = advance().lexeme;
        std::transform(node->alternateRecordKey.begin(), node->alternateRecordKey.end(),
                       node->alternateRecordKey.begin(), ::toupper);
        skipNewlines();
        if (consume(TokenType::WITH)) {
          skipNewlines();
          consume(TokenType::DUPLICATES);
          node->alternateWithDuplicates = true;
          skipNewlines();
        }
      } else if (consume(TokenType::FILE_SECTION)) {
        // FILE STATUS clause: FILE STATUS IS variable
        if (check(TokenType::STATUS)) {
          consume(TokenType::STATUS);
          skipNewlines();
          expect(TokenType::IS, "IS");
          skipNewlines();
          node->fileStatusVar = advance().lexeme;
          std::transform(node->fileStatusVar.begin(), node->fileStatusVar.end(),
                         node->fileStatusVar.begin(), ::toupper);
          skipNewlines();
        }
      } else if (consume(TokenType::LOCK)) {
        // LOCK MODE clause: LOCK MODE IS MANUAL|AUTOMATIC
        skipNewlines();
        expect(TokenType::MODE, "MODE");
        skipNewlines();
        expect(TokenType::IS, "IS");
        skipNewlines();
        if (consume(TokenType::MANUAL)) {
          node->lockMode = "MANUAL";
        } else if (consume(TokenType::AUTOMATIC)) {
          node->lockMode = "AUTOMATIC";
        }
        skipNewlines();
      } else {
        advance();
      }
    }

    expect(TokenType::DOT, ".");
    return node;
  }
}

std::unique_ptr<FileDescriptionNode> Parser::parseFileDescription() {
  {
    auto node = std::make_unique<FileDescriptionNode>();
    expect(TokenType::FD, "FD");
    skipNewlines();
    node->name = advance().lexeme;
    std::transform(node->name.begin(), node->name.end(), node->name.begin(),
                   ::toupper);
    skipNewlines();

    // Skip optional clauses (LABEL RECORD, etc.)
    while (!check(TokenType::DOT) && !check(TokenType::EOF_TOKEN) &&
           !check(TokenType::NEWLINE)) {
      advance();
    }
    expect(TokenType::DOT, ".");
    skipNewlines();

    while (!check(TokenType::FD) && !check(TokenType::WORKING_STORAGE) &&
           !check(TokenType::PROCEDURE) && !check(TokenType::EOF_TOKEN) &&
           (check(TokenType::NUMBER) || check(TokenType::IDENTIFIER))) {
      node->records.push_back(parseDataItem());
      skipNewlines();
    }
    return node;
  }
}

std::unique_ptr<DataItemNode> Parser::parseDataItem() {
  {
    auto item = std::make_unique<DataItemNode>();
    if (!check(TokenType::NUMBER)) {
      throw std::runtime_error("Expected level number at line " +
                               std::to_string(peek().line));
    }
    item->level = std::stoi(advance().lexeme);
    item->name = advance().lexeme;
    std::transform(item->name.begin(), item->name.end(), item->name.begin(),
                   ::toupper);
    skipNewlines();

    if (check(TokenType::REDEFINES)) {
      advance();
      skipNewlines();
      if (!check(TokenType::IDENTIFIER)) {
        throw std::runtime_error(
            "Expected identifier after REDEFINES at line " +
            std::to_string(peek().line));
      }
      item->redefines = advance().lexeme;
      std::transform(item->redefines.begin(), item->redefines.end(),
                     item->redefines.begin(), ::toupper);
      skipNewlines();
    }

    if (check(TokenType::PIC)) {
      advance();
      consume(TokenType::IS);
      item->pic = parsePictureClause();
      item->picDesc = analyzePicture(item->pic);
      if (consume(TokenType::JUST)) {
        item->picDesc.isJustified = true;
        if (consume(TokenType::LEFT)) {
          item->picDesc.justifyLeft = true;
        } else if (consume(TokenType::RIGHT)) {
          item->picDesc.justifyLeft = false;
        }
      }
      skipNewlines();
    }

    if (check(TokenType::VALUE)) {
      advance();
      if (check(TokenType::NUMBER) || check(TokenType::STRING) ||
          check(TokenType::IDENTIFIER)) {
        item->value = advance().lexeme;
      } else if (check(TokenType::MINUS)) {
        advance();
        if (!check(TokenType::NUMBER)) {
          throw std::runtime_error(
              "Expected number after '-' in VALUE clause at line " +
              std::to_string(peek().line));
        }
        item->value = "-" + advance().lexeme;
      } else if (check(TokenType::ZEROS)) {
        advance();
        item->value = "0";
      } else if (check(TokenType::SPACES)) {
        advance();
        item->value = " ";
      } else {
        throw std::runtime_error("Expected literal after VALUE at line " +
                                 std::to_string(peek().line));
      }
      skipNewlines();
    }

    if (check(TokenType::OCCURS)) {
      advance();
      skipNewlines();
      if (!check(TokenType::NUMBER)) {
        throw std::runtime_error(
            "Expected number after OCCURS at line " +
            std::to_string(peek().line));
      }
      item->occursCount = std::stoi(advance().lexeme);
      skipNewlines();
      if (check(TokenType::TIMES))
        advance();
      skipNewlines();
    }

    // PIC clause may appear after OCCURS in some COBOL styles
    if (!item->pic.empty() || check(TokenType::PIC)) {
      if (check(TokenType::PIC)) {
        advance();
        consume(TokenType::IS);
        item->pic = parsePictureClause();
        item->picDesc = analyzePicture(item->pic);
        if (consume(TokenType::JUST)) {
          item->picDesc.isJustified = true;
          if (consume(TokenType::LEFT)) {
            item->picDesc.justifyLeft = true;
          } else if (consume(TokenType::RIGHT)) {
            item->picDesc.justifyLeft = false;
          }
        }
        skipNewlines();
      }
    }

    // Parse INDEXED BY clause
    if (check(TokenType::INDEXED)) {
      advance();
      skipNewlines();
      expect(TokenType::BY, "BY");
      skipNewlines();
      if (!check(TokenType::IDENTIFIER)) {
        throw std::runtime_error(
            "Expected identifier after INDEXED BY at line " +
            std::to_string(peek().line));
      }
      item->indexedBy = advance().lexeme;
      std::transform(item->indexedBy.begin(), item->indexedBy.end(),
                     item->indexedBy.begin(), ::toupper);
      skipNewlines();
    }

    expect(TokenType::DOT, ".");
    return item;
  }
}

std::unique_ptr<ASTNode> Parser::parseStatement() {
  {
    // Handle subscripted variable reference: IDENTIFIER '(' ...
    // Also handle SEARCH, SEARCH ALL, and SET statements
    if (check(TokenType::SEARCH)) {
      return parseSearch();
    }
    if (check(TokenType::SET)) {
      return parseSet();
    }
    if (check(TokenType::MOVE))
      return parseMove();
    if (check(TokenType::ADD))
      return parseAdd();
    if (check(TokenType::MULTIPLY))
      return parseMultiply();
    if (check(TokenType::SUBTRACT))
      return parseSubtract();
    if (check(TokenType::DIVIDE))
      return parseDivide();
    if (check(TokenType::COMPUTE))
      return parseCompute();
    if (check(TokenType::DISPLAY))
      return parseDisplay();
    if (check(TokenType::PRINT))
      return parsePrint();
    if (check(TokenType::STOP))
      return parseStop();
    if (check(TokenType::IF))
      return parseIf();
    if (check(TokenType::PERFORM))
      return parsePerform();
    if (check(TokenType::GO))
      return parseGoTo();
    if (check(TokenType::INSPECT))
      return parseInspect();
    if (check(TokenType::OPEN))
      return parseOpen();
    if (check(TokenType::CLOSE))
      return parseClose();
    if (check(TokenType::START))
      return parseStart();
    if (check(TokenType::READ))
      return parseRead();
    if (check(TokenType::WRITE))
      return parseWrite();
    if (check(TokenType::ACCEPT))
      return parseAccept();
    if (check(TokenType::EVALUATE))
      return parseEvaluate();
    if (check(TokenType::STRING_OP))
      return parseString();
    if (check(TokenType::UNSTRING))
      return parseUnstring();

    throw std::runtime_error("Unexpected token '" + peek().lexeme +
                             "' at line " + std::to_string(peek().line));
  }
}

std::unique_ptr<ASTNode> Parser::parseSearch() {
  {
    advance(); // SEARCH or SEARCH ALL
    bool all = false;
    if (check(TokenType::ALL)) {
      advance();
      all = true;
    }

    if (!check(TokenType::IDENTIFIER)) {
      throw std::runtime_error(
          "Expected table name after SEARCH at line " +
          std::to_string(peek().line));
    }
    std::string tableName = advance().lexeme;
    std::transform(tableName.begin(), tableName.end(), tableName.begin(), ::toupper);
    skipNewlines();

    auto node = std::make_unique<SearchNode>();
    node->tableName = tableName;
    node->isSearchAll = all;

    // Parse AT END / NOT AT END clauses
    while (check(TokenType::AT) || check(TokenType::NOT)) {
      if (check(TokenType::AT)) {
        advance();
        expect(TokenType::END, "END");
        skipNewlines();
        node->hasAtEnd = true;
        node->atEndStatements =
            parseBlock({TokenType::NOT, TokenType::END_SEARCH, TokenType::END_IF,
                        TokenType::END_PERFORM, TokenType::DOT});
      } else if (check(TokenType::NOT)) {
        advance();
        expect(TokenType::AT, "AT");
        expect(TokenType::END, "END");
        skipNewlines();
        node->hasNotAtEnd = true;
        node->notAtEndStatements =
            parseBlock({TokenType::END_SEARCH, TokenType::END_IF,
                        TokenType::END_PERFORM, TokenType::DOT});
      }
      skipNewlines();
    }

    if (check(TokenType::END_SEARCH))
      advance();

    return node;
  }
}

std::unique_ptr<ASTNode> Parser::parseSet() {
  {
    advance(); // SET
    skipNewlines();

    if (!check(TokenType::IDENTIFIER)) {
      throw std::runtime_error(
          "Expected index name after SET at line " +
          std::to_string(peek().line));
    }
    std::string indexName = advance().lexeme;
    std::transform(indexName.begin(), indexName.end(), indexName.begin(), ::toupper);
    skipNewlines();

    // SET index UP BY ...
    if (check(TokenType::UP)) {
      advance();
      expect(TokenType::BY, "BY");
      skipNewlines();

      auto node = std::make_unique<SetIndexNode>();
      node->indexName = indexName;
      node->direction = 1;

      if (check(TokenType::NUMBER)) {
        node->amount = advance().lexeme;
        node->amountIsLiteral = true;
      } else if (check(TokenType::IDENTIFIER)) {
        node->amount = advance().lexeme;
        node->amountIsLiteral = false;
      } else {
        node->amount = "1";
        node->amountIsLiteral = true;
      }
      skipNewlines();
      expect(TokenType::DOT, ".");
      return node;
    }

    // SET index DOWN BY ...
    if (check(TokenType::DOWN)) {
      advance();
      expect(TokenType::BY, "BY");
      skipNewlines();

      auto node = std::make_unique<SetIndexNode>();
      node->indexName = indexName;
      node->direction = -1;

      if (check(TokenType::NUMBER)) {
        node->amount = advance().lexeme;
        node->amountIsLiteral = true;
      } else if (check(TokenType::IDENTIFIER)) {
        node->amount = advance().lexeme;
        node->amountIsLiteral = false;
      } else {
        node->amount = "1";
        node->amountIsLiteral = true;
      }
      skipNewlines();
      expect(TokenType::DOT, ".");
      return node;
    }

    // SET index TO ...
    auto node = std::make_unique<SetIndexNode>();
    node->indexName = indexName;
    node->direction = 0;

    expect(TokenType::TO, "TO");
    skipNewlines();

    if (check(TokenType::NUMBER)) {
      node->amount = advance().lexeme;
      node->amountIsLiteral = true;
    } else if (check(TokenType::IDENTIFIER)) {
      node->amount = advance().lexeme;
      node->amountIsLiteral = false;
    } else {
      throw std::runtime_error(
          "Expected number or variable after SET index TO at line " +
          std::to_string(peek().line));
    }
    skipNewlines();
    expect(TokenType::DOT, ".");
    return node;
  }
}

std::unique_ptr<ASTNode> Parser::parseOpen() {
  {
    auto node = std::make_unique<OpenNode>();
    advance(); // OPEN
    skipNewlines();

    if (check(TokenType::INPUT)) {
      node->mode = "INPUT";
      advance();
    } else if (check(TokenType::OUTPUT)) {
      node->mode = "OUTPUT";
      advance();
    } else if (check(TokenType::I_O)) {
      node->mode = "I-O";
      advance();
    } else if (check(TokenType::EXTEND)) {
      node->mode = "EXTEND";
      advance();
    }

    skipNewlines();
    node->fileName = advance().lexeme;
    std::transform(node->fileName.begin(), node->fileName.end(),
                   node->fileName.begin(), ::toupper);
    return node;
  }
}

std::unique_ptr<ASTNode> Parser::parseClose() {
  {
    auto node = std::make_unique<CloseNode>();
    advance(); // CLOSE
    skipNewlines();
    node->fileName = advance().lexeme;
    std::transform(node->fileName.begin(), node->fileName.end(),
                   node->fileName.begin(), ::toupper);
    return node;
  }
}

std::unique_ptr<ASTNode> Parser::parseRead() {
  {
    auto node = std::make_unique<ReadNode>();
    advance(); // READ
    skipNewlines();
    // Accept either "READ <file> [NEXT RECORD]" or "READ NEXT RECORD <file>"
    if (check(TokenType::NEXT)) {
      // form: READ NEXT RECORD <file>
      advance();
      skipNewlines();
      if (check(TokenType::RECORD))
        advance();
      skipNewlines();
      node->nextRecord = true;
      node->fileName = advance().lexeme;
      std::transform(node->fileName.begin(), node->fileName.end(),
                     node->fileName.begin(), ::toupper);
      skipNewlines();
    } else {
      // form: READ <file> [NEXT RECORD]
      node->fileName = advance().lexeme;
      std::transform(node->fileName.begin(), node->fileName.end(),
                     node->fileName.begin(), ::toupper);
      skipNewlines();

      // Optional NEXT RECORD after file name
      if (consume(TokenType::NEXT)) {
        skipNewlines();
        consume(TokenType::RECORD);
        node->nextRecord = true;
        skipNewlines();
      }
    }

    if (consume(TokenType::INTO)) {
      skipNewlines();
      node->intoVar = advance().lexeme;
      std::transform(node->intoVar.begin(), node->intoVar.end(),
                     node->intoVar.begin(), ::toupper);
      skipNewlines();
    }

    // Allow NEXT RECORD to appear after INTO as well: READ <file> INTO <var>
    // NEXT RECORD
    if (!node->nextRecord && consume(TokenType::NEXT)) {
      skipNewlines();
      consume(TokenType::RECORD);
      node->nextRecord = true;
      skipNewlines();
    }

    // Optional KEY IS clause for indexed/relative file reads
    if (consume(TokenType::KEY)) {
      skipNewlines();
      if (consume(TokenType::IS))
        skipNewlines();
      if (check(TokenType::IDENTIFIER)) {
        node->keyVar = advance().lexeme;
        std::transform(node->keyVar.begin(), node->keyVar.end(),
                       node->keyVar.begin(), ::toupper);
        skipNewlines();
      } else {
        throw std::runtime_error(
            "Expected identifier after KEY IS in READ at line " +
            std::to_string(peek().line));
      }
    }

    // Parse AT END, NOT AT END, INVALID KEY, NOT INVALID KEY clauses
    while (check(TokenType::AT) || check(TokenType::NOT) || check(TokenType::INVALID)) {
      if (check(TokenType::AT)) {
        advance();
        expect(TokenType::END, "END");
        skipNewlines();
        node->hasAtEnd = true;
        node->atEndStatements =
            parseBlock({TokenType::NOT, TokenType::INVALID, TokenType::END_READ, TokenType::END_IF,
                        TokenType::END_PERFORM, TokenType::DOT});
      } else if (check(TokenType::NOT)) {
        advance();
        skipNewlines();
        if (check(TokenType::AT)) {
          advance();
          expect(TokenType::END, "END");
          skipNewlines();
          node->hasNotAtEnd = true;
          node->notAtEndStatements =
              parseBlock({TokenType::NOT, TokenType::INVALID, TokenType::END_READ, TokenType::END_IF,
                          TokenType::END_PERFORM, TokenType::DOT});
        } else if (check(TokenType::INVALID)) {
          advance();
          skipNewlines();
          expect(TokenType::KEY, "KEY");
          skipNewlines();
          node->hasNotInvalidKey = true;
          node->notInvalidKeyStatements =
              parseBlock({TokenType::END_READ, TokenType::END_IF,
                          TokenType::END_PERFORM, TokenType::DOT});
        } else {
          break;
        }
      } else if (check(TokenType::INVALID)) {
        advance();
        skipNewlines();
        expect(TokenType::KEY, "KEY");
        skipNewlines();
        node->hasInvalidKey = true;
        node->invalidKeyStatements =
            parseBlock({TokenType::NOT, TokenType::END_READ, TokenType::END_IF,
                        TokenType::END_PERFORM, TokenType::DOT});
      }
      skipNewlines();
    }

    if (check(TokenType::END_READ))
      advance();
    return node;
  }
}

std::unique_ptr<ASTNode> Parser::parseStart() {
  auto node = std::make_unique<StartNode>();
  advance(); // START
  skipNewlines();
  node->fileName = advance().lexeme;
  std::transform(node->fileName.begin(), node->fileName.end(),
                 node->fileName.begin(), ::toupper);
  skipNewlines();

  // KEY [IS] ...
  if (consume(TokenType::KEY)) {
    skipNewlines();
    if (consume(TokenType::IS))
      skipNewlines();

    bool hasNot = false;
    if (consume(TokenType::NOT))
      hasNot = true;

    // Parse comparison operator
    if (consume(TokenType::LESS)) {
      consume(TokenType::THAN); // consume "THAN"
      bool hasEqual = false;
      if (consume(TokenType::OR)) {
        expect(TokenType::EQUAL, "EQUAL");
        hasEqual = true;
      }
      consume(TokenType::TO); // optional "TO"
      if (hasNot)
        node->comp = hasEqual ? "LESS_THAN" : "GREATER_THAN_OR_EQUAL";
      else
        node->comp = hasEqual ? "LESS_THAN_OR_EQUAL" : "LESS_THAN";
    } else if (consume(TokenType::GREATER)) {
      consume(TokenType::THAN);
      bool hasEqual = false;
      if (consume(TokenType::OR)) {
        expect(TokenType::EQUAL, "EQUAL");
        hasEqual = true;
      }
      consume(TokenType::TO);
      if (hasNot)
        node->comp = hasEqual ? "GREATER_THAN" : "LESS_THAN_OR_EQUAL";
      else
        node->comp = hasEqual ? "GREATER_THAN_OR_EQUAL" : "GREATER_THAN";
    } else if (consume(TokenType::EQUAL)) {
      consume(TokenType::TO);
      if (hasNot)
        node->comp = "NOT_EQUAL";
      else
        node->comp = "EQUAL";
    } else if (check(TokenType::GREATER_THAN)) {
      advance();
      node->comp = hasNot ? "LESS_THAN_OR_EQUAL" : "GREATER_THAN";
    } else if (check(TokenType::LESS_THAN)) {
      advance();
      node->comp = hasNot ? "GREATER_THAN_OR_EQUAL" : "LESS_THAN";
    } else if (check(TokenType::EQUALS)) {
      advance();
      node->comp = hasNot ? "NOT_EQUAL" : "EQUAL";
    } else {
      throw std::runtime_error(
          "Expected comparison operator in START at line " +
          std::to_string(peek().line));
    }

    skipNewlines();
    if (!check(TokenType::IDENTIFIER)) {
      throw std::runtime_error(
          "Expected identifier for RECORD KEY in START at line " +
          std::to_string(peek().line) + ", got '" + peek().lexeme + "'");
    }
    node->keyVar = advance().lexeme;
    std::transform(node->keyVar.begin(), node->keyVar.end(),
                   node->keyVar.begin(), ::toupper);
    skipNewlines();
  }

  // Parse INVALID KEY / NOT INVALID KEY ... END-START
  while (!check(TokenType::END_START) && !check(TokenType::DOT) &&
         !check(TokenType::EOF_TOKEN)) {
    if (check(TokenType::INVALID)) {
      advance();
      skipNewlines();
      expect(TokenType::KEY, "KEY");
      skipNewlines();
      node->invalidKeyStatements =
          parseBlock({TokenType::NOT, TokenType::END_START, TokenType::DOT});
    } else if (check(TokenType::NOT)) {
      advance();
      skipNewlines();
      if (check(TokenType::INVALID)) {
        advance();
        skipNewlines();
        expect(TokenType::KEY, "KEY");
        skipNewlines();
        node->notInvalidKeyStatements =
            parseBlock({TokenType::END_START, TokenType::DOT});
      } else {
        break;
      }
    } else {
      advance(); // skip stray tokens
    }
    skipNewlines();
  }

  if (check(TokenType::END_START))
    advance();
  return node;
}

std::unique_ptr<ASTNode> Parser::parseWrite() {
  {
    auto node = std::make_unique<WriteNode>();
    advance(); // WRITE
    skipNewlines();
    node->recordName = advance().lexeme;
    std::transform(node->recordName.begin(), node->recordName.end(),
                   node->recordName.begin(), ::toupper);
    skipNewlines();
    if (consume(TokenType::TO)) {
      skipNewlines();
      node->fileName = advance().lexeme;
      std::transform(node->fileName.begin(), node->fileName.end(),
                     node->fileName.begin(), ::toupper);
    }

    while (check(TokenType::INVALID) || check(TokenType::NOT)) {
      if (check(TokenType::INVALID)) {
        advance();
        skipNewlines();
        expect(TokenType::KEY, "KEY");
        skipNewlines();
        node->hasInvalidKey = true;
        node->invalidKeyStatements =
            parseBlock({TokenType::NOT, TokenType::END_WRITE, TokenType::END_IF,
                        TokenType::END_PERFORM, TokenType::DOT});
      } else if (check(TokenType::NOT)) {
        advance();
        skipNewlines();
        if (check(TokenType::INVALID)) {
          advance();
          skipNewlines();
          expect(TokenType::KEY, "KEY");
          skipNewlines();
          node->hasNotInvalidKey = true;
          node->notInvalidKeyStatements =
              parseBlock({TokenType::END_WRITE, TokenType::END_IF,
                          TokenType::END_PERFORM, TokenType::DOT});
        } else {
          break;
        }
      }
      skipNewlines();
    }

    if (check(TokenType::END_WRITE))
      advance();
    return node;
  }
}

std::unique_ptr<ASTNode> Parser::parseMove() {
  {
    auto node = std::make_unique<MoveNode>();
    advance();
    if (check(TokenType::NUMBER)) {
      node->source = advance().lexeme;
      node->sourceIsLiteral = true;
    } else if (check(TokenType::MINUS)) {
      advance();
      if (!check(TokenType::NUMBER)) {
        throw std::runtime_error("Expected number after '-' in MOVE at line " +
                                 std::to_string(peek().line));
      }
      node->source = "-" + advance().lexeme;
      node->sourceIsLiteral = true;
    } else if (check(TokenType::STRING)) {
      node->source = advance().lexeme;
      node->sourceIsLiteral = true;
    } else if (check(TokenType::IDENTIFIER)) {
      node->source = readVariable();
    } else {
      throw std::runtime_error(
          "Unexpected token '" + peek().lexeme + "' in MOVE at line " +
          std::to_string(peek().line) +
          ": expected identifier, number, or string literal");
    }
    expect(TokenType::TO, "TO");
    if (check(TokenType::IDENTIFIER))
      node->dest = readVariable();
    else {
      node->dest = advance().lexeme;
      std::transform(node->dest.begin(), node->dest.end(), node->dest.begin(),
                     ::toupper);
    }
    return node;
  }
}

std::unique_ptr<ASTNode> Parser::parseAdd() {
  {
    auto node = std::make_unique<AddNode>();
    advance();

    if (check(TokenType::NUMBER)) {
      node->left = advance().lexeme;
      node->leftIsLiteral = true;
    } else if (check(TokenType::MINUS)) {
      advance();
      if (!check(TokenType::NUMBER)) {
        throw std::runtime_error("Expected number after '-' in ADD at line " +
                                 std::to_string(peek().line));
      }
      node->left = "-" + advance().lexeme;
      node->leftIsLiteral = true;
    } else if (check(TokenType::IDENTIFIER)) {
      node->left = advance().lexeme;
      std::transform(node->left.begin(), node->left.end(), node->left.begin(),
                     ::toupper);
    } else {
      throw std::runtime_error(
          "Unexpected token '" + peek().lexeme + "' in ADD at line " +
          std::to_string(peek().line) + ": expected identifier or number");
    }

    expect(TokenType::TO, "TO");

    if (check(TokenType::NUMBER)) {
      node->right = advance().lexeme;
      node->rightIsLiteral = true;
    } else if (check(TokenType::MINUS)) {
      advance();
      if (!check(TokenType::NUMBER)) {
        throw std::runtime_error("Expected number after '-' in ADD at line " +
                                 std::to_string(peek().line));
      }
      node->right = "-" + advance().lexeme;
      node->rightIsLiteral = true;
    } else if (check(TokenType::IDENTIFIER)) {
      node->right = advance().lexeme;
      std::transform(node->right.begin(), node->right.end(),
                     node->right.begin(), ::toupper);
    } else {
      throw std::runtime_error(
          "Unexpected token '" + peek().lexeme + "' in ADD at line " +
          std::to_string(peek().line) + ": expected identifier or number");
    }

    if (check(TokenType::GIVING)) {
      advance();
      node->dest = advance().lexeme;
      std::transform(node->dest.begin(), node->dest.end(), node->dest.begin(),
                     ::toupper);
    } else {
      node->dest = node->right;
    }

    return node;
  }
}

std::unique_ptr<ASTNode> Parser::parseMultiply() {
  {
    auto node = std::make_unique<MultiplyNode>();
    advance();

    if (check(TokenType::NUMBER)) {
      node->left = advance().lexeme;
      node->leftIsLiteral = true;
    } else if (check(TokenType::MINUS)) {
      advance();
      if (!check(TokenType::NUMBER)) {
        throw std::runtime_error(
            "Expected number after '-' in MULTIPLY at line " +
            std::to_string(peek().line));
      }
      node->left = "-" + advance().lexeme;
      node->leftIsLiteral = true;
    } else if (check(TokenType::IDENTIFIER)) {
      node->left = advance().lexeme;
      std::transform(node->left.begin(), node->left.end(), node->left.begin(),
                     ::toupper);
    } else {
      throw std::runtime_error(
          "Unexpected token '" + peek().lexeme + "' in MULTIPLY at line " +
          std::to_string(peek().line) + ": expected identifier or number");
    }

    expect(TokenType::BY, "BY");

    if (check(TokenType::NUMBER)) {
      node->right = advance().lexeme;
      node->rightIsLiteral = true;
    } else if (check(TokenType::MINUS)) {
      advance();
      if (!check(TokenType::NUMBER)) {
        throw std::runtime_error(
            "Expected number after '-' in MULTIPLY at line " +
            std::to_string(peek().line));
      }
      node->right = "-" + advance().lexeme;
      node->rightIsLiteral = true;
    } else if (check(TokenType::IDENTIFIER)) {
      node->right = advance().lexeme;
      std::transform(node->right.begin(), node->right.end(),
                     node->right.begin(), ::toupper);
    } else {
      throw std::runtime_error(
          "Unexpected token '" + peek().lexeme + "' in MULTIPLY at line " +
          std::to_string(peek().line) + ": expected identifier or number");
    }

    if (consume(TokenType::GIVING)) {
      node->dest = advance().lexeme;
      std::transform(node->dest.begin(), node->dest.end(), node->dest.begin(),
                     ::toupper);
    } else {
      node->dest = node->right;
    }

    return node;
  }
}

std::unique_ptr<ASTNode> Parser::parseSubtract() {
  {
    auto node = std::make_unique<SubtractNode>();
    advance();

    if (check(TokenType::NUMBER)) {
      node->left = advance().lexeme;
      node->leftIsLiteral = true;
    } else if (check(TokenType::MINUS)) {
      advance();
      if (!check(TokenType::NUMBER)) {
        throw std::runtime_error(
            "Expected number after '-' in SUBTRACT at line " +
            std::to_string(peek().line));
      }
      node->left = "-" + advance().lexeme;
      node->leftIsLiteral = true;
    } else if (check(TokenType::IDENTIFIER)) {
      node->left = advance().lexeme;
      std::transform(node->left.begin(), node->left.end(), node->left.begin(),
                     ::toupper);
    } else {
      throw std::runtime_error(
          "Unexpected token '" + peek().lexeme + "' in SUBTRACT at line " +
          std::to_string(peek().line) + ": expected identifier or number");
    }

    expect(TokenType::FROM, "FROM");

    if (check(TokenType::NUMBER)) {
      node->right = advance().lexeme;
      node->rightIsLiteral = true;
    } else if (check(TokenType::MINUS)) {
      advance();
      if (!check(TokenType::NUMBER)) {
        throw std::runtime_error(
            "Expected number after '-' in SUBTRACT at line " +
            std::to_string(peek().line));
      }
      node->right = "-" + advance().lexeme;
      node->rightIsLiteral = true;
    } else if (check(TokenType::IDENTIFIER)) {
      node->right = advance().lexeme;
      std::transform(node->right.begin(), node->right.end(),
                     node->right.begin(), ::toupper);
    } else {
      throw std::runtime_error(
          "Unexpected token '" + peek().lexeme + "' in SUBTRACT at line " +
          std::to_string(peek().line) + ": expected identifier or number");
    }

    if (consume(TokenType::GIVING)) {
      node->dest = advance().lexeme;
      std::transform(node->dest.begin(), node->dest.end(), node->dest.begin(),
                     ::toupper);
    } else {
      node->dest = node->right;
    }

    return node;
  }
}

std::unique_ptr<ASTNode> Parser::parseDivide() {
  {
    auto node = std::make_unique<DivideNode>();
    advance();

    if (check(TokenType::NUMBER)) {
      node->left = advance().lexeme;
      node->leftIsLiteral = true;
    } else if (check(TokenType::MINUS)) {
      advance();
      if (!check(TokenType::NUMBER)) {
        throw std::runtime_error(
            "Expected number after '-' in DIVIDE at line " +
            std::to_string(peek().line));
      }
      node->left = "-" + advance().lexeme;
      node->leftIsLiteral = true;
    } else if (check(TokenType::IDENTIFIER)) {
      node->left = advance().lexeme;
      std::transform(node->left.begin(), node->left.end(), node->left.begin(),
                     ::toupper);
    } else {
      throw std::runtime_error(
          "Unexpected token '" + peek().lexeme + "' in DIVIDE at line " +
          std::to_string(peek().line) + ": expected identifier or number");
    }

    // DIVIDE supports both INTO and BY syntax
    // DIVIDE dividend INTO divisor GIVING quotient
    // DIVIDE dividend BY divisor GIVING quotient
    bool divideBy = false;
    if (check(TokenType::INTO)) {
      advance();
    } else if (check(TokenType::BY)) {
      advance();
      divideBy = true;
    }

    if (check(TokenType::NUMBER)) {
      node->right = advance().lexeme;
      node->rightIsLiteral = true;
    } else if (check(TokenType::MINUS)) {
      advance();
      if (!check(TokenType::NUMBER)) {
        throw std::runtime_error(
            "Expected number after '-' in DIVIDE at line " +
            std::to_string(peek().line));
      }
      node->right = "-" + advance().lexeme;
      node->rightIsLiteral = true;
    } else if (check(TokenType::IDENTIFIER)) {
      node->right = advance().lexeme;
      std::transform(node->right.begin(), node->right.end(),
                     node->right.begin(), ::toupper);
    } else {
      throw std::runtime_error(
          "Unexpected token '" + peek().lexeme + "' in DIVIDE at line " +
          std::to_string(peek().line) + ": expected identifier or number");
    }

    // For DIVIDE BY: left is dividend, right is divisor (swap for computation)
    // For DIVIDE INTO: left is dividend, right is divisor

    if (consume(TokenType::GIVING)) {
      node->dest = advance().lexeme;
      std::transform(node->dest.begin(), node->dest.end(), node->dest.begin(),
                     ::toupper);
    } else {
      node->dest = node->right;
    }

    if (check(TokenType::REMAINDER)) {
      advance();
      node->remainderDest = advance().lexeme;
      std::transform(node->remainderDest.begin(), node->remainderDest.end(),
                     node->remainderDest.begin(), ::toupper);
    }

    return node;
  }
}

std::unique_ptr<ASTNode> Parser::parseCompute() {
  {
    auto node = std::make_unique<ComputeNode>();
    advance();
    if (check(TokenType::IDENTIFIER))
      node->dest = readVariable();
    else {
      node->dest = advance().lexeme;
      std::transform(node->dest.begin(), node->dest.end(), node->dest.begin(),
                     ::toupper);
    }
    expect(TokenType::EQUALS, "=");
    node->expr = parseExpression();
    return node;
  }
}

std::unique_ptr<ASTNode> Parser::parseDisplay() {
  {
    auto node = std::make_unique<DisplayNode>();
    advance();
    while (!check(TokenType::DOT) && !check(TokenType::EOF_TOKEN) &&
           !check(TokenType::ELSE) && !check(TokenType::END_IF) &&
           !check(TokenType::END_PERFORM) && !check(TokenType::END_EVALUATE) &&
           !check(TokenType::END_STRING_OP) && !check(TokenType::END_UNSTRING) &&
           !check(TokenType::AT) && !check(TokenType::NOT) && !check(TokenType::END_READ) &&
           !check(TokenType::IF) && !check(TokenType::PERFORM) &&
           !check(TokenType::GO) && !check(TokenType::OPEN) &&
           !check(TokenType::CLOSE) && !check(TokenType::READ) &&
           !check(TokenType::WRITE) && !check(TokenType::STOP) &&
           !check(TokenType::MOVE) && !check(TokenType::ADD) &&
           !check(TokenType::SUBTRACT) && !check(TokenType::MULTIPLY) &&
           !check(TokenType::DIVIDE) && !check(TokenType::COMPUTE) &&
           !check(TokenType::DISPLAY) && !check(TokenType::INSPECT) &&
           !check(TokenType::EVALUATE) && !check(TokenType::STRING_OP) &&
           !check(TokenType::UNSTRING)) {
      if (check(TokenType::NUMBER)) {
        node->operands.push_back(advance().lexeme);
        node->isLiteral.push_back(true);
      } else if (check(TokenType::MINUS)) {
        advance();
        if (!check(TokenType::NUMBER)) {
          throw std::runtime_error(
              "Expected number after '-' in DISPLAY at line " +
              std::to_string(peek().line));
        }
        node->operands.push_back("-" + advance().lexeme);
        node->isLiteral.push_back(true);
      } else if (check(TokenType::STRING)) {
        node->operands.push_back(advance().lexeme);
        node->isLiteral.push_back(true);
      } else if (check(TokenType::IDENTIFIER)) {
        node->operands.push_back(readVariable());
        node->isLiteral.push_back(false);
      } else if (check(TokenType::COMMA)) {
        advance();
      } else {
        advance();
      }
    }
    return node;
  }
}

std::unique_ptr<ASTNode> Parser::parsePrint() {
  {
    auto node = std::make_unique<PrintNode>();
    advance();
    while (!check(TokenType::DOT) && !check(TokenType::EOF_TOKEN) &&
           !check(TokenType::ELSE) && !check(TokenType::END_IF) &&
           !check(TokenType::END_PERFORM) && !check(TokenType::END_EVALUATE) &&
           !check(TokenType::END_STRING_OP) && !check(TokenType::END_UNSTRING) &&
           !check(TokenType::AT) && !check(TokenType::NOT) && !check(TokenType::END_READ) &&
           !check(TokenType::IF) && !check(TokenType::PERFORM) &&
           !check(TokenType::GO) && !check(TokenType::OPEN) &&
           !check(TokenType::CLOSE) && !check(TokenType::READ) &&
           !check(TokenType::WRITE) && !check(TokenType::STOP) &&
           !check(TokenType::MOVE) && !check(TokenType::ADD) &&
           !check(TokenType::SUBTRACT) && !check(TokenType::MULTIPLY) &&
           !check(TokenType::DIVIDE) && !check(TokenType::COMPUTE) &&
           !check(TokenType::DISPLAY) && !check(TokenType::INSPECT) &&
           !check(TokenType::EVALUATE) && !check(TokenType::STRING_OP) &&
           !check(TokenType::UNSTRING)) {
      if (check(TokenType::NUMBER)) {
        node->operands.push_back(advance().lexeme);
        node->isLiteral.push_back(true);
      } else if (check(TokenType::MINUS)) {
        advance();
        if (!check(TokenType::NUMBER)) {
          throw std::runtime_error(
              "Expected number after '-' in PRINT at line " +
              std::to_string(peek().line));
        }
        node->operands.push_back("-" + advance().lexeme);
        node->isLiteral.push_back(true);
      } else if (check(TokenType::STRING)) {
        node->operands.push_back(advance().lexeme);
        node->isLiteral.push_back(true);
      } else if (check(TokenType::IDENTIFIER)) {
        node->operands.push_back(readVariable());
        node->isLiteral.push_back(false);
      } else if (check(TokenType::COMMA)) {
        advance();
      } else {
        // Skip unknown tokens (e.g., NEWLINE) without adding as operands
        advance();
      }
    }
    return node;
  }
}

std::unique_ptr<ASTNode> Parser::parseStop() {
  {
    advance();
    expect(TokenType::RUN, "RUN");
    return std::make_unique<StopRunNode>();
  }
}

std::unique_ptr<ASTNode> Parser::parseIf() {
  {
    auto node = std::make_unique<IfNode>();
    advance();
    node->conditions.push_back(parseCondition());
    // collect AND/OR connected conditions (allow newlines between parts)
    skipNewlines();
    while (check(TokenType::AND) || check(TokenType::OR)) {
      TokenType op = peek().type;
      advance();
      skipNewlines();
      node->condOps.push_back(op);
      node->conditions.push_back(parseCondition());
      skipNewlines();
    }

    // Accept optional THEN (many COBOL sources omit it)
    if (check(TokenType::THEN))
      advance();

    if (check(TokenType::DOT))
      advance();

    node->thenStatements = parseBlock({TokenType::ELSE, TokenType::END_IF});

    if (check(TokenType::ELSE)) {
      advance();
      if (check(TokenType::DOT))
        advance();
      node->elseStatements = parseBlock({TokenType::END_IF});
    }

    if (check(TokenType::END_IF))
      advance();
    return node;
  }
}

std::unique_ptr<ASTNode> Parser::parsePerform() {
  {
    advance();
    skipNewlines();

    if (check(TokenType::UNTIL)) {
      advance();
      auto node = std::make_unique<PerformUntilNode>();
      node->condition = parseCondition();
      skipNewlines();
      if (consume(TokenType::WITH)) {
        skipNewlines();
        if (consume(TokenType::TRUE)) {
          skipNewlines();
          if (check(TokenType::IDENTIFIER)) {
            std::string keyword = peek().lexeme;
            std::transform(keyword.begin(), keyword.end(), keyword.begin(), ::toupper);
            if (keyword == "AFTER") {
              advance();
              node->testBefore = false;
            }
          }
        }
      }
      if (check(TokenType::DOT))
        advance();
      node->body = parseBlock({TokenType::END_PERFORM});
      if (check(TokenType::END_PERFORM))
        advance();
      return node;
    } else if (check(TokenType::VARYING)) {
      advance();
      auto node = std::make_unique<PerformVaryingNode>();
      node->counter = advance().lexeme;
      std::transform(node->counter.begin(), node->counter.end(),
                     node->counter.begin(), ::toupper);
      expect(TokenType::FROM, "FROM");
      if (check(TokenType::NUMBER)) {
        node->from = advance().lexeme;
        node->fromIsLiteral = true;
      } else if (check(TokenType::MINUS)) {
        advance();
        if (!check(TokenType::NUMBER)) {
          throw std::runtime_error(
              "Expected number after '-' in PERFORM VARYING at line " +
              std::to_string(peek().line));
        }
        node->from = "-" + advance().lexeme;
        node->fromIsLiteral = true;
      } else {
        node->from = advance().lexeme;
        std::transform(node->from.begin(), node->from.end(), node->from.begin(),
                       ::toupper);
      }
      expect(TokenType::BY, "BY");
      if (check(TokenType::NUMBER)) {
        node->by = advance().lexeme;
        node->byIsLiteral = true;
      } else if (check(TokenType::MINUS)) {
        advance();
        if (!check(TokenType::NUMBER)) {
          throw std::runtime_error(
              "Expected number after '-' in PERFORM VARYING at line " +
              std::to_string(peek().line));
        }
        node->by = "-" + advance().lexeme;
        node->byIsLiteral = true;
      } else {
        node->by = advance().lexeme;
        std::transform(node->by.begin(), node->by.end(), node->by.begin(),
                       ::toupper);
      }
      expect(TokenType::UNTIL, "UNTIL");
      node->untilCondition = parseCondition();
      skipNewlines();
      if (consume(TokenType::WITH)) {
        skipNewlines();
        if (consume(TokenType::TRUE)) {
          skipNewlines();
          if (check(TokenType::IDENTIFIER)) {
            std::string keyword = peek().lexeme;
            std::transform(keyword.begin(), keyword.end(), keyword.begin(), ::toupper);
            if (keyword == "AFTER") {
              advance();
              node->testBefore = false;
            }
          }
        }
      }
      if (check(TokenType::DOT))
        advance();
      node->body = parseBlock({TokenType::END_PERFORM});
      if (check(TokenType::END_PERFORM))
        advance();
      return node;
    } else if (check(TokenType::NUMBER)) {
      auto node = std::make_unique<PerformTimesNode>();
      node->count = advance().lexeme;
      node->countIsLiteral = true;
      expect(TokenType::TIMES, "TIMES");
      if (check(TokenType::DOT))
        advance();
      node->body = parseBlock({TokenType::END_PERFORM});
      if (check(TokenType::END_PERFORM))
        advance();
      return node;
    } else if (!check(TokenType::EOF_TOKEN) && !check(TokenType::NEWLINE) &&
               !check(TokenType::DOT) && !check(TokenType::NUMBER) &&
               !peek().lexeme.empty()) {
      // PERFORM <para> [WITH TEST BEFORE|AFTER] [UNTIL <condition>]
      // Paragraph target may be IDENTIFIER or a keyword used as a label.
      auto node = std::make_unique<PerformParagraphNode>();
      node->target = advance().lexeme;
      std::transform(node->target.begin(), node->target.end(),
                     node->target.begin(), ::toupper);
      skipNewlines();
      if (consume(TokenType::WITH)) {
        skipNewlines();
        if (consume(TokenType::TRUE)) {
          skipNewlines();
          if (check(TokenType::IDENTIFIER)) {
            std::string keyword = peek().lexeme;
            std::transform(keyword.begin(), keyword.end(), keyword.begin(), ::toupper);
            if (keyword == "AFTER") {
              advance();
              node->testBefore = false;
            }
          }
        }
      }
      skipNewlines();
      if (check(TokenType::UNTIL)) {
        advance();
        skipNewlines();
        node->untilCondition = parseCondition();
      }
      return node;
    }

    throw std::runtime_error("Expected UNTIL, VARYING, TIMES, or paragraph "
                             "name after PERFORM at line " +
                             std::to_string(peek().line));
  }
}

std::unique_ptr<ASTNode> Parser::parseGoTo() {
  {
    advance();
    expect(TokenType::TO, "TO");
    auto node = std::make_unique<GoToNode>();
    node->target = advance().lexeme;
    std::transform(node->target.begin(), node->target.end(),
                   node->target.begin(), ::toupper);
    return node;
  }
}

std::unique_ptr<ASTNode> Parser::parseInspect() {
  {
    advance(); // INSPECT
    auto node = std::make_unique<InspectNode>();
    node->target = advance().lexeme;
    std::transform(node->target.begin(), node->target.end(),
                   node->target.begin(), ::toupper);

    expect(TokenType::REPLACING, "REPLACING");
    expect(TokenType::ALL, "ALL");

    // Parse old value
    if (check(TokenType::ZEROS)) {
      advance();
      node->oldValue = "0";
      node->oldIsFigurative = true;
    } else if (check(TokenType::SPACES)) {
      advance();
      node->oldValue = " ";
      node->oldIsFigurative = true;
    } else if (check(TokenType::STRING)) {
      node->oldValue = advance().lexeme;
      node->oldIsFigurative = false;
      if (node->oldValue.empty()) {
        throw std::runtime_error(
            "Empty string not allowed in INSPECT at line " +
            std::to_string(peek().line));
      }
    } else if (check(TokenType::NUMBER)) {
      node->oldValue = advance().lexeme;
      node->oldIsFigurative = false;
    } else {
      throw std::runtime_error("Expected literal or figurative constant after "
                               "ALL in INSPECT at line " +
                               std::to_string(peek().line));
    }

    expect(TokenType::BY, "BY");

    // Parse new value
    if (check(TokenType::ZEROS)) {
      advance();
      node->newValue = "0";
      node->newIsFigurative = true;
    } else if (check(TokenType::SPACES)) {
      advance();
      node->newValue = " ";
      node->newIsFigurative = true;
    } else if (check(TokenType::STRING)) {
      node->newValue = advance().lexeme;
      node->newIsFigurative = false;
      if (node->newValue.empty()) {
        throw std::runtime_error(
            "Empty string not allowed in INSPECT at line " +
            std::to_string(peek().line));
      }
    } else if (check(TokenType::NUMBER)) {
      node->newValue = advance().lexeme;
      node->newIsFigurative = false;
    } else {
      throw std::runtime_error("Expected literal or figurative constant after "
                               "BY in INSPECT at line " +
                               std::to_string(peek().line));
    }

    return node;
  }
}

std::unique_ptr<ASTNode> Parser::parseAccept() {
  {
    advance(); // ACCEPT
    skipNewlines();
    auto node = std::make_unique<AcceptNode>();
    if (check(TokenType::IDENTIFIER))
      node->dest = readVariable();
    else
      node->dest = advance().lexeme;
    std::transform(node->dest.begin(), node->dest.end(), node->dest.begin(),
                   ::toupper);
    skipNewlines();
    if (consume(TokenType::FROM)) {
      skipNewlines();
      if (check(TokenType::IDENTIFIER)) {
        std::string from = peek().lexeme;
        std::transform(from.begin(), from.end(), from.begin(), ::toupper);
        if (from == "DATE" || from == "TIME" || from == "DAY") {
          advance();
          node->fromType = from;
        }
      }
    }
    if (check(TokenType::DOT))
      advance();
    return node;
  }
}

std::unique_ptr<ASTNode> Parser::parseEvaluate() {
  {
    advance(); // EVALUATE
    skipNewlines();
    auto node = std::make_unique<EvaluateNode>();

    if (consume(TokenType::TRUE)) {
      node->subjectIsTrue = true;
    } else {
      skipNewlines();
      if (check(TokenType::NUMBER) || check(TokenType::STRING)) {
        node->subject = advance().lexeme;
        node->subjectIsLiteral = true;
      } else if (check(TokenType::MINUS)) {
        advance();
        if (!check(TokenType::NUMBER)) {
          throw std::runtime_error(
              "Expected number after '-' in EVALUATE subject at line " +
              std::to_string(peek().line));
        }
        node->subject = "-" + advance().lexeme;
        node->subjectIsLiteral = true;
      } else if (check(TokenType::IDENTIFIER)) {
        node->subject = readVariable();
      } else {
        throw std::runtime_error(
            "Expected expression after EVALUATE at line " +
            std::to_string(peek().line));
      }
    }

    skipNewlines();
    if (consume(TokenType::WITH)) {
      skipNewlines();
      if (consume(TokenType::TRUE)) {
        skipNewlines();
        if (check(TokenType::IDENTIFIER)) {
          std::string keyword = peek().lexeme;
          std::transform(keyword.begin(), keyword.end(), keyword.begin(), ::toupper);
          if (keyword == "BEFORE" || keyword == "AFTER") {
            advance();
            node->testType = keyword;
          } else {
            node->testType = "BEFORE";
          }
        } else {
          node->testType = "BEFORE";
        }
      }
    }

    skipNewlines();
    while (check(TokenType::WHEN) || check(TokenType::OTHER)) {
      auto whenClause = std::make_unique<EvaluateWhenClause>();
      skipNewlines();

      if (check(TokenType::OTHER)) {
        advance();
        whenClause->isOther = true;
        whenClause->hasCondition = false;
      } else {
        advance(); // consume WHEN
        skipNewlines();
        if (check(TokenType::OTHER)) {
          advance();
          whenClause->isOther = true;
          whenClause->hasCondition = false;
        } else if (check(TokenType::IDENTIFIER)) {
          std::string left = readVariable();
          skipNewlines();
          bool isCond = false;
          if (check(TokenType::EQUAL) || check(TokenType::GREATER) ||
              check(TokenType::LESS) || check(TokenType::GREATER_THAN) ||
              check(TokenType::LESS_THAN) || check(TokenType::EQUALS)) {
            isCond = true;
          }
          if (isCond) {
            whenClause->hasCondition = true;
            auto cond = std::make_unique<ConditionNode>();
            cond->left = left;
            cond->leftIsLiteral = false;
            cond->line = peek().line;
            skipNewlines();
            if (check(TokenType::EQUAL)) {
              advance();
              consume(TokenType::TO);
              cond->op = ConditionNode::EQ;
            } else if (check(TokenType::GREATER)) {
              advance();
              if (consume(TokenType::THAN)) {
                if (consume(TokenType::OR)) {
                  expect(TokenType::EQUAL, "EQUAL");
                  expect(TokenType::TO, "TO");
                  cond->op = ConditionNode::GE;
                } else {
                  cond->op = ConditionNode::GT;
                }
              } else {
                cond->op = ConditionNode::GT;
              }
            } else if (check(TokenType::LESS)) {
              advance();
              if (consume(TokenType::THAN)) {
                if (consume(TokenType::OR)) {
                  expect(TokenType::EQUAL, "EQUAL");
                  expect(TokenType::TO, "TO");
                  cond->op = ConditionNode::LE;
                } else {
                  cond->op = ConditionNode::LT;
                }
              } else {
                cond->op = ConditionNode::LT;
              }
            } else if (check(TokenType::GREATER_THAN)) {
              advance();
              if (check(TokenType::EQUALS)) {
                advance();
                cond->op = ConditionNode::GE;
              } else {
                cond->op = ConditionNode::GT;
              }
            } else if (check(TokenType::LESS_THAN)) {
              advance();
              if (check(TokenType::EQUALS)) {
                advance();
                cond->op = ConditionNode::LE;
              } else {
                cond->op = ConditionNode::LT;
              }
            } else if (check(TokenType::EQUALS)) {
              advance();
              cond->op = ConditionNode::EQ;
            } else {
              throw std::runtime_error(
                  "Expected comparison operator in EVALUATE WHEN at line " +
                  std::to_string(peek().line));
            }
            skipNewlines();
            if (check(TokenType::NUMBER) || check(TokenType::STRING)) {
              cond->right = advance().lexeme;
              cond->rightIsLiteral = true;
            } else if (check(TokenType::MINUS)) {
              advance();
              if (!check(TokenType::NUMBER)) {
                throw std::runtime_error(
                    "Expected number after '-' in EVALUATE WHEN condition at line " +
                    std::to_string(peek().line));
              }
              cond->right = "-" + advance().lexeme;
              cond->rightIsLiteral = true;
            } else if (check(TokenType::IDENTIFIER)) {
              cond->right = readVariable();
            } else {
              throw std::runtime_error(
                  "Expected literal or identifier after comparison operator in EVALUATE at line " +
                  std::to_string(peek().line));
            }
            whenClause->conditions.push_back(std::move(cond));
          } else {
            whenClause->hasCondition = false;
            whenClause->subjects.push_back(left);
            whenClause->subjectIsLiteral.push_back(false);
          }
        } else if (check(TokenType::NUMBER) || check(TokenType::STRING)) {
          whenClause->hasCondition = false;
          whenClause->subjects.push_back(advance().lexeme);
          whenClause->subjectIsLiteral.push_back(true);
        } else if (check(TokenType::MINUS)) {
          advance();
          if (!check(TokenType::NUMBER)) {
            throw std::runtime_error(
                "Expected number after '-' in EVALUATE WHEN at line " +
                std::to_string(peek().line));
          }
          whenClause->hasCondition = false;
          whenClause->subjects.push_back("-" + advance().lexeme);
          whenClause->subjectIsLiteral.push_back(true);
        } else {
          throw std::runtime_error(
              "Expected value or condition after WHEN in EVALUATE at line " +
              std::to_string(peek().line));
        }

        while (consume(TokenType::COMMA)) {
          skipNewlines();
          if (check(TokenType::NUMBER) || check(TokenType::STRING)) {
            whenClause->subjects.push_back(advance().lexeme);
            whenClause->subjectIsLiteral.push_back(true);
          } else if (check(TokenType::MINUS)) {
            advance();
            if (!check(TokenType::NUMBER)) {
              throw std::runtime_error(
                  "Expected number after '-' in EVALUATE WHEN list at line " +
                  std::to_string(peek().line));
            }
            whenClause->subjects.push_back("-" + advance().lexeme);
            whenClause->subjectIsLiteral.push_back(true);
          } else if (check(TokenType::IDENTIFIER)) {
            whenClause->subjects.push_back(readVariable());
            whenClause->subjectIsLiteral.push_back(false);
          } else {
            break;
          }
        }

        if (consume(TokenType::THRU)) {
          skipNewlines();
          whenClause->hasThru = true;
          if (check(TokenType::NUMBER) || check(TokenType::STRING)) {
            whenClause->thruValue = advance().lexeme;
            whenClause->thruValueIsLiteral = true;
          } else if (check(TokenType::MINUS)) {
            advance();
            if (!check(TokenType::NUMBER)) {
              throw std::runtime_error(
                  "Expected number after '-' in EVALUATE THRU at line " +
                  std::to_string(peek().line));
            }
            whenClause->thruValue = "-" + advance().lexeme;
            whenClause->thruValueIsLiteral = true;
          } else if (check(TokenType::IDENTIFIER)) {
            whenClause->thruValue = readVariable();
            whenClause->thruValueIsLiteral = false;
          } else {
            throw std::runtime_error(
                "Expected value after THRU in EVALUATE at line " +
                std::to_string(peek().line));
          }
        }
      }

      skipNewlines();
      whenClause->body = parseBlock({TokenType::WHEN, TokenType::OTHER,
                                     TokenType::END_EVALUATE, TokenType::DOT,
                                     TokenType::EOF_TOKEN});
      node->whenClauses.push_back(std::move(whenClause));
      skipNewlines();
    }

    if (check(TokenType::END_EVALUATE))
      advance();
    else if (check(TokenType::DOT))
      advance();
    else if (check(TokenType::EOF_TOKEN))
      throw std::runtime_error(
          "Syntax error: EVALUATE missing END-EVALUATE at line " +
          std::to_string(peek().line));
    else
      throw std::runtime_error(
          "Syntax error: expected END-EVALUATE or '.' at line " +
          std::to_string(peek().line) + ", got '" + peek().lexeme + "'");

    return node;
  }
}

std::unique_ptr<ASTNode> Parser::parseString() {
  {
    advance(); // STRING
    skipNewlines();
    auto node = std::make_unique<StringNode>();

    while (!check(TokenType::INTO) && !check(TokenType::EOF_TOKEN) &&
           !check(TokenType::DOT)) {
      skipNewlines();
      StringSourceClause clause;

      if (check(TokenType::STRING)) {
        clause.source = advance().lexeme;
        clause.sourceIsLiteral = true;
      } else if (check(TokenType::NUMBER)) {
        clause.source = advance().lexeme;
        clause.sourceIsLiteral = true;
      } else if (check(TokenType::IDENTIFIER)) {
        clause.source = readVariable();
        clause.sourceIsLiteral = false;
      } else if (check(TokenType::ZEROS)) {
        advance();
        clause.source = "0";
        clause.sourceIsLiteral = true;
      } else if (check(TokenType::SPACES)) {
        advance();
        clause.source = " ";
        clause.sourceIsLiteral = true;
      } else {
        break;
      }

      skipNewlines();
      expect(TokenType::DELIMITED, "DELIMITED");
      skipNewlines();
      expect(TokenType::BY, "BY");
      skipNewlines();
      if (consume(TokenType::SIZE)) {
        clause.delimitedBySize = true;
      } else if (check(TokenType::SPACES)) {
        advance();
        clause.delimiterChar = ' ';
      } else if (check(TokenType::ZEROS)) {
        advance();
        clause.delimiterChar = '0';
      } else if (check(TokenType::STRING)) {
        std::string delim = advance().lexeme;
        if (!delim.empty())
          clause.delimiterChar = delim[0];
      } else if (check(TokenType::IDENTIFIER)) {
        clause.delimiterVar = readVariable();
        clause.delimiterIsVariable = true;
      } else {
        clause.delimitedBySize = true;
      }
      node->sources.push_back(std::move(clause));
      skipNewlines();
    }

    expect(TokenType::INTO, "INTO");
    skipNewlines();
    if (check(TokenType::IDENTIFIER)) {
      node->dest = readVariable();
    } else {
      throw std::runtime_error(
          "Expected destination variable after INTO in STRING at line " +
          std::to_string(peek().line));
    }

    skipNewlines();
    if (consume(TokenType::WITH)) {
      skipNewlines();
      if (consume(TokenType::POINTER)) {
        skipNewlines();
        if (check(TokenType::IDENTIFIER)) {
          node->pointerVar = readVariable();
          node->hasPointer = true;
        } else {
          throw std::runtime_error(
              "Expected pointer variable after POINTER in STRING at line " +
              std::to_string(peek().line));
        }
      }
    }

    skipNewlines();
    if (check(TokenType::IDENTIFIER) && peek().lexeme == "ON") {
      advance();
      skipNewlines();
      expect(TokenType::OVERFLOW, "OVERFLOW");
      skipNewlines();
      if (check(TokenType::DOT))
        advance();
      node->overflowBody =
          parseBlock({TokenType::NOT, TokenType::END_STRING_OP, TokenType::DOT});
      node->hasOverflow = true;
      if (check(TokenType::NOT)) {
        advance();
        skipNewlines();
        expect(TokenType::ON, "ON");
        skipNewlines();
        expect(TokenType::OVERFLOW, "OVERFLOW");
        skipNewlines();
        if (check(TokenType::DOT))
          advance();
        node->notOverflowBody =
            parseBlock({TokenType::END_STRING_OP, TokenType::DOT});
        node->hasNotOverflow = true;
      }
    }

    skipNewlines();
    if (check(TokenType::END_STRING_OP))
      advance();
    else if (check(TokenType::DOT))
      advance();

    return node;
  }
}

std::unique_ptr<ASTNode> Parser::parseUnstring() {
  {
    advance(); // UNSTRING
    skipNewlines();
    auto node = std::make_unique<UnstringNode>();

    if (check(TokenType::IDENTIFIER)) {
      node->source = readVariable();
    } else {
      throw std::runtime_error(
          "Expected source variable after UNSTRING at line " +
          std::to_string(peek().line));
    }

    skipNewlines();

    // Optional global DELIMITED BY clause
    if (check(TokenType::DELIMITED)) {
      advance(); // DELIMITED
      skipNewlines();
      expect(TokenType::BY, "BY");
      skipNewlines();

      if (check(TokenType::ALL)) {
        advance();
        node->delimiterAll = true;
        skipNewlines();
      }

      if (check(TokenType::STRING)) {
        node->delimiter = advance().lexeme;
        node->delimiterIsLiteral = true;
      } else if (check(TokenType::SPACES)) {
        advance();
        node->delimiter = " ";
        node->delimiterIsLiteral = true;
      } else if (check(TokenType::ZEROS)) {
        advance();
        node->delimiter = "0";
        node->delimiterIsLiteral = true;
      } else if (check(TokenType::IDENTIFIER)) {
        node->delimiter = readVariable();
        node->delimiterIsLiteral = false;
      } else {
        throw std::runtime_error(
            "Expected delimiter after DELIMITED BY in UNSTRING at line " +
            std::to_string(peek().line));
      }
      skipNewlines();
    }

    expect(TokenType::INTO, "INTO");
    skipNewlines();

    while (check(TokenType::IDENTIFIER)) {
      UnstringIntoClause clause;
      clause.dest = readVariable();
      clause.destIsLiteral = false;
      node->intoClauses.push_back(std::move(clause));
      skipNewlines();
      consume(TokenType::COMMA);
      skipNewlines();
    }

    skipNewlines();
    if (consume(TokenType::WITH)) {
      skipNewlines();
      expect(TokenType::POINTER, "POINTER");
      skipNewlines();
      if (check(TokenType::IDENTIFIER)) {
        node->pointerVar = readVariable();
        node->hasPointer = true;
      } else {
        throw std::runtime_error(
            "Expected pointer variable after POINTER in UNSTRING at line " +
            std::to_string(peek().line));
      }
    }

    skipNewlines();
    if (consume(TokenType::TALLYING)) {
      skipNewlines();
      expect(TokenType::IN, "IN");
      skipNewlines();
      if (check(TokenType::IDENTIFIER)) {
        node->tallyVar = readVariable();
        node->hasTally = true;
      } else {
        throw std::runtime_error(
            "Expected tally variable after IN in UNSTRING at line " +
            std::to_string(peek().line));
      }
    }

    skipNewlines();
    if (check(TokenType::END_UNSTRING))
      advance();
    else if (check(TokenType::DOT))
      advance();

    return node;
  }
}
