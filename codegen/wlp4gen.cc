#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

int endNewCount = 0;
int endDeleteCount = 0;

enum class wlp4Type {
    INT,
    PTR,
    UNKNOWN,
};

struct symbolInfo {
    wlp4Type type = wlp4Type::UNKNOWN;
    int offset;
};

int IF_COUNT = 0;
int WHILE_COUNT = 0;

int MIN_OFFSET = 8;

map<string, map<string, symbolInfo>> symbolTable = {{"wain", {}}};
string SCOPE = "wain";

string GET_VARIABLE(string ID) {
    if (symbolTable[SCOPE].contains(ID)) {
        symbolInfo info = symbolTable[SCOPE][ID];
        return "lw $3, " + to_string(info.offset) + "($29)\n";
    }
    return "NO SUCH VARIABLE " + SCOPE + " " + ID + "\n";
}

string RETURN(int offset) {
    return "lw $3, " + to_string(offset) + "($29)\n";
}

string PUSH(int reg) {
    MIN_OFFSET -= 4;
    return "sw $" + to_string(reg) + ", -4($30)\nsub $30, $30, $4\n";
}

string POP(int reg) {
    MIN_OFFSET += 4;
    return "add $30, $30, $4\nlw $" + to_string(reg) + ", -4($30)\n";
}

string EPILOGUE(int varCount) {
    string result = "";
    for (int i = 0; i < varCount; i++) {
        result += "add $30, $30, $4\n";
    }
    result += "jr $31\n";
    return result;
}

string P_LABEL(string label) {
    return "P" + label + ":\n";
}

string PROLOGUE = ".import init\n.import new\n.import delete\n.import print\nlis $4\n.word 4\nlis $11\n.word 1\n";
// string PROLOGUE = "lis $4\n.word 4\nlis $11\n.word 1\n";
string SET_FRAME_PTR = "sub $29, $30, $4\n";

string concatCode(const vector<string> &codelines) {
    string result = "";
    for (auto &codeline : codelines) {
        result += codeline;
    }
    return result;
}

const std::set<std::string> terminalSymbol = {
    "ID",
    "NUM",
    "LPAREN",
    "RPAREN",
    "LBRACE",
    "RBRACE",
    "RETURN",
    "IF",
    "ELSE",
    "WHILE",
    "PRINTLN",
    "PUTCHAR",
    "GETCHAR",
    "WAIN",
    "BECOMES",
    "INT",
    "EQ",
    "NE",
    "LT",
    "GT",
    "LE",
    "GE",
    "PLUS",
    "MINUS",
    "STAR",
    "SLASH",
    "PCT",
    "COMMA",
    "SEMI",
    "NEW",
    "DELETE",
    "LBRACK",
    "RBRACK",
    "AMP",
    "NULL",
    "BOF",
    "EOF",
};

bool isTerminal(const std::string &token) {
    return terminalSymbol.contains(token);
}

void outputError(std::string msg = "") {
    std::cerr << "ERROR: " << msg << std::endl;
}

struct Node {
    string lhs = "";
    string rhs = "";
    wlp4Type type = wlp4Type::UNKNOWN;
    vector<unique_ptr<Node>> children = {};
    explicit Node(std::string lhs) : lhs(lhs) {}
    void populate(std::istream &in) {
        std::string read;
        getline(in, read);
        std::istringstream iss{read};
        iss >> read;
        if (isTerminal(read)) {
            // leaf node
            iss >> rhs;
            if (iss >> read && read == ":") {
                iss >> read;
                if (read == "int") {
                    type = wlp4Type::INT;
                } else if (read == "int*") {
                    type = wlp4Type::PTR;
                }
            }
            return;
        }
        while (iss >> read) {
            if (read == ".EMPTY") {
                rhs = ".EMPTY";
                return;
            }
            if (read == ":") {
                iss >> read;
                if (read == "int") {
                    type = wlp4Type::INT;
                } else if (read == "int*") {
                    type = wlp4Type::PTR;
                }
                break;
            }
            if (rhs.length() > 0) {
                rhs += " ";
            }
            rhs += read;
            children.emplace_back(std::make_unique<Node>(read));
        }
        for (auto &child : children) {
            child->populate(in);
        }
    }
    void print() {
        std::cout << lhs << " " << rhs;
        if (type != wlp4Type::UNKNOWN) {
            std::cout << " : " << (type == wlp4Type::INT ? "int" : "int*");
        }
        std::cout << std::endl;
        for (auto &child : children) {
            child->print();
        }
    }

    int getLvalueOffset() {
        if (lhs != "lvalue") {
            return -1;
        }
        if (rhs == "ID") {
            Node *ID = children[0].get();
            return symbolTable[SCOPE][ID->rhs].offset;
        }
        if (rhs == "LPAREN lvalue RPAREN") {
            return children[1]->getLvalueOffset();
        }
        return -1;
    }

    int countArgs() {
        if (lhs == "arglist") {
            if (rhs == "expr") {
                return 1;
            }
            if (rhs == "expr COMMA arglist") {
                return 1 + children[2]->countArgs();
            }
        }
        return 0;
    }

    int processParamlist() {
        if (lhs != "paramlist") {
            return 0;
        }
        if (rhs == "dcl" || rhs == "dcl COMMA paramlist") {
            Node *dcl = children[0].get();
            Node *ID = dcl->children[1].get();
            string IDStr = ID->rhs;
            wlp4Type idType = ID->type;
            symbolTable[SCOPE][IDStr] = {idType, MIN_OFFSET};
            MIN_OFFSET -= 4;
            if (rhs == "dcl") {
                return 1;
            }
            if (rhs == "dcl COMMA paramlist") {
                return 1 + children[2]->processParamlist();
            }
        }
        return 0;
    }

    void processParams() {
        if (lhs == "params") {
            if (rhs == "paramlist") {
                children[0]->processParamlist();
            }
        }
    }

    int countParams() {
        if (lhs == "params") {
            if (rhs == "paramlist") {
                return children[0]->count_Paramlist();
            }
        }
        return 0;
    }

    string getLvalueTerminal() {
        if (lhs == "lvalue") {
            if (rhs == "LPAREN lvalue RPAREN") {
                return children[1]->getLvalueTerminal();
            } else {
                return rhs;
            }
        }
        return "";
    }

    int count_Paramlist() {
        if (lhs == "paramlist") {
            if (rhs == "dcl") {
                return 1;
            }
            if (rhs == "dcl COMMA paramlist") {
                return 1 + children[2]->count_Paramlist();
            }
        }
        return 0;
    }

    string code(int pushReg = 3) {
        if (lhs == "start" && rhs == "BOF procedures EOF") {
            return children[1]->code();
        }
        if (lhs == "procedures") {
            if (rhs == "main") {
                return children[0]->code();
            }
            if (rhs == "procedure procedures") {
                return concatCode({children[1]->code(), children[0]->code()});
            }
        }
        if (lhs == "procedure" && rhs == "INT ID LPAREN params RPAREN LBRACE dcls statements RETURN expr SEMI RBRACE") {
            Node *ID = children[1].get();
            Node *dcls = children[6].get();
            Node *statements = children[7].get();
            Node *params = children[3].get();
            Node *expr = children[9].get();
            int saveMin = MIN_OFFSET;
            string saveScope = SCOPE;
            SCOPE = ID->rhs;
            MIN_OFFSET = params->countParams() * 4;
            params->processParams();

            string pushReg = "";
            string popReg = "";
            string dclsCode = dcls->code();
            for (int i = 0; i < 21; ++i) {
                if (i == 3)
                    continue;
                pushReg += PUSH(i);
            }
            string code = concatCode({
                P_LABEL(ID->rhs),
                "sub $29, $30, $4\n",
                dclsCode,
                pushReg,
                statements->code(),
                expr->code(),

            });
            for (int i = 20; i >= 0; --i) {
                if (i == 3)
                    continue;
                popReg += POP(i);
            }
            code += concatCode({
                popReg,
                "add $30, $29, $4\n",
                "jr $31\n",
            });

            MIN_OFFSET = saveMin;
            SCOPE = saveScope;
            return code;
        }
        if (lhs == "main" && rhs == "INT WAIN LPAREN dcl COMMA dcl RPAREN LBRACE dcls statements RETURN expr SEMI RBRACE") {
            Node *dcl1 = children[3].get();
            Node *dcl2 = children[5].get();
            Node *dcls = children[8].get();
            Node *statements = children[9].get();
            Node *returnExp = children[11].get();
            string initCode = concatCode({
                PUSH(2),
                "add $2, $0, $0\n",
                "lis $10\n.word init\nsw $31, -4($30)\nsub $30, $30, $4\njalr $10\nadd $30, $30, $4\nlw $31, -4($30)\n",
                POP(2),
            });
            wlp4Type dcl1Type = dcl1->children[1]->type;
            if (dcl1Type == wlp4Type::PTR) {
                initCode = "lis $10\n.word init\nsw $31, -4($30)\nsub $30, $30, $4\njalr $10\nadd $30, $30, $4\nlw $31, -4($30)\n";
            }
            return concatCode({PROLOGUE, initCode, dcl1->code(1), dcl2->code(2), SET_FRAME_PTR, dcls->code(),
                               statements->code(), returnExp->code(), EPILOGUE(2)});
        }
        if (lhs == "dcls") {
            if (rhs == "dcls dcl BECOMES NUM SEMI") {
                Node *dcl = children[1].get();
                Node *Num = children[3].get();
                return concatCode({children[0]->code(), Num->code(), dcl->code()});
            }
            if (rhs == "dcls dcl BECOMES NULL SEMI") {
                Node *dcl = children[1].get();
                return concatCode({children[0]->code(), "add $3, $11, $0\n", dcl->code()});
            }
        }
        if (lhs == "statements" && rhs == "statements statement") {
            return concatCode({children[0]->code(), children[1]->code()});
        }

        if (lhs == "dcl" && rhs == "type ID") {
            Node *ID = children[1].get();
            string IDStr = ID->rhs;
            wlp4Type idType = ID->type;
            symbolTable[SCOPE][IDStr] = {idType, MIN_OFFSET};
            return PUSH(pushReg);
        }
        if (lhs == "expr") {
            if (rhs == "term") {
                return children[0]->code();
            }
            if (rhs == "expr PLUS term") {
                Node *expr = children[0].get();
                Node *term = children[2].get();
                wlp4Type termType = term->type;
                wlp4Type exprType = expr->type;
                if (termType == wlp4Type::INT && exprType == wlp4Type::INT) {
                    return concatCode({children[0]->code(), PUSH(3), children[2]->code(),
                                       POP(5), "add $3, $5, $3\n"});
                }
                if (exprType == wlp4Type::PTR && termType == wlp4Type::INT) {
                    return concatCode({
                        expr->code(),
                        PUSH(3),
                        term->code(),
                        "mult $3, $4\nmflo $3\n",
                        POP(5),
                        "add $3, $5, $3\n",
                    });
                }
                if (exprType == wlp4Type::INT && termType == wlp4Type::PTR) {
                    return concatCode({
                        term->code(),
                        PUSH(3),
                        expr->code(),
                        "mult $3, $4\nmflo $3\n",
                        POP(5),
                        "add $3, $5, $3\n",
                    });
                }
            }
            if (rhs == "expr MINUS term") {
                Node *expr = children[0].get();
                Node *term = children[2].get();
                wlp4Type termType = term->type;
                wlp4Type exprType = expr->type;
                if (exprType == wlp4Type::INT && termType == wlp4Type::INT) {
                    return concatCode({children[0]->code(), PUSH(3), children[2]->code(),
                                       POP(5), "sub $3, $5, $3\n"});
                }
                if (exprType == wlp4Type::PTR && termType == wlp4Type::INT) {
                    return concatCode({
                        expr->code(),
                        PUSH(3),
                        term->code(),
                        "mult $3, $4\nmflo $3\n",
                        POP(5),
                        "sub $3, $5, $3\n",
                    });
                }
                if (exprType == wlp4Type::PTR && termType == wlp4Type::PTR) {
                    return concatCode({
                        expr->code(),
                        PUSH(3),
                        term->code(),
                        POP(5),
                        "sub $3, $5, $3\n",
                        "div $3, $4\nmflo $3\n",
                    });
                }
            }
        }
        if (lhs == "statement") {
            if (rhs == "lvalue BECOMES expr SEMI") {
                Node *lvalue = children[0].get();
                Node *expr = children[2].get();
                if (lvalue->getLvalueTerminal() == "ID") {
                    int offset = lvalue->getLvalueOffset();
                    return concatCode({expr->code(), "sw $3, " + to_string(offset) + "($29)\n"});
                }
                if (lvalue->getLvalueTerminal() == "STAR factor") {
                    Node *factor = lvalue->children[1].get();
                    return concatCode({
                        expr->code(),
                        PUSH(3),
                        factor->code(),
                        POP(5),
                        "sw $5, 0($3)\n",
                    });
                }
            }
            if (rhs == "IF LPAREN test RPAREN LBRACE statements RBRACE ELSE LBRACE statements RBRACE") {
                Node *test = children[2].get();
                Node *statements1 = children[5].get();
                Node *statements2 = children[9].get();
                int ifCount = IF_COUNT++;
                return concatCode({test->code(), "beq $3, $0, ELSE" + to_string(ifCount) + "\n",
                                   statements1->code(), "beq $0, $0, ENDIF" + to_string(ifCount) + "\n",
                                   "ELSE" + to_string(ifCount) + ":\n", statements2->code(),
                                   "ENDIF" + to_string(ifCount) + ":\n"});
            }
            if (rhs == "WHILE LPAREN test RPAREN LBRACE statements RBRACE") {
                Node *test = children[2].get();
                Node *statements = children[5].get();
                int whileCount = WHILE_COUNT++;
                return concatCode({"WHILE" + to_string(whileCount) + ":\n", test->code(),
                                   "beq $3, $0, ENDWHILE" + to_string(whileCount) + "\n",
                                   statements->code(), "beq $0, $0, WHILE" + to_string(whileCount) + "\n",
                                   "ENDWHILE" + to_string(whileCount) + ":\n"});
            }
            if (rhs == "PUTCHAR LPAREN expr RPAREN SEMI") {
                return concatCode({children[2]->code(),
                                   "lis $5\n.word 0xffff000c\nsw $3, 0($5)\n"});
            }

            if (rhs == "PRINTLN LPAREN expr RPAREN SEMI") {
                return concatCode({children[2]->code(),
                                   "add $1, $3, $0\n",
                                   "lis $10\n.word print\nsw $31, -4($30)\nsub $30, $30, $4\njalr $10\nadd $30, $30, $4\nlw $31, -4($30)\n"});
            }
            if (rhs == "DELETE LBRACK RBRACK expr SEMI") {
                Node *expr = children[3].get();
                endDeleteCount++;
                return concatCode({expr->code(),
                                   "beq $3, $11, ENDDELETE" + to_string(endDeleteCount) + "\n",
                                   "add $1, $3, $0\n",
                                   "lis $10\n.word delete\nsw $31, -4($30)\nsub $30, $30, $4\njalr $10\nadd $30, $30, $4\nlw $31, -4($30)\n",
                                   "ENDDELETE" + to_string(endDeleteCount) + ":\n"});
            }
        }
        if (lhs == "test") {
            Node *expr1 = children[0].get();
            Node *expr2 = children[2].get();
            string sltFunction = "slt";
            wlp4Type expr1Type = expr1->type;
            wlp4Type expr2Type = expr2->type;
            if (expr1Type == wlp4Type::PTR && expr2Type == wlp4Type::PTR) {
                sltFunction = "sltu";
            }
            if (rhs == "expr EQ expr") {
                return concatCode({children[0]->code(), PUSH(3), children[2]->code(), POP(5),
                                   sltFunction,
                                   " $6, $3, $5\n",
                                   sltFunction,
                                   " $7, $5, $3\n",
                                   "add $3, $6, $7\n",
                                   "sub $3, $11, $3\n"});
            }
            if (rhs == "expr NE expr") {
                return concatCode({children[0]->code(), PUSH(3), children[2]->code(), POP(5),
                                   sltFunction, " $6, $3, $5\n",
                                   sltFunction, " $7, $5, $3\n",
                                   "add $3, $6, $7\n"});
            }
            if (rhs == "expr LT expr") {
                return concatCode({children[0]->code(), PUSH(3), children[2]->code(), POP(5),
                                   sltFunction, " $3, $5, $3\n"});
            }
            if (rhs == "expr LE expr") {
                return concatCode({children[0]->code(), PUSH(3), children[2]->code(), POP(5),
                                   sltFunction, " $3, $3, $5\nsub $3, $11, $3\n"});
            }
            if (rhs == "expr GE expr") {
                return concatCode({children[0]->code(), PUSH(3), children[2]->code(), POP(5),
                                   sltFunction, " $3, $5, $3\nsub $3, $11, $3\n"});
            }
            if (rhs == "expr GT expr") {
                return concatCode({children[0]->code(), PUSH(3), children[2]->code(), POP(5),
                                   sltFunction, " $3, $3, $5\n"});
            }
        }
        if (lhs == "term") {
            if (rhs == "factor") {
                return children[0]->code();
            }
            if (rhs == "term STAR factor") {
                return concatCode({children[0]->code(), PUSH(3), children[2]->code(), POP(5), "mult $3, $5\nmflo $3\n"});
            }
            if (rhs == "term SLASH factor") {
                return concatCode({children[0]->code(), PUSH(3), children[2]->code(), POP(5), "div $5, $3\nmflo $3\n"});
            }
            if (rhs == "term PCT factor") {
                return concatCode({children[0]->code(), PUSH(3), children[2]->code(), POP(5), "div $5, $3\nmfhi $3\n"});
            }
        }
        if (lhs == "NUM") {
            return "lis $3\n.word " + rhs + "\n";
        }
        if (lhs == "arglist") {
            if (rhs == "expr") {
                return concatCode({children[0]->code(), PUSH(3)});
            }
            if (rhs == "expr COMMA arglist") {
                return concatCode({children[0]->code(), PUSH(3), children[2]->code()});
            }
        }
        if (lhs == "factor") {
            if (rhs == "NEW INT LBRACK expr RBRACK") {
                Node *expr = children[3].get();
                endNewCount++;
                return concatCode({
                    expr->code(),
                    "add $1, $3, $0\n",
                    "lis $10\n.word new\nsw $31, -4($30)\nsub $30, $30, $4\njalr $10\nadd $30, $30, $4\nlw $31, -4($30)\n",
                    "bne $3, $0, ENDNEW" + to_string(endNewCount) + "\nadd $3, $11, $0\nENDNEW" + to_string(endNewCount) + ":\n",
                });
            }
            if (rhs == "NULL") {
                return "add $3, $11, $0\n";
            }
            if (rhs == "STAR factor") {
                Node *factor = children[1].get();
                return concatCode({factor->code(), "lw $3, 0($3)\n"});
            }
            if (rhs == "AMP lvalue") {
                Node *lvalue = children[1].get();
                if (lvalue->getLvalueTerminal() == "ID") {
                    int offset = lvalue->getLvalueOffset();
                    return "lis $3\n.word " + to_string(offset) + "\nadd $3, $29, $3\n";
                }
                if (lvalue->getLvalueTerminal() == "STAR factor") {
                    Node *factor = lvalue->children[1].get();
                    return factor->code();
                }
            }
            if (rhs == "ID LPAREN RPAREN") {
                Node *ID = children[0].get();

                string procTag = "P" + ID->rhs;
                return concatCode({
                    PUSH(29),
                    PUSH(31),
                    "lis $5\n.word " + procTag + "\njalr $5\n",
                    POP(31),
                    POP(29),
                });
            }
            if (rhs == "ID LPAREN arglist RPAREN") {
                Node *ID = children[0].get();
                Node *arglist = children[2].get();
                int argsCount = arglist->countArgs();

                string procTag = "P" + ID->rhs;
                string popArgs = "";
                for (int i = 0; i < argsCount; i++) {
                    popArgs += POP(5);
                }
                return concatCode({
                    PUSH(29),
                    PUSH(31),
                    arglist->code(),
                    "lis $5\n.word " + procTag + "\njalr $5\n",
                    popArgs,
                    POP(31),
                    POP(29),
                });
            }
            if (rhs == "NUM") {
                return children[0]->code();
            }
            if (rhs == "ID") {
                Node *ID = children[0].get();
                string IDStr = ID->rhs;
                return GET_VARIABLE(IDStr);
            }
            if (rhs == "LPAREN expr RPAREN") {
                return children[1]->code();
            }
            if (rhs == "GETCHAR LPAREN RPAREN") {
                return "lis $5\n.word 0xffff0004\nlw $3, 0($5)\n";
            }
        }
        return "";
    }
};

int main() {
    Node root{"start"};
    root.populate(std::cin);
    std::cout << root.code() << std::endl;
}
