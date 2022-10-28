/*!
 * \file astral_gl_generator/main.cpp
 * \brief file astral_gl_generator/main.cpp
 *
 * Copyright 2022 by InvisionApp.
 *
 * Author: Itai Danan
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 */

// OpenGL Header Generator

#include <algorithm>
#include <cctype>
#include <exception>
#include <fstream>
#include <ios>
#include <iostream>
#include <sstream>
#include <map>
#include <regex>
#include <set>
#include <string>
#include <sstream>
#include <utility>
#include <vector>

#define PUGIXML_HEADER_ONLY
#include "pugixml.hpp"

using namespace std;

// CONFIGURATION START
//
// Constants in this section control the subset of GL/GLES generated and output naming.

// Prefixes used to name Astral declarations.
const char* const ASTRAL_ENUM_PREFIX = "ASTRAL_";
const char* const ASTRAL_TYPE_PREFIX = "astral_";

// Set supported VENDORS here for Native and WASM. Only enums matching one of these vendors are processed.
const set<string> VENDORS[] = { { "ARB" }, { "ARB"} };

const map<string,regex> API_VERSIONS_NATIVE = { { "gl", regex(".*") }, { "gles2", regex("2\\.[0-9]|3\\.[012]") } };
const map<string,regex> API_VERSIONS_WASM = { { "gles2", regex("2\\.[0-9]|3\\.0") } };

const set<string> API_EXTENSIONS_NATIVE = { "glcore", "glCoreARBPat", "gles2" };
const set<string> API_EXTENSIONS_WASM = { };

// Template to construct output macros. Its a sequence of strins with placeholders:
// %F => official function name only EX: glCreateShader
// %R => Astral return type          EX: ASTRAL_GLuint
// %P => Astral procedure macro      EX: ASTRAL_PFNGLCREATESHADERPROC
// %A => Astral function arguments   EX: ASTRAL_GLenum arg0
// %U => Astral unused arguments     EX: ASTRAL_GLenum
// %N => Argument name strings       EX: const char* argName0
// %M => Macro arguments             EX: arg0
// %H => Stringified macro arguments EX: "#arg0"
// %# => Hashed macro arguments      EX: #arg0
// %, => Comma plus space if not void function.
// %O => Streamed args and values.   EX: << argName0 << "=0x" << arg0
const char* const MACRO_TEMPLATE_NATIVE[] =
    {
        "// Define ", "%F", "\n",
        "typedef ", "%R", "(ASTRAL_GL_APIENTRY* ", "%P", ")(", "%A", ");\n",
        "namespace astral\n{\nnamespace gl_binding\n{\n",
        "extern ", "%P", " function_ptr_", "%F", ";\n",
        "bool exists_function_", "%F", "(void);\n",
        "%P", " get_function_ptr_", "%F", "(void);\n",
        "#ifdef ASTRAL_GL_DEBUG\n",
        "%R", " debug_function_", "%F", "(", "%A", "%,", "const char* file, int line, const char* call", "%,", "%N", ");\n",
        "#define astral_", "%F", "(", "%M", ") astral::gl_binding::debug_function_", "%F",
        "(", "%M", "%,", "__FILE__, __LINE__, \"", "%F", "(", "%H", ")\"", "%,", "%#", ")\n",
        "#else\n",
        "#define astral_", "%F", "(", "%M", ") astral::gl_binding::function_ptr_", "%F", "(", "%M", ")\n",
        "#endif\n",
        "}\n}\n",
        0
    };

const char* const MACRO_TEMPLATE_WASM[] =
    {
        "namespace astral\n{\nnamespace gl_binding\n{\n",
        "// Define ", "%F", "\n",
        "#ifdef ASTRAL_GL_DEBUG\n",
        "%R", " debug_function_", "%F", "(", "%A", "%,", "const char* file, int line, const char* call", "%,", "%N", ");\n",
        "#define astral_", "%F", "(", "%M", ") astral::gl_binding::debug_function_", "%F",
        "(", "%M", "%,", "__FILE__, __LINE__, \"", "%F", "(", "%H", ")\"", "%,", "%#", ")\n",
        "#else\n",
        "#define astral_", "%F", " ", "%F", "\n",
        "#endif\n",
        "}\n}\n",
        0
    };

const char* const DEFINITION_TEMPLATE_NATIVE[] =
    {
        "ASTRAL_GLAPI ", "%R", " ASTRAL_GL_APIENTRY local_function_", "%F", "(", "%A", ");\n",
        "%P", " function_ptr_", "%F", " = local_function_", "%F", ";\n\n",
        "%P", " get_function_ptr_", "%F", "(void);\n",
        "ASTRAL_GLAPI ", "%R", " ASTRAL_GL_APIENTRY local_function_", "%F", "(", "%A", ")\n",
        "{\n",
        "    get_function_ptr_", "%F", "();\n",
        "    return function_ptr_", "%F", "(", "%M", ");\n",
        "}\n\n",
        "ASTRAL_GLAPI ", "%R", " ASTRAL_GL_APIENTRY do_nothing_function_", "%F", "(", "%U", ")\n",
        "{\n",
        "    call_unloadable_function(\"", "%F", "\");\n",
        "    return empty_return_value<", "%R", ">();\n",
        "}\n\n",
        "%P", " get_function_ptr_", "%F", "(void)\n",
        "{\n",
        "    if (function_ptr_", "%F", " == local_function_", "%F", ")\n",
        "    {\n",
        "        function_ptr_", "%F", " = (", "%P", ")get_proc(\"", "%F", "\");\n",
        "        if (function_ptr_", "%F", " == nullptr)\n",
        "        {\n",
        "            on_load_function_error(\"", "%F", "\");\n",
        "            function_ptr_", "%F", " = do_nothing_function_", "%F", ";\n",
        "        }\n",
        "    }\n",
        "    return function_ptr_", "%F", ";\n",
        "}\n\n",
        "bool exists_function_", "%F", "(void)\n",
        "{\n",
        "    return get_function_ptr_", "%F", "() != do_nothing_function_", "%F", ";\n",
        "}\n\n",
        "#ifdef ASTRAL_GL_DEBUG\n",
        "%R", " debug_function_", "%F", "(", "%A", "%,", "const char* file, int line, const char* call", "%,", "%N", ")\n",
        "{\n",
        "    std::ostringstream call_stream;\n",
        "    call_stream << std::hex << \"", "%F", "(\" << ", "%O", " << \")\"", ";\n",
        "    std::string call_string = call_stream.str();\n",
        "    return debug_invoke(type_tag<", "%R", ">(), call_string.c_str(), file, line, call, \"", "%F", "\", function_ptr_", "%F", "%,", "%M", ");\n",
        "}\n",
        "#endif\n",
        0
    };

const char* const DEFINITION_TEMPLATE_WASM[] =
    {
        "#ifdef ASTRAL_GL_DEBUG\n",
        "%R", " debug_function_", "%F", "(", "%A", "%,", "const char* file, int line, const char* call", "%,", "%N", ")\n",
        "{\n",
        "    std::ostringstream call_stream;\n",
        "    call_stream << std::hex << \"", "%F", "(\" << ", "%O", " << \")\"", ";\n",
        "    std::string call_string = call_stream.str();\n",
        "    return debug_invoke(type_tag<", "%R", ">(), call_string.c_str(), file, line, call, \"", "%F", "\", ", "%F", "%,", "%M", ");\n",
        "}\n",
        "#endif\n",
        0
    };

const char* const LOAD_TEMPLATE_NATIVE[] =
    {
        "    function_ptr_", "%F", " = (", "%P", ")get_proc(\"", "%F", "\");\n",
        "    if (function_ptr_", "%F", " == nullptr)\n",
        "    {\n",
		"        function_ptr_", "%F", " = do_nothing_function_", "%F", ";\n",
		"        if (warning)\n",
        "            on_load_function_error(\"", "%F", "\");\n",
        "    }\n",
        0
    };

// CONFIGURATION END

// Returns true when s starts with the given prefix.
bool starts_with(const string& s, const string& prefix);

// Returns true when s ends with the given suffix.
bool ends_with(const string& s, const string& suffix);

// Returns true if te given arg could start a filename.
bool possible_file_arg(const char* a);

// Replace header extension (.h or .hpp) with the given one, for cases where only one file arg is given.
string replace_extension(const char* filename, const char* extension);

// Return uppercase version of string.
string uppercase(string s);

// Return string without whitepsace at start or end.
string trim(const string& s);

// Extracts the text inside a node that is not part of any subelement.
string extract_text(const pugi::xml_node& node);

// Extract the type referenced in declarations.
string extract_type(const pugi::xml_node& n);

// Extracts the C type from a string.
string extract_type(const string& s);

// Renames a typedef with ASTRAL_ prefix.
string rename_type(const string& s);

// Return the given string with every declaration renamed with ASTRAL_ prefix.
string rename_types(const string& s);

// Renames a typedef declaration to use the ASTRAL_ prefix for the defined type
// and all referenced types for function declarations.
string rename_typedef(bool WIN64, const pair<string,string>& td);

// Renames a declaration with ASTRAL_ prefix.
string rename_param(const string& s);

// Parse a <type> element into an optinal name (for typedef) and the type itself.
pair<string, string> parse_type(const pugi::xml_node& e);

// Returns true if versions supports the given api and number.
bool supported(const map<string,regex>& versions, const std::string& api, const std::string& number);

// Returns true if versions supports the supported XML attribute..
bool supported(const set<string>& extensions, const std::string& supported);

// Stores function declaration components.
struct Function
{
    Function(const string& n ) : name(n) {}

    static Function parse(const pugi::xml_node& e, const string& name);

    string name;
    string result;
    vector<pair<string,string>> params;
};

// For placing in container. Overkill since functions are unlikey to be overloaded but safer.
bool operator<(const Function& f, const Function& g)
{
    if (f.name < g.name)
        return true;

    if (f.name > g.name)
        return false;

    if (f.result != g.result)
        return f.result < g.result;

    if (f.params.size() != g.params.size())
        return f.params.size() < g.params.size();

    for (size_t i = 0; i < f.params.size(); ++i)
    {
        if (f.params[i].first != g.params[i].first)
        {
            return f.params[i].first < g.params[i].first;
        }
    }

    return false;
}

// Parse <command> element defining a function.
Function Function::parse(const pugi::xml_node& command, const string& name)
{
    Function func(name);

    const pugi::xml_node& proto = command.child("proto");
    func.result = extract_type(proto);

    for (pugi::xml_node param = command.child("param"); param; param = param.next_sibling("param"))
    {
        func.params.emplace_back(extract_type(param), param.child_value("name"));
    }

    return func;
}

// Generates the Astral version of the function declaration.
ostream& operator<<(ostream& os, const Function& func)
{
    os << rename_type(func.result) << " " << ASTRAL_TYPE_PREFIX << func.name << "(";

    const auto& last = func.params.back();
    for (const auto& param : func.params)
    {
        os << rename_type(param.first) << " " << param.second;
        if (&param != &last)
        {
            os << ", ";
        }
    }
    os << ");";
    return os;
}

string format_template(const Function& func, const char* const* output_template)
{
    ostringstream output;
    for (const char* const* p = output_template; *p; ++p)
    {
        const char* const segment = *p;
        if (segment[0] == '%')
        {
            switch (segment[1])
            {
            case 'F':
                output << func.name;
                break;

            case 'R':
                output << rename_type(func.result);
                break;

            case 'P':
                output << ASTRAL_ENUM_PREFIX << "PFN" << uppercase(func.name) << "PROC";
                break;

            case 'A':
                for (size_t i = 0; i < func.params.size(); ++i)
                {
                    if (i)
                    {
                        output << ", ";
                    }
                    output << rename_type(func.params[i].first) << " arg" << i;
                }
                break;

            case 'U':
                for (size_t i = 0; i < func.params.size(); ++i)
                {
                    if (i)
                    {
                        output << ", ";
                    }
                    output << rename_type(func.params[i].first);
                }
                break;

            case 'N':
                for (size_t i = 0; i < func.params.size(); ++i)
                {
                    if (i)
                    {
                        output << ", ";
                    }
                    output << "const char* argName" << i;
                }
                break;

            case 'M':
                for (size_t i = 0; i < func.params.size(); ++i)
                {
                    if (i)
                    {
                        output << ", ";
                    }
                    output << "arg" << i;
                }
                break;

            case 'H':
                for (size_t i = 0; i < func.params.size(); ++i)
                {
                    if (i)
                    {
                        output << ", ";
                    }
                    output << "\"#arg" << i << "\"";
                }
                break;

            case '#':
                for (size_t i = 0; i < func.params.size(); ++i)
                {
                    if (i)
                    {
                        output << ", ";
                    }
                    output << "#arg" << i;
                }
                break;

            case ',':
                if (func.params.size())
                {
                    output << ", ";
                }
                break;

            case 'O':
                if (func.params.empty())
                {
                    output << "\"\"";
                }
                else
                {
                    for (size_t i = 0; i < func.params.size(); ++i)
                    {
                        if (i)
                        {
                            output << " << \", \" << ";
                        }

                        if (func.params[i].first.back() == '*')
                        {
                            output << "argName" << i << " << \" = \" << (const void*)arg" << i;
                        }
                        else
                        {
                            output << "argName" << i << " << \" = 0x\" << arg" << i;
                        }
                    }
                }
                break;
            };
        }
        else
        {
            output << segment;
        }
    }

    return output.str();
}

set<string>& select_set(const string& name, set<string>& type_set, set<string>& enum_set, set<string>& command_set);

bool find_toggle(int argc, char* argv[], const char* arg)
{
    if (argc < 3)
        return false;

    char** toggle = std::find_if(argv + 3, argv + argc, [arg](const char* a) { return !strcmp(a, arg); });
    return toggle[0] != nullptr;
}

const char* find_option(int argc, char* argv[], const char* arg)
{
    if (argc < 4)
        return nullptr;

    char** toggle = std::find_if(argv + 3, argv + argc - 1, [arg](const char* a) { return !strcmp(a, arg); });
    return toggle[1];
}

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        cout << "Usage: hgen <SOURCE.XML> <TARGET.H> <TARGET.CPP> [-templates <TEMPLATE_DIR>] [-list-native <FILE1.TXT>] [-list-wasm <FILE2.TXT>]\n";
        cout << "       <SOURCE.XML>      Specifies the XML describing all GL and GLES API.\n";
        cout << "       <TARGET.H>        Specifies header to write.\n";
        cout << "       <TARGET.CPP>      Specifies implementation to write.\n";
        cout << "       <TEMPLATE_DIR>    Specifies the location of template files.\n";
        cout << "       -list-native      Outputs selected Native entities to <FILE1.TXT>.\n";
        cout << "       -list-wasm        Outputs selected WASM entities to <FILE2.TXT>.\n";
        return 0;
    }

    try
    {
        ifstream input(argv[1]);
        if (!input)
        {
            cerr << "ERROR: Unable to read " << argv[1] << "\n";
            return -1;
        }

        pugi::xml_document doc;
        pugi::xml_parse_result result = doc.load(input);

        if (!result)
        {
            cerr << "ERROR: Failed to parse " << argv[1] << "\n";
            return -1;
        }

        string header_name = argv[2];
        ofstream header(header_name);
        if (!header)
        {
            cerr << "ERROR: Unable to write " << argv[2] << "\n";
            return -1;
        }

        string filename = argc > 3 && possible_file_arg(argv[3]) ? string(argv[3]) : replace_extension(argv[2], "cpp");
        ofstream code(filename);
        if (!code)
        {
            cerr << "ERROR: Unable to write " << filename << "\n";
            return -1;
        }

        string templates = "ngl_generator/templates/";
        if (const char* t = find_option(argc, argv, "-templates"))
        {
            templates = t;

            if (templates.back() != '/')
                templates.push_back('/');
        }

        string preamble = templates + "pre.hpp";
        ifstream pre(preamble);
        if (!pre)
        {
            cerr << "ERROR: Preamble file is missing: " << preamble << "\n";
            return -1;
        }

        string postamble = templates + "post.hpp";
        ifstream post(postamble);
        if (!post)
        {
            cerr << "ERROR: Postamble file is missing: " << postamble << "\n";
            return -1;
        }

        string head_code = templates + "head.cpp";
        ifstream head(head_code);
        if (!head)
        {
            cerr << "ERROR: Head file is missing: " << head_code << "\n";
            return -1;
        }

        string preamble_code = templates + "pre.cpp";
        ifstream pre_code(preamble_code);
        if (!pre_code)
        {
            cerr << "ERROR: Preamble file is missing: " << preamble_code << "\n";
            return -1;
        }

        string postamble_code = templates + "post.cpp";
        ifstream post_code(postamble_code);
        if (!post_code)
        {
            cerr << "ERROR: Postamble file is missing: " << postamble_code << "\n";
            return -1;
        }

        cout << "Generating Unified GL Header\n";

        // Global Element.
        pugi::xml_node registry = doc.child("registry");

        // Beging Phase I
        //
        // The first phase is to collect all enums, types and commands that are revevant to
        // the desired subset of GL and GLES. A set of each is build and then in phase 2 we
        // only output those corresponding elements that match.
        //
        // Set array below have the first element for Native and the second for WASM.
        constexpr size_t NATIVE_INDEX = 0;
        constexpr size_t WASM_INDEX = 1;
        set<string> type_set[2];
        set<string> enum_set[2];
        set<string> command_set[2];

        // Each <feature> element lists the elements belonging to a particular GL/GLES version.
        for (pugi::xml_node feature = registry.child("feature"); feature; feature = feature.next_sibling("feature"))
        {
            string api = feature.attribute("api").value();
            string number = feature.attribute("number").value();

            bool native = supported(API_VERSIONS_NATIVE, api, number);
            bool wasm = supported(API_VERSIONS_WASM, api, number);

            for (pugi::xml_node require : feature)
            {
                if (strcmp(require.attribute("profile").value(), "compatibility") == 0)
                    continue;

                bool remove = strcmp(require.name(), "remove") == 0;

                for (pugi::xml_node n : require)
                {
                    set<string>& native_set = select_set(n.name(), type_set[NATIVE_INDEX], enum_set[NATIVE_INDEX], command_set[NATIVE_INDEX]);;
                    set<string>& wasm_set = select_set(n.name(), type_set[WASM_INDEX], enum_set[WASM_INDEX], command_set[WASM_INDEX]);;

                    if (native)
                    {
                        if (remove)
                        {
                            native_set.erase(n.attribute("name").value());
                        }
                        else
                        {
                            native_set.insert(n.attribute("name").value());
                        }
                    }
                    if (wasm)
                    {
                        if (remove)
                        {
                            wasm_set.erase(n.attribute("name").value());
                        }
                        else
                        {
                            wasm_set.insert(n.attribute("name").value());
                        }
                    }
                }
            }
        }

        pugi::xml_node extensions = registry.child("extensions");
        for (pugi::xml_node extension = extensions.child("extension"); extension; extension = extension.next_sibling("extension"))
        {
            string name = extension.attribute("name").value();
            string support = extension.attribute("supported").value();

            bool native = supported(API_EXTENSIONS_NATIVE, support);
            bool wasm = supported(API_EXTENSIONS_WASM, support);

            for (pugi::xml_node require : extension)
            {
                if (strcmp(require.attribute("profile").value(), "compatibility") == 0)
                    continue;

                for (pugi::xml_node n : require)
                {
                    set<string>& native_set = select_set(n.name(), type_set[NATIVE_INDEX], enum_set[NATIVE_INDEX], command_set[NATIVE_INDEX]);;
                    set<string>& wasm_set = select_set(n.name(), type_set[WASM_INDEX], enum_set[WASM_INDEX], command_set[WASM_INDEX]);;

                    if (native)
                    {
                        native_set.insert(n.attribute("name").value());
                    }
                    if (wasm)
                    {
                        wasm_set.insert(n.attribute("name").value());
                    }
                }
            }
        }

        const char* list[2] = { find_option(argc, argv, "-list-native"), find_option(argc, argv, "-list-wasm") };
        for (unsigned int target = 0; target < 2; ++target)
        {
            if (list[target])
            {
                ofstream file(list[target]);
                if (file)
                {
                    for (const string& s : type_set[target])
                    {
                        file << s << "\n";
                    }

                    for (const string& s : enum_set[target])
                    {
                        file << s << "\n";
                    }

                    for (const string& s : command_set[target])
                    {
                        file << s << "\n";
                    }
                }
            }
        }

        // Begin Phase II

        // Write initial file blocks.
        header << pre.rdbuf() << endl;
        header << "#ifndef __EMSCRIPTEN__\n\n";
        header << "// Native Declarations\n";

        code << head.rdbuf() << endl;
        code << pre_code.rdbuf() << endl;
        code << "#ifndef __EMSCRIPTEN__\n\n";
        code << "// Native Definitions\n";

        for (unsigned int target = 0; target < 2; ++target)
        {
            // All Types defined within a single <types> element.
            pugi::xml_node types = registry.child("types");
            for (const pugi::xml_node& type : types)
            {
                // Types with name are not types. Note this is the attribute name. Subelement <name> is expected and converted.
                if (type.attribute("name"))
                    continue;

                try
                {
                    pair<string, string> td = parse_type(type);

                    if (td.second.empty())
                    {
                        // No alias, hence a forward declaration. Cop as-is.
                        header << td.first << ";\n";
                    }
                    else
                    {
                        // Substitute type dependencies. This occurs exactly once: typedef GLintptr GLvdpauSurfaceNV.
                        if (type.attribute("requires"))
                        {
                            size_t pos = td.first.find(type.attribute("requires").value());
                            if (pos != string::npos)
                            {
                                td.first = td.first.substr(0, pos) + ASTRAL_TYPE_PREFIX + td.first.substr(pos);
                            }
                        }

                        header << "#if defined(_WIN64)\n"
                               << rename_typedef(true, td) << ";\n"
                               << "#else\n"
                               << rename_typedef(false, td) << ";\n"
                               << "#endif\n";

                        code << "#if defined(_WIN64)\n"
                             << rename_typedef(true, td) << ";\n"
                             << "#else\n"
                             << rename_typedef(false, td) << ";\n"
                             << "#endif\n";
                    }
                }
                catch (const domain_error& e)
                {
                    cerr << e.what() << "\n";
                }
            }

            header << endl;

            // Enum values are defined by a number of <enums> element.
            for (pugi::xml_node enums = registry.child("enums"); enums; enums = enums.next_sibling("enums"))
            {
                string ns = enums.attribute("namespace").value();
                if (ns != "GL")
                    continue;

                for (pugi::xml_node e = enums.child("enum"); e; e = e.next_sibling("enum"))
                {
                    string name = e.attribute("name").value();
                    string value = e.attribute("value").value();

                    if (name.empty() || value.empty())
                    {
                        cerr << "Enum missing attributes.\n";
                        continue;
                    }

                    if (name == "ASTRAL_GL_ACTIVE_PROGRAM_EXT" && strcmp(e.attribute("api").value(), "gl") == 0)
                    {
                        // IMPORTANT: ASTRAL_GL_ACTIVE_PROGRAM_EXT exists for both gl and gles2 with different values.
                        // This condition discards the gl version (0x8B8D) so that gles2 one (0x8259) gets used.
                        continue;
                    }

                    if (enum_set[target].find(name) == enum_set[target].end())
                    {
                        continue;
                    }
                    enum_set[target].erase(name);

                    header << "#define " << ASTRAL_ENUM_PREFIX << name << " " << value << "\n";
                }
            }

            header << endl;

            // Set of functions to load after generating definitions. Does not apply to WASM.
            set<Function> load_set;

            // Functions are grouped under a single <commands> element.
            pugi::xml_node commands = registry.child("commands");
            for (const pugi::xml_node& command : commands)
            {
                string name = command.child("proto").child_value("name");

                if (command_set[target].find(name) == command_set[target].end())
                {
                    continue;
                }
                command_set[target].erase(name);

                Function func = Function::parse(command, name);
                // header << func << "\n";

                header << format_template(func, target == WASM_INDEX ? MACRO_TEMPLATE_WASM : MACRO_TEMPLATE_NATIVE) << "\n";
                code << format_template(func, target == WASM_INDEX ? DEFINITION_TEMPLATE_WASM : DEFINITION_TEMPLATE_NATIVE) << "\n";

                if (target == NATIVE_INDEX)
                {
                    load_set.insert(func);
                }
            }

            if (target == NATIVE_INDEX)
            {
                header << "#else\n\n";
                header << "// WASM Declarations\n";

                code << "void load_all_functions(bool warning)\n";
                code << "{\n";
                for (const Function& f : load_set)
                {
                    code << format_template(f, LOAD_TEMPLATE_NATIVE) << "\n";
                }
                code << "}\n";

                code << "\n#else\n\n";
                code << "// WASM Definitions\n";
            }
        }

        header << "\n#endif // Emscriptem close.\n\n";

        // WASM Dummy function to pretend loading functions.
        code << "void load_all_functions(bool warning)\n";
        code << "{\n";
        code << "    (void)warning;\n";
        code << "}\n";

        code << "\n#endif // Emscriptem close.\n\n";

        // Write final file blocks.
        header << post.rdbuf() << endl;
        code << post_code.rdbuf() << endl;

        for (unsigned int target = 0; target < 2; ++target)
        {
            for (const string& s : enum_set[target])
            {
                cerr << "WARNING: Definition not encountred for enum " << s << (target ? " (WASM)" : " (Native)" ) << "\n";
            }
            for (const string& s : command_set[target])
            {
                cerr << "WARNING: Definition not encountred for command " << s << "\n";
            }
        }

    }
    catch (const exception& e)
    {
        cerr << "ERROR: Exception caught " << e.what() << "\n";
        return -2;
    }
    catch (...)
    {
        cerr << "ERROR: Unknown exception caught.\n";
        return -2;
    }

    return 0;
}

bool starts_with(const string& s, const string& prefix)
{
    return s.compare(0, prefix.size(), prefix) == 0;
}

bool ends_with(const string& s, const string& suffix)
{
    return s.size() > suffix.size() &&  s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

string uppercase(string s)
{
    for (char& c : s)
    {
        if (isalpha(c))
        {
            c = toupper(c);
        }
    }
    return s;
}

string trim(const string& s)
{
    string::const_iterator start = s.begin();
    while (start != s.end() && isspace(*start))
        ++start;

    string::const_iterator limit = s.end();
    while (limit != start && isspace(*(limit-1)))
        --limit;

    return string(start, limit);
}

string extract_type(const string& s)
{
    static const string prefix = "typedef";

    if (!starts_with(s, prefix))
    {
        throw domain_error("Only typedef supported for types.");
    }

    size_t off = prefix.size();
    while (isspace(s[off]))
        ++off;

    size_t limit = s.size() - 1;
    while (limit && (s[limit] == ';' || isspace(s[limit])))
        --limit;

    return s.substr(off, limit - off + 1);
}

string extract_text(const pugi::xml_node& node)
{
    string output;
    for (const pugi::xml_node& child : node.children())
    {
        if (child.type() == pugi::node_pcdata)
            output += child.value();
    }
    return output;
}

string rename_type(const string& s)
{
    size_t space = s.rfind(' ');
    if (space == string::npos)
    {
        if (starts_with(trim(s), "void"))
            return s;

        return ASTRAL_TYPE_PREFIX + s;
    }

    // Structures with underscore are not renamed with Astral prefix.
    if (starts_with(s, "struct _"))
    {
        return s;
    }

    string type = s.substr(space+1);
    if (starts_with(type, "void"))
    {
        return s;
    }

    return s.substr(0, space+1) + ASTRAL_TYPE_PREFIX + type;
}

string rename_types(const string& s)
{
    string output;
    size_t start = 0;
    size_t limit = 0;

    while (limit != string::npos)
    {
        limit = s.find_first_of(",)", start);
        if (limit == string::npos)
            break;

        string param = s.substr(start, limit - start);
        output += rename_param(param) + s[limit];

        if (s[limit] == ',')
            output += ' ';

        start = limit + 1;
    }

    return output;
}

string strip_khronos(bool WIN64, const string& s)
{
    static const string KHRONOS = "khronos_";
    if (starts_with(s, KHRONOS))
    {
        string t = s.substr(KHRONOS.size());
        if (t == "float_t")
            return "float";

        if (WIN64)
          {
            if (t == "ssize_t")
              return "signed long long int";

            if (t == "usize_t")
              return "unsigned long long int";
          }
        else
          {
            if (t == "ssize_t")
              return "signed long int";

            if (t == "usize_t")
              return "unsigned long int";
          }

        return t;
    }

    return s;
}

string clean_type(const string& input)
{
    string output;
    for (size_t i = 0; i < input.size(); ++i)
    {
        if (input[i] == '*' || input[i] == '&' || input[i] == ' ')
        {
            if (output.size() && output.back() == ' ')
            {
                output.pop_back();
            }
        }

        output.push_back(input[i]);
    }

    return output;
}

string rename_typedef(bool WIN64, const pair<string,string>& td)
{
    // Only function typedef have parenthesis.
    size_t paren = td.first.find('(');

    if (paren == string::npos)
    {
        return "typedef " + clean_type(strip_khronos(WIN64, td.first)) + " " + ASTRAL_TYPE_PREFIX + td.second;
    }

    paren = td.first.find(')', paren);
    if (paren == string::npos)
    {
        throw domain_error("Malformed function typedef missing closing parenthesis.");
    }

    std::string declare = "typedef " + clean_type(td.first.substr(0, paren)) + ASTRAL_TYPE_PREFIX + td.second;

    size_t args = td.first.find('(', paren);
    if (paren == string::npos)
    {
        throw domain_error("Malformed function typedef missing parameter list.");
    }

    declare += td.first.substr(paren, args - paren + 1);

    paren = td.first.rfind(')');
    if (paren == string::npos)
    {
        throw domain_error("Malformed function typedef missing closing parenthesis.");
    }

    string params = td.first.substr(args + 1, paren);
    declare += rename_types(params);

    return declare;
}

string rename_param(const string& s)
{
    string trimmed = trim(s);
    if (trimmed.empty())
        return trimmed; // Only blank space, so the declaration is an implicit void.

    // Qualified void.
    if (starts_with(trimmed, "void"))
        return trimmed;

    size_t space = trimmed.rfind(' ');
    if (space == string::npos)
        return ASTRAL_TYPE_PREFIX + trimmed; // Unnamed parameter without storage class.

    size_t start = trimmed.rfind(' ', space - 1);
    if (start == string::npos)
        return ASTRAL_TYPE_PREFIX + trimmed; // Simple type followed by parameter.

    if (starts_with(trimmed.substr(start + 1, space), "void"))
        return trimmed;

    return trimmed.substr(0, start + 1) + ASTRAL_TYPE_PREFIX + trimmed.substr(start + 1);
}

pair<string, string> parse_type(const pugi::xml_node& e)
{
    static const string prefix = "struct";

    string name = e.child_value("name");

    // When the <name> element starts with 'struct', it is type to be forward declared and it is not typedefed.
    if (starts_with(name, prefix))
        return { name, "" };

    // When <name> is not a type, then its the name to 'typedef' to and the type is the remaining text data.
    string type = extract_type(extract_text(e));
    return { type, name };
}

void extract_type_helper(std::ostream& dst, pugi::xml_node n)
{
    /* This space is needed when hanlding const qualifiers
     * that come before the ptype element, without it there
     * would be no space between const and the actual type.
     */
    dst << " " << trim(n.value());
    for (pugi::xml_node i = n.first_child(); i; i = i.next_sibling())
    {
        extract_type_helper(dst, i);
    }
}

string extract_type(const pugi::xml_node& n)
{
  std::string t;

  pugi::xml_node ptype = n.child("ptype");
  if (ptype)
  {
      std::ostringstream r;

      /* This is necesary because gl.xml from Khronos often places the '*' for
       * pointer types as an unnamed child element, for example
       *
       * <command>
       *     <proto>void <name>glDeleteTextures</name></proto>
       *     <param><ptype>GLsizei</ptype> <name>n</name></param>
       *     <param class="texture" group="Texture" len="n">const <ptype>GLuint</ptype> *<name>textures</name></param>
       *     <glx type="single" opcode="144"/>
       * </command>
       *
       * note that the * for textures is not inside of the ptype child, but just somewhere in
       * the middle unnamed; in addition note that the const comes BEFORE the ptype label.
       *
       * Return types also have this funkiness
       * <command>
       *     <proto group="String">const <ptype>GLubyte</ptype> *<name>glGetString</name></proto>
       *     <param group="StringName"><ptype>GLenum</ptype> <name>name</name></param>
       *     <glx type="single" opcode="129"/>
       * </command>
       *
       * so we need to get all the siblings before the ptype too, so we take the first child,
       * to start at, not ptype.
       */
      for (pugi::xml_node i = n.first_child(); i && std::string(i.name()) != "name"; i = i.next_sibling())
      {
          extract_type_helper(r, i);
      }
      r << ptype.value();

      t = trim(r.str());
  }
  else
  {
      t = trim(n.child_value());
  }

  return clean_type(t);
}

bool supported(const map<string,regex>& versions, const std::string& api, const std::string& number)
{
    map<string,regex>::const_iterator api_versions = versions.find(api);
    return api_versions != versions.end() && regex_match(number, api_versions->second);
}

bool supported(const set<string>& extensions, const std::string& support)
{
    return find_if(extensions.begin(), extensions.end(), [&support](const string& s) { return support.find(s) != string::npos; }) != extensions.end();
}

set<string>& select_set(const string& name, set<string>& type_set, set<string>& enum_set, set<string>& command_set)
{
    if (name == "type")
        return type_set;

    if (name == "enum")
        return enum_set;

    if (name == "command")
        return command_set;

    throw domain_error("Unexpected XML Entity.");
}

bool possible_file_arg(const char* a)
{
    return a[0] == '/' || a[0] == '.' || isalnum(a[0]);
}

string replace_extension(const char* filename, const char* extension)
{
    string output = filename;

    if (ends_with(output, ".h") || ends_with(output, ".hpp"))
    {
        size_t pos = output.rfind('.') + 1;
        return output.replace(pos, output.size() - pos, extension);
    }

    return output + "." + extension;
}
