/**
 * @file tarefa_tratamento_sensores.cpp
 * @brief Implementação da thread Tratamento de Sensores.
 *
 * @objetivo Responsável por ler os dados brutos de posicionamento (com ruído) 
 * vindos do simulador, aplicar um filtro de média móvel para tratá-los 
 * e disponibilizar os dados limpos para as outras tarefas no Buffer_Circular.
 *
 * @entradas (Inputs)
 * 1. MQTT (subscribe): Assina os tópicos "i_pos_x", "i_pos_y" e "i_angulo_x" 
 * publicados pelo simulador.
 *
 * @saidas (Outputs)
 * 1. Buffer Circular (escrita): Escreve as variáveis tratadas (após o filtro)
 * de "i_pos_x", "i_pos_y" e "i_angulo_x" no buffer.
 */