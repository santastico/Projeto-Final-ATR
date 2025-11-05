#ifndef CONFIG_H
#define CONFIG_H

#include <string>

// O docker-compose define o nome do broker como "mqtt-broker"
const std::string BROKER_ADDRESS = "tcp://localhost:1883";
const int BROKER_PORT = 1883;

#endif