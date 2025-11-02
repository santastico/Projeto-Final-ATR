/**
 * @file tarefa_planejamento_rota.cpp
 * @brief Implementação da thread Planejamento de Rota.
 *
 * @objetivo Responsável por calcular a rota do caminhão. Lê a posição atual 
 * (do Buffer Circular) e o destino final (da Gestão da Mina via MQTT) 
 * e, com base nisso, define os setpoints imediatos (velocidade e ângulo) 
 * para a tarefa de Controle de Navegação.
 *
 * @entradas (Inputs)
 * 1. Buffer Circular (leitura): Lê as variáveis tratadas "i_pos_x", "i_pos_y", 
 * "i_angulo_x".
 * 2. MQTT (subscribe): Assina o tópico "setpoint_posicao_final" 
 * publicado pela Gestão da Mina.
 *
 * @saidas (Outputs)
 * 1. Buffer Circular (escrita): Escreve as variáveis 
 * "setpoint_velocidade" e "setpoint_posicao_angular".
 * 2. MQTT (publish): Publica no tópico "posicao_inicial" para 
 * informar a Gestão da Mina.
 */