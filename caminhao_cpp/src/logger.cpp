#include "tarefas.h"

#include <fstream>
#include <mutex>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <filesystem>
namespace atr {

static std::ofstream g_logfile;
static std::mutex    g_mtx_log;
static int           g_log_caminhao_id = 0;

// Função interna para abrir arquivo de log (chamada pelo coletor)
void iniciar_log_para_caminhao(int caminhao_id) {
    std::lock_guard<std::mutex> lock(g_mtx_log);

    g_log_caminhao_id = caminhao_id;

    if (g_logfile.is_open()) {
        g_logfile.close();
    }

    // Caminho dentro do container; monte ./output -> /app/output no docker-compose
    std::string path = "/app/output/cam_" + std::to_string(caminhao_id) + ".log";
    g_logfile.open(path, std::ios::trunc); // sempre recomeça do zero

    if (!g_logfile.is_open()) {
        std::cerr << "[logger] ERRO ao abrir arquivo de log: " << path << "\n";
    } else {
        std::cout << "[logger] Caixa-preta aberta em: " << path << "\n";
    }
}

void registrar_evento_log(
    int               caminhao_id,
    const char*       origem,
    const std::string& mensagem)
{
    std::lock_guard<std::mutex> lock(g_mtx_log);
    if (!g_logfile.is_open() || caminhao_id != g_log_caminhao_id) {
        return; // ainda não inicializado para este caminhão
    }

    using clock = std::chrono::system_clock;
    auto now    = clock::now();
    auto now_tt = clock::to_time_t(now);

    g_logfile << "[" << std::put_time(std::localtime(&now_tt), "%F %T") << "] "
              << "cam=" << caminhao_id
              << " origem=" << origem
              << " msg=" << mensagem
              << "\n";
    g_logfile.flush();
}

} // namespace atr
