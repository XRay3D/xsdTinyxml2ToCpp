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

    println(std::cout, "Парсинг завершен успешно!");
    println(std::cout, "Найдено перечислений: {}", enums.size());
    println(std::cout, "Найдено complexType: {}", complexTypes.size());
    println(std::cout, "Найдено элементов: {}", elements.size());

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

    println(std::cout, "Код успешно сгенерирован в директории: {}", outputDir);
    return true;
}

void Parser::clear() {
    enums.clear();
    complexTypes.clear();
    elements.clear();
    doc.root.clear();
}
#if 0
void Parser::parseComplexType(const XML::Element* element) {
    ComplexType complexType;

    // Получаем имя типа
    string_view name = element->attrVal("name");
    if(name.empty()) {
        println(std::cerr, "ComplexType без имени, пропускаем");
        return;
    }
    complexType.name = sanitizeName(name);

    // Получаем документацию
    complexType.documentation = getDocumentation(element);

    // Проверяем, является ли абстрактным
    string_view abstract = element->attrVal("abstract");
    if(abstract && abstract=="true"sv) {
        complexType.isAbstract = true;
    }

    // Ищем sequence, choice, all или complexContent/simpleContent
    const XML::Element* sequence = element->firstChild("xs:sequence");
    if(!sequence) sequence = element->firstChild("sequence");

    const XML::Element* choice = element->firstChild("xs:choice");
    if(!choice) choice = element->firstChild("choice");

    const XML::Element* all = element->firstChild("xs:all");
    if(!all) all = element->firstChild("all");

    const XML::Element* complexContent = element->firstChild("xs:complexContent");
    if(!complexContent) complexContent = element->firstChild("complexContent");

    const XML::Element* simpleContent = element->firstChild("xs:simpleContent");
    if(!simpleContent) simpleContent = element->firstChild("simpleContent");

    // Обработка атрибутов
    const XML::Element* attrValGroup = element->firstChild("xs:attrValGroup");
    if(!attrValGroup) attrValGroup = element->firstChild("attrValGroup");

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

void Parser::parseSequenceElements(const XML::Element* sequence, ComplexType& complexType) {
    for(const XML::Element* child = sequence->firstChild();
        child != nullptr;
        child = child->NextSiblingElement()) {

        string_view childName = child->Name();

        if(testName(childName, "xs:element"sv)) {

            Field field;
            field.isAttribute = false;

            // Имя элемента
            string_view name = child->attrVal("name");
            if(name) {
                field.name = sanitizeName(name);
            }

            // Тип элемента
            string_view type = child->attrVal("type");
            if(type) {
                field.type = convertXsdTypeToCpp(type);
            }

            // Документация
            field.documentation = getDocumentation(child);

            // Min/max occurs
            string_view minOccurs = child->attrVal("minOccurs");
            if(minOccurs) {
                field.minOccurs = atoi(minOccurs);
                field.isOptional = (field.minOccurs == 0);
            }

            string_view maxOccurs = child->attrVal("maxOccurs");
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

void Parser::parseAttributes(const XML::Element* element,
    ComplexType& complexType) {
    for(const XML::Element* child = element->firstChild();
        child != nullptr;
        child = child->NextSiblingElement()) {

        string_view childName = child->Name();

        if(testName(childName, "xs:attrVal"sv)) {

            Field field;
            field.isAttribute = true;

            // Имя атрибута
            string_view name = child->attrVal("name");
            if(name) {
                field.name = sanitizeName(name);
            }

            // Тип атрибута
            string_view type = child->attrVal("type");
            if(type) {
                field.type = convertXsdTypeToCpp(type);
            }

            // Обязательность
            string_view use = child->attrVal("use");
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
    const XML::Element* restriction = element->firstChild("xs:restriction");
    if(!restriction)
        restriction = element->firstChild("restriction");

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
void Parser::parseComplexType(const XML::Element* element) {
    ComplexType complexType;

    // Получаем имя типа
    string_view name = element->attrVal("name");
    if(name.empty()) {
        // Анонимный тип - генерируем имя
        static int anonymousComplexCounter = 0;
        complexType.ccName = complexType.name = "AnonymousComplexType_" + std::to_string(anonymousComplexCounter++);
    } else {
        complexType.oprigName = name;
        complexType.name = sanitizeName(name);
        complexType.ccName = toCamelCase(sanitizeName(name));
    }

    // Получаем документацию
    complexType.documentation = getDocumentation(element);

    // Проверяем, является ли абстрактным
    complexType.isAbstract = element->attrVal("abstract") == "true"sv;

    // Проверяем complexContent/simpleContent
    const XML::Element* complexContent = element->firstChild("xs:complexContent");
    if(!complexContent) complexContent = element->firstChild("complexContent");

    const XML::Element* simpleContent = element->firstChild("xs:simpleContent");
    if(!simpleContent) simpleContent = element->firstChild("simpleContent");

    if(complexContent) {
        handleComplexContent(complexContent, complexType);
    } else if(simpleContent) {
        handleSimpleContent(simpleContent, complexType);
    } else {
        // Обычный complexType с последовательностью/выбором/всем
        const XML::Element* sequence = element->firstChild("xs:sequence");
        if(!sequence) sequence = element->firstChild("sequence");

        const XML::Element* choice = element->firstChild("xs:choice");
        if(!choice) choice = element->firstChild("choice");

        const XML::Element* all = element->firstChild("xs:all");
        if(!all) all = element->firstChild("all");

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
        if(sequence) {
            parseSequenceElements(sequence, complexType);
        } else if(choice) {
            parseChoiceElements(choice, complexType);
        } else if(all) {
            parseAllElements(all, complexType);
        } else {
            // Проверяем, есть ли простой контент
            const XML::Element* simpleContent = element->firstChild("xs:simpleContent");
            if(!simpleContent) simpleContent = element->firstChild("simpleContent");

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
        println(std::cout, "  Предупреждение: тип '{}' уже существует, пропускаем дубликат", complexType.name);
    }
}

void Parser::parseElement(const XML::Element* element) {
    Element xsdElement;

    if(string_view name = element->attrVal("name"); name.size())
        xsdElement.name = sanitizeName(name);

    if(string_view type = element->attrVal("type"); type.size()) {
        xsdElement.type = type;

        // Проверяем, является ли complexType
        // В реальном парсере нужно проверять по списку complexTypes
        if(xsdElement.type.find(":") != std::string::npos)
            xsdElement.isComplex = true;
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

    if(documentation.size())
        println(ss, "/*\n{}\n*/", documentation);

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

    if(!documentation.empty()) {
        println(ss, "/**\n * {}\n */", documentation);
    }

    // println(ss, R"([[="{}"sv]] //)", name2);
    println(ss, "struct {} {{", ccName);

    // Поля
    for(const auto& field: fields) {
        if(field.documentation.size())
            println(ss, "    // {}", field.documentation);

        string type = field.type;

        // Если поле может встречаться много раз
        if(field.maxOccurs == -1 || field.maxOccurs > 1)
            type = "std::vector<" + type + ">";

        // Если поле опциональное (minOccurs == 0)
        // if(field.isOptional && field.maxOccurs == 1)
        // type = "std::optional<" + type + ">";

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

    println(ss, "}};\n");

    return ss.str();
}

// Дополним метод generateSourceCode в ComplexType
string ComplexType::generateSourceCode() const {
#if 0
    std::stringstream ss;

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

    return ss.str();

#endif
}

void Parser::parseSchema(const XML::Element* schemaElement) {
    // Парсим все дочерние элементы

    for(auto&& child: *schemaElement) {
        string_view elementName = child->name();
        /*  */ if(testName(elementName, "xs:simpleType"sv)) {
            parseSimpleType(child.get());
        } else if(testName(elementName, "xs:complexType"sv)) {
            parseComplexType(child.get());
        } else if(testName(elementName, "xs:element"sv)) {
            parseElement(child.get());
        } else {
            println(std::cerr, "{}", elementName);
        }
    }
}

string Parser::getDocumentation(const XML::Element* element) const {
    const XML::Element* annotation = element->firstChild("xs:annotation");
    if(!annotation) annotation = element->firstChild("annotation");

    if(annotation) {
        const XML::Element* documentation = annotation->firstChild("xs:documentation");
        if(!documentation) documentation = annotation->firstChild("documentation");

        if(documentation && documentation->text().size()) {
            return trim(documentation->text());
        }
    }

    return "";
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
    size_t first = str.find_first_not_of(" \t\n\r");
    if(first == std::string::npos) return "";

    size_t last = str.find_last_not_of(" \t\n\r");
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
                const XML::Element* simpleType = child->firstChild("xs:simpleType");
                if(!simpleType) simpleType = child->firstChild("simpleType");

                if(simpleType) {
                    const XML::Element* restriction = simpleType->firstChild("xs:restriction");
                    if(!restriction) restriction = simpleType->firstChild("restriction");

                    if(restriction) {

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
                if(!field.documentation.empty()) {
                    field.documentation += " ";
                }
                field.documentation += "[По умолчанию: " + std::string(defaultValue) + "]";
            }

            // Фиксированное значение

            if(string_view fixedValue = child->attrVal("fixed"); fixedValue.size()) {
                if(!field.documentation.empty()) {
                    field.documentation += " ";
                }
                field.documentation += "[Фиксированное значение: " + std::string(fixedValue) + "]";
            }

            complexType.fields.push_back(field);
        }
        // Обработка групп атрибутов
        else if(testName(childName, "xs:attrValGroup"sv)) {

            if(string_view ref = child->attrVal("ref"); ref.size()) {
                println(std::cout, "  Информация: ссылка на группу атрибутов '{}' - требуется предварительное определение", ref);
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
            if(!field.name.empty()) complexType.fields.push_back(field);
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
            println(std::cout, "  Предупреждение: элемент <any> не поддерживается, пропускаем");
        }
    }
}

void Parser::parseChoiceElements(const XML::Element* choice, ComplexType& complexType) {

    if(!choice) return;

    // Для choice мы создаем специальное поле с вариантами
    // В реальной реализации можно создать union или variant

    println(std::cout, "  Предупреждение: элемент <choice> требует ручной обработки");

    // Временная реализация - обрабатываем как последовательность

    Field vField;

    vField.type = "std::variant<";

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
        }
    }

    if(!vField.name.empty()) {
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

void Parser::parseGroupReference(const XML::Element* groupRef,
    ComplexType& complexType) {
    if(!groupRef) return;

    string_view refName = groupRef->attrVal("ref");
    if(refName.empty()) {
        println(std::cerr, "  Ошибка: элемент group без атрибута ref");
        return;
    }

    println(std::cout, "  Информация: ссылка на группу '{}' - группы требуют предварительного определения", refName);

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
        const XML::Element* simpleType = elementNode->firstChild("xs:simpleType");
        if(!simpleType) simpleType = elementNode->firstChild("simpleType");

        const XML::Element* complexTypeElem = elementNode->firstChild("xs:complexType");
        if(!complexTypeElem) complexTypeElem = elementNode->firstChild("complexType");

        if(simpleType) {
            // Обрабатываем встроенный простой тип
            field.type = "std::string"; // По умолчанию

            const XML::Element* restriction = simpleType->firstChild("xs:restriction");
            if(!restriction) restriction = simpleType->firstChild("restriction");
            if(restriction) {
                if(string_view base = restriction->attrVal("base"); base.size()) {
                    bool isbuiltin{};
                    field.type = convertXsdTypeToCpp(base, &isbuiltin);
                    if(!isbuiltin) field.type = toCamelCase(field.type);
                }
            }
        } else if(complexTypeElem) {
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
        if(!field.documentation.empty()) {
            field.documentation += " ";
        }
        field.documentation += "[По умолчанию: " + std::string(defaultValue) + "]";
    }

    // Обрабатываем фиксированное значение
    if(string_view fixedValue = elementNode->attrVal("fixed"); fixedValue.size()) {
        if(!field.documentation.empty()) {
            field.documentation += " ";
        }
        field.documentation += "[Фиксированное значение: " + std::string(fixedValue) + "]";
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
    const XML::Element* extension = complexContent->firstChild("xs:extension");
    if(!extension) extension = complexContent->firstChild("extension");

    if(extension) {

        if(string_view base = extension->attrVal("base"); base.size()) {
            complexType.baseType = convertXsdTypeToCpp(base);
        }

        // Обрабатываем содержимое extension
        const XML::Element* sequence = extension->firstChild("xs:sequence");
        if(!sequence) sequence = extension->firstChild("sequence");

        if(sequence) {
            parseSequenceElements(sequence, complexType);
        }

        // Обрабатываем атрибуты в extension
        parseAttributes(extension, complexType);
    }

    // Обрабатываем restriction
    const XML::Element* restriction = complexContent->firstChild("xs:restriction");
    if(!restriction) restriction = complexContent->firstChild("restriction");

    if(restriction) {
        // Обработка restriction (более сложная)
        println(std::cout, "  Предупреждение: complexContent/restriction требует специальной обработки");
    }
}

void Parser::handleSimpleContent(const XML::Element* simpleContent,
    ComplexType& complexType) {
    if(!simpleContent) return;

    // simpleContent используется, когда complexType содержит только текст и атрибуты

    // Обрабатываем extension
    const XML::Element* extension = simpleContent->firstChild("xs:extension");
    if(!extension) extension = simpleContent->firstChild("extension");

    if(extension) {

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
    const XML::Element* restriction = simpleContent->firstChild("xs:restriction");
    if(!restriction) restriction = simpleContent->firstChild("restriction");

    if(restriction) {

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

} // namespace Xsd
