#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <regex>
#include <sstream>
#include <iostream>
#include <stdexcept>
// Forward declaration (replaceVars is defined in CatLang.cpp)
std::string replaceVars(
    const std::string &expr,
    const std::unordered_map<std::string, std::string> &strVars,
    const std::unordered_map<std::string, double> &numVars,
    const std::unordered_map<std::string, bool> &boolVars);

// --- Variant type for function return value ---
using CatValue = std::variant<std::monostate, std::string, double, bool>;
// --- Function argument representation ---
struct FuncArg
{
    std::string type; // "num", "str", "bool"
    std::string name;
};

// --- CatLang function representation ---
struct CatFunction
{
    std::string returnType;        // "num", "str", "bool", "void"
    std::vector<FuncArg> args;     // Function arguments
    std::vector<std::string> body; // Lines inside { }
};

// --- Parse a function from script lines ---
// lines: vector of all script lines
// index: current line index (will be updated to closing '}')
CatFunction parseFunction(const std::vector<std::string> &lines, size_t &index)
{
    CatFunction func;

    std::regex funcHeader(R"(^\s*(num|str|bool|void)\s+([a-zA-Z_]\w*)\s*\((.*)\)\s*\{\s*$)");
    std::smatch match;

    if (!std::regex_match(lines[index], match, funcHeader))
    {
        throw std::runtime_error("Invalid function definition: " + lines[index]);
    }

    func.returnType = match[1];
    std::string funcName = match[2];
    std::string argsList = match[3];

    // Parse arguments
    std::stringstream ss(argsList);
    std::string arg;
    while (std::getline(ss, arg, ','))
    {
        std::stringstream argStream(arg);
        std::string type, name;
        argStream >> type >> name;
        if (!type.empty() && !name.empty())
        {
            func.args.push_back({type, name});
        }
    }

    // Parse body until closing '}'
    index++; // move to first line of body
    while (index < lines.size() && lines[index].find('}') == std::string::npos)
    {
        func.body.push_back(lines[index]);
        index++;
    }

    if (index >= lines.size())
    {
        throw std::runtime_error("Function missing closing }");
    }

    // index now points to closing '}'
    return func;
}

// --- Execute a function ---
// strVars, numVars, boolVars: global variable maps
CatValue executeFunction(
    const CatFunction &func,
    const std::vector<CatValue> &args,
    std::unordered_map<std::string, std::string> &strVars,
    std::unordered_map<std::string, double> &numVars,
    std::unordered_map<std::string, bool> &boolVars)
{
    // Create local scope copies
    auto localStrVars = strVars;
    auto localNumVars = numVars;
    auto localBoolVars = boolVars;

    // Assign arguments to local scope
    for (size_t i = 0; i < func.args.size() && i < args.size(); ++i)
    {
        const auto &[type, name] = func.args[i];
        const CatValue &val = args[i];

        if (type == "str" && std::holds_alternative<std::string>(val))
            localStrVars[name] = std::get<std::string>(val);
        else if (type == "num" && std::holds_alternative<double>(val))
            localNumVars[name] = std::get<double>(val);
        else if (type == "bool" && std::holds_alternative<bool>(val))
            localBoolVars[name] = std::get<bool>(val);
    }

    CatValue returnValue = std::monostate{};

    // Execute function body line by line
    for (std::string line : func.body)
    {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);

        if (line.empty())
            continue;

        // --- Handle return ---
        if (line.rfind("return ", 0) == 0)
        {
            std::string retExpr = line.substr(7);
            if (func.returnType == "str")
            {
                if (localStrVars.count(retExpr))
                    returnValue = localStrVars[retExpr];
                else if (retExpr.front() == '"' && retExpr.back() == '"')
                    returnValue = retExpr.substr(1, retExpr.size() - 2);
            }
            else if (func.returnType == "num")
            {
                if (localNumVars.count(retExpr))
                    returnValue = localNumVars[retExpr];
                else
                    try
                    {
                        returnValue = stod(retExpr);
                    }
                    catch (...)
                    {
                    }
            }
            else if (func.returnType == "bool")
            {
                if (localBoolVars.count(retExpr))
                    returnValue = localBoolVars[retExpr];
                else if (retExpr == "true" || retExpr == "false")
                    returnValue = (retExpr == "true");
            }
            break;
        }

        // --- Handle purr output inside functions ---
        std::smatch match;
        std::regex purrRegex(R"(^\s*purr\s*~>\s*(.+);\s*$)");
        if (std::regex_match(line, match, purrRegex))
        {
            std::string expr = match[1];
            std::string replaced = replaceVars(expr, localStrVars, localNumVars, localBoolVars);

            std::regex concatRegex(R"(\s*\+\s*)");
            std::sregex_token_iterator iter(replaced.begin(), replaced.end(), concatRegex, -1);
            std::sregex_token_iterator end;
            std::string output;

            for (; iter != end; ++iter)
            {
                std::string part = iter->str();
                if (part == "endl")
                {
                    std::cout << output << std::endl;
                    output.clear();
                }
                else if (part.size() >= 2 && part.front() == '"' && part.back() == '"')
                {
                    part = part.substr(1, part.size() - 2);
                    output += part;
                }
                else
                {
                    output += part;
                }
            }
            if (!output.empty())
                std::cout << output;
            continue;
        }

        // Could add num/str/bool handling inside functions later
    }

    return returnValue;
}

std::unordered_map<std::string, CatFunction> functions;
inline std::vector<CatValue> parseFunctionArgs(
    const std::string &argList,
    const std::unordered_map<std::string, std::string> &strVars,
    const std::unordered_map<std::string, double> &numVars,
    const std::unordered_map<std::string, bool> &boolVars)
{
    std::vector<CatValue> argValues;
    std::stringstream ss(argList);
    std::string arg;

    while (std::getline(ss, arg, ','))
    {
        // Trim whitespace
        arg.erase(0, arg.find_first_not_of(" \t"));
        arg.erase(arg.find_last_not_of(" \t") + 1);

        if (arg.empty())
            continue;

        // Literal string
        if (arg.front() == '"' && arg.back() == '"')
        {
            argValues.push_back(arg.substr(1, arg.size() - 2));
        }
        // Literal boolean
        else if (arg == "true" || arg == "TRUE")
        {
            argValues.push_back(true);
        }
        else if (arg == "false" || arg == "FALSE")
        {
            argValues.push_back(false);
        }
        // Numeric literal
        else
        {
            try
            {
                argValues.push_back(std::stod(arg));
            }
            catch (...)
            {
                // Check if it's a variable
                if (strVars.count(arg))
                    argValues.push_back(strVars.at(arg));
                else if (numVars.count(arg))
                    argValues.push_back(numVars.at(arg));
                else if (boolVars.count(arg))
                    argValues.push_back(boolVars.at(arg));
                else
                    argValues.push_back(std::monostate{}); // undefined
            }
        }
    }

    return argValues;
}
double evaluateNumericExpression(const std::string &expr, const std::unordered_map<std::string, double> &numVars)
{
    std::string replaced = expr;
    // Replace variable names with their values
    for (const auto &[var, val] : numVars)
        replaced = std::regex_replace(replaced, std::regex("\\b" + var + "\\b"), std::to_string(val));

    // Use std::stringstream to parse simple expressions (very basic, left-to-right, no operator precedence)
    std::stringstream ss(replaced);
    double result = 0;
    double num;
    char op = '+';
    while (ss >> num)
    {
        if (op == '+')
            result += num;
        else if (op == '-')
            result -= num;
        else if (op == '*')
            result *= num;
        else if (op == '/')
            result /= num;
        ss >> op; // read next operator
    }
    return result;
}
