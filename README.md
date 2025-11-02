# Projeto: Sistema de Controle de Caminhão Autônomo (ATR)

Este projeto implementa um sistema de controle simplificado para N caminhões autônomos de mineração, em conformidade com as especificações da disciplina de Automação em Tempo Real. O sistema abrange tanto o controle embarcado do veículo quanto a integração com sistemas centrais, aplicando conceitos de programação concorrente , mecanismos de sincronização e comunicação entre processos (IPC e MQTT).

## Arquitetura do Sistema

O sistema é dividido em **4 programas (processos) principais** que rodam de forma independente:

1.  **`caminhao_embarcado` (C++)**: O "cérebro" do caminhão. Este é um programa multi-thread que executa as 6 tarefas principais (sensores , falhas , lógica, etc.). Ele compartilha dados internamente usando um objeto de estado (BufferCircular) thread-safe.
2.  **`simulador_mina` (Python)**: Um único processo que simula a física de N caminhões. Ele publica dados de sensores (posição, falhas) e assina comandos de atuadores (aceleração, direção) para todos os caminhões via MQTT.
3.  **`gestao_mina` (Python)**: Uma GUI central que monitora a posição de todos os caminhões e permite ao gestor definir setpoints de rota (destino final) via MQTT.
4.  **`interface_local` (Python)**: Uma interface (TUI/GUI) para controle manual de um caminhão específico. Ela se comunica com o processo `caminhao_embarcado` correspondente via IPC.

## Gerenciamento e Simulação (N=15 Caminhões)

A escalabilidade para `N` caminhões é gerenciada da seguinte forma:

1.  **Criação dos Caminhões**: Não há *um* programa que cria os 15 caminhões. Em vez disso, executamos o processo `caminhao_embarcado` 15 vezes (em 15 terminais ou por um script). Cada instância recebe um **ID único** como argumento (ex: `./caminhao_embarcado 1`, `./caminhao_embarcado 2`, ...).

2.  **Tópicos MQTT Únicos**: Cada instância do `caminhao_embarcado` usa seu ID para se conectar a tópicos MQTT únicos. Por exemplo, o Caminhão 1 assina `caminhao/1/sensores`  e publica em `caminhao/1/atuadores`.

3.  **Simulação Centralizada**: O `simulador_mina` é um **único processo** que mantém uma lista interna do estado dos 15 veículos. Ele usa um *wildcard* do MQTT (ex: `caminhao/+/atuadores` ) para receber os comandos de todos os caminhões e, em seu loop, publica os dados dos sensores no tópico específico de cada caminhão (ex: `caminhao/1/sensores` .

4.  **Interfaces**: A `gestao_mina` (central) se conecta a todos os tópicos para monitorar todos os veículos. A `interface_local` é executada 15 vezes (uma para cada caminhão), e cada instância se conecta ao seu processo `caminhao_embarcado` correspondente via IPC.

## Como Compilar e Executar (Resumido)

1.  **Iniciar o Broker MQTT**:
    ```bash
    docker-compose up -d
    ```

2.  **Compilar o Caminhão (C++)**:
    ```bash
    cd caminhao_embarcado
    mkdir build && cd build
    cmake ..
    make
    ```

3.  **Executar o Ecossistema**:
    * **Simulador (Python)**: (Em um terminal)
        ```bash
        python3 simulador_mina/simulador.py
        ```
    * **Gestão da Mina (Python)**: (Em outro terminal)
        ```bash
        python3 gestao_mina/gestao_gui.py
        ```
    * **Caminhões (C++)**: (Em 15 terminais diferentes, ou via script)
        ```bash
        ./caminhao_embarcado/build/caminhao_embarcado 1
        ./caminhao_embarcado/build/caminhao_embarcado 2
        ...
        ./caminhao_embarcado/build/caminhao_embarcado 15
        ```
    * **Interface Local (Python)**: (Para controlar o Caminhão 1)
        ```bash
        python3 interface_local/local_ui.py 1
        ```