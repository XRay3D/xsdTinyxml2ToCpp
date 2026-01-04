cmake_minimum_required(VERSION 3.16)
# https://msiter.ru/references/xsd-elements
project(XSD_TO_CPP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/bin)

file(GLOB SRC *.h *.cpp)

include(FetchContent)
FetchContent_Declare(
  xrxml
  GIT_REPOSITORY ${CMAKE_CURRENT_LIST_DIR}/../XRXml
  GIT_TAG master)
FetchContent_MakeAvailable(xrxml)

add_executable(XSD_TO_CPP ${SRC})

include_directories(bin)

target_link_libraries(XSD_TO_CPP PRIVATE XR::Xml)

include(GNUInstallDirs)
install(
  TARGETS XSD_TO_CPP
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

#[[

cmake_minimum_required(VERSION 3.10)
project(XsdParser)

set(CMAKE_CXX_STANDARD 17)

# Находим tinyxml2
find_package(tinyxml2 REQUIRED)

# Основная программа
add_executable(xsd_parser
    src/main.cpp
    src/XsdParser.cpp
)

target_include_directories(xsd_parser
    PRIVATE
        include
)

target_link_libraries(xsd_parser
    tinyxml2::tinyxml2
)

# Пример использования
add_executable(example
    examples/example_usage.cpp
    generated/Enums.cpp
    generated/Types.cpp
)

target_include_directories(example
    PRIVATE
        generated
)

target_link_libraries(example
    tinyxml2::tinyxml2
)


]]
