# ===============================
# Configuração principal do projeto
# ===============================
cmake_minimum_required(VERSION 3.10)
project(caminhao_embarcado LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ===============================
# Bibliotecas necessárias
# ===============================
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(nlohmann_json REQUIRED) # requer nlohmann-json3-dev no sistema

# ===============================
# Tenta via pkg-config primeiro
# ===============================
set(HAVE_PAHO_PKGCONFIG OFF)
pkg_check_modules(PAHO_MQTTPP QUIET IMPORTED_TARGET paho-mqttpp3)
pkg_check_modules(PAHO_MQTT   QUIET IMPORTED_TARGET paho-mqtt3a)

if(PAHO_MQTTPP_FOUND AND PAHO_MQTT_FOUND)
    set(HAVE_PAHO_PKGCONFIG ON)
    message(STATUS "Paho via pkg-config encontrado.")
else()
    message(WARNING "pkg-config não encontrou paho-mqttpp3 / paho-mqtt3a. Tentando fallback manual em /usr/local e /usr.")
endif()

# ===============================
# Fallback manual se pkg-config falhar
# ===============================
if(NOT HAVE_PAHO_PKGCONFIG)
    # includes
    find_path(PAHO_MQTTPP_INCLUDE_DIR
        NAMES mqtt/async_client.h
        PATHS /usr/local/include /usr/include
    )
    find_path(PAHO_MQTT_C_INCLUDE_DIR
        NAMES MQTTClient.h
        PATHS /usr/local/include /usr/include
    )

    if(NOT PAHO_MQTTPP_INCLUDE_DIR OR NOT PAHO_MQTT_C_INCLUDE_DIR)
        message(FATAL_ERROR "Não foi possível localizar includes do Paho MQTT C/C++")
    endif()

    # libs (C++)
    find_library(PAHO_MQTTPP_LIBRARY
        NAMES paho-mqttpp3
        PATHS /usr/local/lib /usr/lib /usr/lib/x86_64-linux-gnu
    )
    # libs (C) — escolha a variant: paho-mqtt3a (async, sem SSL) OU paho-mqtt3as (com SSL)
    find_library(PAHO_MQTT_C_LIBRARY
        NAMES paho-mqtt3a paho-mqtt3as
        PATHS /usr/local/lib /usr/lib /usr/lib/x86_64-linux-gnu
    )

    if(NOT PAHO_MQTTPP_LIBRARY OR NOT PAHO_MQTT_C_LIBRARY)
        message(FATAL_ERROR "Não foi possível localizar bibliotecas do Paho MQTT C/C++")
    endif()

    # Guarda variáveis de include/link para usar depois
    set(PAHO_INCLUDE_DIRS ${PAHO_MQTTPP_INCLUDE_DIR} ${PAHO_MQTT_C_INCLUDE_DIR})
    set(PAHO_LIBRARIES ${PAHO_MQTTPP_LIBRARY} ${PAHO_MQTT_C_LIBRARY})
endif()

# ===============================
# Diretórios e fontes
# ===============================
include_directories(${CMAKE_SOURCE_DIR}/include)

file(GLOB SRC_FILES "src/*.cpp")
add_executable(caminhao_embarcado ${SRC_FILES})

# ===============================
# Linkagem
# ===============================
if(HAVE_PAHO_PKGCONFIG)
    target_link_libraries(caminhao_embarcado
        PRIVATE
            Threads::Threads
            PkgConfig::PAHO_MQTTPP
            PkgConfig::PAHO_MQTT
            nlohmann_json::nlohmann_json
    )
else()
    target_include_directories(caminhao_embarcado PRIVATE ${PAHO_INCLUDE_DIRS})
    target_link_libraries(caminhao_embarcado
        PRIVATE
            Threads::Threads
            ${PAHO_LIBRARIES}
            nlohmann_json::nlohmann_json
    )
endif()

message(STATUS "Compilando projeto caminhao_embarcado")
message(STATUS "Fontes: ${SRC_FILES}")
