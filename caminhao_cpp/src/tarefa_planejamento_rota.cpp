#include <mqtt/async_client.h>
#include <nlohmann/json.hpp>

#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <cmath>
#include <string>

namespace atr {

using json = nlohmann::json;

struct DestinoCompartilhado {
    double x = 0.0;
    double y = 0.0;
    bool   ativo = false;
    std::mutex mtx;
    std::condition_variable cv;
};

static double wrap_deg(double a) {
    while (a > 180.0) a -= 360.0;
    while (a < -180.0) a += 360.0;
    return a;
}

void tarefa_planejamento_rota(int id, BufferCircular& buffer)
{
    std::cout << "[Planejamento " << id << "] Thread iniciada.\n";

    const std::string broker    = "tcp://localhost:1883";
    const std::string client_id = "planner_" + std::to_string(id);
    const std::string topic_sp  = "atr/" + std::to_string(id) + "/gestao/setpoint_posicao_final";
    const std::string topic_log = "atr/" + std::to_string(id) + "/planner/log";

    mqtt::async_client cli(broker, client_id);
    mqtt::connect_options connOpts;
    connOpts.set_clean_session(true);

    DestinoCompartilhado destino;

    try {
        cli.connect(connOpts)->wait();
        cli.subscribe(topic_sp, 1)->wait();
        std::cout << "[Planejamento " << id << "] Conectado ao broker.\n";
    } catch (const std::exception& e) {
        std::cerr << "[Planejamento " << id << "] ERRO MQTT: " << e.what() << "\n";
        return;
    }

    class Cb : public virtual mqtt::callback {
        DestinoCompartilhado& dest_;
        std::string topic_log_;
        mqtt::async_client& cli_;
    public:
        Cb(DestinoCompartilhado& d, std::string log, mqtt::async_client& c)
            : dest_(d), topic_log_(std::move(log)), cli_(c) {}

        void message_arrived(mqtt::const_message_ptr msg) override {
            try {
                auto j = json::parse(msg->to_string());
                if (!j.contains("x") || !j.contains("y"))
                    return;

                {
                    std::lock_guard<std::mutex> lk(dest_.mtx);
                    dest_.x = j["x"].get<double>();
                    dest_.y = j["y"].get<double>();
                    dest_.ativo = true;
                }

                cli_.publish(topic_log_, "Novo destino recebido", 1, false);
                dest_.cv.notify_all();
            } catch (const std::exception& e) {
                std::cerr << "[Planejamento] erro parse setpoint: " << e.what() << "\n";
            }
        }
    };

    auto cb = std::make_shared<Cb>(destino, topic_log, cli);
    cli.set_callback(*cb);

    const double V_MAX    = 2.0;
    const double KP_DIST  = 0.8;
    const double KP_ANG   = 2.0;
    const double DIST_TOL = 0.25;
    const double ANG_TOL  = 2.0;

    while (true) {
        // Espera até alguém definir um destino ativo
        {
            std::unique_lock<std::mutex> lk(destino.mtx);
            destino.cv.wait(lk, [&]{ return destino.ativo; });
        }

        // Enquanto houver um destino ativo, gerar setpoints
        while (true) {
            double gx, gy;
            {
                std::lock_guard<std::mutex> lk(destino.mtx);
                if (!destino.ativo)
                    break;
                gx = destino.x;
                gy = destino.y;
            }

            // Lê posição tratada do buffer (usa nomes reais das structs)
            BufferCircular::PosicaoData pos = buffer.get_posicao_tratada();
            double x   = static_cast<double>(pos.i_pos_x);
            double y   = static_cast<double>(pos.i_pos_y);
            double ang = static_cast<double>(pos.i_angulo_x);

            // Erros
            double dx   = gx - x;
            double dy   = gy - y;
            double dist = std::sqrt(dx*dx + dy*dy);

            double desired_ang = std::atan2(dy, dx) * 180.0 / M_PI;
            double err_ang     = wrap_deg(desired_ang - ang);

            // Setpoints (limita velocidade)
            double sp_vel = std::min(V_MAX, KP_DIST * dist);
            double sp_ang = wrap_deg(ang + KP_ANG * err_ang);

            // Escreve nos setpoints de navegação do buffer
            BufferCircular::SetpointsNavegacao sp{};
            sp.set_velocidade = sp_vel;
            sp.set_pos_angular = sp_ang;
            buffer.set_setpoints_navegacao(sp);



            // Condição de chegada
            if (dist < DIST_TOL && std::fabs(err_ang) < ANG_TOL) {
                {
                    std::lock_guard<std::mutex> lk(destino.mtx);
                    destino.ativo = false;
                }
                cli.publish(topic_log, "Destino atingido", 1, false);
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50)); // ~20 Hz
        }
    }
}

} // namespace atr
