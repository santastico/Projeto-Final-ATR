#include "tarefas.h"
#include "Buffer_Circular.h"
#include "config.h"

#include <mqtt/async_client.h>
#include <nlohmann/json.hpp>
#include <deque>
#include <mutex>
#include <atomic>
#include <iostream>
#include <cctype>

using json = nlohmann::json;

namespace atr {

// ====== estado vinculado pelo bind ======
static BufferCircular* g_buf = nullptr;
static int g_local_id = 1;
static std::atomic<bool> g_stop{false};

void tratamento_sensores(BufferCircular* buffer_ptr, int caminhao_id) {
    g_buf = buffer_ptr;
    g_local_id = caminhao_id;
    std::cout << "[Tratamento] bind: id=" << g_local_id << " buffer=" << (void*)g_buf << "\n";
}

// ====== filtro de média móvel ======
struct MovingAvg {
    std::deque<double> w; size_t M=5; double sum=0;
    double push(double v){ w.push_back(v); sum+=v; if(w.size()>M){ sum-=w.front(); w.pop_front(); } return sum/w.size(); }
};

static MovingAvg g_fx{};
static MovingAvg g_fy{};
static MovingAvg g_fang{};
static std::mutex g_mtx;

// extrai número do truck_id (aceita "1" ou "T001")
static int parse_truck_num(const std::string& tid){
    try { return std::stoi(tid); }
    catch(...) {
        std::string d; for(char c: tid) if (std::isdigit((unsigned char)c)) d.push_back(c);
        return d.empty()? 0 : std::stoi(d);
    }
}

static void handle_raw_sample(const json& j){
    if(!g_buf) return;

    int num = parse_truck_num(j.value("truck_id", "0"));
    if (num != g_local_id) return; // só o meu caminhão

    // campos definidos no simulador (Tabela 1)
    double x   = j.value("i_posicao_x", 0.0);
    double y   = j.value("i_posicao_y", 0.0);
    double ang = j.value("i_angulo_x",  0.0);

    double fx, fy, fang;
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        fx   = g_fx.push(x);
        fy   = g_fy.push(y);
        fang = g_fang.push(ang);
    }

    BufferCircular::PosicaoData pos{};
    pos.i_pos_x    = fx;
    pos.i_pos_y    = fy;
    pos.i_angulo_x = fang;
    g_buf->set_posicao_tratada(pos);
}

void tarefa_tratamento_sensores_run(const std::string& broker) {
    if (!g_buf) {
        std::cerr << "[Tratamento] ERRO: chame tratamento_sensores(&buffer, id) antes da thread!\n";
        return;
    }
    const std::string uri = "tcp://" + broker + ":1883";
    mqtt::async_client cli(uri, "cpp_filter_" + std::to_string(::time(nullptr)));
    mqtt::connect_options conn; conn.set_clean_session(true);

    try {
        cli.connect(conn)->wait();
        cli.start_consuming();
        cli.subscribe("atr/+/sensor/raw", 1)->wait();
        std::cout << "[Tratamento] conectado ao broker " << uri << " (local_id=" << g_local_id << ")\n";

        while (!g_stop.load()) {
            // Versão compatível: bloqueia até chegar mensagem (ou desconectar)
            auto msg = cli.consume_message();
            if (!msg) continue;

            try {
                auto j = json::parse(msg->get_payload());
                handle_raw_sample(j);
            } catch (const std::exception& e) {
                std::cerr << "[Tratamento] parse erro: " << e.what() << "\n";
            }
        }

        cli.unsubscribe("atr/+/sensor/raw")->wait();
        cli.stop_consuming();
        cli.disconnect()->wait();
    } catch (const std::exception& e) {
        std::cerr << "[Tratamento] MQTT erro: " << e.what() << "\n";
    }
}

} // namespace atr
