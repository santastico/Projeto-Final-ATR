# Define a versão mínima do CMake.
cmake_minimum_required(VERSION 3.10)

# Define o nome do projeto (isso criará o executável com esse nome).
project(caminhao_embarcado)

# Define o padrão C++ que queremos usar.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Encontra a biblioteca de Threads (para std::thread)
find_package(Threads REQUIRED)

# --- INÍCIO DA CORREÇÃO ---
# O pacote 'libmosquittopp-dev' do Ubuntu 22.04 não fornece um
# arquivo de config, então 'find_package(Mosquittopp)' falha.
# Vamos encontrar os arquivos manualmente.

# 1. Encontra o arquivo de cabeçalho (mosquittopp.h)
find_path(MOSQUITOPP_INCLUDE_DIR NAMES mosquittopp.h
          PATHS /usr/include /usr/local/include)

# 2. Encontra o arquivo da biblioteca (libmosquittopp.so)
find_library(MOSQUITOPP_LIBRARY NAMES mosquittopp
             PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu)

# 3. Adiciona o diretório de cabeçalhos ao nosso projeto
if(MOSQUITOPP_INCLUDE_DIR)
    include_directories(${MOSQUITOPP_INCLUDE_DIR})
else()
    message(FATAL_ERROR "Nao foi possivel encontrar o cabecalho mosquittopp.h")
endif()
# --- FIM DA CORREÇÃO ---

# Informa ao CMake onde estão nossos arquivos de cabeçalho (.h).
include_directories(include)

# Encontra todos os nossos arquivos .cpp dentro da pasta "src".
file(GLOB SRC_FILES "src/*.cpp")

# Cria o executável final chamado "caminhao_embarcado"
add_executable(caminhao_embarcado ${SRC_FILES})

# Linka as bibliotecas necessárias ao nosso executável.
target_link_libraries(caminhao_embarcado
    PUBLIC
    Threads::Threads
    # Linka a biblioteca que encontramos manualmente
    ${MOSQUITOPP_LIBRARY}
)