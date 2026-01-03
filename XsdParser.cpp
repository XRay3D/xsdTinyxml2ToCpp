#include "XsdParser.h"
#include <filesystem>
#include <format>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

namespace Xsd {

using std ::println;

constexpr auto testName(std::string_view src, std::string_view dst) -> bool {
    return src == dst || src == dst.substr(3);
};

Parser::~Parser() { clear(); }

bool Parser::parse(const string& filename) {
    clear();

    // Загружаем XML документ
    tinyxml2::XMLError error = doc_.LoadFile(filename.c_str());
    if(error != tinyxml2::XML_SUCCESS) {
        println(std::cerr, "Ошибка загрузки файла: {}", filename);
        println(std::cerr, "Код ошибки: {}", doc_.ErrorStr());
        return false;
    }

    // Получаем корневой элемент
    const tinyxml2::XMLElement* root = doc_.FirstChildElement("xs:schema");
    if(!root) {
        root = doc_.FirstChildElement("schema");
    }

    if(!root) {
        println(std::cerr, "Не найден корневой элемент schema");
        return false;
    }

    // Парсим схему
    parseSchema(root);

    std::cout << "Парсинг завершен успешно!" << std::endl;
    std::cout << "Найдено перечислений: " << enums.size() << std::endl;
    std::cout << "Найдено complexType: " << complexTypes.size() << std::endl;
    std::cout << "Найдено элементов: " << elements.size() << std::endl;

    return true;
}

bool Parser::generateCppCode(const string& outputDir,
    const string& namespaceName) {
    // Создаем директорию, если не существует
    if(!fs::exists(outputDir)) {
        if(!fs::create_directories(outputDir)) {
            println(std::cerr, "Не удалось создать директорию: {}", outputDir);
            return false;
        }
    }

    // Генерируем заголовочный файл с перечислениями
    std::ofstream enumHeader(outputDir + "/Enums.h");
    if(!enumHeader.is_open()) {
        println(std::cerr, "Не удалось создать файл: {}/Enums.h", outputDir);
        return false;
    }

    // Заголовок файла с перечислениями
    println(enumHeader, "#pragma once\n");
    println(enumHeader, "#include <string>");
    println(enumHeader, "#include <map>");
    println(enumHeader, "#include <stdexcept>\n");

    if(!namespaceName.empty()) {
        println(enumHeader, "namespace {} {{\n", namespaceName);
    }

    println(enumHeader, "template <typename E>concept Enum=std::is_enum_v<E>;\n");
    println(enumHeader, "template <Enum E>");
    println(enumHeader, "E stringTo(const std::string& str);\n");
    // println(enumHeader, "template <Enum E>");
    // println(enumHeader, "std::string toString(E value);\n");

    for(const auto& enumType: enums) {
        enumHeader << enumType.generateHeaderCode();
    }

    if(!namespaceName.empty()) {
        println(enumHeader, "}} // namespace {}", namespaceName);
    }

    enumHeader.close();

    // Генерируем исходный файл с перечислениями
    std::ofstream enumSource(outputDir + "/Enums.cpp");
    if(!enumSource.is_open()) {
        println(std::cerr, "Не удалось создать файл: {}/Enums.cpp", outputDir);
        return false;
    }

    enumSource << "#include \"Enums.h\"\n";
    enumSource << "#include <algorithm>\n\n";

    if(!namespaceName.empty()) {
        enumSource << "namespace " << namespaceName << " {\n\n";
    }

    for(const auto& enumType: enums) {
        enumSource << enumType.generateSourceCode();
    }

    if(!namespaceName.empty()) {
        enumSource << "} // namespace " << namespaceName << "\n";
    }

    enumSource.close();

    // Генерируем заголовочный файл со структурами
    std::ofstream structHeader(outputDir + "/Types.h");
    if(!structHeader.is_open()) {
        println(std::cerr, "Не удалось создать файл: {}/Types.h", outputDir);
        return false;
    }

    println(structHeader, "#pragma once\n");
    println(structHeader, "#include <string>");
    println(structHeader, "#include <vector>");
    println(structHeader, "#include <optional>");
    println(structHeader, "#include <stdexcept>");
    println(structHeader, "#include \"tinyxml2.h\"");
    println(structHeader, "#include \"Enums.h\"\n");

    if(!namespaceName.empty()) {
        println(structHeader, "namespace {} {{\n", namespaceName);
    }

    for(const auto& complexType: complexTypes) {
        structHeader << complexType.generateHeaderCode();
    }

    if(!namespaceName.empty()) {
        println(structHeader, "}} // namespace {}", namespaceName);
    }

    structHeader.close();

    if(0) { // Генерируем исходный файл со структурами
        std::ofstream structSource(outputDir + "/Types.cpp");
        if(!structSource.is_open()) {
            println(std::cerr, "Не удалось создать файл: {}/Types.cpp", outputDir);
            return false;
        }

        println(structSource, "#include \"Types.h\"");
        println(structSource, "#include <sstream>\n");

        if(!namespaceName.empty()) {
            println(structSource, "namespace {} {{\n", namespaceName);
        }

        for(const auto& complexType: complexTypes) {
            structSource << complexType.generateSourceCode();
        }

        if(!namespaceName.empty()) {
            println(structSource, "}} // namespace ", namespaceName);
        }

        structSource.close();
    }

    // Генерируем CMakeLists.txt для удобства
    std::ofstream cmakeFile(outputDir + "/CMakeLists.txt");
    if(cmakeFile.is_open()) {
        println(cmakeFile, "cmake_minimum_required(VERSION 3.10)");
        println(cmakeFile, "project(Generated)\n");
        println(cmakeFile, "set(CMAKE_CXX_STANDARD 20)\n");
        println(cmakeFile, "# Находим tinyxml2");
        println(cmakeFile, "find_package(tinyxml2 REQUIRED)\n");
        println(cmakeFile, "# Создаем библиотеку");
        println(cmakeFile, "add_library(xsd_generated");
        println(cmakeFile, "    Enums.cpp");
        println(cmakeFile, "    Types.cpp");
        println(cmakeFile, ")\n");
        println(cmakeFile, "target_include_directories(xsd_generated");
        println(cmakeFile, "    PUBLIC");
        println(cmakeFile, "        ${{CMAKE_CURRENT_SOURCE_DIR}}");
        println(cmakeFile, ")\n");
        println(cmakeFile, "target_link_libraries(xsd_generated");
        println(cmakeFile, "    PUBLIC");
        println(cmakeFile, "        tinyxml2::tinyxml2");
        println(cmakeFile, ")");
        cmakeFile.close();
    }

    std::cout << "Код успешно сгенерирован в директории: " << outputDir << std::endl;
    return true;
}

void Parser::clear() {
    enums.clear();
    complexTypes.clear();
    elements.clear();
    doc_.Clear();
}
#if 0
void Parser::parseComplexType(const tinyxml2::XMLElement* element) {
    ComplexType complexType;

    // Получаем имя типа
    const char* name = element->Attribute("name");
    if(!name) {
        println(std::cerr, "ComplexType без имени, пропускаем");
        return;
    }
    complexType.name = sanitizeName(name);

    // Получаем документацию
    complexType.documentation = getDocumentation(element);

    // Проверяем, является ли абстрактным
    const char* abstract = element->Attribute("abstract");
    if(abstract && abstract=="true"sv) {
        complexType.isAbstract = true;
    }

    // Ищем sequence, choice, all или complexContent/simpleContent
    const tinyxml2::XMLElement* sequence = element->FirstChildElement("xs:sequence");
    if(!sequence) sequence = element->FirstChildElement("sequence");

    const tinyxml2::XMLElement* choice = element->FirstChildElement("xs:choice");
    if(!choice) choice = element->FirstChildElement("choice");

    const tinyxml2::XMLElement* all = element->FirstChildElement("xs:all");
    if(!all) all = element->FirstChildElement("all");

    const tinyxml2::XMLElement* complexContent = element->FirstChildElement("xs:complexContent");
    if(!complexContent) complexContent = element->FirstChildElement("complexContent");

    const tinyxml2::XMLElement* simpleContent = element->FirstChildElement("xs:simpleContent");
    if(!simpleContent) simpleContent = element->FirstChildElement("simpleContent");

    // Обработка атрибутов
    const tinyxml2::XMLElement* attributeGroup = element->FirstChildElement("xs:attributeGroup");
    if(!attributeGroup) attributeGroup = element->FirstChildElement("attributeGroup");

    // Парсим элементы в sequence
    if(sequence) {
        parseSequenceElements(sequence, complexType);
    }

    // Парсим элементы в choice
    if(choice) {
        parseChoiceElements(choice, complexType);
    }

    // Парсим элементы в all
    if(all) {
        parseAllElements(all, complexType);
    }

    // Парсим атрибуты
    parseAttributes(element, complexType);

    complexTypes_.push_back(complexType);
}

void Parser::parseSequenceElements(const tinyxml2::XMLElement* sequence, ComplexType& complexType) {
    for(const tinyxml2::XMLElement* child = sequence->FirstChildElement();
        child != nullptr;
        child = child->NextSiblingElement()) {

        const char* childName = child->Name();

        if(testName(childName, "xs:element"sv)) {

            Field field;
            field.isAttribute = false;

            // Имя элемента
            const char* name = child->Attribute("name");
            if(name) {
                field.name = sanitizeName(name);
            }

            // Тип элемента
            const char* type = child->Attribute("type");
            if(type) {
                field.type = convertXsdTypeToCpp(type);
            }

            // Документация
            field.documentation = getDocumentation(child);

            // Min/max occurs
            const char* minOccurs = child->Attribute("minOccurs");
            if(minOccurs) {
                field.minOccurs = atoi(minOccurs);
                field.isOptional = (field.minOccurs == 0);
            }

            const char* maxOccurs = child->Attribute("maxOccurs");
            if(maxOccurs) {
                if(maxOccurs=="unbounded"sv) {
                    field.maxOccurs = -1; // unbounded
                } else {
                    field.maxOccurs = atoi(maxOccurs);
                }
            }

            complexType.fields.push_back(field);
        }
    }
}

void Parser::parseAttributes(const tinyxml2::XMLElement* element,
    ComplexType& complexType) {
    for(const tinyxml2::XMLElement* child = element->FirstChildElement();
        child != nullptr;
        child = child->NextSiblingElement()) {

        const char* childName = child->Name();

        if(testName(childName, "xs:attribute"sv)) {

            Field field;
            field.isAttribute = true;

            // Имя атрибута
            const char* name = child->Attribute("name");
            if(name) {
                field.name = sanitizeName(name);
            }

            // Тип атрибута
            const char* type = child->Attribute("type");
            if(type) {
                field.type = convertXsdTypeToCpp(type);
            }

            // Обязательность
            const char* use = child->Attribute("use");
            if(use && use=="required"sv) {
                field.isOptional = false;
                field.minOccurs = 1;
            } else {
                field.isOptional = true;
                field.minOccurs = 0;
            }

            field.maxOccurs = 1; // Атрибуты всегда 0 или 1

            complexType.fields.push_back(field);
        }
    }
}
#endif
void Parser::printSummary() const {
    println(std::cout, "=== XSD Parser Summary ===");
    println(std::cout, "Enums: {}", enums.size());
    for(const auto& e: enums) {
        println(std::cout, "  - {} ({} values)", e.name, e.values.size());
    }

    println(std::cout, "\nComplex Types: {}", complexTypes.size());
    for(const auto& ct: complexTypes) {
        println(std::cout, "  - {} ({} fields)", ct.name, ct.fields.size());
    }

    println(std::cout, "\nElements: {}", elements.size());
    for(const auto& elem: elements) {
        println(std::cout, "  - {} ({})", elem.name, elem.type);
    }
}

void Parser::parseSimpleType(const tinyxml2::XMLElement* element) {
    Enum enumType;

    // Получаем имя перечисления
    const char* name = element->Attribute("name");
    if(!name) {
        println(std::cerr, "Простой тип без имени, пропускаем");
        return;
    }
    enumType.name = sanitizeName(name);

    // Получаем документацию
    enumType.documentation = getDocumentation(element);

    // Ищем restriction/enumeration
    const tinyxml2::XMLElement* restriction = element->FirstChildElement("xs:restriction");
    if(!restriction) restriction = element->FirstChildElement("restriction");

    if(restriction) {
        // Получаем базовый тип
        const char* base = restriction->Attribute("base");
        if(base) {
            enumType.baseType = convertXsdTypeToCpp(base);
        }

        // Парсим значения перечисления
        for(const tinyxml2::XMLElement* enumElem = restriction->FirstChildElement("xs:enumeration");
            enumElem != nullptr;
            enumElem = enumElem->NextSiblingElement("xs:enumeration")) {
            const char* value = enumElem->Attribute("value");
            if(value) {
                enumType.values.push_back(value);
            }
        }

        // Если не нашли через xs:enumeration, пробуем без префикса
        if(enumType.values.empty()) {
            for(const tinyxml2::XMLElement* enumElem = restriction->FirstChildElement("enumeration");
                enumElem != nullptr;
                enumElem = enumElem->NextSiblingElement("enumeration")) {

                const char* value = enumElem->Attribute("value");
                if(value) {
                    enumType.values.push_back(value);
                }
            }
        }

        if(enums.size())
            enums.push_back(enumType);
        else
            typeMap.emplace(name, "std::string"sv);
    }
}

// Обновленный метод parseComplexType с поддержкой complexContent и simpleContent
void Parser::parseComplexType(const tinyxml2::XMLElement* element) {
    ComplexType complexType;

    // Получаем имя типа
    const char* name = element->Attribute("name");
    if(!name) {
        // Анонимный тип - генерируем имя
        static int anonymousComplexCounter = 0;
        complexType.name = "AnonymousComplexType_" + std::to_string(anonymousComplexCounter++);
    } else {
        complexType.name = sanitizeName(name);
    }

    // Получаем документацию
    complexType.documentation = getDocumentation(element);

    // Проверяем, является ли абстрактным
    const char* abstract = element->Attribute("abstract");
    if(abstract && abstract == "true"sv) {
        complexType.isAbstract = true;
    }

    // Проверяем complexContent/simpleContent
    const tinyxml2::XMLElement* complexContent = element->FirstChildElement("xs:complexContent");
    if(!complexContent) complexContent = element->FirstChildElement("complexContent");

    const tinyxml2::XMLElement* simpleContent = element->FirstChildElement("xs:simpleContent");
    if(!simpleContent) simpleContent = element->FirstChildElement("simpleContent");

    if(complexContent) {
        handleComplexContent(complexContent, complexType);
    } else if(simpleContent) {
        handleSimpleContent(simpleContent, complexType);
    } else {
        // Обычный complexType с последовательностью/выбором/всем
        const tinyxml2::XMLElement* sequence = element->FirstChildElement("xs:sequence");
        if(!sequence) sequence = element->FirstChildElement("sequence");

        const tinyxml2::XMLElement* choice = element->FirstChildElement("xs:choice");
        if(!choice) choice = element->FirstChildElement("choice");

        const tinyxml2::XMLElement* all = element->FirstChildElement("xs:all");
        if(!all) all = element->FirstChildElement("all");

        // Проверяем наличие mixed content
        const char* mixed = element->Attribute("mixed");
        if(mixed && mixed == "true"sv) {
            // mixed="true" означает, что могут быть и текст, и элементы
            Field textField;
            textField.name = "textContent";
            textField.type = "std::string";
            textField.documentation = "Текстовое содержимое mixed content";
            textField.isAttribute = false;
            textField.isOptional = true;
            textField.minOccurs = 0;
            textField.maxOccurs = 1;

            complexType.fields.push_back(textField);
        }

        // Парсим содержимое
        if(sequence) {
            parseSequenceElements(sequence, complexType);
        } else if(choice) {
            parseChoiceElements(choice, complexType);
        } else if(all) {
            parseAllElements(all, complexType);
        } else {
            // Проверяем, есть ли простой контент
            const tinyxml2::XMLElement* simpleContent = element->FirstChildElement("xs:simpleContent");
            if(!simpleContent) simpleContent = element->FirstChildElement("simpleContent");

            if(simpleContent) {
                handleSimpleContent(simpleContent, complexType);
            }
        }

        // Парсим атрибуты
        parseAttributes(element, complexType);
    }

    // Проверяем, не является ли этот тип дубликатом
    bool isDuplicate = false;
    for(const auto& existingType: complexTypes) {
        if(existingType.name == complexType.name) {
            isDuplicate = true;
            break;
        }
    }

    if(!isDuplicate) {
        complexTypes.push_back(complexType);
    } else {
        std::cout << "  Предупреждение: тип '" << complexType.name
                  << "' уже существует, пропускаем дубликат" << std::endl;
    }
}

void Parser::parseElement(const tinyxml2::XMLElement* element) {
    Element xsdElement;

    const char* name = element->Attribute("name");
    if(name) {
        xsdElement.name = sanitizeName(name);
    }

    const char* type = element->Attribute("type");
    if(type) {
        xsdElement.type = type;

        // Проверяем, является ли complexType
        // В реальном парсере нужно проверять по списку complexTypes
        if(xsdElement.type.find(":") != std::string::npos) {
            xsdElement.isComplex = true;
        }
    }

    xsdElement.documentation = getDocumentation(element);

    elements.push_back(xsdElement);
}

string normalize(string str) {
    std::ranges::replace(str, '-', '_');
    std::ranges::replace(str, ' ', '_');
    if(str.ends_with('+'))
        str.pop_back(), str += "Plus";
    else if(str.ends_with('*'))
        str.pop_back(), str += "Star";
    return str;
}

// Реализация методов генерации кода для Enum
string Enum::generateHeaderCode() const {
    std::stringstream ss;

    if(!documentation.empty()) {
        println(ss, "/*\n{}\n*/", documentation);
    }

    println(ss, "enum class {} {{", name);

    for(auto&& value: values)
        if(auto norm = normalize(value); norm != value)
            println(ss, "    {}, // {}", norm, value);
        else
            println(ss, "    {},", value);

    println(ss, "}};\n");

    if(values.size()) {
        // Функции преобразования
        println(ss, "// Функции преобразования для {}", name);
        println(ss, "extern template {0} stringTo<{0}>(const std::string& str);", name);
        println(ss, "std::string toString({} value);\n", name);
    }

    return ss.str();
}

string Enum::generateSourceCode() const {
    std::stringstream ss;

    if(values.empty()) return {};

    // stringToEnum
    println(ss, "template<{0}> {0} stringTo(const std::string& str) {{", name);
    println(ss, "    static const std::map<std::string, {}> mapping = {{", name);

    for(const auto& value: values)
        println(ss, "        {{\"{}\", {}::{}}},", normalize(value), name, normalize(value));
    for(const auto& value: values) {
        if(auto norm = normalize(value); norm != value)
            println(ss, "        {{\"{}\", {}::{}}},", value, name, normalize(value));
    }

    println(ss, "    }};\n");
    println(ss, "    auto it = mapping.find(str);");
    println(ss, "    if (it != mapping.end()) return it->second;");
    println(ss, "    throw std::runtime_error(\"Invalid value for {}: \" + str);", name);
    println(ss, "}}\n");

    // enumToString
    println(ss, "std::string toString({} value) {{", name);
    println(ss, "    switch(value) {{");

    for(const auto& value: values)
        println(ss, "        case {}::{}: return \"{}\";", name, normalize(value), value);

    println(ss, "        default: throw std::runtime_error(\"Invalid {} value\");", name);
    println(ss, "    }}");
    println(ss, "}}\n");

    return ss.str();
}

// Реализация методов генерации кода для ComplexType
string ComplexType::generateHeaderCode() const {
    std::stringstream ss;

    if(!documentation.empty()) {
        println(ss, "/**\n * {}\n */", documentation);
    }

    println(ss, "struct {} {{", name);

    // Поля
    for(const auto& field: fields) {
        if(!field.documentation.empty()) {
            println(ss, "    // {}", field.documentation);
        }

        string type = field.type;

        // Если поле может встречаться много раз
        if(field.maxOccurs == -1 || field.maxOccurs > 1) {
            type = "std::vector<" + type + ">";
        }

        // Если поле опциональное (minOccurs == 0)
        string optionalPrefix = "";
        if(field.isOptional && field.maxOccurs == 1) {
            // Для C++17 можно использовать std::optional
            optionalPrefix = "std::optional<";
            type = optionalPrefix + type + ">";
        }

        println(ss, "    {} {};", type, field.name);
    }

    // println(ss, "\n    // Конструкторы");
    // println(ss, "    {}() = default;", name);
    // println(ss, "    ~{}() = default;\n", name);

    // Методы сериализации
    // println(ss, "    // Сериализация/десериализация");
    // println(ss, "    std::string toXml() const;");
    // println(ss, "    static {} fromXml(const std::string& xml);", name);
    // println(ss, "    static {} fromXmlNode(const tinyxml2::XMLElement* element);", name);
    // println(ss, "    tinyxml2::XMLElement* toXmlNode(tinyxml2::XMLDocument& doc) const;\n", name);

    // Операторы сравнения
    // println(ss, "    // Операторы сравнения");
    // println(ss, "    bool operator==(const {}& other) const;", name);
    // println(ss, "    bool operator!=(const {}& other) const;", name);

    println(ss, "}};\n");

    return ss.str();
}
#if 0
std::string ComplexType::generateSourceCode() const {
    std::stringstream ss;

    // Метод toXmlNode
    println(ss, "tinyxml2::XMLElement* " << name << "::toXmlNode(tinyxml2::XMLDocument& doc) const {{");
    println(ss, "    auto* element = doc.NewElement(\"" << name << "\");\n");

    for(const auto& field: fields) {
        if(field.isAttribute) {
            // Атрибуты
            if(!field.isOptional) {
                ss << "    element->SetAttribute(\"{0}\",,field.name "
                   << field.name << ");\n";
            } else {
                println(ss, "    if({0}) {{",field.name);
                ss << "        element->SetAttribute(\"{0}\",,field.name "
                   << "*{0});\n,field.name";
                println(ss, "    }}");
            }
        } else {
            // Элементы
            if(field.maxOccurs == -1 || field.maxOccurs > 1) {
                // Вектор
                println(ss, "    for(const auto& item : {0}) {{",field.name);
                println(ss, "        auto* child = doc.NewElement(\"{0}\");",field.name);
                println(ss, "        child->SetText(item.c_str());");
                println(ss, "        element->InsertEndChild(child);");
                println(ss, "    }}");
            } else if(field.isOptional) {
                // Опциональное поле
                println(ss, "    if({0}) {{",field.name);
                println(ss, "        auto* child = doc.NewElement(\"{0}\");",field.name);
                println(ss, "        child->SetText(" << "*{0});",field.name);
                println(ss, "        element->InsertEndChild(child);");
                println(ss, "    }}");
            } else {
                // Обязательное поле
                println(ss, "    auto* child = doc.NewElement(\"{0}\");",field.name);
                println(ss, "    child->SetText({0});",field.name);
                println(ss, "    element->InsertEndChild(child);");
            }
        }
    }

    println(ss, "    return element;");
    println(ss, "}}\n");

    // Метод fromXmlNode
    println(ss, name << " " << name << "::fromXmlNode(const tinyxml2::XMLElement* element) {{");
    println(ss, "    " << name << " result;\n");

    for(const auto& field: fields) {
        if(field.isAttribute) {
            // Атрибуты
            if(!field.isOptional) {
                println(ss, "    {{");
                println(ss, "        const char* value = element->Attribute(\"{0}\");",field.name);
                println(ss, "        if(value) {{");
                println(ss, "            result.{0} = value;",field.name);
                println(ss, "        } else {{");
                ss << "            throw std::runtime_error(\"Missing required attribute: "
                   << field.name << "\");\n";
                println(ss, "        }");
                println(ss, "    }}");
            } else {
                println(ss, "    {{");
                println(ss, "        const char* value = element->Attribute(\"{0}\");",field.name);
                println(ss, "        if(value) {{");
                println(ss, "            result.{0} = value;",field.name);
                println(ss, "        }");
                println(ss, "    }}");
            }
        } else {
            // Элементы
            if(field.maxOccurs == -1 || field.maxOccurs > 1) {
                // Вектор
                println(ss, "    {{");
                ss << "        const tinyxml2::XMLElement* child = "
                   << "element->FirstChildElement(\"{0}\");\n,field.name";
                println(ss, "        while(child) {{");
                println(ss, "            const char* text = child->GetText();");
                println(ss, "            if(text) {{");
                println(ss, "                result.{0}.push_back(text);",field.name);
                println(ss, "            }");
                println(ss, "            child = child->NextSiblingElement(\"{0}\");",field.name);
                println(ss, "        }");
                println(ss, "    }}");
            } else if(field.isOptional) {
                // Опциональное поле
                println(ss, "    {{");
                ss << "        const tinyxml2::XMLElement* child = "
                   << "element->FirstChildElement(\"{0}\");\n,field.name";
                println(ss, "        if(child) {{");
                println(ss, "            const char* text = child->GetText();");
                println(ss, "            if(text) {{");
                println(ss, "                result.{0} = text;",field.name);
                println(ss, "            }");
                println(ss, "        }");
                println(ss, "    }}");
            } else {
                // Обязательное поле
                println(ss, "    {{");
                ss << "        const tinyxml2::XMLElement* child = "
                   << "element->FirstChildElement(\"{0}\");\n,field.name";
                println(ss, "        if(child) {{");
                println(ss, "            const char* text = child->GetText();");
                println(ss, "            if(text) {{");
                println(ss, "                result.{0} = text;",field.name);
                println(ss, "            } else {{");
                ss << "                throw std::runtime_error(\"Empty required element: "
                   << field.name << "\");\n";
                println(ss, "            }");
                println(ss, "        } else {{");
                ss << "            throw std::runtime_error(\"Missing required element: "
                   << field.name << "\");\n";
                println(ss, "        }");
                println(ss, "    }}");
            }
        }
    }

    println(ss, "    return result;");
    println(ss, "}}\n");

    // Метод toXml
    println(ss, "std::string " << name << "::toXml() const {{");
    println(ss, "    tinyxml2::XMLDocument doc;");
    println(ss, "    auto* element = toXmlNode(doc);");
    println(ss, "    doc.InsertFirstChild(element);\n");
    println(ss, "    tinyxml2::XMLPrinter printer;");
    println(ss, "    doc.Print(&printer);\n");
    println(ss, "    return std::string(printer.CStr());");
    println(ss, "}}\n");

    // Метод fromXml
    println(ss, name << " " << name << "::fromXml(const std::string& xml) {{");
    println(ss, "    tinyxml2::XMLDocument doc;");
    println(ss, "    if(doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS) {{");
    println(ss, "        throw std::runtime_error(\"Failed to parse XML\");");
    println(ss, "    }\n");
    println(ss, "    const tinyxml2::XMLElement* root = doc.RootElement();");
    println(ss, "    if(!root) {{");
    println(ss, "        throw std::runtime_error(\"No root element found\");");
    println(ss, "    }\n");
    println(ss, "    return fromXmlNode(root);");
    println(ss, "}}\n");

    // Операторы сравнения
    println(ss, "bool " << name << "::operator==(const " << name << "& other) const {{");
    print(ss, "    return ");

    for(size_t i = 0; i < fields.size(); ++i) {
        print(ss, "(" << fields[i].name << " == other." << fields[i].name << ")");
        if(i < fields.size() - 1) {
            print(ss, " &&\n           ");
        }
    }

    if(fields.empty()) {
        print(ss, "true");
    }

    println(ss, ";");
    println(ss, "}}\n");

    println(ss, "bool " << name << "::operator!=(const " << name << "& other) const {{");
    println(ss, "    return !(*this == other);");
    println(ss, "}}\n");

    return ss.str();
}
#endif

void Parser::parseSchema(const tinyxml2::XMLElement* schemaElement) {
    // Парсим все дочерние элементы
    for(const tinyxml2::XMLElement* child = schemaElement->FirstChildElement();
        child != nullptr;
        child = child->NextSiblingElement()) {

        const char* elementName = child->Name();

        if(testName(elementName, "xs:simpleType"sv)) { // enumerators
            parseSimpleType(child);
        } else if(testName(elementName, "xs:complexType"sv)) {
            parseComplexType(child);
        } else if(testName(elementName, "xs:element"sv)) {
            parseElement(child);
        }
    }
}

string Parser::getDocumentation(const tinyxml2::XMLElement* element) const {
    const tinyxml2::XMLElement* annotation = element->FirstChildElement("xs:annotation");
    if(!annotation) annotation = element->FirstChildElement("annotation");

    if(annotation) {
        const tinyxml2::XMLElement* documentation = annotation->FirstChildElement("xs:documentation");
        if(!documentation) documentation = annotation->FirstChildElement("documentation");

        if(documentation && documentation->GetText()) {
            return trim(documentation->GetText());
        }
    }

    return "";
}

string Parser::convertXsdTypeToCpp(const string& xsdType) const {
    // Проверяем в карте типов
    auto it = typeMap.find(xsdType);
    if(it != typeMap.end()) {
        return string{it->second};
    }

    // Если тип не найден, проверяем, является ли он пользовательским типом
    // Удаляем префикс пространства имен, если есть
    size_t colonPos = xsdType.find(":");
    string typeName = (colonPos != std::string::npos) ? xsdType.substr(colonPos + 1) : xsdType;

    // Проверяем, является ли это перечислением
    for(const auto& enumType: enums) {
        if(enumType.name == typeName) {
            return typeName;
        }
    }

    // Проверяем, является ли это complexType
    for(const auto& complexType: complexTypes) {
        if(complexType.name == typeName) {
            return typeName;
        }
    }

    // Если не нашли, возвращаем как есть (будет сгенерирован класс)
    return sanitizeName(typeName);
}

string Parser::sanitizeName(string name) {
    // Заменяем недопустимые символы
    for(char& c: name)
        if("-.:"sv.contains(c)) c = '_';

    // Убеждаемся, что имя начинается с буквы
    if(!name.empty() && isdigit(name.front()))
        name.insert(name.begin(), '_');
    if(name.ends_with("Type"sv))
        name.resize(name.size() - 4);

    return toCamelCase(name);
}

string Parser::toCamelCase(const string& str) {
    string result;
    bool makeUpper = true;

    for(char c: str) {
        if("-_"sv.contains(c)) {
            makeUpper = true;
        } else if(makeUpper) {
            result += std::toupper(c);
            makeUpper = false;
        } else {
            result += c;
        }
    }

    return result;
}

string Parser::toUpperCase(string str) {
    constexpr int (*toupper)(int) = std::toupper;
    std::transform(str.begin(), str.end(), str.begin(), toupper);
    return str;
}
/////////////////////////////////////////////////////////////////////

// Вспомогательные функции
string Parser::trim(const string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if(first == std::string::npos) return "";

    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

// Обновленный метод parseAttributes
void Parser::parseAttributes(const tinyxml2::XMLElement* element, ComplexType& complexType) {

    for(const tinyxml2::XMLElement* child = element->FirstChildElement();
        child != nullptr;
        child = child->NextSiblingElement()) {

        const char* childName = child->Name();

        if(childName && (testName(childName, "xs:attribute"sv))) {

            Field field;
            field.isAttribute = true;

            // Имя атрибута
            const char* name = child->Attribute("name");
            if(name) {
                field.name = sanitizeName(name);
            } else {
                continue; // Пропускаем атрибуты без имени
            }

            // Тип атрибута
            const char* type = child->Attribute("type");
            if(type) {
                field.type = convertXsdTypeToCpp(type);
            } else {
                // Проверяем встроенный тип
                const tinyxml2::XMLElement* simpleType = child->FirstChildElement("xs:simpleType");
                if(!simpleType) simpleType = child->FirstChildElement("simpleType");

                if(simpleType) {
                    const tinyxml2::XMLElement* restriction = simpleType->FirstChildElement("xs:restriction");
                    if(!restriction) restriction = simpleType->FirstChildElement("restriction");

                    if(restriction) {
                        const char* base = restriction->Attribute("base");
                        if(base) {
                            field.type = convertXsdTypeToCpp(base);
                        } else {
                            field.type = "std::string";
                        }
                    } else {
                        field.type = "std::string";
                    }
                } else {
                    field.type = "std::string";
                }
            }

            // Документация
            field.documentation = getDocumentation(child);

            // Обязательность
            const char* use = child->Attribute("use");
            if(use) {
                if(use == "required"sv) {
                    field.isOptional = false;
                    field.minOccurs = 1;
                } else if(use == "optional"sv) {
                    field.isOptional = true;
                    field.minOccurs = 0;
                } else if(use == "prohibited"sv) {
                    continue; // Пропускаем запрещенные атрибуты
                }
            } else {
                // По умолчанию optional
                field.isOptional = true;
                field.minOccurs = 0;
            }

            field.maxOccurs = 1; // Атрибуты всегда 0 или 1

            // Значение по умолчанию
            const char* defaultValue = child->Attribute("default");
            if(defaultValue) {
                if(!field.documentation.empty()) {
                    field.documentation += " ";
                }
                field.documentation += "[По умолчанию: " + std::string(defaultValue) + "]";
            }

            // Фиксированное значение
            const char* fixedValue = child->Attribute("fixed");
            if(fixedValue) {
                if(!field.documentation.empty()) {
                    field.documentation += " ";
                }
                field.documentation += "[Фиксированное значение: " + std::string(fixedValue) + "]";
            }

            complexType.fields.push_back(field);
        }
        // Обработка групп атрибутов
        else if(childName && (testName(childName, "xs:attributeGroup"sv))) {

            const char* ref = child->Attribute("ref");
            if(ref) {
                std::cout << "  Информация: ссылка на группу атрибутов '" << ref
                          << "' - требуется предварительное определение" << std::endl;
            }
        }
    }
}

void Parser::parseSequenceElements(const tinyxml2::XMLElement* sequence,
    ComplexType& complexType) {

    if(!sequence) return;

    // Обрабатываем все дочерние элементы sequence
    for(const tinyxml2::XMLElement* child = sequence->FirstChildElement();
        child != nullptr;
        child = child->NextSiblingElement()) {

        const char* childName = child->Name();
        string elementName = childName ? childName : "";

        if(testName(elementName, "xs:element"sv)) { // Элемент
            Field field;
            parseElementDetails(child, field);
            if(!field.name.empty()) complexType.fields.push_back(field);
        } else if(testName(elementName, "xs:group"sv)) { // Группа элементов
            parseGroupReference(child, complexType);
        } else if(testName(elementName, "xs:sequence"sv)) { // Последовательность внутри последовательности (вложенная)
            parseSequenceElements(child, complexType);
        } else if(testName(elementName, "xs:choice"sv)) { // Выбор
            parseChoiceElements(child, complexType);
        } else if(testName(elementName, "xs:all"sv)) { // Все элементы
            parseAllElements(child, complexType);
        } else if(testName(elementName, "xs:any"sv)) { // Любой элемент
            // Пропускаем xs:any - сложно отобразить на статический C++ код
            std::cout << "  Предупреждение: элемент <any> не поддерживается, пропускаем" << std::endl;
        }
    }
}

void Parser::parseChoiceElements(const tinyxml2::XMLElement* choice,
    ComplexType& complexType) {

    if(!choice) return;

    // Для choice мы создаем специальное поле с вариантами
    // В реальной реализации можно создать union или variant

    std::cout << "  Предупреждение: элемент <choice> требует ручной обработки" << std::endl;

    // Временная реализация - обрабатываем как последовательность
    for(const tinyxml2::XMLElement* child = choice->FirstChildElement();
        child != nullptr;
        child = child->NextSiblingElement()) {

        const char* childName = child->Name();
        string elementName = childName ? childName : "";

        if(testName(elementName, "xs:element")) {
            Field field;
            field.isAttribute = false;

            parseElementDetails(child, field);

            // Для choice отмечаем поле как опциональное
            field.isOptional = true;
            field.minOccurs = 0;
            field.maxOccurs = 1;

            if(!field.name.empty()) {
                complexType.fields.push_back(field);
            }
        }
    }
}

void Parser::parseAllElements(const tinyxml2::XMLElement* all,
    ComplexType& complexType) {

    if(!all) return;

    // xs:all означает, что все элементы могут быть в любом порядке,
    // но каждый может появляться только 0 или 1 раз

    for(const tinyxml2::XMLElement* child = all->FirstChildElement();
        child != nullptr;
        child = child->NextSiblingElement()) {

        const char* childName = child->Name();
        string elementName = childName ? childName : "";

        if(testName(elementName, "xs:element")) {
            Field field;
            field.isAttribute = false;

            parseElementDetails(child, field);

            // В xs:all элементы могут быть опциональными
            // minOccurs по умолчанию 1, но может быть 0
            if(field.minOccurs == 0) {
                field.isOptional = true;
            }

            // В xs:all maxOccurs всегда 1
            field.maxOccurs = 1;

            if(!field.name.empty()) {
                complexType.fields.push_back(field);
            }
        }
    }
}

void Parser::parseGroupReference(const tinyxml2::XMLElement* groupRef,
    ComplexType& complexType) {

    if(!groupRef) return;

    const char* refName = groupRef->Attribute("ref");
    if(!refName) {
        println(std::cerr, "  Ошибка: элемент group без атрибута ref");
        return;
    }

    println(std::cout, "  Информация: ссылка на группу '{}' - группы требуют предварительного определения", refName);

    // В полной реализации здесь нужно найти определение группы
    // и добавить все её элементы в complexType
}

void Parser::parseElementDetails(const tinyxml2::XMLElement* elementNode,
    Field& field) {

    // Получаем имя элемента
    const char* name = elementNode->Attribute("name");
    if(name) {
        field.name = sanitizeName(name);
    } else {
        // Элемент может быть анонимным (inline type)
        // Генерируем уникальное имя
        static int anonymousCounter = 0;
        field.name = "anonymousElement_" + std::to_string(anonymousCounter++);
    }

    // Получаем тип элемента
    const char* typeAttr = elementNode->Attribute("type");
    if(typeAttr) {
        field.type = convertXsdTypeToCpp(typeAttr);
    } else {
        // Проверяем встроенный тип
        const tinyxml2::XMLElement* simpleType = elementNode->FirstChildElement("xs:simpleType");
        if(!simpleType) simpleType = elementNode->FirstChildElement("simpleType");

        const tinyxml2::XMLElement* complexTypeElem = elementNode->FirstChildElement("xs:complexType");
        if(!complexTypeElem) complexTypeElem = elementNode->FirstChildElement("complexType");

        if(simpleType) {
            // Обрабатываем встроенный простой тип
            field.type = "std::string"; // По умолчанию

            const tinyxml2::XMLElement* restriction = simpleType->FirstChildElement("xs:restriction");
            if(!restriction) restriction = simpleType->FirstChildElement("restriction");

            if(restriction) {
                const char* base = restriction->Attribute("base");
                if(base) {
                    field.type = convertXsdTypeToCpp(base);
                }
            }
        } else if(complexTypeElem) {
            // Обрабатываем встроенный сложный тип
            // Генерируем уникальное имя для типа
            static int inlineTypeCounter = 0;
            string inlineTypeName = field.name + "_t" + std::to_string(inlineTypeCounter++);

            // Создаем временный complexType
            ComplexType inlineType;
            inlineType.name = inlineTypeName;

            // Рекурсивно парсим встроенный тип
            parseComplexType(complexTypeElem);

            field.type = inlineTypeName;
        } else {
            // Тип по умолчанию
            field.type = "std::string";
        }
    }

    // Получаем документацию
    field.documentation = getDocumentation(elementNode);

    // Обрабатываем minOccurs и maxOccurs
    const char* minOccurs = elementNode->Attribute("minOccurs");
    if(minOccurs) {
        field.minOccurs = atoi(minOccurs);
        field.isOptional = (field.minOccurs == 0);
    } else {
        // По умолчанию minOccurs = 1
        field.minOccurs = 1;
        field.isOptional = false;
    }

    const char* maxOccurs = elementNode->Attribute("maxOccurs");
    if(maxOccurs) {
        if(maxOccurs == "unbounded"sv) {
            field.maxOccurs = -1; // unbounded
        } else {
            field.maxOccurs = atoi(maxOccurs);
        }
    } else {
        // По умолчанию maxOccurs = 1
        field.maxOccurs = 1;
    }

    // Обрабатываем значение по умолчанию
    const char* defaultValue = elementNode->Attribute("default");
    if(defaultValue) {
        if(!field.documentation.empty()) {
            field.documentation += " ";
        }
        field.documentation += "[По умолчанию: " + std::string(defaultValue) + "]";
    }

    // Обрабатываем фиксированное значение
    const char* fixedValue = elementNode->Attribute("fixed");
    if(fixedValue) {
        if(!field.documentation.empty()) {
            field.documentation += " ";
        }
        field.documentation += "[Фиксированное значение: " + std::string(fixedValue) + "]";
    }

    // Обрабатываем nillable
    const char* nillable = elementNode->Attribute("nillable");
    if(nillable && nillable == "true"sv) {
        field.isOptional = true;
        field.minOccurs = 0;
    }
}

void Parser::handleComplexContent(const tinyxml2::XMLElement* complexContent,
    ComplexType& complexType) {

    if(!complexContent) return;

    // Обрабатываем extension (наследование)
    const tinyxml2::XMLElement* extension = complexContent->FirstChildElement("xs:extension");
    if(!extension) extension = complexContent->FirstChildElement("extension");

    if(extension) {
        const char* base = extension->Attribute("base");
        if(base) {
            complexType.baseType = convertXsdTypeToCpp(base);
        }

        // Обрабатываем содержимое extension
        const tinyxml2::XMLElement* sequence = extension->FirstChildElement("xs:sequence");
        if(!sequence) sequence = extension->FirstChildElement("sequence");

        if(sequence) {
            parseSequenceElements(sequence, complexType);
        }

        // Обрабатываем атрибуты в extension
        parseAttributes(extension, complexType);
    }

    // Обрабатываем restriction
    const tinyxml2::XMLElement* restriction = complexContent->FirstChildElement("xs:restriction");
    if(!restriction) restriction = complexContent->FirstChildElement("restriction");

    if(restriction) {
        // Обработка restriction (более сложная)
        std::cout << "  Предупреждение: complexContent/restriction требует специальной обработки" << std::endl;
    }
}

void Parser::handleSimpleContent(const tinyxml2::XMLElement* simpleContent,
    ComplexType& complexType) {

    if(!simpleContent) return;

    // simpleContent используется, когда complexType содержит только текст и атрибуты

    // Обрабатываем extension
    const tinyxml2::XMLElement* extension = simpleContent->FirstChildElement("xs:extension");
    if(!extension) extension = simpleContent->FirstChildElement("extension");

    if(extension) {
        const char* base = extension->Attribute("base");
        if(base) {
            // Добавляем поле для текстового содержимого
            Field textField;
            textField.name = "value";
            textField.type = convertXsdTypeToCpp(base);
            textField.documentation = "Текстовое значение элемента";
            textField.isAttribute = false;
            textField.isOptional = false;
            textField.minOccurs = 1;
            textField.maxOccurs = 1;

            complexType.fields.push_back(textField);
        }

        // Обрабатываем атрибуты
        parseAttributes(extension, complexType);
    }

    // Обрабатываем restriction
    const tinyxml2::XMLElement* restriction = simpleContent->FirstChildElement("xs:restriction");
    if(!restriction) restriction = simpleContent->FirstChildElement("restriction");

    if(restriction) {
        const char* base = restriction->Attribute("base");
        if(base) {
            Field textField;
            textField.name = "value";
            textField.type = convertXsdTypeToCpp(base);
            textField.documentation = "Текстовое значение элемента с ограничениями";
            textField.isAttribute = false;
            textField.isOptional = false;
            textField.minOccurs = 1;
            textField.maxOccurs = 1;

            complexType.fields.push_back(textField);
        }
    }
}

// Вспомогательные методы
#if 0
bool Parser::isBuiltInType(const string& typeName) const {
    println(std::cout, ">>>{}", typeName);

    // Проверяем, является ли тип встроенным XSD типом
    static const vector<string> builtInTypes = {
        "string",
        "int",
        "integer",
        "long",
        "short",
        "decimal",
        "float",
        "double",
        "boolean",
        "date",
        "dateTime",
        "time",
        "base64Binary",
        "hexBinary",
        "anyURI",
        "QName",
        "normalizedString",
        "token",
        "unsignedInt",
        "unsignedLong",
        "unsignedShort",
        "positiveInteger",
        "nonNegativeInteger",
        "scaledNonNegativeInteger",
    };

    string localName = extractLocalName(typeName);

    for(const auto& builtInType: builtInTypes) {
        if(localName == builtInType) {
            return true;
        }
    }

    return false;
}
#endif

string Parser::extractLocalName(const string& qualifiedName) const {
    size_t colonPos = qualifiedName.find(":");
    if(colonPos != std::string::npos) {
        return qualifiedName.substr(colonPos + 1);
    }
    return qualifiedName;
}

string Parser::getNamespacePrefix(const string& qualifiedName) const {
    size_t colonPos = qualifiedName.find(":");
    if(colonPos != std::string::npos) {
        return qualifiedName.substr(0, colonPos);
    }
    return "";
}

// Дополним метод generateSourceCode в ComplexType
string ComplexType::generateSourceCode() const {
    std::stringstream ss;

    // Конструкторы
    println(ss, "{0}::{0}() {{", name);

    // Инициализация полей со значениями по умолчанию
    for(const auto& field: fields) {
        // Можно добавить инициализацию по умолчанию
    }

    println(ss, "}}\n");

    // Метод toXmlNode с улучшенной обработкой
    println(ss, "tinyxml2::XMLElement* {}::toXmlNode(tinyxml2::XMLDocument& doc) const {{", name);
    println(ss, "    auto* element = doc.NewElement(\"{}\");\n", name);

    // Обработка текстового содержимого для simpleContent
    bool hasSimpleContent = false;
    for(const auto& field: fields) {
        if(field.name == "value" && field.type != "std::string") {
            hasSimpleContent = true;
            println(ss, "    // Текстовое содержимое для simpleContent");
            println(ss, "    element->SetText({});\n", field.name);
            break;
        }
    }

    if(!hasSimpleContent) {
        // Обработка mixed content
        for(const auto& field: fields) {
            if(field.name == "textContent") {
                println(ss, "    // Текстовое содержимое mixed content");
                println(ss, "    if(!textContent.empty()) {{");
                println(ss, "        auto* textNode = doc.NewText(textContent.c_str());");
                println(ss, "        element->InsertEndChild(textNode);");
                println(ss, "    }}\n");
                break;
            }
        }
    }

    // Обработка обычных полей
    for(const auto& field: fields) {
        if(field.name == "value" || field.name == "textContent") {
            continue; // Уже обработали
        }

        if(field.isAttribute) {
            // Атрибуты
            println(ss, "    // Атрибут: {}", field.name);
            if(!field.isOptional) {
                if(field.type == "std::string") {
                    println(ss, "    element->SetAttribute(\"{0}\", {0}.c_str());", field.name);
                } else {
                    println(ss, "    element->SetAttribute(\"{0}\", {0});", field.name);
                }
            } else {
                print(ss, "    if(");
                if(field.type == "std::string") {
                    print(ss, "!{0}.empty()", field.name);
                } else {
                    print(ss, "{}.has_value()", field.name);
                }
                println(ss, ") {{");
                print(ss, "        element->SetAttribute(\"{}\", ", field.name);
                if(field.type == "std::string") {
                    print(ss, "{}.c_str()", field.name);
                } else {
                    print(ss, "*{}", field.name);
                }
                println(ss, ");");
                println(ss, "    }}");
            }
        } else {
            // Элементы
            println(ss, "    // Элемент: {}", field.name);

            if(field.maxOccurs == -1 || field.maxOccurs > 1) {
                // Вектор элементов
                println(ss, "    for(const auto& item : {0}) {{", field.name);
                println(ss, "        auto* child = doc.NewElement(\"{0}\");", field.name);

                if(field.type == "std::string") {
                    println(ss, "        child->SetText(item.c_str());");
                } else {
                    // Проверяем, является ли тип пользовательским
                    bool isCustomType = false;
                    for(const auto& ct: complexTypes_) {
                        if(ct.name == field.type) {
                            isCustomType = true;
                            break;
                        }
                    }

                    if(isCustomType) {
                        println(ss, "        auto* itemNode = item.toXmlNode(doc);");
                        println(ss, "        child->InsertEndChild(itemNode);");
                    } else {
                        println(ss, "        child->SetText(std::to_string(item).c_str());");
                    }
                }

                println(ss, "        element->InsertEndChild(child);");
                println(ss, "    }}");
            } else if(field.isOptional) {
                // Опциональный элемент
                println(ss, "    if({0}.has_value()) {{", field.name);
                println(ss, "        auto* child = doc.NewElement(\"{0}\");", field.name);

                if(field.type == "std::string") {
                    println(ss, "        child->SetText({0}->c_str());", field.name);
                } else {
                    // Проверяем, является ли тип пользовательским
                    bool isCustomType = false;
                    for(const auto& ct: complexTypes_) {
                        if(ct.name == field.type) {
                            isCustomType = true;
                            break;
                        }
                    }

                    if(isCustomType) {
                        println(ss, "        auto* valueNode = {}->toXmlNode(doc);", field.name);
                        println(ss, "        child->InsertEndChild(valueNode);");
                    } else {
                        println(ss, "        child->SetText(std::to_string(*{}).c_str());", field.name);
                    }
                }

                println(ss, "        element->InsertEndChild(child);");
                println(ss, "    }}");
            } else {
                // Обязательный элемент
                println(ss, "    auto* child = doc.NewElement(\"{}\");", field.name);

                if(field.type == "std::string") {
                    println(ss, "    child->SetText({}.c_str());", field.name);
                } else {
                    // Проверяем, является ли тип пользовательским
                    bool isCustomType = false;
                    for(const auto& ct: complexTypes_) {
                        if(ct.name == field.type) {
                            isCustomType = true;
                            break;
                        }
                    }

                    if(isCustomType) {
                        println(ss, "    auto* valueNode = {}.toXmlNode(doc);", field.name);
                        println(ss, "    child->InsertEndChild(valueNode);");
                    } else {
                        println(ss, "    child->SetText(std::to_string({}).c_str());", field.name);
                    }
                }

                println(ss, "    element->InsertEndChild(child);");
            }
        }
        println(ss, "");
    }

    println(ss, "    return element;");
    println(ss, "}}\n");

    return ss.str();
}

} // namespace Xsd
