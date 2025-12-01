# Projeto: Sistema de Controle de Caminhão Autônomo (ATR)

Simulação de frota de caminhões autônomos de mineração, com núcleo embarcado em C++, simulador de mina em Python e ferramentas de gestão, todos integrados via MQTT e orquestrados com Docker Compose.

## Arquitetura (visão rápida)

O ambiente é composto por múltiplos contêineres:

- **infra_mina**  
  - Broker MQTT (Mosquitto)  
  - Simulador de mina em Python (`simulator_view.py`)  
- **caminhao_01 ... caminhao_05**  
  - Núcleo C++ embarcado (`caminhao_embarcado`) com tarefas de:
    - tratamento de sensores  
    - planejamento de rota  
    - controle de navegação  
    - lógica de comando  
    - monitoramento de falhas  
    - coletor de dados / “caixa‑preta”  

Cada caminhão usa tópicos MQTT próprios baseados em seu ID.

## Como subir o ambiente

Na raiz do projeto:

docker compose up --build

Isso irá:

- subir o broker MQTT e o simulador de mina (`infra_mina`),  
- subir os contêineres `caminhao_01` a `caminhao_05` com o núcleo C++.

Para derrubar tudo:

docker compose down

## Interface de gestão (CLI)

Com o ambiente rodando, em outro terminal:

python cli_gestao.py

Com o CLI é possível:

- criar/remover caminhões no simulador,  
- definir destinos (setpoints de posição),  
- injetar falhas (elétrica, hidráulica, térmica),  
- inspecionar o estado de cada caminhão em texto.

## Logs e “caixa‑preta” dos caminhões

Cada caminhão grava um log próprio em:

output/cam_<ID>.log

Exemplos:

- `output/cam_1.log`  
- `output/cam_2.log`  

Esses arquivos registram, em ordem temporal:

- leituras tratadas de sensores,  
- decisões de planejamento de rota, 
- eventos de falha detectados pelo monitor.

A ideia é que, após remover um caminhão (via CLI/simulador), o respectivo `cam_<ID>.log` funcione como uma “caixa‑preta” para análise da execução.
