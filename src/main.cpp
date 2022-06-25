#include <iostream>
#include <filesystem>
#include <fstream>
#include "json.hpp"

namespace fs = std::filesystem;

using namespace std;
using namespace nlohmann;

struct ResourceContent
{
    string _path;
    string _name;
    string _namespace;
};

struct Resource
{
    string _name;
    string _namespace;
    vector<string> _dependencies;
    vector<ResourceContent> _content;
    string _relativePath;
};

template<typename ... Args>
std::string string_format( const std::string& format, Args ... args )
{
    int size_s = std::snprintf( nullptr, 0, format.c_str(), args ... ) + 1; // Extra space for '\0'
    if( size_s <= 0 ){ throw std::runtime_error( "Error during formatting." ); }
    auto size = static_cast<size_t>( size_s );
    std::unique_ptr<char[]> buf( new char[ size ] );
    std::snprintf( buf.get(), size, format.c_str(), args ... );
    return std::string( buf.get(), buf.get() + size - 1 ); // We don't want the '\0' inside
}

void CleanupDirectory(fs::path path);
Resource LoadResourceDataFromJSON(fs::path jsonPath);
void ProcessResource(Resource& resource, bool isRootResource);
string ProcessFileContent(fs::path filePath, string name, string* header);
bool IsShaderFile(string extension);

fs::path resourceDirPath;
fs::path resourceOutDirPath;

int main(int argc, char* argv[])
{

    if (argc <= 1)
    {
        cerr << "ERROR: No valid arguments found. Please pass at least 1 argument to run the ResourceCompiler." << endl;
        return 1;
    }

    fs::path inputResourceJsonPath = fs::path(argv[1]);
    if (!inputResourceJsonPath.has_extension() ||
        (inputResourceJsonPath.extension() != ".json" && inputResourceJsonPath.filename().string().find(".rc") == string::npos))
    {
        cerr << "ERROR: The 1st argument doesn't have an extension: .rc.json" << endl;
        return 2;
    }

    fs::path currentPath = fs::current_path();
    resourceDirPath = inputResourceJsonPath.is_absolute()
                      ? inputResourceJsonPath.parent_path()
                      : (currentPath / inputResourceJsonPath.parent_path());

    fs::path outputDirPath = (argc > 2 ? fs::path(argv[2]) : resourceDirPath);

    resourceOutDirPath = outputDirPath.is_absolute() ? outputDirPath : (currentPath / outputDirPath);

    if (!is_directory(resourceDirPath))
    {
        cerr << "ERROR: No directory found at path " << resourceDirPath.string() << endl;
        return 3;
    }

    CleanupDirectory(resourceOutDirPath);

    Resource mainResource = LoadResourceDataFromJSON(inputResourceJsonPath);

    ProcessResource(mainResource, true);

    return 0;
}

bool IsShaderFile(string extension)
{
    return extension == ".hlsl" || extension == ".fx";
}

void CleanupDirectory(fs::path path)
{
    for (const auto & entry : fs::recursive_directory_iterator(path))
    {
        auto ext = entry.path().extension();
        if (ext == ".c" || ext == ".h" || ext == ".cpp" || ext == ".hpp")
        {
            fs::remove(entry);
        }
    }
}

void ProcessResource(Resource& resource, bool isRootResource)
{
    string dependencies = "";
    string internalDependencies = "";
    fs::path resourcePath = (resourceDirPath / resource._relativePath);
    fs::path resourceJsonDirectory = resourcePath.parent_path();
    fs::path outDirectory = (resourceOutDirPath / resource._relativePath).parent_path();

    for (auto &item: resource._dependencies)
    {
        dependencies += string_format("#include <%s>\n", item.c_str());
    }

    vector<Resource> subresources;
    string data;
    string headerData;
    string curNamespace = resource._namespace;

    for (const auto &item: resource._content)
    {
        auto itemPath = resourceJsonDirectory / item._path;
        if (itemPath == (resourceDirPath / resource._relativePath)) continue;
        if (itemPath.extension() == ".rc")
        {
            Resource subresource = LoadResourceDataFromJSON(itemPath);
            subresources.push_back(subresource);
            ProcessResource(subresource, false);
            internalDependencies += string_format("#include \"%s\"\n",
                                          fs::path(subresource._relativePath).parent_path().append(subresource._name + ".h").string().c_str());
        }
        else
        {
            string header;
            if (!item._namespace.empty())
            {
                header += "\n#if __cplusplus\n";
                header += "namespace " + item._namespace + " {\n";
                header += "#endif\n";
            }
            data += ProcessFileContent(itemPath, item._name, &header);
            if (!item._namespace.empty())
            {
                header += "\n#if __cplusplus\n";
                header += "};\n";
                header += "#endif\n";
            }
            headerData += header;
        }
    }

    ofstream outFile;
    outFile.open(outDirectory / (resource._name + ".c"), ios::out);

    outFile << dependencies;
    outFile << data;

    outFile.close();

    cout << "Out: " << (outDirectory / (resource._name + ".c")).string() << endl;

    outFile.open(outDirectory / (resource._name + ".h"), ios::out);

    outFile << "// Auto-generated resource file. Do not edit.\n\n#pragma once\n\n";
    outFile << dependencies;
    outFile << internalDependencies;
    if (!curNamespace.empty())
    {
        outFile << "#if __cplusplus\n";
        outFile << "namespace " << curNamespace << "\n{\n";
        outFile << "#endif\n";
    }
    outFile << headerData;
    if (!curNamespace.empty())
    {
        outFile << "#if __cplusplus\n";
        outFile << "};\n";
        outFile << "#endif\n";
    }

    outFile.close();

    cout << "Out: " << (outDirectory / (resource._name + ".h")).string() << endl;
}

string ProcessFileContent(fs::path sourceFile, string name, string* header)
{
    if (!fs::exists(sourceFile))
    {
        throw fs::filesystem_error("ERROR: No file found at address: " + sourceFile.string() + "\n For name " + name, error_code());
    }
    ifstream inFile;
    inFile.open(sourceFile, ios::binary);

    size_t length = fs::file_size(sourceFile);
    char *data = new char[length];
    inFile.read(data, length);
    inFile.close();

    ostringstream outString;
    ostringstream headerString;

    outString << "const char " << name << "[] = {\n";
    for (int i = 0; i < length; ++i)
    {
        outString << (unsigned int)data[i] << ",";
    }
    outString << "\n};\n";

    outString << "const size_t " << name << "_len = sizeof(" << name << ");\n";

    headerString << "\nextern \"C\" const char " << name << "[];\n";
    headerString << "extern \"C\" const size_t " << name << "_len;\n";
    *header += headerString.str();

    delete[] data;

    return outString.str();
}

string ReadAsciiFileToString(fs::path filePath)
{
    ifstream inFile;
    inFile.open(filePath);
    stringstream buffer;
    buffer << inFile.rdbuf();
    return buffer.str();
}

Resource LoadResourceDataFromJSON(fs::path jsonPath)
{
    if (jsonPath.extension() != ".json")
        jsonPath += fs::path(".json");
    cout << "Resource JSON: " << jsonPath.string() << endl;

    auto relativeSourcePath = fs::relative(jsonPath, resourceDirPath);
    auto targetPath = resourceOutDirPath / relativeSourcePath;
    auto targetPathParent = targetPath.parent_path();

    string jsonString = ReadAsciiFileToString(jsonPath);
    auto jsonObj = json::parse(jsonString);

    Resource resource;
    resource._relativePath = relativeSourcePath.string();
    resource._name = jsonObj["name"];
    resource._namespace = jsonObj["namespace"];
    resource._dependencies.clear();
    for (int i = 0; i < jsonObj["dependencies"].size(); ++i)
    {
        resource._dependencies.push_back(jsonObj["dependencies"][i]);
    }
    resource._content.clear();
    for (int i = 0; i < jsonObj["content"].size(); ++i)
    {
        auto obj = jsonObj["content"][i];
        resource._content.push_back({obj["path"],
                                     obj["name"],
                                     obj.contains("namespace") ? obj["namespace"] : ""
        });
    }
    return resource;
}


