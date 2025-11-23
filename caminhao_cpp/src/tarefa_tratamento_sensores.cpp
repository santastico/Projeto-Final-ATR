#include "tarefas.h"
#include "Buffer_Circular.h"

#include <mqtt/async_client.h>
#include <nlohmann/json.hpp>

#include <iostream>
#include <string>
#include <deque>
#include <mutex>
#include <cmath>    // std::round

using json = nlohmann::json;

namespace atr {

// ---------------------------------------------------------------------
// Variáveis globais usadas pela tarefa
// (armazenam o que o main passar em tratamento_sensores)
// ---------------------------------------------------------------------

// ponteiros para os buffers criados no main
static BufferCircular<std::string>* buffer_posicao_bruta   = nullptr;
static BufferCircular<std::string>* buffer_posicao_tratada = nullptr;

// ponteiro para o mutex criado no main
static std::mutex* mtx_posicao_tratada_ptr = nullptr;

// id do caminhão (vem do main)
static int caminhao_id_global = 1;

// ordem da média móvel (quantas amostras usar)
static const std::size_t ORDEM_MEDIA = 5;

// filas para armazenar as últimas amostras inteiras
static std::deque<int> janela_x;
static std::deque<int> janela_y;
static std::deque<int> janela_ang;
static std::deque<int> janela_temp;

// ---------------------------------------------------------------------
// Função chamada pelo main para configurar a tarefa
// ---------------------------------------------------------------------

void tratamento_sensores(BufferCircular<std::string>* buffer_posicao_bruta_param,
                         BufferCircular<std::string>* buffer_posicao_tratada_param,
                         std::mutex&     mtx_posicao_tratada,
                         int             caminhao_id)
{
    // guarda os ponteiros / valores em variáveis globais
    buffer_posicao_bruta    = buffer_posicao_bruta_param;
    buffer_posicao_tratada  = buffer_posicao_tratada_param;
    mtx_posicao_tratada_ptr = &mtx_posicao_tratada;
    caminhao_id_global      = caminhao_id;

    std::cout << "[tratamento_sensores] Configurado para caminhao_id = "
              << caminhao_id_global << std::endl;
}


// ---------------------------------------------------------------------
// Função auxiliar: média móvel com inteiros
// (soma inteira / número de amostras)
// ---------------------------------------------------------------------

static int media_movel(std::deque<int>& janela, int novo_valor)
{
    janela.push_back(novo_valor);

    // mantém no máximo ORDEM_MEDIA amostras
    if (janela.size() > ORDEM_MEDIA) {
        janela.pop_front();
    }

    int soma = 0;
    for (int v : janela) {
        soma += v;
    }

    int quantidade = static_cast<int>(janela.size());
    if (quantidade == 0) return 0;

    // média inteira (se quiser “arredondar”, pode usar double + round)
    return soma / quantidade;
}


// ---------------------------------------------------------------------
// Função que processa UMA mensagem JSON vinda do simulador
// - escreve BRUTO no buffer_posicao_bruta
// - filtra, escreve FILTRADO no buffer_posicao_tratada
// ---------------------------------------------------------------------

static void processar_mensagem(const std::string& texto_json)
{
    if (!buffer_posicao_bruta || !buffer_posicao_tratada || !mtx_posicao_tratada_ptr) {
        std::cerr << "[tratamento_sensores] ERRO: tarefa nao configurada.\n";
        return;
    }

    // 1) grava JSON BRUTO no buffer_posicao_bruta
    if (!buffer_posicao_bruta->escrever(texto_json)) {
        std::cerr << "[tratamento_sensores] buffer_posicao_bruta CHEIO, amostra descartada.\n";
    }

    // 2) converte o texto para JSON
    json dados = json::parse(texto_json, nullptr, false);
    if (dados.is_discarded()) {
        std::cerr << "[tratamento_sensores] JSON invalido.\n";
        return;
    }

    // opcional: confere se truck_id bate com o caminhao_id_global
    int truck_id_json = 0;
    if (dados.contains("truck_id")) {
        if (dados["truck_id"].is_number_integer()) {
            truck_id_json = dados["truck_id"].get<int>();
        } else if (dados["truck_id"].is_string()) {
            try {
                truck_id_json = std::stoi(dados["truck_id"].get<std::string>());
            } catch (...) {
                truck_id_json = 0;
            }
        }
    }


    // 3) lê valores BRUTOS (posições e ângulo)
    //    - podem vir como double no JSON, então fazemos round -> int
    double x_lido   = dados.value("i_posicao_x", 0.0);
    double y_lido   = dados.value("i_posicao_y", 0.0);
    double ang_lido = dados.value("i_angulo_x",  0.0);
    double temp_lido = dados.value("i_temperatura", 0.0);

    double x_bruto   = static_cast<int>(std::round(x_lido));
    double y_bruto   = static_cast<int>(std::round(y_lido));
    double ang_bruto = static_cast<int>(std::round(ang_lido));
    double temp_bruto = static_cast<int>(std::round(temp_lido));

    // 4) aplica média móvel inteira
    int x_filtrado   = media_movel(janela_x,   x_bruto);
    int y_filtrado   = media_movel(janela_y,   y_bruto);
    int ang_filtrado = media_movel(janela_ang, ang_bruto);
    int temp_filtrada = media_movel(janela_temp, temp_bruto);

    auto arred3 = [](double v) {
        return std::round(v * 1000.0) / 1000.0;
    };

    // 5) monta JSON FILTRADO copiando tudo do original e
    //    acrescentando campos filtrados como inteiros
    json dados_filtrados = dados;
    dados_filtrados["f_posicao_x"]  = arred3(x_filtrado);
    dados_filtrados["f_posicao_y"]  = arred3(y_filtrado);
    dados_filtrados["f_angulo_x"]   = arred3(ang_filtrado);
    dados_filtrados["f_temperatura"] = arred3(temp_filtrada);
    dados_filtrados["ordem_media"]  = static_cast<int>(ORDEM_MEDIA);

    std::string texto_filtrado = dados_filtrados.dump();

    // 6) escreve JSON FILTRADO no buffer_posicao_tratada
    {
        std::lock_guard<std::mutex> trava(*mtx_posicao_tratada_ptr);
        if (!buffer_posicao_tratada->escrever(texto_filtrado)) {
            std::cerr << "[tratamento_sensores] buffer_posicao_tratada CHEIO, amostra TRATADA descartada.\n";
        }
    }

    std::cout << "[BRUTO->BUFFER] " << texto_json << std::endl;
    std::cout << "[FILTRADO->BUFFER] " << texto_filtrado << std::endl;
}


// ---------------------------------------------------------------------
// Função que roda na thread (loop principal da tarefa)
// ---------------------------------------------------------------------

void tarefa_tratamento_sensores_run(const std::string& broker_uri)
{
    if (!buffer_posicao_bruta || !buffer_posicao_tratada || !mtx_posicao_tratada_ptr) {
        std::cerr << "[tratamento_sensores] ERRO: chame tratamento_sensores() antes de criar a thread.\n";
        return;
    }

    std::string id_cliente = "tratamento_sensores_all";
    const std::string topico = "atr/+/sensor/raw";   // único tópico, com wildcard

    mqtt::async_client cliente(broker_uri, id_cliente);
    mqtt::connect_options opcoes;
    opcoes.set_clean_session(true);

    try {
        std::cout << "[tratamento_sensores] Conectando em " << broker_uri << "...\n";
        cliente.connect(opcoes)->wait();
        std::cout << "[tratamento_sensores] Conectado.\n";

        cliente.start_consuming();
        cliente.subscribe(topico, 1)->wait();
        std::cout << "[tratamento_sensores] Assinado topico " << topico << "\n";

        while (true) {
            auto msg = cliente.consume_message();
            if (!msg) {
                if (!cliente.is_connected()) {
                    std::cerr << "[tratamento_sensores] Conexao MQTT perdida.\n";
                    break;
                }
                continue;
            }
            std::string texto = msg->to_string();
            processar_mensagem(texto);
        }

        cliente.stop_consuming();
        cliente.disconnect()->wait();
    }
    catch (const std::exception& e) {
        std::cerr << "[tratamento_sensores] ERRO MQTT: " << e.what() << "\n";
    }

    std::cout << "[tratamento_sensores] Thread encerrada.\n";
}

} // namespace atr
