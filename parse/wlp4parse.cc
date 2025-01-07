#include "wlp4data.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
struct Rule {
    string lhs;
    vector<string> rhs;
    bool nullable() {
        return rhs.size() > 0 && rhs[0] == ".EMPTY";
    };
};

enum class NodeType {
    Rule,
    leaf,
};

struct Node {
    NodeType type;
    Rule rule;
    string token;
    string lexeme;
    vector<unique_ptr<Node>> children;

public:
    Node(Rule &rule) : type(NodeType::Rule), rule(rule), token(""), lexeme("") {}
    Node(const string &token, const string &lexeme) : type(NodeType::leaf), rule(Rule{}), token(token), lexeme(lexeme) {}
    void addChild(unique_ptr<Node> child) { children.emplace_back(move(child)); }
    void addChildren(vector<unique_ptr<Node>> &children) {
        for (auto &child : children) {
            this->children.emplace_back(move(child));
        }
    }
    void print() {
        if (type == NodeType::leaf) {
            cout << token << " " << lexeme << endl;
            return;
        }
        cout << rule.lhs << " ";
        for (auto &rhs : rule.rhs) {
            cout << rhs << " ";
        }
        cout << endl;
        for (auto &child : children) {
            child->print();
        }
    }
};

struct Tree {
    unique_ptr<Node> root;
    void print() { root->print(); }
};

struct Transition {
    int fromState;
    int toState;
    string symbol;
};

struct Reduction {
    int state;
    int ruleId;
    string symbol;
};

class CFG {
    vector<Rule> rules;

public:
    void reduce(int id, vector<string> &redSeq) {
        Rule &rule = getRuleById(id);
        if (rule.nullable()) {
            redSeq.push_back(rule.lhs);
            return;
        }
        for (int i = 0; i < rule.rhs.size(); ++i) {
            redSeq.pop_back();
        }
        redSeq.push_back(rule.lhs);
    }
    void readLine(string line) {
        istringstream iss(line);
        string lhs;
        iss >> lhs;
        vector<string> rhs;
        string s;
        while (iss >> s) {
            rhs.push_back(s);
        }
        rules.push_back({lhs, rhs});
    }
    Rule &getRuleById(int id) { return rules[id]; }
};

struct symbol {
    string name;
    string token;
    string lexeme;
    bool isTerminal = false;

public:
    symbol(string name) : name(name) {}
    symbol(string token, string lexeme) : name(token), token(token), lexeme(lexeme), isTerminal(true) {}
};

class SLR {
    CFG cfg;
    map<int, map<string, int>> transitions = {};
    map<int, map<string, int>> reductions = {};
    vector<int> stateStack = {};
    vector<symbol> inputSeq = {};
    vector<symbol> redSeq = {};
    vector<unique_ptr<Node>> treeStack = {};
    Tree tree;

public:
    SLR() : stateStack(1, 0) {}
    void setInputSeq(vector<symbol> &inputSeq) { this->inputSeq = inputSeq; }
    void shift() {
        redSeq.emplace_back(inputSeq.back());
        string token = inputSeq.back().token;
        string lexme = inputSeq.back().lexeme;
        unique_ptr<Node> leaf = make_unique<Node>(token, lexme);
        treeStack.emplace_back(move(leaf));
        inputSeq.pop_back();
    }
    // returns size of the rule rhs
    int reduce(int id) {
        Rule &rule = cfg.getRuleById(id);
        if (rule.nullable()) {
            treeStack.emplace_back(move(make_unique<Node>(rule)));
            redSeq.emplace_back(rule.lhs);
            return 0;
        }
        unique_ptr<Node> newNode = make_unique<Node>(rule);
        for (int i = treeStack.size() - rule.rhs.size();
             i < treeStack.size(); ++i) {

            newNode->addChild(move(treeStack[i]));
        }
        for (int i = 0; i < rule.rhs.size(); ++i) {
            redSeq.pop_back();
            treeStack.pop_back();
        }
        treeStack.emplace_back(move(newNode));
        redSeq.emplace_back(rule.lhs);
        return rule.rhs.size();
    }

    bool done() {
        return (inputSeq.size() == 0);
    }
    void parse() {
        tree.root = make_unique<Node>(cfg.getRuleById(0));
        int symCount = 0;
        vector<unique_ptr<Node>> nodes; // temporary storage for nodes
        while (!done()) {
            int state = stateStack.back();
            while (reductions.count(state) &&
                   reductions[state].count(inputSeq.back().name)) {
                int n = reduce(reductions[state][inputSeq.back().name]);
                for (int i = 0; i < n; ++i) {
                    stateStack.pop_back();
                }

                state = stateStack.back();
                stateStack.emplace_back(transitions[state][redSeq.back().name]);
                state = stateStack.back();
            }

            shift();

            if (transitions.count(state) && transitions[state].count(redSeq.back().name)) {
                stateStack.emplace_back(transitions[state][redSeq.back().name]);
            } else {
                // reject
                cerr << "ERROR at " << symCount + 1 << endl;
                return;
            }
            if (
                (redSeq.back().name != "BOF" && redSeq.back().name != "EOF"))
                ++symCount;
        }
        // accept
        reduce(reductions[stateStack.back()][".ACCEPT"]);
        treeStack[0]->print();
    }
    void printSeq() {
        for (size_t i = 0; i < redSeq.size(); ++i) {
            cout << redSeq[i].name << " ";
        }
        cout << ". ";
        for (int i = inputSeq.size() - 1; i >= 0; --i) {
            cout << inputSeq[i].name << " ";
        }
        cout << endl;
    }
    void getTransLine(istream &in) {
        int fromState = 0;
        int toState = 0;
        string symbol = "";
        string read = "";
        getline(in, read);
        istringstream iss(read);
        iss >> fromState >> symbol >> toState;
        if (!transitions.count(fromState)) {
            transitions[fromState] = {};
        }
        transitions[fromState][symbol] = toState;
    }
    void getRedLIne(istream &in) {
        int state;
        int ruleId;
        string symbol;
        string read;
        getline(in, read);
        istringstream iss(read);
        iss >> state >> ruleId >> symbol;
        reductions[state][symbol] = ruleId;
    }
    void readCfgLine(string line) { cfg.readLine(line); }
};

int main() {
    SLR slr;
    string read;
    istringstream wlpin{WLP4_COMBINED};
    getline(wlpin, read); // Skip the first line
    vector<symbol> inputSeq;
    while (getline(wlpin, read) && read[0] != '.') // CFG
    {
        slr.readCfgLine(read);
    }
    // while (cin >> read && read[0] != '.') // INPUT
    // {
    //     inputSeq.emplace_back(read);
    // }

    // // reverse the vec so the back is the leftmost
    // reverse(inputSeq.begin(), inputSeq.end());
    // slr.setInputSeq(inputSeq);

    // TRANSITIONS
    while (getline(wlpin, read) && read[0] != '.') {
        istringstream iss{read};
        slr.getTransLine(iss);
    }
    // REDUCTIONS
    while (getline(wlpin, read) && read[0] != '.') {
        istringstream iss{read};
        slr.getRedLIne(iss);
    }

    // WLP4 from stdin
    inputSeq.emplace_back(symbol{"BOF", "BOF"});
    while (getline(cin, read)) {
        istringstream iss{read};
        string token, lexeme;
        iss >> token >> lexeme;
        inputSeq.emplace_back(symbol{token, lexeme});
    }
    inputSeq.emplace_back(symbol{"EOF", "EOF"});
    reverse(inputSeq.begin(), inputSeq.end());
    slr.setInputSeq(inputSeq);
    slr.parse();
}
