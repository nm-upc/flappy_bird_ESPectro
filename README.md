# 🐦 Flappy Bird

Implementación del clásico Flappy Bird para la consola portátil ESPectro (ESP32-S3).

Parte del proyecto ESPectro — [base_espectro](https://github.com/bf-upc/base_espectro)

## Descripción

Flappy Bird es un juego arcade donde el jugador controla un pájaro que debe atravesar una serie infinita de tuberías sin colisionar con ellas ni tocar el suelo.

Cada vez que el pájaro supera una tubería, se obtiene un punto. La dificultad aumenta a medida que la partida avanza, poniendo a prueba los reflejos y la precisión del jugador.

## Controles

| Control | Acción |
|----------|----------|
| Botón A | Saltar |
| Botón B | Salir de la partida |
| Botón B (menú principal) | Acceder al Game Loader |

## Sistema de puntuación

- +1 punto por cada tubería superada.
- El récord se guarda automáticamente en la memoria flash (NVS).
- El historial de las últimas 20 partidas es accesible desde el dashboard web.
- Las estadísticas se actualizan automáticamente tras cada partida.

## Mecánica de juego

- El pájaro está afectado por la gravedad.
- Cada pulsación del botón A genera un salto.
- Las tuberías aparecen de forma continua con aperturas aleatorias.
- La partida termina al:
  - Chocar contra una tubería.
  - Tocar el suelo.
- El objetivo es conseguir la máxima puntuación posible.

## Dashboard

Con la consola encendida, conéctate a la red WiFi **ESPectro**:

**SSID:** `ESPectro`  
**Contraseña:** `gameloader`

Abre en tu navegador:

```text
http://192.168.4.1
```

Desde el dashboard podrás:

- Consultar récords y estadísticas.
- Ver el historial de partidas.
- Monitorizar información del sistema.
- Instalar nuevas versiones del juego mediante OTA.
- Gestionar otros juegos compatibles con ESPectro.

## Audio

El juego utiliza salida de audio I2S para reproducir:

- Sonido al saltar.
- Sonido al superar una tubería.
- Sonido de Game Over.
- Melodía de inicio de ESPectro.

## Compilar y flashear

```bash
git clone https://github.com/bf-upc/base_espectro
cd base_espectro
pio run --target upload
```

## Requisitos

- PlatformIO
- ESP32-S3
- Librería `lovyan03/LovyanGFX @ ^1.1.12`

## Características

- Gráficos optimizados para pantalla ILI9488.
- Control mediante botones físicos.
- Sistema de récords persistente.
- Historial de partidas.
- Dashboard web integrado.
- Actualización OTA mediante Game Loader.
- Efectos de sonido mediante I2S.
- Integración completa con la plataforma ESPectro.

## Firmware

El binario compilado se encuentra normalmente en:

```text
.pio/build/rymcu-esp32-s3-devkitc-1/firmware.bin
```

Este archivo puede cargarse directamente desde el Game Loader sin necesidad de utilizar conexión USB.

## Proyecto ESPectro

ESPectro es una consola portátil basada en ESP32-S3 diseñada para desarrollar, distribuir y ejecutar videojuegos mediante actualizaciones OTA y un sistema de gestión integrado.
