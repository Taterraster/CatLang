#include <regex>
#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>
using namespace std;

// Forward declaration so linker knows about this
void executeLine(const string& line,
                 unordered_map<string, string>& strVars,
                 unordered_map<string, double>& numVars,
                 unordered_map<string, bool>& boolVars,
                 unordered_map<string, CatFunction>& functions);

bool evaluateCondition(const string& expr,
                       unordered_map<string, string>& strVars,
                       unordered_map<string, double>& numVars,
                       unordered_map<string, bool>& boolVars);

// Executes if/else blocks
void executeIfStatement(bool condition,
                        const vector<string>& trueBlock,
                        const vector<string>& falseBlock,
                        unordered_map<string, string>& strVars,
                        unordered_map<string, double>& numVars,
                        unordered_map<string, bool>& boolVars,
                        unordered_map<string, CatFunction>& functions) {
    const vector<string>& block = condition ? trueBlock : falseBlock;
    for (const auto& line : block) {
        executeLine(line, strVars, numVars, boolVars, functions);
    }
}

// Main if/else handling logic inside interpreter loop
void handleIfElse(const vector<string>& programLines,
                  unordered_map<string, string>& strVars,
                  unordered_map<string, double>& numVars,
                  unordered_map<string, bool>& boolVars,
                  unordered_map<string, CatFunction>& functions) {

    regex ifRegex(R"(^\s*if\s*\((.+)\)\s*\{\s*$)");
    regex elseRegex(R"(^\s*(?:\}\s*)?else\s*\{\s*$)");
    smatch match;

    bool inIfBlock = false;
    bool inElseBlock = false;
    string currentCondition;
    vector<string> trueBlock;
    vector<string> falseBlock;

    for (const auto& line : programLines) {
        if (!inIfBlock && regex_match(line, match, ifRegex)) {
            // Beginning of an if-statement
            inIfBlock = true;
            currentCondition = match[1];
            trueBlock.clear();
            falseBlock.clear();
        }
        else if (inIfBlock && !inElseBlock && regex_match(line, match, elseRegex)) {
            // Start of else-block
            inElseBlock = true;
        }
        else if (line.find("}") != string::npos && inIfBlock) {
            // End of if/else block
            bool conditionResult = evaluateCondition(currentCondition, strVars, numVars, boolVars);
            executeIfStatement(conditionResult, trueBlock, falseBlock, strVars, numVars, boolVars, functions);

            // Reset state
            inIfBlock = false;
            inElseBlock = false;
            trueBlock.clear();
            falseBlock.clear();
        }
        else if (inIfBlock) {
            // Collect lines inside current block
            if (inElseBlock)
                falseBlock.push_back(line);
            else
                trueBlock.push_back(line);
        }
        else {
            // Handle as a normal line outside of if/else
            executeLine(line, strVars, numVars, boolVars, functions);
        }
    }
}
bool evaluateCondition(const string& expr,
                       unordered_map<string, string>& strVars,
                       unordered_map<string, double>& numVars,
                       unordered_map<string, bool>& boolVars) {
    smatch match;

    
    // Example: if ("cat" == "cat") or if (str1 == str2)
    regex strEq(R"delim(^\s*"([^"]+)"\s*==\s*"([^"]+)"\s*$)delim");
    if (regex_match(expr, match, strEq)) {
        return match[1].str() == match[2].str();
    }

    
    // Handles: ==, !=, <, >, <=, >= between variables or literals
    regex numComp(R"(^\s*([A-Za-z_]\w*|\d+(?:\.\d+)?)\s*(==|!=|<|>|<=|>=)\s*([A-Za-z_]\w*|\d+(?:\.\d+)?)\s*$)");
    if (regex_match(expr, match, numComp)) {
        string left = match[1];
        string op = match[2];
        string right = match[3];

        auto getValue = [&](const string& token) -> double {
            if (numVars.count(token)) return numVars[token];
            try { return stod(token); } catch (...) { return 0.0; }
        };

        double lhs = getValue(left);
        double rhs = getValue(right);

        if (op == "==") return lhs == rhs;
        if (op == "!=") return lhs != rhs;
        if (op == "<")  return lhs < rhs;
        if (op == ">")  return lhs > rhs;
        if (op == "<=") return lhs <= rhs;
        if (op == ">=") return lhs >= rhs;
    }

    
    if (boolVars.count(expr)) return boolVars[expr];

    
    cerr << "Invalid condition: " << expr << endl;
    return false;
}
