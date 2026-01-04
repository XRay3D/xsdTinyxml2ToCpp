#include "XsdParser.h"
#include <iostream>

int main(int argc, const char* argv[]) {

    const char* argv_[]{
        argv[0],
        // "../example.xsd",
        "../CMSIS-SVD.xsd",
        ".",
    };
    argv = argv_;
    argc = 3;

    if(argc < 2) {
        std::println(std::cerr, "Использование: {} <xsd_file> [output_dir]", argv[0]);
        return 1;
    }

    std::string xsdFile = argv[1];
    std::string outputDir = "generated";

    if(argc > 2) outputDir = argv[2];

    try {
        Xsd::Parser parser;

        // Парсим XSD схему
        if(!parser.parse(xsdFile)) {
            std::println(std::cerr, "Ошибка при парсинге XSD схемы");
            return 1;
        }

        // Выводим информацию о схеме
        parser.printSummary();

        // Генерируем C++ код
        if(!parser.generateCppCode(outputDir, "Generated")) {
            std::println(std::cerr, "Ошибка при генерации C++ кода");
            return 1;
        }

        std::println(std::cout, "\nГенерация завершена успешно!");
        std::println(std::cout, "Сгенерированные файлы:");
        std::println(std::cout, "  - {}/Enums.h", outputDir);
        std::println(std::cout, "  - {}/Enums.cpp", outputDir);
        std::println(std::cout, "  - {}/Types.h", outputDir);
        std::println(std::cout, "  - {}/Types.cpp", outputDir);
        std::println(std::cout, "  - {}/CMakeLists.txt", outputDir);

    } catch(const std::exception& e) {
        std::println(std::cerr, "Исключение: {}", e.what());
        return 1;
    }

    return 0;
}
