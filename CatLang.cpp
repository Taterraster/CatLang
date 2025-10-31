// CatLang.cpp
#include <iostream>
#include <fstream>
#include <string>
#include <regex>
#include <unordered_map>
#include <sstream>
#include <variant>
#include <vector>
#include <functional>
#include <cctype>
#include <algorithm>
#include "function.hpp"
#include "statements.hpp"

using namespace std;

void executeLine(const string &line,
                 unordered_map<string, string> &strVars,
                 unordered_map<string, double> &numVars,
                 unordered_map<string, bool> &boolVars,
                 unordered_map<string, CatFunction> &functions)
{
    // This simply reuses your existing main loop logic for line execution.
    // For now, just re-run the logic that handles "purr", variables, and function calls.
    smatch match;
    regex purrRegex(R"(^\s*purr\s*~>\s*(.+);\s*$)");
    regex strVarRegex(R"(^\s*str\s+([a-zA-Z_]\w*)\s*~>\s*(.+);\s*$)");
    regex numVarRegex(R"(^\s*num\s+([a-zA-Z_]\w*)\s*~>\s*(.+);\s*$)");
    regex boolVarRegex(R"(^\s*bool\s+([a-zA-Z_]\w*)\s*~>\s*(true|false)\s*;\s*$)", regex_constants::icase);
    regex funcCallRegex(R"((\w+)\s*\((.*)\))");
    regex ifRegex(R"(^\s*if\s*\((.*)\)\s*\{\s*$)");
    regex elseRegex(R"(^\s*else\s*\{\s*$)");
    if (regex_match(line, match, purrRegex))
    {
        string expr = match[1];
        string replaced = replaceVars(expr, strVars, numVars, boolVars);

        regex concatRegex(R"(\s*\+\s*)");
        sregex_token_iterator iter(replaced.begin(), replaced.end(), concatRegex, -1);
        sregex_token_iterator end;
        string output;

        for (; iter != end; ++iter)
        {
            string part = iter->str();
            if (part == "endl")
                cout << output << endl, output.clear();
            else if (part.size() >= 2 && part.front() == '"' && part.back() == '"')
                output += part.substr(1, part.size() - 2);
            else
                output += part;
        }
        if (!output.empty())
            cout << output;
        return;
    }

    if (regex_match(line, match, strVarRegex))
    {
        string name = match[1];
        string val = match[2];
        if (!val.empty() && val.front() == '"' && val.back() == '"')
            val = val.substr(1, val.size() - 2);
        strVars[name] = val;
        return;
    }

    if (regex_match(line, match, numVarRegex))
    {
        string name = match[1];
        string val = match[2];
        try
        {
            numVars[name] = stod(val);
        }
        catch (...)
        {
            cerr << "Invalid numeric value: " << name << endl;
        }
        return;
    }

    if (regex_match(line, match, boolVarRegex))
    {
        string name = match[1];
        string val = match[2];
        boolVars[name] = (val == "true" || val == "TRUE");
        return;
    }

    if (regex_match(line, match, funcCallRegex))
    {
        string funcName = match[1];
        string args = match[2];

        if (!functions.count(funcName))
        {
            cerr << "Undefined function: " << funcName << endl;
            return;
        }

        const CatFunction &func = functions[funcName];
        vector<CatValue> argValues;
        stringstream ss(args);
        string arg;

        while (getline(ss, arg, ','))
        {
            arg.erase(0, arg.find_first_not_of(" \t"));
            arg.erase(arg.find_last_not_of(" \t") + 1);
            if (arg.empty())
                continue;
            if (arg.front() == '"' && arg.back() == '"')
                argValues.push_back(arg.substr(1, arg.size() - 2));
            else if (numVars.count(arg))
                argValues.push_back(numVars[arg]);
            else if (strVars.count(arg))
                argValues.push_back(strVars[arg]);
            else if (boolVars.count(arg))
                argValues.push_back(boolVars[arg]);
            else
                try
                {
                    argValues.push_back(stod(arg));
                }
                catch (...)
                {
                    argValues.push_back(arg == "true");
                }
        }

        executeFunction(func, argValues, strVars, numVars, boolVars);
        return;
    }

    cerr << "Unknown command: " << line << endl;
}

// helper to trim
static inline string trim(string s)
{
    s.erase(0, s.find_first_not_of(" \t\r\n"));
    s.erase(s.find_last_not_of(" \t\r\n") + 1);
    return s;
}

// file extension check
bool hasValidCatExtension(const string &filename)
{
    const string ext1 = ".cat";
    const string ext2 = ".catlang";
    return (filename.size() >= ext1.size() &&
            filename.compare(filename.size() - ext1.size(), ext1.size(), ext1) == 0) ||
           (filename.size() >= ext2.size() &&
            filename.compare(filename.size() - ext2.size(), ext2.size(), ext2) == 0);
}

// format number (trim trailing zeros)
string formatNumber(double num)
{
    ostringstream oss;
    oss << num;
    string s = oss.str();
    if (s.find('.') != string::npos)
    {
        while (!s.empty() && s.back() == '0')
            s.pop_back();
        if (!s.empty() && s.back() == '.')
            s.pop_back();
    }
    return s;
}

// replace variables outside quotes (keeps literals intact)
string replaceVars(const string &expr,
                   const unordered_map<string, string> &strVars,
                   const unordered_map<string, double> &numVars,
                   const unordered_map<string, bool> &boolVars)
{
    string result;
    bool inQuotes = false;
    string segment;

    for (size_t i = 0; i < expr.size(); ++i)
    {
        char c = expr[i];
        if (c == '"')
        {
            inQuotes = !inQuotes;
            result += c;
            continue;
        }
        if (inQuotes)
        {
            result += c;
            continue;
        }

        segment += c;

        // Process at end or at function call
        if ((!inQuotes && (i + 1 == expr.size() || expr[i + 1] == '"' || expr[i + 1] == '(')) && !segment.empty())
        {
            // Skip segments that are function calls
            if (segment.find('(') == string::npos)
            {
                // Replace only known variables
                for (const auto &[var, val] : strVars)
                    segment = regex_replace(segment, regex("\\b" + var + "\\b"), val);
                for (const auto &[var, val] : numVars)
                    segment = regex_replace(segment, regex("\\b" + var + "\\b"), formatNumber(val));
                for (const auto &[var, val] : boolVars)
                    segment = regex_replace(segment, regex("\\b" + var + "\\b"), val ? "true" : "false");
            }
            result += segment;
            segment.clear();
        }
    }

    return result;
}

// Evaluate numeric expression with parentheses and function calls.
// This function will:
//   - substitute known numeric variables
//   - find function calls (non-nested in this pass) and execute them via executeFunction()
//   - then parse expression with correct precedence (parentheses, *,/, +,-).
// NOTE: it uses parseFunctionArgs and executeFunction from function.hpp
double evalNumericExpression(string expr,
                             const unordered_map<string, double> &numVars,
                             const unordered_map<string, bool> &boolVars,
                             unordered_map<string, string> &strVars_ref, // needed for parseFunctionArgs
                             unordered_map<string, double> &numVars_ref,
                             unordered_map<string, bool> &boolVars_ref,
                             const unordered_map<string, CatFunction> &functions)
{
    // 1) replace simple variables (numbers & bools) with numeric literal text
    // but keep identifiers for function-call detection (we replace variables by number tokens)
    // We'll replace identifiers that match numVars or boolVars to their numeric string
    // but leave others (like function names) intact
    auto replaceSimpleVars = [&](string &s)
    {
        string out;
        for (size_t i = 0; i < s.size();)
        {
            if (isalpha((unsigned char)s[i]) || s[i] == '_')
            {
                size_t j = i;
                while (j < s.size() && (isalnum((unsigned char)s[j]) || s[j] == '_'))
                    ++j;
                string ident = s.substr(i, j - i);
                if (numVars.count(ident))
                    out += formatNumber(numVars.at(ident));
                else if (boolVars.count(ident))
                    out += (boolVars.at(ident) ? "1" : "0");
                else
                    out += ident; // maybe a function name or undefined (will be caught)
                i = j;
            }
            else
            {
                out.push_back(s[i++]);
            }
        }
        s.swap(out);
    };

    replaceSimpleVars(expr);

    // 2) Execute inner-most function calls iteratively:
    // pattern: name(arg1, arg2, ...)
    // We'll find calls with no nested parentheses inside the parentheses (i.e. handle simplest cases first).
    // For nested calls, repeated application will handle them.
    regex funcCallPattern(R"((\b[a-zA-Z_]\w*)\s*\((([^()]|(?R))*)\))"); // using PCRE-like recursion isn't supported; we'll instead use a simpler loop
    // Simpler approach: find leftmost '(' and find its matching ')' and check token before '(' for name.
    auto findNextFuncCall = [&](const string &s, size_t &startPos, size_t &endPos, string &fname, string &argsout) -> bool
    {
        // find '('
        size_t p = s.find('(', startPos);
        if (p == string::npos)
            return false;
        // find matching ')' by counting
        size_t j = p;
        int depth = 0;
        for (; j < s.size(); ++j)
        {
            if (s[j] == '(')
                ++depth;
            else if (s[j] == ')')
            {
                --depth;
                if (depth == 0)
                    break;
            }
        }
        if (j >= s.size())
            return false;
        // find function name before '(' (skip whitespace)
        size_t k = p;
        while (k > 0 && isspace((unsigned char)s[k - 1]))
            --k;
        size_t nameEnd = k;
        size_t nameStart = nameEnd;
        while (nameStart > 0 && (isalnum((unsigned char)s[nameStart - 1]) || s[nameStart - 1] == '_'))
            --nameStart;
        if (nameStart == nameEnd)
            return false;
        fname = s.substr(nameStart, nameEnd - nameStart);
        argsout = s.substr(p + 1, j - (p + 1));
        startPos = nameStart;
        endPos = j;
        return true;
    };

    // iterate until no function calls remain
    while (true)
    {
        size_t sp = 0;
        size_t ep = 0;
        string fname, argsstr;
        if (!findNextFuncCall(expr, sp, ep, fname, argsstr))
            break;

        // ensure function exists
        if (!functions.count(fname))
        {
            cerr << "Undefined function: " << fname << endl;
            // remove the call to avoid infinite loop: replace with 0
            expr.replace(sp, ep - sp + 1, "0");
            continue;
        }

        // parse args using helper in function.hpp
        vector<CatValue> parsedArgs = parseFunctionArgs(argsstr, strVars_ref, numVars_ref, boolVars_ref);

        // execute
        CatValue cres = executeFunction(functions.at(fname), parsedArgs, strVars_ref, numVars_ref, boolVars_ref);

        // convert result to numeric string (for insertion)
        string numericReplacement = "0";
        if (holds_alternative<double>(cres))
            numericReplacement = formatNumber(get<double>(cres));
        else if (holds_alternative<bool>(cres))
            numericReplacement = get<bool>(cres) ? "1" : "0";
        else if (holds_alternative<string>(cres))
        {
            // string in numeric context -> try parse number, else 0
            try
            {
                double v = stod(get<string>(cres));
                numericReplacement = formatNumber(v);
            }
            catch (...)
            {
                numericReplacement = "0";
            }
        }
        else
        {
            numericReplacement = "0";
        }

        // replace fname(...) range with numericReplacement
        // note: ep is ')' index; we replace from sp .. ep inclusive
        expr.replace(sp, ep - sp + 1, numericReplacement);
    }

    // 3) Now we have an expression with only numbers, operators, parentheses (hopefully). Evaluate it with precedence.

    // Shunting-yard or recursive descent parser. Implement recursive descent:
    const string s = expr;
    size_t pos = 0;
    function<void()> skipSpaces = [&]()
    { while (pos < s.size() && isspace((unsigned char)s[pos])) ++pos; };

    function<double()> parseExpr; // forward
    function<double()> parseTerm;
    function<double()> parseFactor;

    parseFactor = [&]() -> double
    {
        skipSpaces();
        if (pos < s.size() && s[pos] == '(')
        {
            ++pos;
            double v = parseExpr();
            skipSpaces();
            if (pos < s.size() && s[pos] == ')')
                ++pos;
            return v;
        }
        // parse number (with optional leading + or -)
        size_t start = pos;
        if (pos < s.size() && (s[pos] == '+' || s[pos] == '-'))
            ++pos;
        bool dotSeen = false;
        while (pos < s.size() && (isdigit((unsigned char)s[pos]) || s[pos] == '.'))
        {
            if (s[pos] == '.')
            {
                if (dotSeen)
                    break;
                dotSeen = true;
            }
            ++pos;
        }
        string numtok = s.substr(start, pos - start);
        if (numtok.empty() || numtok == "+" || numtok == "-")
        {
            // invalid - return 0
            return 0.0;
        }
        try
        {
            return stod(numtok);
        }
        catch (...)
        {
            return 0.0;
        }
    };

    parseTerm = [&]() -> double
    {
        double val = parseFactor();
        while (true)
        {
            skipSpaces();
            if (pos >= s.size())
                break;
            char op = s[pos];
            if (op != '*' && op != '/')
                break;
            ++pos;
            double rhs = parseFactor();
            if (op == '*')
                val *= rhs;
            else if (op == '/')
                val /= rhs;
        }
        return val;
    };

    parseExpr = [&]() -> double
    {
        double val = parseTerm();
        while (true)
        {
            skipSpaces();
            if (pos >= s.size())
                break;
            char op = s[pos];
            if (op != '+' && op != '-')
                break;
            ++pos;
            double rhs = parseTerm();
            if (op == '+')
                val += rhs;
            else
                val -= rhs;
        }
        return val;
    };

    try
    {
        pos = 0;
        double res = parseExpr();
        return res;
    }
    catch (...)
    {
        cerr << "Invalid numeric expression: " << expr << endl;
        return 0.0;
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        cerr << "Usage: catlang <file>.cat" << endl;
        return 1;
    }
    string filename = argv[1];
    if (!hasValidCatExtension(filename))
    {
        cerr << "Only .cat or .catlang files allowed" << endl;
        return 1;
    }

    ifstream file(filename);
    if (!file.is_open())
    {
        cerr << "Could not open file: " << filename << endl;
        return 1;
    }

    unordered_map<string, string> strVars;
    unordered_map<string, double> numVars;
    unordered_map<string, bool> boolVars;
    unordered_map<string, CatFunction> functions;

    string line;
    string outputLineBuffer; // buffer for purr concatenation across purr statements

    // regexes
    regex purrRegex(R"(^\s*purr\s*~>\s*(.*?)\s*;\s*$)");
    regex strVarRegex(R"(^\s*str\s+([a-zA-Z_]\w*)\s*~>\s*(.+);\s*$)");
    regex numVarRegex(R"(^\s*num\s+([a-zA-Z_]\w*)\s*~>\s*(.+);\s*$)");
    regex boolVarRegex(R"(^\s*bool\s+([a-zA-Z_]\w*)\s*~>\s*(true|false)\s*;\s*$)", regex_constants::icase);
    regex funcDefRegex(R"(^\s*(num|str|bool|void)\s+([a-zA-Z_]\w*)\s*\((.*)\)\s*\{\s*$)");
    regex funcCallRegex(R"((\w+)\(([^)]*)\))");
    regex assignFuncCallRegex(R"(^\s*(num|str|bool)\s+([a-zA-Z_]\w*)\s*~>\s*([a-zA-Z_]\w*\(.*\))\s*;\s*$)");
    regex funcCallOnlyRegex(R"(^\s*([a-zA-Z_]\w*)\s*\((.*)\)\s*;\s*$)");
    regex funcRegex(R"(^\s*(num|str|bool|void)\s+([a-zA-Z_]\w*)\s*\(([^)]*)\)\s*\{\s*$)");
    regex ifRegex(R"(^\s*if\s*\((.+)\)\s*\{\s*$)");
    regex elseRegex(R"(^\s*(?:\}\s*)?else\s*\{\s*$)");
    bool inMultilineComment = false;
    string lineBuffer;
    while (getline(file, line))
    {
        // handle multi-line comment continuations
        if (inMultilineComment)
        {
            size_t endc = line.find("*/");
            if (endc != string::npos)
            {
                line = line.substr(endc + 2);
                inMultilineComment = false;
            }
            else
                continue;
        }

        // strip start of multiline comment on this line
        size_t startc = line.find("/*");
        if (startc != string::npos)
        {
            size_t endc = line.find("*/", startc + 2);
            if (endc != string::npos)
            {
                // comment open and close same line: remove the segment
                line.erase(startc, endc - startc + 2);
            }
            else
            {
                line = line.substr(0, startc);
                inMultilineComment = true;
            }
        }

        // single-line comments
        size_t singlec = line.find("//");
        if (singlec != string::npos)
            line = line.substr(0, singlec);

        // skip empty
        if (line.find_first_not_of(" \t\r\n") == string::npos)
            continue;

        smatch match;

        // 1) function definition (must be handled before other patterns)
        if (regex_match(line, match, funcDefRegex))
        {
            string returnType = match[1];
            string fname = match[2];
            string argsList = match[3];

            // parse args
            vector<FuncArg> args;
            stringstream ss(argsList);
            string a;
            while (getline(ss, a, ','))
            {
                a = trim(a);
                if (a.empty())
                    continue;
                stringstream as(a);
                string t, n;
                as >> t >> n;
                if (!t.empty() && !n.empty())
                    args.push_back({t, n});
            }

            // collect body with brace counting
            vector<string> body;
            int braceCount = 1; // already saw opening '{' in the header line
            while (getline(file, line))
            {
                // adjust counts for nested braces
                for (char ch : line)
                {
                    if (ch == '{')
                        ++braceCount;
                    else if (ch == '}')
                        --braceCount;
                }
                body.push_back(line);
                if (braceCount == 0)
                    break;
            }

            // store (note: body includes the closing '}' line; executeFunction should handle lines/returns)
            functions[fname] = CatFunction{returnType, args, body};
            continue;
        }

        // 2) assignment from function call like: num x ~> add(a,b);
        if (regex_match(line, match, assignFuncCallRegex))
        {
            string varType = match[1];
            string varName = match[2];
            string funcCall = match[3];

            // extract function name and arg list
            smatch callm;
            regex callRegex(R"(^\s*([a-zA-Z_]\w*)\s*\((.*)\)\s*$)");
            if (!regex_match(funcCall, callm, callRegex))
            {
                cerr << "Invalid function call in assignment: " << funcCall << endl;
                continue;
            }
            string fname = callm[1];
            string argList = callm[2];

            if (!functions.count(fname))
            {
                cerr << "Undefined function: " << fname << endl;
                continue;
            }

            vector<CatValue> parsedArgs = parseFunctionArgs(argList, strVars, numVars, boolVars);
            CatValue cres = executeFunction(functions.at(fname), parsedArgs, strVars, numVars, boolVars);

            // assign according to varType
            if (varType == "num")
            {
                if (holds_alternative<double>(cres))
                    numVars[varName] = get<double>(cres);
                else if (holds_alternative<bool>(cres))
                    numVars[varName] = get<bool>(cres) ? 1.0 : 0.0;
                else
                {
                    cerr << "Type mismatch: expected num from function " << fname << endl;
                }
            }
            else if (varType == "str")
            {
                if (holds_alternative<string>(cres))
                    strVars[varName] = get<string>(cres);
                else
                {
                    cerr << "Type mismatch: expected str from function " << fname << endl;
                }
            }
            else if (varType == "bool")
            {
                if (holds_alternative<bool>(cres))
                    boolVars[varName] = get<bool>(cres);
                else
                {
                    cerr << "Type mismatch: expected bool from function " << fname << endl;
                }
            }
            continue;
        }

        // --- purr command ---
        if (regex_match(line, match, purrRegex))
        {
            string expr = match[1];

            // Check if it's a function call like hello(thing)
            regex funcCallOnlyRegex(R"((\w+)\((.*)\))");
            smatch funcMatch;
            string output;

            if (regex_match(expr, funcMatch, funcCallOnlyRegex))
            {
                string funcName = funcMatch[1];
                string argList = funcMatch[2];

                if (!functions.count(funcName))
                {
                    cerr << "Undefined function: " << funcName << endl;
                }
                else
                {
                    vector<CatValue> argValues = parseFunctionArgs(argList, strVars, numVars, boolVars);
                    CatValue result = executeFunction(functions[funcName], argValues, strVars, numVars, boolVars);

                    // Convert result to string for printing
                    if (holds_alternative<string>(result))
                        output = get<string>(result);
                    else if (holds_alternative<double>(result))
                        output = formatNumber(get<double>(result));
                    else if (holds_alternative<bool>(result))
                        output = get<bool>(result) ? "true" : "false";
                }
            }
            else
            {
                // Handle concatenation with '+'
                regex concatRegex(R"(\s*\+\s*)");
                sregex_token_iterator iter(expr.begin(), expr.end(), concatRegex, -1);
                sregex_token_iterator end;

                for (; iter != end; ++iter)
                {
                    string part = iter->str();
                    part.erase(0, part.find_first_not_of(" \t"));
                    part.erase(part.find_last_not_of(" \t") + 1);

                    if (part == "endl")
                    {
                        output += "\n";
                    }
                    else if (!part.empty() && part.front() == '"' && part.back() == '"')
                    {
                        output += part.substr(1, part.size() - 2); // strip quotes
                    }
                    else if (strVars.count(part))
                    {
                        output += strVars[part];
                    }
                    else if (numVars.count(part))
                    {
                        output += formatNumber(numVars[part]);
                    }
                    else if (boolVars.count(part))
                    {
                        output += boolVars[part] ? "true" : "false";
                    }
                    else
                    {
                        output += part; // fallback
                    }
                }
            }

            cout << output;
            continue;
        }

        // 4) variable declarations that may include expressions or function calls
        if (regex_match(line, match, numVarRegex))
        {
            string varName = match[1];
            string expr = match[2];
            try
            {
                double value = evaluateNumericExpression(expr, numVars); // new function
                numVars[varName] = value;
            }
            catch (...)
            {
                cerr << "Invalid numeric value: " << varName << endl;
            }
            continue;
        }

        if (regex_match(line, match, strVarRegex))
        {
            string varName = match[1];
            string rhs = trim(match[2]);
            // if rhs is a function call, handle it
            smatch fm;
            regex callRx(R"(^\s*([a-zA-Z_]\w*)\s*\((.*)\)\s*$)");
            if (regex_match(rhs, fm, callRx))
            {
                string fname = fm[1];
                string argList = fm[2];
                if (!functions.count(fname))
                {
                    cerr << "Undefined function: " << fname << endl;
                    continue;
                }
                vector<CatValue> parsedArgs = parseFunctionArgs(argList, strVars, numVars, boolVars);
                CatValue cres = executeFunction(functions.at(fname), parsedArgs, strVars, numVars, boolVars);
                if (holds_alternative<string>(cres))
                    strVars[varName] = get<string>(cres);
                else
                    cerr << "Type mismatch: expected str from function " << fname << endl;
            }
            else
            {
                // literal string expected
                if (!rhs.empty() && rhs.front() == '"' && rhs.back() == '"')
                    strVars[varName] = rhs.substr(1, rhs.size() - 2);
                else
                {
                    // maybe variable name
                    if (strVars.count(rhs))
                        strVars[varName] = strVars[rhs];
                    else
                    {
                        cerr << "Invalid string assignment: " << rhs << endl;
                    }
                }
            }
            continue;
        }

        if (regex_match(line, match, boolVarRegex))
        {
            string varName = match[1];
            string rhs = trim(match[2]);
            // rhs could be function call
            smatch fm;
            regex callRx(R"(^\s*([a-zA-Z_]\w*)\s*\((.*)\)\s*$)");
            if (regex_match(rhs, fm, callRx))
            {
                string fname = fm[1];
                string argList = fm[2];
                if (!functions.count(fname))
                {
                    cerr << "Undefined function: " << fname << endl;
                    continue;
                }
                vector<CatValue> parsedArgs = parseFunctionArgs(argList, strVars, numVars, boolVars);
                CatValue cres = executeFunction(functions.at(fname), parsedArgs, strVars, numVars, boolVars);
                if (holds_alternative<bool>(cres))
                    boolVars[varName] = get<bool>(cres);
                else
                    cerr << "Type mismatch: expected bool from function " << fname << endl;
            }
            else
            {
                boolVars[varName] = (rhs == "true" || rhs == "TRUE");
            }
            continue;
        }

        // 5) standalone function call with semicolon, e.g., sayGoodbye(username);
        if (regex_match(line, match, funcCallOnlyRegex))
        {
            string fname = match[1];
            string argList = match[2];
            if (!functions.count(fname))
            {
                cerr << "Undefined function: " << fname << endl;
                continue;
            }
            vector<CatValue> parsedArgs = parseFunctionArgs(argList, strVars, numVars, boolVars);
            executeFunction(functions.at(fname), parsedArgs, strVars, numVars, boolVars);
            continue;
        }
        // --- Function definitions ---
        if (regex_match(line, match, funcRegex))
        {
            string returnType = match[1];
            string funcName = match[2];
            string argsList = match[3];

            vector<FuncArg> args;
            stringstream ss(argsList);
            string arg;
            while (getline(ss, arg, ','))
            {
                stringstream argStream(arg);
                string type, name;
                argStream >> type >> name;
                if (!type.empty() && !name.empty())
                {
                    args.push_back({type, name});
                }
            }

            vector<string> funcBody;
            int braceCount = 1; // already found one '{'

            // Read the function body until closing '}'
            while (getline(file, line))
            {
                size_t openBraces = count(line.begin(), line.end(), '{');
                size_t closeBraces = count(line.begin(), line.end(), '}');
                braceCount += openBraces;
                braceCount -= closeBraces;

                funcBody.push_back(line);
                if (braceCount == 0)
                    break;
            }

            // Register the function
            CatFunction func;
            func.returnType = returnType;
            func.args = args;
            func.body = funcBody;
            functions[funcName] = func;

            continue; // go to next line after the function
        }
        // --- If statements ---

        if (regex_match(line, match, ifRegex))
        {
            string conditionExpr = match[1];

            // Gather true block
            vector<string> trueBlock;
            int braceDepth = 1;
            while (getline(file, line))
            {
                if (line.find('{') != string::npos)
                    braceDepth++;
                if (line.find('}') != string::npos)
                    braceDepth--;
                if (braceDepth == 0)
                    break;
                trueBlock.push_back(line);
            }

            // Check if next line is else
            streampos prevPos = file.tellg();
            string nextLine;
            vector<string> falseBlock;
            if (getline(file, nextLine) && regex_match(nextLine, elseRegex))
            {
                braceDepth = 1;
                while (getline(file, line))
                {
                    if (line.find('{') != string::npos)
                        braceDepth++;
                    if (line.find('}') != string::npos)
                        braceDepth--;
                    if (braceDepth == 0)
                        break;
                    falseBlock.push_back(line);
                }
            }
            else
            {
                file.seekg(prevPos);
            }

            bool condResult = evaluateCondition(conditionExpr, strVars, numVars, boolVars);
            executeIfStatement(condResult, trueBlock, falseBlock, strVars, numVars, boolVars, functions);
            continue;
        }
        if (regex_match(line, match, ifRegex))
        {
            std::string condExpr = match[1];
            bool condResult = evaluateCondition(condExpr, strVars, numVars, boolVars);

            std::vector<std::string> trueBlock;
            std::vector<std::string> falseBlock;

            int braceCount = 1; // already consumed opening {

            bool readingTrue = true;
            while (getline(file, line))
            {
                braceCount += std::count(line.begin(), line.end(), '{');
                braceCount -= std::count(line.begin(), line.end(), '}');

                if (regex_match(line, elseRegex) && braceCount == 1)
                {
                    readingTrue = false;
                    continue; // skip the `else {` line
                }

                if (readingTrue)
                    trueBlock.push_back(line);
                else
                    falseBlock.push_back(line);

                if (braceCount == 0)
                    break; // finished both blocks
            }

            executeIfStatement(condResult, trueBlock, falseBlock, strVars, numVars, boolVars, functions);
            continue; // do not fall through to unknown command
        }
        // 6) unknown command
        cerr << "Unknown command: " << line << endl;
    }

    // flush any pending purr buffer
    if (!outputLineBuffer.empty())
    {
        cout << outputLineBuffer << endl;
        outputLineBuffer.clear();
    }

    return 0;
}
