#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

enum class wlp4Type {
    INT,
    PTR,
    UNKNOWN,
};
typedef std::map<std::string, std::map<std::string, wlp4Type>> Tables;
typedef std::map<std::string, std::vector<wlp4Type>> ProcTable;
ProcTable activeSignature = std::map<std::string, std::vector<wlp4Type>>{};
std::string scope;

std::string getTypeString(wlp4Type type) {
    switch (type) {
    case wlp4Type::INT:
        return "int";
    case wlp4Type::PTR:
        return "int*";
    case wlp4Type::UNKNOWN:
        return "unknown";
    }
    return "unknown";
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
    std::string lhs = "";
    std::string lexeme = "";
    wlp4Type type = wlp4Type::UNKNOWN;
    std::vector<std::unique_ptr<Node>> children = {};
    std::string getRhsString() {
        if (children.empty()) {
            return lexeme;
        }
        std::string rhs = children[0]->lhs;
        for (size_t i = 1; i < children.size(); ++i) {
            rhs += " " + children[i]->lhs;
        }
        return rhs;
    }

    explicit Node(std::string lhs) : lhs(lhs) {}
    void populate(std::istream &in) {
        std::string read;
        getline(in, read);
        std::istringstream iss{read};
        iss >> read;
        if (isTerminal(read)) {
            // leaf node
            iss >> lexeme;
            return;
        }
        while (iss >> read) {
            if (read == ".EMPTY") {
                lexeme = ".EMPTY";
                return;
            }
            children.emplace_back(std::make_unique<Node>(read));
        }
        for (auto &child : children) {
            child->populate(in);
        }
    }
    void processParams(Tables &tables) {
        if (lhs != "params") {
            return;
        }
        if (children.empty()) {
            return;
        }
        if (getRhsString() == "paramlist") {
            children[0]->processParamList(tables);
        }
    }
    void processParamList(Tables &tables) {
        if (lhs != "paramlist") {
            return;
        }
        if (getRhsString() == "dcl COMMA paramlist") {
            Node *dcl = children[0].get();
            Node *dclType = dcl->children[0].get();
            if (dclType->getRhsString() == "INT") {
                activeSignature[scope].emplace_back(wlp4Type::INT);
            } else if (dclType->getRhsString() == "INT STAR") {
                activeSignature[scope].emplace_back(wlp4Type::PTR);
            }
            dcl->processDeclaration(tables);
            children[2]->processParamList(tables);
        } else if (getRhsString() == "dcl") {
            Node *dcl = children[0].get();
            Node *dclType = dcl->children[0].get();
            if (dclType->getRhsString() == "INT") {
                activeSignature[scope].emplace_back(wlp4Type::INT);
            } else if (dclType->getRhsString() == "INT STAR") {
                activeSignature[scope].emplace_back(wlp4Type::PTR);
            }
            dcl->processDeclaration(tables);
        }
    }
    void processDeclaration(Tables &tables) {
        if (lhs == "dcl" && getRhsString() == "type ID") {
            wlp4Type dclType = wlp4Type::UNKNOWN;
            if (children[0]->getRhsString() == "INT") {
                dclType = wlp4Type::INT;
            } else if (children[0]->getRhsString() == "INT STAR") {
                dclType = wlp4Type::PTR;
            }
            if (tables[scope].contains(children[1]->lexeme)) {
                throw std::exception();
            }
            tables[scope][children[1]->lexeme] = dclType;
            children[1]->type = dclType;
        } else if (lhs == "procedure" && getRhsString() == "INT ID LPAREN params RPAREN LBRACE dcls statements RETURN expr SEMI RBRACE") {
            if (activeSignature.contains(children[1]->lexeme)) {
                throw std::exception();
            }
            activeSignature[children[1]->lexeme] = {};
            scope = children[1]->lexeme;
            children[3]->processParams(tables);
        }
    }
    void findDeclarations(Tables &tables) {
        if (lhs == "dcl" && getRhsString() == "type ID") {
            processDeclaration(tables);
        } else {
            for (auto &child : children) {
                child->findDeclarations(tables);
            }
        }
    }

    bool semanticCheck() {
        if (children.empty()) {
            return true;
        }
        if (lhs == "main" && getRhsString() == "INT WAIN LPAREN dcl COMMA dcl RPAREN LBRACE dcls statements RETURN expr SEMI RBRACE") {
            Node *dcl1 = children[3].get();
            Node *dcl2 = children[5].get();
            Node *dcl1Id = dcl1->children[1].get();
            Node *dcl2Id = dcl2->children[1].get();
            Node *returnExp = children[11].get();
            if (dcl2Id->type != wlp4Type::INT) {
                return false;
            }
            if (dcl1Id->lexeme == dcl2Id->lexeme) {
                return false;
            }
            if (returnExp->type != wlp4Type::INT) {
                return false;
            }
        }
        if (lhs == "statement") {
            if (getRhsString() == "lvalue BECOMES expr SEMI") {
                if (children[0]->type != children[2]->type) {
                    return false;
                }
            } else if (getRhsString() == "PRINTLN LPAREN expr RPAREN SEMI" ||
                       getRhsString() == "PUTCHAR LPAREN expr RPAREN SEMI") {
                if (children[2]->type != wlp4Type::INT) {
                    return false;
                }
            } else if (getRhsString() == "DELETE LBRACK RBRACK expr SEMI") {
                if (children[3]->type != wlp4Type::PTR) {
                    return false;
                }
            }
        }
        if (lhs == "test" && children.size() == 3) {
            if (children[0]->type != children[2]->type) {
                return false;
            }
        }
        if (lhs == "dcls") {
            if (getRhsString() == "dcls dcl BECOMES NUM SEMI") {
                Node *dcl = children[1].get();
                Node *dclId = dcl->children[1].get();
                if (dclId->type != wlp4Type::INT) {
                    return false;
                }
            }
            if (getRhsString() == "dcls dcl BECOMES NULL SEMI") {
                Node *dcl = children[1].get();
                Node *dclId = dcl->children[1].get();
                if (dclId->type != wlp4Type::PTR) {
                    return false;
                }
            }
        }
        for (auto &child : children) {
            if (!child->semanticCheck()) {
                return false;
            }
        }
        return true;
    }
    bool compareArgs(std::vector<wlp4Type> &signature, Tables &tables) {
        if (lhs != "arglist") {
            return false;
        }
        if (getRhsString() == "expr") {
            if (signature.size() != 1 || children[0]->fillType(tables) != signature[0]) {
                return false;
            }
            return true;
        } else if (getRhsString() == "expr COMMA arglist") {
            if (signature.empty()) {
                return false;
            }
            if (children[0]->fillType(tables) != signature[0]) {
                return false;
            }
            std::vector<wlp4Type> nextSignature = std::vector<wlp4Type>(signature.begin() + 1, signature.end());
            return children[2]->compareArgs(nextSignature, tables);
        }
        return false;
    }
    wlp4Type fillType(Tables &tables) {
        if (lhs == "main") {
            scope = "wain";
        }
        if ((lhs == "dcl" && getRhsString() == "type ID") || lhs == "procedure") {
            processDeclaration(tables);
            if (lhs == "procedure" && getRhsString() == "INT ID LPAREN params RPAREN LBRACE dcls statements RETURN expr SEMI RBRACE") {
                Node *dcls = children[6].get();
                Node *statements = children[7].get();
                Node *returnExp = children[9].get();
                dcls->typeCheck(tables);
                statements->typeCheck(tables);
                returnExp->typeCheck(tables);
                if (returnExp->type != wlp4Type::INT) {
                    throw std::exception();
                }
                return type;
            }
        }
        if (children.empty()) {
            if (lhs == "NUM") {
                type = wlp4Type::INT;
            } else if (lhs == "NULL") {
                type = wlp4Type::PTR;
            } else if (lhs == "ID" && tables[scope].contains(lexeme)) {
                type = tables[scope][lexeme];
            }
        } else if (children.size() == 1) {
            type = children[0]->fillType(tables);
        } else if (lhs == "factor" && getRhsString() == "LPAREN expr RPAREN") {
            type = children[1]->fillType(tables);
        } else if (lhs == "expr" && children.size() == 3 && children[0]->lhs == "expr" && children[2]->lhs == "term") {
            wlp4Type exprType = children[0]->fillType(tables);
            wlp4Type termType = children[2]->fillType(tables);
            if (exprType == wlp4Type::INT && termType == wlp4Type::INT) {
                type = wlp4Type::INT;
                return wlp4Type::INT;
            } else if (((exprType == wlp4Type::PTR && termType == wlp4Type::INT) ||
                        (exprType == wlp4Type::INT && termType == wlp4Type::PTR)) &&
                       children[1]->lhs == "PLUS") {
                type = wlp4Type::PTR;
                return wlp4Type::PTR;
            } else if ((exprType == wlp4Type::PTR && termType == wlp4Type::INT) &&
                       children[1]->lhs == "MINUS") {
                type = wlp4Type::PTR;
                return wlp4Type::PTR;
            } else if (exprType == wlp4Type::PTR && termType == wlp4Type::PTR && children[1]->lhs == "MINUS") {
                type = wlp4Type::INT;
                return wlp4Type::INT;
            } else {
                throw std::exception();
            }
        } else if (lhs == "term" && children.size() == 3 && children[0]->lhs == "term" && children[2]->lhs == "factor") {
            wlp4Type termType = children[0]->fillType(tables);
            wlp4Type factorType = children[2]->fillType(tables);
            if (termType == wlp4Type::INT && factorType == wlp4Type::INT) {
                type = wlp4Type::INT;
                return wlp4Type::INT;
            } else {
                throw std::exception();
            }
        } else if (lhs == "factor") {
            if (getRhsString() == "AMP lvalue") {
                Node *lvalue = children[1].get();
                wlp4Type lvalueType = lvalue->fillType(tables);
                if (lvalueType == wlp4Type::INT) {
                    type = wlp4Type::PTR;
                    return wlp4Type::PTR;
                } else {
                    throw std::exception();
                }
            } else if (getRhsString() == "STAR factor") {
                // The type of a factor or lvalue deriving STAR factor is int. The type of the derived factor (i.e. the one preceded by STAR) must be int*.
                Node *factor = children[1].get();
                wlp4Type factorType = factor->fillType(tables);
                if (factorType == wlp4Type::PTR) {
                    type = wlp4Type::INT;
                    return wlp4Type::INT;
                } else {
                    throw std::exception();
                }
            } else if (getRhsString() == "NEW INT LBRACK expr RBRACK") {
                Node *expr = children[3].get();
                wlp4Type exprType = expr->fillType(tables);
                if (exprType == wlp4Type::INT) {
                    type = wlp4Type::PTR;
                    return wlp4Type::PTR;
                } else {
                    throw std::exception();
                }
            } else if (getRhsString() == "ID LPAREN RPAREN") {
                Node *id = children[0].get();
                if (tables[scope].contains(id->lexeme)) {
                    throw std::exception();
                }
                if (activeSignature.contains(id->lexeme) && activeSignature[id->lexeme].empty()) {
                    type = wlp4Type::INT;
                    return wlp4Type::INT;
                } else {
                    throw std::exception();
                }
            } else if (getRhsString() == "ID LPAREN arglist RPAREN") {
                if (tables[scope].contains(children[0]->lexeme)) {
                    throw std::exception();
                }
                if (activeSignature.contains(children[0]->lexeme)) {
                    std::vector<wlp4Type> &signature = activeSignature[children[0]->lexeme];
                    Node *arglist = children[2].get();
                    if (arglist->compareArgs(signature, tables)) {
                        type = wlp4Type::INT;
                        return wlp4Type::INT;
                    } else {
                        throw std::exception();
                    }
                } else {
                    throw std::exception();
                }
            } else if (getRhsString() == "GETCHAR LPAREN RPAREN") {
                type = wlp4Type::INT;
                return wlp4Type::INT;
            }
        } else if (lhs == "lvalue") {
            if (getRhsString() == "ID") {
                wlp4Type idType = children[0]->fillType(tables);
                if (idType != wlp4Type::UNKNOWN) {
                    type = idType;
                    return idType;
                } else {
                    throw std::exception();
                }
            } else if (getRhsString() == "STAR factor") {
                Node *factor = children[1].get();
                wlp4Type factorType = factor->fillType(tables);
                if (factorType == wlp4Type::PTR) {
                    type = wlp4Type::INT;
                    return wlp4Type::INT;
                } else {
                    throw std::exception();
                }
            } else if (getRhsString() == "LPAREN lvalue RPAREN") {
                type = children[1]->fillType(tables);
                return type;
            }
        } else {
            for (auto &child : children) {
                child->fillType(tables);
            }
            return wlp4Type::UNKNOWN;
        }
        return type;
    }
    void typeCheck(Tables &tables) {
        fillType(tables);
    }
    void print() {
        std::cout << lhs << " ";
        if (lexeme != "") {
            std::cout << lexeme;
        } else {
            std::cout << getRhsString();
        }
        if (type != wlp4Type::UNKNOWN) {
            std::cout << " : " << getTypeString(type);
        }
        std::cout << std::endl;
        for (auto &child : children) {
            child->print();
        }
    }
};

class Tree {
    std::unique_ptr<Node> root;
    Tables tables = std::map<std::string, std::map<std::string, wlp4Type>>{{"wain", std::map<std::string, wlp4Type>{}}};

public:
    explicit Tree(std::istream &in) : root(std::make_unique<Node>("start")) {
        root->populate(in);
    }
    void typeCheck() {
        root->typeCheck(tables);
    }
    bool semanticCheck() {
        return root->semanticCheck();
    }
    void print() {
        root->print();
    }
};

int main() {
    Tree tree(std::cin);
    try {
        tree.typeCheck();
    } catch (const std::exception &e) {
        outputError("invalid type");
        return 0;
    }

    if (!tree.semanticCheck()) {
        outputError("semantic error");
        return 0;
    }
    tree.print();
}
