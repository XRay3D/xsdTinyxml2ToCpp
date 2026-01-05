#include "XsdParser.h"
#include <filesystem>
#include <format>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

namespace Xsd {

using std ::println;

#if 0

inline constexpr uint64_t stupidHash(string_view sv) noexcept {
    std::array<char, 8> arr{};
    std::ranges::copy_n(sv.data(), std::min(arr.size(), sv.size()), arr.begin());
    return std::bit_cast<uint64_t>(arr);
}

inline consteval uint64_t operator""_case(const char* str, size_t len) noexcept {
    return stupidHash({str, len});
}

#endif

constexpr auto testName(std::string_view src, std::string_view dst) -> bool {
    return src == dst || src == dst.substr(3);
};

Parser::~Parser() { clear(); }

bool Parser::parse(const string& filename) {
    clear();
    doc.saveComments = true;

    // Загружаем XML документ
    bool error = doc.load(filename);
    if(!error) {
        println(std::cerr, "Ошибка загрузки файла: {}", filename);
        // println(std::cerr, "Код ошибки: {}", doc_.ErrorStr());
        return false;
    }

    // Получаем корневой элемент
    const XML::Element* root = doc.root.firstChild("xs:schema");
    if(!root) {
        root = doc.root.firstChild("schema");
    }

    if(!root) {
        println(std::cerr, "Не найден корневой элемент schema");
        return false;
    }

    // Парсим схему
    parseSchema(root);

    println("Парсинг завершен успешно!");
    println("Найдено перечислений: {}", enums.size());
    println("Найдено complexType: {}", complexTypes.size());
    println("Найдено элементов: {}", elements.size());

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
    println(structHeader, "#include <variant>");
    println(structHeader, "#include \"xrxmlser.hpp\"");
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
        // println(cmakeFile, "    Types.cpp");
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

    println("Код успешно сгенерирован в директории: {}", outputDir);
    return true;
}

void Parser::clear() {
    enums.clear();
    complexTypes.clear();
    elements.clear();
    doc.root.clear();
}

void Parser::printSummary() const {
    println("=== XSD Parser Summary ===");
    println("Enums: {}", enums.size());
    for(const auto& e: enums) {
        println("  - {} ({} values)", e.name, e.values.size());
    }

    println("\nComplex Types: {}", complexTypes.size());
    for(const auto& ct: complexTypes) {
        println("  - {} ({} fields)", ct.name, ct.fields.size());
    }

    println("\nElements: {}", elements.size());
    for(const auto& elem: elements) {
        println("  - {} ({})", elem.name, elem.type);
    }
}

void Parser::parseSimpleType(const XML::Element* element) {
    Enum enumType;

    // Получаем имя перечисления
    string_view name = element->attrVal("name");
    if(name.empty()) {
        println(std::cerr, "Простой тип без имени, пропускаем");
        return;
    }

    enumType.name = sanitizeName(name);
    enumType.ccName = toCamelCase(enumType.name);

    // Получаем документацию
    enumType.documentation = getDocumentation(element);

    // Ищем restriction/enumeration
    const XML::Element* restriction = element->firstChildOf({"xs:restriction", "restriction"});

    if(!restriction) {
        println(std::cerr, "Простой тип без имени, пропускаем");
        return;
    }

    // Получаем базовый тип

    if(string_view base = restriction->attrVal("base"); base.size()) {
        enumType.baseType = convertXsdTypeToCpp(base);
    }

    // Парсим значения перечисления
    auto filter = v::filter([](auto&& elem) {
        return testName(elem->name(), "xs:enumeration"sv);
    });

    for(auto&& enumElem: *restriction | filter) {

        if(string_view value = enumElem->attrVal("value"); value.size())
            enumType.values.emplace_back(value);
    }

    if(enumType.values.size())
        enums.push_back(enumType);
    else
        typeMap.emplace(name, "std::string"sv);
}

// Обновленный метод parseComplexType с поддержкой complexContent и simpleContent
bool Parser::parseComplexType(const XML::Element* element) {
    ComplexType complexType;

    // Получаем имя типа
    string_view name = element->attrVal("name");
    if(name.empty()) {
        // Анонимный тип - генерируем имя
        static int counter = 0;
        complexType.ccName = complexType.name = "AnonymousComplexType_" + std::to_string(counter++);
    } else {
        complexType.origName = name;
        complexType.name = sanitizeName(name);
        complexType.ccName = toCamelCase(sanitizeName(name));
    }

    // Получаем документацию
    complexType.documentation = getDocumentation(element);

    // Проверяем, является ли абстрактным
    complexType.isAbstract = element->attrVal("abstract") == "true";

    // Проверяем complexContent/simpleContent

    if(const XML::Element* complexContent = element->firstChildOf({"xs:complexContent", "complexContent"});
        complexContent) {
        handleComplexContent(complexContent, complexType);
    } else if(const XML::Element* simpleContent = element->firstChildOf({"xs:simpleContent", "simpleContent"});
        simpleContent) {
        handleSimpleContent(simpleContent, complexType);
    } else {
        // Обычный complexType с последовательностью/выбором/всем
        // Проверяем наличие mixed content
        if(element->attrVal("mixed") == "true"sv) {
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
        if(const XML::Element* sequence = element->firstChildOf({"xs:sequence", "sequence"});
            sequence) {
            parseSequenceElements(sequence, complexType);
        } else if(const XML::Element* choice = element->firstChildOf({"xs:choice", "choice"});
            choice) {
            parseChoiceElements(choice, complexType);
        } else if(const XML::Element* all = element->firstChildOf({"xs:all", "all"});
            all) {
            parseAllElements(all, complexType);
        } else if(const XML::Element* simpleContent = element->firstChildOf({"xs:simpleContent", "simpleContent"});
            simpleContent) { // Проверяем, есть ли простой контент
            handleSimpleContent(simpleContent, complexType);
        }

        // Парсим атрибуты
        parseAttributes(element, complexType);
    }

    // Проверяем, не является ли этот тип дубликатом
    bool isDuplicate = r::find(complexTypes, complexType.name, &ComplexType::name) != complexTypes.end();

    if(!isDuplicate) {
        complexTypes.push_back(complexType);
    } else {
        println("  Предупреждение: тип '{}' уже существует, пропускаем дубликат", complexType.name);
    }
    return !isDuplicate;
}

void Parser::parseGroup(const XML::Element* element) {
    ComplexType complexType;

    // Получаем имя типа
    string_view name = element->attrVal("name");
    if(name.empty()) {
        // Анонимный тип - генерируем имя
        static int counter = 0;
        complexType.ccName = complexType.name = "AnonymousGroupType_" + std::to_string(counter++);
    } else {
        complexType.origName = name;
        complexType.name = sanitizeName(name);
        complexType.ccName = toCamelCase(sanitizeName(name));
    }

    // Получаем документацию
    complexType.documentation = getDocumentation(element);

    // Проверяем, является ли абстрактным
    complexType.isAbstract = element->attrVal("abstract") == "true";

    // Проверяем complexContent/simpleContent

    // if(const XML::Element* complexContent = element->firstChildOf({"xs:complexContent", "complexContent"});
    //     complexContent) {
    //     handleComplexContent(complexContent, complexType);
    // } else if(const XML::Element* simpleContent = element->firstChildOf({"xs:simpleContent", "simpleContent"});
    //     simpleContent) {
    //     handleSimpleContent(simpleContent, complexType);
    // } else {
    // Обычный complexType с последовательностью/выбором/всем
    // Проверяем наличие mixed content
    // if(element->attrVal("mixed") == "true"sv) {
    //     // mixed="true" означает, что могут быть и текст, и элементы
    //     Field textField;
    //     textField.name = "textContent";
    //     textField.type = "std::string";
    //     textField.documentation = "Текстовое содержимое mixed content";
    //     textField.isAttribute = false;
    //     textField.isOptional = true;
    //     textField.minOccurs = 0;
    //     textField.maxOccurs = 1;
    //     complexType.fields.push_back(textField);
    // }

    // Парсим содержимое
    if(const XML::Element* sequence = element->firstChildOf({"xs:sequence", "sequence"});
        sequence) {
        parseSequenceElements(sequence, complexType);
    } else if(const XML::Element* choice = element->firstChildOf({"xs:choice", "choice"});
        choice) {
        parseChoiceElements(choice, complexType);
    } else if(const XML::Element* all = element->firstChildOf({"xs:all", "all"});
        all) {
        parseAllElements(all, complexType);
    } else if(const XML::Element* simpleContent = element->firstChildOf({"xs:simpleContent", "simpleContent"});
        simpleContent) { // Проверяем, есть ли простой контент
        handleSimpleContent(simpleContent, complexType);
    }

    // Парсим атрибуты
    parseAttributes(element, complexType);
    // }

    if(!groups.emplace(complexType.origName, std::move(complexType)).second)
        println("  Предупреждение: group '{}' уже существует, пропускаем дубликат", complexType.name);
}

void Parser::parseElement(const XML::Element* element) {
    Element xsdElement;

    bool isComplexType = testName(element->front()->name(), "xs:complexType")
        && parseComplexType(element->front().get());

    if(string_view name = element->attrVal("name"); name.size()) {
        xsdElement.name = sanitizeName(name);
        if(isComplexType) {
            complexTypes.back().isRoot = true;
            complexTypes.back().origName = name;
            complexTypes.back().name = sanitizeName(name);
            complexTypes.back().ccName = toCamelCase(sanitizeName(name));
        }
    }

    if(string_view type = element->attrVal("type"); type.size()) {
        xsdElement.type = type;
        // Проверяем, является ли complexType
        // В реальном парсере нужно проверять по списку complexTypes
        xsdElement.isComplex = xsdElement.type.contains(":");
    }

    xsdElement.documentation = getDocumentation(element);

    elements.push_back(xsdElement);
}

string normalize(string str) {
    std::ranges::replace(str, '-', '_');
    std::ranges::replace(str, ' ', '_');
    if(str.ends_with('+')) str.pop_back(), str += "Plus";
    if(str.ends_with('*')) str.pop_back(), str += "Star";
    return str;
}

// Реализация методов генерации кода для Enum
string Enum::generateHeaderCode() const {
    std::stringstream ss;

    for(auto&& line: documentation | v::split('\n'))
        println(ss, "// {:s}", line | v::drop_while(isspace));

    println(ss, "enum class {} {{", ccName);

    for(auto&& value: values)
        if(auto norm = normalize(value); norm != value)
            println(ss, "    {}, // {}", norm, value);
        else
            println(ss, "    {},", value);

    println(ss, "}};\n");

    if(values.size()) {
        // Функции преобразования
        println(ss, "// Функции преобразования для {}", name);
        println(ss, "extern template {0} stringTo<{0}>(const std::string& str);", ccName);
        println(ss, "std::string toString({} value);\n", ccName);
    }

    return ss.str();
}

string Enum::generateSourceCode() const {
    std::stringstream ss;

    if(values.empty()) return {};

    // stringToEnum
    println(ss, "template<{0}> {0} stringTo(const std::string& str) {{", ccName);
    println(ss, "    static const std::map<std::string, {}> mapping = {{", ccName);

    for(const auto& value: values)
        println(ss, "        {{\"{}\", {}::{}}},", normalize(value), ccName, normalize(value));
    for(const auto& value: values) {
        if(auto norm = normalize(value); norm != value)
            println(ss, "        {{\"{}\", {}::{}}},", value, ccName, normalize(value));
    }

    println(ss, "    }};\n");
    println(ss, "    auto it = mapping.find(str);");
    println(ss, "    if (it != mapping.end()) return it->second;");
    println(ss, "    throw std::runtime_error(\"Invalid value for {}: \" + str);", ccName);
    println(ss, "}}\n");

    // enumToString
    println(ss, "std::string toString({} value) {{", ccName);
    println(ss, "    switch(value) {{");

    for(const auto& value: values)
        println(ss, "        case {}::{}: return \"{}\";", ccName, normalize(value), value);

    println(ss, "        default: throw std::runtime_error(\"Invalid {} value\");", ccName);
    println(ss, "    }}");
    println(ss, "}}\n");

    return ss.str();
}

// Реализация методов генерации кода для ComplexType
string ComplexType::generateHeaderCode() const {
    std::stringstream ss;

    for(auto&& line: documentation | v::split('\n')) {
        println(ss, "// {:s}", line | v::drop_while(isspace));
    }

    // println(ss, R"([[="{}"sv]] //)", name2);

    string attr = isRoot
        ? std::format(R"([[=XML::Root("{}")]])", origName)
        : std::format(R"([[=XML::Name("{:s}")]])", origName | v::take(origName.size() - 4));

    println(ss, "struct {} {} {{", attr, ccName);

    // Поля
    for(const auto& field: fields) {
        for(auto&& line: field.documentation | v::split('\n'))
            println(ss, "    // {:s}", line | v::drop_while(isspace));

        string type = field.type;

        // Если поле может встречаться много раз
        if(field.maxOccurs == -1 || field.maxOccurs > 1)
            type = "[[= XML::Array]] std::vector<" + type + ">";

        // Если поле опциональное (minOccurs == 0)
        if(field.isOptional && field.maxOccurs == 1)
            type = "std::optional<" + type + ">";

        println(ss, "    {} {};", type, field.name);
    }

    // println(ss, "\n    // Конструкторы");
    // println(ss, "    {}() = default;", name);
    // println(ss, "    ~{}() = default;\n", name);

    // Методы сериализации
    // println(ss, "    // Сериализация/десериализация");
    // println(ss, "    std::string toXml() const;");
    // println(ss, "    static {} fromXml(const std::string& xml);", name);
    // println(ss, "    static {} fromXmlNode(const XML::Element* element);", name);
    // println(ss, "    XML::Element* toXmlNode(XML::Document& doc) const;\n", name);

    // Операторы сравнения
    // println(ss, "    // Операторы сравнения");
    // println(ss, "    bool operator==(const {}& other) const;", name);
    // println(ss, "    bool operator!=(const {}& other) const;", name);

    // println(ss, "    constexpr auto operator<=>(const {}&) const = default;", ccName);
    println(ss, "}};\n");

    return ss.str();
}

// Дополним метод generateSourceCode в ComplexType
string ComplexType::generateSourceCode() const {
    std::stringstream ss;

#if 0
    // Конструкторы
    println(ss, "{0}::{0}() {{", name);

    // Инициализация полей со значениями по умолчанию
    for(const auto& field: fields) {
        // Можно добавить инициализацию по умолчанию
    }

    println(ss, "}}\n");

    // Метод toXmlNode с улучшенной обработкой
    println(ss, "XML::Element* {}::toXmlNode(XML::Document& doc) const {{", name);
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
                    println(ss, "    element->SetattrVal(\"{0}\", {0}.c_str());", field.name);
                } else {
                    println(ss, "    element->SetattrVal(\"{0}\", {0});", field.name);
                }
            } else {
                print(ss, "    if(");
                if(field.type == "std::string") {
                    print(ss, "!{0}.empty()", field.name);
                } else {
                    print(ss, "{}.has_value()", field.name);
                }
                println(ss, ") {{");
                print(ss, "        element->SetattrVal(\"{}\", ", field.name);
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
#endif

    return ss.str();
}

void Parser::parseSchema(const XML::Element* schemaElement) {
    // Парсим все дочерние элементы
    for(auto&& child: *schemaElement) {
        string_view name = child->name();
        if(testName(name, "xs:complexType"sv)) parseComplexType(child.get());
        else if(testName(name, "xs:element"sv)) parseElement(child.get());
        else if(testName(name, "xs:group"sv)) parseGroup(child.get());
        else if(testName(name, "xs:simpleType"sv)) parseSimpleType(child.get());
        else if(name.size()) println(std::cerr, "name {}", name);
    }
}

string Parser::getDocumentation(const XML::Element* element) const {
    if(const XML::Element* annotation = element->firstChildOf({"xs:annotation", "annotation"});
        annotation) {
        if(const XML::Element* documentation = annotation->firstChildOf({"xs:documentation", "documentation"});
            documentation && documentation->text().size()) {
            return trim(documentation->text());
        }
    }

    string documentation;
    element = element->sibling(-1);
    while(element && element->name().empty()) {
        if(auto text = element->text(); text.starts_with("<!--") && text.ends_with("-->")) {
            text.remove_suffix(3);
            text.remove_prefix(4);
            documentation = text + documentation;
        }
        element = element->sibling(-1);
    }

    return documentation;
}

string Parser::convertXsdTypeToCpp(string_view xsdType, bool* isb) const {
    // Проверяем в карте типов
    auto it = typeMap.find(xsdType);
    if(it != typeMap.end())
        return (isb ? *isb = true : true),
               string{it->second};

    // Если тип не найден, проверяем, является ли он пользовательским типом
    // Удаляем префикс пространства имен, если есть
    size_t colonPos = xsdType.find(':');
    string_view typeName = (colonPos != std::string::npos)
        ? xsdType.substr(++colonPos)
        : xsdType;

    // Проверяем, является ли это перечислением
    for(const auto& enumType: enums)
        if(enumType.name == typeName)
            return string{typeName};

    // Проверяем, является ли это complexType
    for(const auto& complexType: complexTypes)
        if(complexType.name == typeName)
            return string{typeName};

    // Если не нашли, возвращаем как есть (будет сгенерирован класс)
    typeName.remove_suffix(4);
    return string{typeName}; // sanitizeName(typeName);
}

string Parser::sanitizeName(string_view name) {
    string ret{name};

    if(ret == "register"sv) return ret + '_';

    // Заменяем недопустимые символы
    for(char& c: ret)
        if("-.:"sv.contains(c)) c = '_';

    // Убеждаемся, что имя начинается с буквы
    if(!ret.empty() && isdigit(ret.front()))
        ret.insert(ret.begin(), '_');
    if(ret.ends_with("Type"sv))
        ret.resize(ret.size() - 4);

    return ret;
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
string Parser::trim(string_view str) {
    size_t first = str.find_first_not_of(" \t\n\r"sv);
    if(first == std::string::npos) return "";

    size_t last = str.find_last_not_of(" \t\n\r"sv);
    return string{str.substr(first, (last - first + 1))};
}

// Обновленный метод parseAttributes
void Parser::parseAttributes(const XML::Element* element, ComplexType& complexType) {

    for(auto&& child: *element) {
        string_view childName = child->name();

        if(testName(childName, "xs:attrVal"sv)) {
            Field field;
            field.isAttribute = true;

            // Имя атрибута

            if(string_view name = child->attrVal("name"); name.size()) {
                field.name = toCamelCase(sanitizeName(name));
            } else {
                continue; // Пропускаем атрибуты без имени
            }

            // Тип атрибута

            if(string_view type = child->attrVal("type"); type.size()) {
                field.type = convertXsdTypeToCpp(type);
            } else {
                // Проверяем встроенный тип

                if(const XML::Element* simpleType = child->firstChildOf({"xs:simpleType", "simpleType"});
                    simpleType) {
                    if(const XML::Element* restriction = simpleType->firstChildOf({"xs:restriction", "restriction"});
                        restriction) {

                        if(string_view base = restriction->attrVal("base"); base.size()) {
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
            field.documentation = getDocumentation(child.get());

            // Обязательность

            if(string_view use = child->attrVal("use"); use.size()) {
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

            if(string_view defaultValue = child->attrVal("default"); defaultValue.size()) {
                field.documentation += "\n[По умолчанию: " + std::string(defaultValue) + "]";
            }

            // Фиксированное значение

            if(string_view fixedValue = child->attrVal("fixed"); fixedValue.size()) {
                field.documentation += "\n[Фиксированное значение: " + std::string(fixedValue) + "]";
            }

            complexType.fields.push_back(field);
        }
        // Обработка групп атрибутов
        else if(testName(childName, "xs:attrValGroup"sv)) {
            if(string_view ref = child->attrVal("ref"); ref.size()) {
                println("  Информация: ссылка на группу атрибутов '{}' - требуется предварительное определение", ref);
            }
        }
    }
}

void Parser::parseSequenceElements(const XML::Element* sequence, ComplexType& complexType) {
    if(!sequence) return;
    // Обрабатываем все дочерние элементы sequence
    for(auto&& child: *sequence) {
        string_view elementName = child->name();
        if(testName(elementName, "xs:element"sv)) { // Элемент
            Field field;
            parseElementDetails(child.get(), field);
            if(!field.name.empty())
                complexType.fields.emplace_back(std::move(field));
        } else if(testName(elementName, "xs:group"sv)) { // Группа элементов
            parseGroupReference(child.get(), complexType);
        } else if(testName(elementName, "xs:sequence"sv)) { // Последовательность внутри последовательности (вложенная)
            parseSequenceElements(child.get(), complexType);
        } else if(testName(elementName, "xs:choice"sv)) { // Выбор
            parseChoiceElements(child.get(), complexType);
        } else if(testName(elementName, "xs:all"sv)) { // Все элементы
            parseAllElements(child.get(), complexType);
        } else if(testName(elementName, "xs:any"sv)) { // Любой элемент
            // Пропускаем xs:any - сложно отобразить на статический C++ код
            println("  Предупреждение: элемент <any> не поддерживается, пропускаем");
        }
    }
}

void Parser::parseChoiceElements(const XML::Element* choice, ComplexType& complexType) {

    if(!choice) return;

    // Для choice мы создаем специальное поле с вариантами
    // В реальной реализации можно создать union или variant

    println("  Предупреждение: элемент <choice> требует ручной обработки");

    // Временная реализация - обрабатываем как последовательность

    Field vField;

    vField.type = "// std::variant<";

    for(auto&& child: *choice) {
        string_view elementName = child->name();

        if(testName(elementName, "xs:element")) {
            Field field;
            field.isAttribute = false;
            parseElementDetails(child.get(), field);
            // Для choice отмечаем поле как опциональное
            field.isOptional = true;
            field.minOccurs = 0;
            field.maxOccurs = 1;
            vField.name += field.name + '_';
            vField.type += field.type + ", ";
            if(field.documentation.size())
                vField.documentation += field.documentation + '\n';
        }
    }

    if(!vField.name.empty()) {
        if(vField.documentation.size()) vField.documentation.pop_back();
        vField.type.pop_back();
        vField.type.back() = '>';
        complexType.fields.push_back(vField);
    }
}

void Parser::parseAllElements(const XML::Element* all,
    ComplexType& complexType) {
    if(!all) return;

    // xs:all означает, что все элементы могут быть в любом порядке,
    // но каждый может появляться только 0 или 1 раз

    for(auto&& child: *all) {
        string_view elementName = child->name();
        if(testName(elementName, "xs:element")) {
            Field field;
            field.isAttribute = false;

            parseElementDetails(child.get(), field);

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

void Parser::parseGroupReference(const XML::Element* groupRef, ComplexType& complexType) {
    if(!groupRef) return;

    string_view refName = groupRef->attrVal("ref");
    if(refName.empty()) {
        println(std::cerr, "  Ошибка: элемент group без атрибута ref");
        return;
    }

    if(auto it = groups.find(refName); it == groups.end())
        println("  Информация: ссылка на группу '{}' - группы требуют предварительного определения", refName);
    else
        complexType.fields.append_range(it->second.fields);

    // В полной реализации здесь нужно найти определение группы
    // и добавить все её элементы в complexType
}

void Parser::parseElementDetails(const XML::Element* elementNode, Field& field) {

    // Получаем имя элемента
    if(string_view name = elementNode->attrVal("name");
        name.size()) {
        field.name = sanitizeName(name);
    } else {
        // Элемент может быть анонимным (inline type)
        // Генерируем уникальное имя
        static int anonymousCounter = 0;
        field.name = "anonymousElement_" + std::to_string(anonymousCounter++);
    }

    // Получаем тип элемента
    if(string_view typeAttr = elementNode->attrVal("type");
        typeAttr.size()) {
        bool isbuiltin{};
        field.type = convertXsdTypeToCpp(typeAttr, &isbuiltin);
        if(!isbuiltin) field.type = toCamelCase(field.type);
    } else {
        // Проверяем встроенный тип

        if(const XML::Element* simpleType = elementNode->firstChildOf({"xs:simpleType", "simpleType"});
            simpleType) {
            // Обрабатываем встроенный простой тип
            field.type = "std::string"; // По умолчанию

            if(const XML::Element* restriction = simpleType->firstChildOf({"xs:restriction", "restriction"});
                restriction) {
                if(string_view base = restriction->attrVal("base"); base.size()) {
                    bool isbuiltin{};
                    field.type = convertXsdTypeToCpp(base, &isbuiltin);
                    if(!isbuiltin) field.type = toCamelCase(field.type);
                }
            }
        } else if(const XML::Element* complexTypeElem = elementNode->firstChildOf({"xs:complexType", "complexType"});
            complexTypeElem) {
            const_cast<XML::Element*>(complexTypeElem)->attributes.emplace_back("name"sv, toCamelCase(field.name));
            parseComplexType(complexTypeElem);
            field.type = complexTypes.back().name;
#if 0
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
#endif
        } else {
            // Тип по умолчанию
            field.type = "std::string";
        }
    }

    // Получаем документацию
    field.documentation = getDocumentation(elementNode);

    // Обрабатываем minOccurs и maxOccurs
    if(string_view minOccurs = elementNode->attrVal("minOccurs"); minOccurs.size()) {
        std::from_chars(minOccurs.begin(), minOccurs.end(), field.minOccurs);
        field.isOptional = (field.minOccurs == 0);
    } else {
        // По умолчанию minOccurs = 1
        field.minOccurs = 1;
        field.isOptional = false;
    }

    if(string_view maxOccurs = elementNode->attrVal("maxOccurs"); maxOccurs.size()) {
        if(maxOccurs == "unbounded"sv) {
            field.maxOccurs = -1; // unbounded
        } else {
            std::from_chars(maxOccurs.begin(), maxOccurs.end(), field.maxOccurs);
        }
    } else {
        // По умолчанию maxOccurs = 1
        field.maxOccurs = 1;
    }

    // Обрабатываем значение по умолчанию
    if(string_view defaultValue = elementNode->attrVal("default"); defaultValue.size()) {
        field.documentation += "\n[По умолчанию: " + std::string(defaultValue) + "]";
    }

    // Обрабатываем фиксированное значение
    if(string_view fixedValue = elementNode->attrVal("fixed"); fixedValue.size()) {
        field.documentation += "\n[Фиксированное значение: " + std::string(fixedValue) + "]";
    }

    // Обрабатываем nillable
    if(string_view nillable = elementNode->attrVal("nillable"); nillable == "true"sv) {
        field.isOptional = true;
        field.minOccurs = 0;
    }
}

void Parser::handleComplexContent(const XML::Element* complexContent,
    ComplexType& complexType) {
    if(!complexContent) return;

    // Обрабатываем extension (наследование)

    if(const XML::Element* extension = complexContent->firstChildOf({"xs:extension", "extension"});
        extension) {

        if(string_view base = extension->attrVal("base"); base.size()) {
            complexType.baseType = convertXsdTypeToCpp(base);
        }

        // Обрабатываем содержимое extension

        if(const XML::Element* sequence = extension->firstChildOf({"xs:sequence", "sequence"});
            sequence) {
            parseSequenceElements(sequence, complexType);
        }

        // Обрабатываем атрибуты в extension
        parseAttributes(extension, complexType);
    }

    // Обрабатываем restriction

    if(const XML::Element* restriction = complexContent->firstChildOf({"xs:restriction", "restriction"});
        restriction) {
        // Обработка restriction (более сложная)
        println("  Предупреждение: complexContent/restriction требует специальной обработки");
    }
}

void Parser::handleSimpleContent(const XML::Element* simpleContent,
    ComplexType& complexType) {
    if(!simpleContent) return;

    // simpleContent используется, когда complexType содержит только текст и атрибуты

    // Обрабатываем extension

    if(const XML::Element* extension = simpleContent->firstChildOf({"xs:extension", "extension"});
        extension) {

        if(string_view base = extension->attrVal("base"); base.size()) {
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

    if(const XML::Element* restriction = simpleContent->firstChildOf({"xs:restriction", "restriction"});
        restriction) {

        if(std::string_view base = restriction->attrVal("base"); base.size()) {
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
    println(/*std::cout, */">>>{}", typeName);

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

} // namespace Xsd
