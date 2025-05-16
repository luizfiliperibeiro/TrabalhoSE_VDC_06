# 🌊 Estação de Alerta de Enchente com Simulação por Joystick

Projeto desenvolvido por **Luiz Filipe Ribeiro de Jesus**  
Polo: Vitória da Conquista    
Professor: Wilton Lacerda Silva
Mentor: Auere Vasconcelos Veras

---

## 📌 Descrição

Este projeto implementa uma **estação de monitoramento de cheias**, utilizando a placa **BitDog Lab com o microcontrolador RP2040**, o sistema operacional **FreeRTOS** e periféricos embarcados.

Os dados de **nível da água** e **volume de chuva** são simulados a partir das leituras analógicas do **joystick** da placa. A partir desses dados, o sistema gera alertas com diferentes recursos:

- 📺 Display OLED (informações em tempo real)
- 🔴 LED RGB (alerta visual)
- 🔊 Buzzer (alerta sonoro)
- 🟥 Matriz WS2812 (símbolo de alerta piscante)

---

## 🎮 Modos de Operação

### ✅ Modo Normal
- Exibe continuamente os valores de nível de água e volume de chuva no display OLED.
- Nenhum alerta é acionado.

### 🚨 Modo Alerta
Ativado automaticamente quando:
- **Nível da água ≥ 70%** ou
- **Volume de chuva ≥ 80%**

Neste modo:
- LED RGB acende em **vermelho**
- Buzzer emite **três bips curtos**
- Matriz WS2812 exibe um **triângulo vermelho piscante**
- Display destaca visualmente o status de **ALERTA**

---

## 🧠 Tecnologias e Conceitos

- ⏱️ FreeRTOS: sistema operacional em tempo real com multitarefas
- 📬 Filas (`xQueueSend`, `xQueueReceive`) para comunicação segura entre tarefas
- 🧵 Tarefas independentes para cada periférico
- 📟 I2C (OLED), PIO (WS2812), GPIOs (LEDs, buzzer, joystick)

---

## 🧩 Arquitetura de Software

| Tarefa             | Função                                | Comunicação via Fila          |
|--------------------|----------------------------------------|-------------------------------|
| `vJoystickTask`    | Leitura dos eixos X/Y do joystick      | Envia para todas as outras    |
| `vDisplayTask`     | Atualiza display OLED via I2C          | `xQueueSensorData`            |
| `vLedRGBTask`      | Acende LED vermelho em modo alerta     | `xQueueSensorAlerta`          |
| `vBuzzerTask`      | Emite bips sonoros                     | `xQueueSensorBuzzer`          |
| `vMatrizTask`      | Mostra triângulo vermelho piscando     | `xQueueSensorMatriz`          |

---

## 🛠️ Componentes Utilizados

- Placa **BitDog Lab (RP2040)**
- Display OLED SSD1306 via I2C
- Matriz de LEDs WS2812 (5x5)
- LED RGB (pinos separados)
- Buzzer (GPIO 21)
- Joystick analógico (GPIOs 26 e 27 – ADC)

---

## 📸 Demonstração em Vídeo

📺 Assista à apresentação do projeto:  
[🔗 Link do vídeo no YouTube/Google Drive](https://drive.google.com/file/d/1esv2T-zMY49zgdwqTdc2FKxznUlw6Yd5/view?usp=drive_link)

---

## 📁 Como Executar

1. Clone este repositório:
```bash
git clone https://github.com/seuusuario/estacao-alerta-enchente.git

2. Compile com SDK do Pico + FreeRTOS integrado (consulte Makefile/CMakeLists.txt)

3. Grave o binário na BitDog Lab (modo BOOTSEL)

4. Abra o monitor serial para acompanhar logs, se desejar.