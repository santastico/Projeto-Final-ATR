import time
import sys

print("[main_ui.py] Interface Unificada iniciada.")
print("[main_ui.py] O broker MQTT e o processo C++ (se adicionado) devem estar rodando.")
print("[main_ui.py] Contêiner está 'vivo' para depuração. Pressione Ctrl+C para parar.")

try:
    # Este loop mantém o script (e o contêiner) rodando
    while True:
        # No futuro, aqui será o loop principal do Pygame
        time.sleep(10)
except KeyboardInterrupt:
    print("\n[main_ui.py] Recebido sinal de parada (Ctrl+C). Encerrando.")
    sys.exit(0)