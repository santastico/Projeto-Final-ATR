#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <cstdlib> // Necessário para std::getenv

// Não usamos mais constante fixa para o endereço
// const std::string BROKER_ADDRESS = "tcp://localhost:1883"; // APAGUE ISSO

const int BROKER_PORT = 1883;

// Função inline para não dar erro de "multiple definition" no linker
inline std::string obter_broker_uri() {
    // Tenta ler a variável que definimos no docker-compose.yml
    const char* env_host = std::getenv("BROKER_HOST");
    
    // Se achou (estamos no Docker), usa o valor (ex: "infra_mina")
    // Se não achou (teste local), usa "localhost"
    std::string host = env_host ? env_host : "localhost";
    
    return "tcp://" + host + ":" + std::to_string(BROKER_PORT);
}

#endif
