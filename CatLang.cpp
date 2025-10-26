#include <iostream>
#include <fstream>
#include <string>
#include <regex>
#include <unordered_map>
#include <sstream>

using namespace std;

// Check for valid .cat or .catlang extension
bool hasValidCatExtension(const string& filename) {
    const string ext1 = ".cat";
    const string ext2 = ".catlang";
    return (filename.size() >= ext1.size() &&
            filename.compare(filename.size() - ext1.size(), ext1.size(), ext1) == 0) ||
           (filename.size() >= ext2.size() &&
            filename.compare(filename.size() - ext2.size(), ext2.size(), ext2) == 0);
}

// Format numbers: remove trailing zeros if decimal
string formatNumber(double num) {
    ostringstream oss;
    oss << num;
    string s = oss.str();
    if (s.find('.') != string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    return s;
}

// Replace variables outside quotes
string replaceVars(const string& expr,
                   const unordered_map<string, string>& strVars,
                   const unordered_map<string, double>& numVars) {
    string result;
    bool inQuotes = false;
    string segment;

    for (size_t i = 0; i < expr.size(); ++i) {
        char c = expr[i];
        if (c == '"') {
            inQuotes = !inQuotes;
            result += c;
        } else if (!inQuotes) {
            segment += c;
        } else {
            result += c;
        }

        if ((!inQuotes && (i + 1 == expr.size() || expr[i + 1] == '"')) && !segment.empty()) {
            // Replace string variables without extra quotes
            for (const auto& [var, val] : strVars)
                segment = regex_replace(segment, regex("\\b" + var + "\\b"), val);

            // Replace numeric variables
            for (const auto& [var, val] : numVars)
                segment = regex_replace(segment, regex("\\b" + var + "\\b"), formatNumber(val));

            // Detect undefined variables
            regex varRegex(R"(\b[a-zA-Z_]\w*\b)");
            sregex_iterator it(segment.begin(), segment.end(), varRegex);
            sregex_iterator end;
            for (; it != end; ++it) {
                string varName = it->str();
                if (!strVars.count(varName) && !numVars.count(varName)) {
                    cerr << "Undefined variable: " << varName << endl;
                    segment = regex_replace(segment, regex("\\b" + varName + "\\b"), "");
                }
            }

            result += segment;
            segment.clear();
        }
    }

    return result;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: catlang <filename>.cat or <filename>.catlang" << endl;
        return 1;
    }

    string filename = argv[1];
    if (!hasValidCatExtension(filename)) {
        cerr << "Error: Only .cat or .catlang files can be interpreted by CatLang" << endl;
        return 1;
    }

    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Could not open file: " << filename << endl;
        return 1;
    }



    unordered_map<string, string> strVars;
    unordered_map<string, double> numVars;

    string line;
    regex purrRegex(R"(^\s*purr\s*~>\s*(.+);\s*$)");
    regex strVarRegex(R"(^\s*str\s+([a-zA-Z_]\w*)\s*~>\s*(.+);\s*$)");
    regex numVarRegex(R"(^\s*num\s+([a-zA-Z_]\w*)\s*~>\s*(.+);\s*$)");
    smatch match;

    while (getline(file, line)) {
        // Remove single-line comments
        size_t commentPos = line.find("//");
        if (commentPos != string::npos)
            line = line.substr(0, commentPos);

        // Skip empty lines
        if (line.find_first_not_of(" \t\n\r") == string::npos)
            continue;

        // purr command
        if (regex_match(line, match, purrRegex)) {
            string expr = match[1];
            string replaced = replaceVars(expr, strVars, numVars);

            // Handle concatenation with +
            regex concatRegex(R"(\s*\+\s*)");
            sregex_token_iterator iter(replaced.begin(), replaced.end(), concatRegex, -1);
            sregex_token_iterator end;
            string output;

            for (; iter != end; ++iter) {
                string part = iter->str();
                if (part.size() >= 2 && part.front() == '"' && part.back() == '"')
                    part = part.substr(1, part.size() - 2);
                output += part;
            }

            cout << output << endl;
        }
        // string variable
        else if (regex_match(line, match, strVarRegex)) {
            string name = match[1];
            string val = match[2];
            if (!val.empty() && val.front() == '"' && val.back() == '"')
                val = val.substr(1, val.size() - 2);
            strVars[name] = val;
        }
        // numeric variable
        else if (regex_match(line, match, numVarRegex)) {
            string name = match[1];
            string val = match[2];
            try {
                numVars[name] = stod(val);
            } catch (...) {
                cerr << "Invalid numeric value for variable: " << name << endl;
            }
        }
        // unknown command
        else if (line.find_first_not_of(" \t\n\r") != string::npos) {
            cerr << "Unknown command: " << line << endl;
        }
    }

    return 0;
}
