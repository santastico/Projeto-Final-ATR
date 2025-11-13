#include <mqtt/async_client.h>
#include <nlohmann/json.hpp>

#include <deque>
#include <mutex>
#include <atomic>
#include <iostream>

using json = nlohmann::json;

namespace atr {

// ---------------------------------------------------------------------
// 1. Estado compartilhado da tarefa (associado a UM caminhão)
// ---------------------------------------------------------------------

// Ponteiro para o quadro de estado (BufferCircular) desse caminhão
static BufferCircular* g_buffer = nullptr;

// ID numérico do caminhão local (ex: 1, 2, 3...)
static int g_truck_id = 1;

// Flag opcional para encerramento limpo (se for usar no futuro)
static std::atomic<bool> g_stop{false};

void tratamento_sensores(BufferCircular* buffer_ptr, int caminhao_id)
{
    g_buffer  = buffer_ptr;
    g_truck_id = caminhao_id;

    std::cout << "[TratamentoSensores] Vinculado ao caminhao_id="
              << g_truck_id << " buffer=" << g_buffer << "\n";
}

// ---------------------------------------------------------------------
// 2. Filtro de Média Móvel (ordem M)
// ---------------------------------------------------------------------

struct MovingAverage {
    std::deque<double> janela;
    std::size_t M = 5;   // ordem do filtro (pode ser configurável)
    double soma = 0.0;

    double adiciona(double v) {
        janela.push_back(v);
        soma += v;

        if (janela.size() > M) {
            soma -= janela.front();
            janela.pop_front();
        }

        return soma / static_cast<double>(janela.size());
    }
};

// Instâncias de filtro para cada sinal
static MovingAverage g_filtro_x;
static MovingAverage g_filtro_y;
static MovingAverage g_filtro_ang;

// ---------------------------------------------------------------------
// SINCRONIZAÇÃO 1: Mutex para proteger os filtros
// ---------------------------------------------------------------------
// As callbacks MQTT rodam na mesma thread do consume_message,
// mas deixar o mutex explícito torna o uso seguro se o modelo mudar.
static std::mutex g_mutex_filtros;

// ---------------------------------------------------------------------
// 3. Funções auxiliares
// ---------------------------------------------------------------------

// Lê o truck_id do JSON (aceita número ou string)
static int extrair_truck_id(const json& j)
{
    if (j.contains("truck_id")) {
        if (j["truck_id"].is_number_integer())
            return j["truck_id"].get<int>();

        if (j["truck_id"].is_string()) {
            try {
                return std::stoi(j["truck_id"].get<std::string>());
            } catch (...) {
                return 0;
            }
        }
    }
    return 0;
}

// Aplica filtro e grava no BufferCircular
static void processar_amostra(const json& j)
{
    if (!g_buffer) return;

    // Confere se a mensagem é para ESTE caminhão
    const int msg_truck = extrair_truck_id(j);
    if (msg_truck != g_truck_id)
        return; // ignora mensagens de outros caminhões

    // Lê valores brutos com ruído vindos do simulador
    const double x_bruto   = j.value("i_posicao_x", 0.0);
    const double y_bruto   = j.value("i_posicao_y", 0.0);
    const double ang_bruto = j.value("i_angulo_x",  0.0);

    double x_filtrado, y_filtrado, ang_filtrado;

    {
        // -----------------------------------------------------------------
        // SINCRONIZAÇÃO 2: Mutex dos filtros
        //
        // Garante que o cálculo da média móvel (estado interno dos filtros)
        // não seja corrompido por acessos concorrentes.
        // -----------------------------------------------------------------
        std::lock_guard<std::mutex> lock(g_mutex_filtros);

        x_filtrado   = g_filtro_x.adiciona(x_bruto);
        y_filtrado   = g_filtro_y.adiciona(y_bruto);
        ang_filtrado = g_filtro_ang.adiciona(ang_bruto);
    }

    // Prepara estrutura com valores TRATADOS
    BufferCircular::PosicaoData pos_tratada;
    pos_tratada.i_pos_x    = static_cast<int>(x_filtrado);
    pos_tratada.i_pos_y    = static_cast<int>(y_filtrado);
    pos_tratada.i_angulo_x = static_cast<int>(ang_filtrado);

    // -----------------------------------------------------------------
    // SINCRONIZAÇÃO 3: Mutex do BufferCircular + variável de condição
    // -----------------------------------------------------------------
    // - Pegamos o mutex EXCLUSIVO do BufferCircular via get_mutex().
    // - Escrevemos os novos valores tratados.
    // - Chamamos notify_all_consumers() para acordar outras tarefas
    //   que possam estar bloqueadas esperando dados novos.
    // -----------------------------------------------------------------
    auto& buf_mtx = g_buffer->get_mutex();
    {
        std::lock_guard<std::mutex> lock(buf_mtx);
        g_buffer->set_posicao_tratada(pos_tratada);
    }

    g_buffer->notify_all_consumers();
}

// ---------------------------------------------------------------------
// 4. Loop principal da tarefa cíclica
// ---------------------------------------------------------------------

void tarefa_tratamento_sensores_run(const std::string& broker)
{
    if (!g_buffer) {
        std::cerr << "[TratamentoSensores] ERRO: chame tratamento_sensores(&buffer, id) antes da thread.\n";
        return;
    }

    // Endereço do broker (deve ser o mesmo usado no start.sh)
    const std::string server_uri = "tcp://" + broker + ":1883";

    // ID do cliente MQTT (só para debug/identificação)
    mqtt::async_client client(
        server_uri,
        "tratamento_sensores_" + std::to_string(g_truck_id)
    );

    mqtt::connect_options conn_opts;
    conn_opts.set_clean_session(true);

    try {
        // Conecta no broker
        client.connect(conn_opts)->wait();

        // Modo "consumer": vamos usar consume_message() num loop
        client.start_consuming();

        // Tópico de telemetria do simulador (JSON com ruído)
        const std::string topic_raw = "atr/+/sensor/raw";
        client.subscribe(topic_raw, 1)->wait();

        std::cout << "[TratamentoSensores] Conectado em "
                  << server_uri << ", assinando " << topic_raw
                  << " para caminhao_id=" << g_truck_id << "\n";

        // ------------------ Loop cíclico concorrente ------------------
        while (!g_stop.load()) {
            // Bloqueia até chegar uma mensagem, ou retorna nullptr se desconectar
            auto msg = client.consume_message();
            if (!msg) {
                // Se der problema de conexão, sai do loop para permitir finalização.
                break;
            }

            try {
                auto j = json::parse(msg->get_payload());
                processar_amostra(j);
            }
            catch (const std::exception& e) {
                std::cerr << "[TratamentoSensores] Erro ao processar mensagem: "
                          << e.what() << "\n";
            }
        }
        // ----------------------------------------------------------------

        // Encerramento limpo
        client.unsubscribe(topic_raw)->wait();
        client.stop_consuming();
        client.disconnect()->wait();
    }
    catch (const std::exception& e) {
        std::cerr << "[TratamentoSensores] ERRO MQTT: " << e.what() << "\n";
    }
}

} // namespace atr
