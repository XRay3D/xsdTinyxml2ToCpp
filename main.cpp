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
        std::cerr << "Использование: " << argv[0] << " <xsd_file> [output_dir]" << std::endl;
        return 1;
    }

    std::string xsdFile = argv[1];
    std::string outputDir = "generated";

    if(argc > 2) outputDir = argv[2];

    try {
        Xsd::Parser parser;

        // Парсим XSD схему
        if(!parser.parse(xsdFile)) {
            std::cerr << "Ошибка при парсинге XSD схемы" << std::endl;
            return 1;
        }

        // Выводим информацию о схеме
        parser.printSummary();

        // Генерируем C++ код
        if(!parser.generateCppCode(outputDir, "Generated")) {
            std::cerr << "Ошибка при генерации C++ кода" << std::endl;
            return 1;
        }

        std::cout << "\nГенерация завершена успешно!" << std::endl;
        std::cout << "Сгенерированные файлы:" << std::endl;
        std::cout << "  - " << outputDir << "/Enums.h" << std::endl;
        std::cout << "  - " << outputDir << "/Enums.cpp" << std::endl;
        std::cout << "  - " << outputDir << "/Types.h" << std::endl;
        std::cout << "  - " << outputDir << "/Types.cpp" << std::endl;
        std::cout << "  - " << outputDir << "/CMakeLists.txt" << std::endl;

    } catch(const std::exception& e) {
        std::cerr << "Исключение: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
