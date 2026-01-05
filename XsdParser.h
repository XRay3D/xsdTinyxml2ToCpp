#pragma once
#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Включаем tinyxml2
// #include "tinyxml2.h"

#include "xrxml.hpp"

namespace Xsd {

using namespace std::literals;

using std ::map;
using std ::string;
using std ::string_view;
using std ::vector;

// struct ReString {
//     string name;
//     string pattern;
// };

// Структура для представления XSD простого типа (enum)
struct Enum {
    string name;
    string ccName;
    string documentation; // Комментарии из <xs:annotation>
    vector<string> values;
    string baseType; // Базовый тип (string, int и т.д.)

    // Генерация C++ кода для перечисления
    string generateHeaderCode() const;
    string generateSourceCode() const;
};

// Структура для представления поля в complexType
struct Field {
    string name;
    string type;
    string documentation;
    bool isOptional{};
    int minOccurs{1};
    int maxOccurs{1};   // -1 означает unbounded
    bool isAttribute{}; // Является ли атрибутом
};

// Структура для представления XSD complexType
struct ComplexType {
    string origName;
    string name;
    string ccName;
    string documentation;
    vector<Field> fields;
    // vector<ComplexType> complexTypes_;
    string baseType; // Наследование
    bool isAbstract{};
    bool isRoot{};

    // Генерация C++ кода для структуры
    string generateHeaderCode() const;
    string generateSourceCode() const;
};

// Структура для представления XSD элемента
struct Element {
    string name;
    string type;
    string documentation;
    bool isComplex{};
};

// Основной класс парсера
class Parser {
public:
    Parser() = default;
    ~Parser();

    // Основные методы
    bool parse(const string& filename);
    bool generateCppCode(const string& outputDir, const string& namespaceName = "");

    // Геттеры
    const vector<Enum>& getEnums() const { return enums; }
    const vector<ComplexType>& getComplexTypes() const { return complexTypes; }
    const vector<Element>& getElements() const { return elements; }

    // Вспомогательные методы
    void clear();
    void printSummary() const;

private:
    // Данные
    // std::map<string, string> /*vector<ReString> */ reStrings;
    vector<Enum> enums;
    vector<ComplexType> complexTypes;
    // vector<ComplexType> groups;
    vector<Element> elements;
    map<string, ComplexType, std::less<>> groups;
    /*inline static const*/ map<string, string_view, std::less<>> typeMap{
        {"xs:string",                "std::string"sv               }, // Для преобразования XSD типов в C++
        {"xs:int",                   "int32_t"sv                   },
        {"xs:integer",               "int32_t"sv                   },
        {"xs:long",                  "int64_t"sv                   },
        {"xs:short",                 "int16_t"sv                   },
        {"xs:decimal",               "double"sv                    },
        {"xs:float",                 "float"sv                     },
        {"xs:double",                "double"sv                    },
        {"xs:boolean",               "bool"sv                      },
        {"xs:date",                  "std::string"sv               }, // Можно использовать std::chrono, но для простоты string},
        {"xs:dateTime",              "std::string"sv               },
        {"xs:time",                  "std::string"sv               },
        {"xs:base64Binary",          "std::vector<unsigned char>"sv},
        {"xs:hexBinary",             "std::vector<unsigned char>"sv},
        {"xs:anyURI",                "std::string"sv               },
        {"xs:QName",                 "std::string"sv               },
        {"xs:Name",                  "std::string"sv               },
        {"xs:normalizedString",      "std::string"sv               },
        {"xs:token",                 "std::string"sv               },
        {"xs:unsignedInt",           "uint32_t"sv                  },
        {"xs:unsignedLong",          "uint64_t"sv                  },
        {"xs:unsignedShort",         "uint16_t"sv                  },
        {"xs:positiveInteger",       "uint32_t"sv                  },
        {"xs:nonNegativeInteger",    "uint32_t"sv                  },
        {"scaledNonNegativeInteger", "uint32_t"sv                  },
    };

    // XML документ
    XML::Document doc;

    // Приватные методы парсинга
    void parseSimpleType(const XML::Element* element);
    bool parseComplexType(const XML::Element* element);
    void parseGroup(const XML::Element* element);
    void parseElement(const XML::Element* element);
    void parseSchema(const XML::Element* schemaElement);

    // Вспомогательные методы
    string getDocumentation(const XML::Element* element) const;
    string convertXsdTypeToCpp(string_view xsdType, bool* isb = {}) const;
    static string sanitizeName(string_view name);

    // Методы генерации кода
    // string generateEnumHeader(const Enum& enumType) const;
    // string generateEnumSource(const Enum& enumType) const;
    // string generateStructHeader(const ComplexType& complexType) const;
    // string generateStructSource(const ComplexType& complexType) const;
    // string generateSerializationCode(const ComplexType& complexType) const;
    // string generateDeserializationCode(const ComplexType& complexType) const;

    // Утилиты для работы со строками
    static string toCamelCase(const string& str);
    static string toUpperCase(string str);
    static string trim(string_view str);

    ////////////////////////////////////////
    // Дополним приватные методы в классе Parser
private:
    void parseAttributes(const XML::Element* element, ComplexType& complexType);

    void parseSequenceElements(const XML::Element* sequence, ComplexType& complexType);
    void parseChoiceElements(const XML::Element* choice, ComplexType& complexType);
    void parseAllElements(const XML::Element* all, ComplexType& complexType);
    void parseGroupReference(const XML::Element* groupRef, ComplexType& complexType);
    void parseElementDetails(const XML::Element* elementNode, Field& field);
    void handleComplexContent(const XML::Element* complexContent, ComplexType& complexType);
    void handleSimpleContent(const XML::Element* simpleContent, ComplexType& complexType);

    // Вспомогательные методы
    // bool isBuiltInType(const string& typeName) const;
    string extractLocalName(const string& qualifiedName) const;
    string getNamespacePrefix(const string& qualifiedName) const;
};

} // namespace Xsd
