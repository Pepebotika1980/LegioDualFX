# LegioDualFX v3.0 - Manual de Usuario

## Descripción General

LegioDualFX es un procesador de efectos de audio de alta calidad con **cuatro modos distintos**: **Filter+Drive**, **Space Echo**, **Shimmer Reverb** y **Shepard Tone Generator**. El firmware ha sido optimizado con algoritmos DSP profesionales para ofrecer calidad de audio comparable a Noise Engineering.

---

## Controles Globales

### Encoder (Botón Rotatorio)
- **Presionar**: Cambiar entre modos (Filter → Echo → Shimmer → Shepard → Filter...)
- **Girar**: Control específico de cada modo (ver abajo)

### LEDs
- **ROJO**: Modo Filter+Drive
- **VERDE**: Modo Space Echo
- **BLANCO**: Modo Shimmer Reverb
- **CYAN**: Modo Shepard Tone Generator

---

## Modo 1: Filter+Drive (ROJO)

Filtro resonante de 24dB/oct con saturación de alta calidad.

### Controles

| Control | Función |
|---------|---------|
| **Knob Superior** | Cutoff Frequency (5Hz - 18kHz) |
| **Knob Inferior** | Resonance |
| **Encoder (Girar)** | Drive Amount (0-100%) |
| **Switch Izquierdo** | Drive Mode |
| **Switch Derecho** | Filter Type |

### Switch Izquierdo - Drive Mode
- **Arriba**: Warm (Saturación suave asimétrica)
- **Centro**: Hard (Saturación agresiva)
- **Abajo**: Destroy (Wavefolder de 3 etapas)

### Switch Derecho - Filter Type
- **Arriba**: High-Pass (24dB/oct)
- **Centro**: Band-Pass (24dB/oct)
- **Abajo**: Low-Pass (24dB/oct)

### Características Técnicas
- Oversampling 2x con interpolación Hermite
- Filtro anti-aliasing de 2 polos (24dB/oct @ 14kHz)
- Noise gate adaptativo (-54dB threshold)
- Stereo spread automático basado en resonancia
- Compensación de ganancia automática

---

## Modo 2: Space Echo (VERDE)

Emulación de delay de cinta con reverb de muelle.

### Controles

| Control | Función |
|---------|---------|
| **Knob Superior** | Delay Time (varía según Head Mode) |
| **Knob Inferior** | Feedback Amount |
| **Encoder (Girar)** | Reverb Amount |
| **Switch Izquierdo** | Head Mode (Delay Range) |
| **Switch Derecho** | Tone Character |

### Switch Izquierdo - Head Mode
- **Arriba**: Short (100-300ms)
- **Centro**: Medium (300-700ms)
- **Abajo**: Long (500-1500ms)

### Switch Derecho - Tone Character
- **Arriba**: Bright (12kHz LPF, 200Hz HPF)
- **Centro**: Normal (4.5kHz LPF, 100Hz HPF)
- **Abajo**: Dark (1.2kHz LPF, 400Hz HPF)

### Características Técnicas
- Flutter LFO (2.5Hz) para wobble de cinta
- Drift LFO (0.2Hz) para variación analógica de pitch/tono
- Saturación asimétrica de cinta
- Compresor en feedback loop (ratio 3:1)
- Soft limiter para prevenir auto-oscilación descontrolada
- Interpolación Hermite en delay lines
- Stereo width automático (15ms offset)

---

## Modo 3: Shimmer Reverb (BLANCO)

Reverb lujosa con pitch shifting y control de frecuencias.

### Controles

| Control | Función |
|---------|---------|
| **Knob Superior** | Decay Time |
| **Knob Inferior** | HPF Cutoff (150Hz - 500Hz) |
| **Encoder (Girar)** | Shimmer Amount |
| **Switch Izquierdo** | Pitch Interval |
| **Switch Derecho** | Tone Character |

### Switch Izquierdo - Pitch Interval
- **Arriba**: +1 Octave
- **Centro**: +5th (Perfect Fifth)
- **Abajo**: -1 Octave

### Switch Derecho - Tone Character
- **Arriba**: Bright (15kHz shimmer LPF, 12kHz reverb LPF)
- **Centro**: Normal (5kHz shimmer LPF, 4kHz reverb LPF)
- **Abajo**: Dark (1kHz shimmer LPF, 1kHz reverb LPF)

### Características Técnicas
- HPF variable de 2 polos (24dB/oct) para control de graves
- Pre-delay de 40ms con interpolación Hermite
- Pitch shifter con smoothing para reducir artefactos
- Compresor en shimmer loop (ratio 3:1, threshold 0.4)
- Anti-rumble filter (150Hz HPF en shimmer loop)
- DC blocker (20Hz HPF)
- Mix fijo 50/50 (dry/wet)
- Stereo detune automático (±5 cents)

---

## Modo 4: Shepard Tone Generator (CYAN) ⭐ NUEVO (v3.2)

Generador "Beautiful Shepard" rediseñado para crear subidones emocionales, limpios y espaciales.

### Controles

| Control | Función |
|---------|---------|
| **Knob Superior** | **Speed** (Velocidad de subida/bajada) |
| **Knob Inferior** | **Tone / Brightness** (Filtro Suave → Brillante) |
| **Encoder (Girar)** | **Reverb Amount** (Espacio y belleza) |
| **Switch Izquierdo** | **Direction** (Up / Pause / Down) |
| **Switch Derecho** | **Range** (Bajo / Medio / Alto) |

### Switch Izquierdo - Direction
- **Arriba**: Up (subida infinita)
- **Centro**: Pause (congelado)
- **Abajo**: Down (bajada infinita)

### Switch Derecho - Range
- **Arriba**: High Range (agudo y brillante)
- **Centro**: Mid Range (balanceado, ideal para builds)
- **Abajo**: Low Range (profundo y oscuro)

### Descripción del Motor "Beautiful" (v3.2)

A diferencia de versiones anteriores, este motor elimina toda distorsión y caos para enfocarse en la pureza:

- **8 Voces de Seno Puro**: Cristalinas y definidas, sin ruido.
- **Reverb Integrada**: Una reverb lujosa incorporada en el propio generador para crear pads "nube".
- **Tone Control**: Filtro LPF suave para oscurecer el sonido (evita cualquier estridencia).
- **Cero Distorsión**: Ganancia ajustada para un headroom perfecto.

### Uso Musical

**El "Subidón Perfecto":**
1.  **Speed**: Medio (12:00) subiendo a rápido.
2.  **Tone**: Empieza cerrado (7:00) y ábrelo lentamente al subir.
3.  **Reverb**: 40-50% para dar sensación de inmensidad.
4.  **Direction**: Up.

**Ambient Drone:**
1.  **Speed**: Muy lento.
2.  **Tone**: Suave (10:00).
3.  **Reverb**: 80% (lavado total).
4.  **Range**: Low.

---

## Características Globales

### Limiters Adaptativos
El firmware ajusta automáticamente el pre-gain del limiter según el modo activo:
- **Filter Mode**: 1.3x (menos headroom necesario)
- **Echo Mode**: 1.5x (puede ser más fuerte)
- **Shimmer Mode**: 1.2x (más headroom para picos)
- **Shepard Mode**: 1.4x (generador optimizado)

### Stereo Widening
Procesamiento Mid/Side con control de ancho estéreo:
- **0.0**: Mono
- **0.5**: Normal (valor por defecto)
- **1.0**: Wide

### Crossfade entre Modos
- Curvas exponenciales para transiciones suaves
- Fade out/in de ~6ms
- Sin clicks ni pops al cambiar de modo

---

## Especificaciones Técnicas

### Hardware
- **Plataforma**: Daisy Legio (STM32H750)
- **Sample Rate**: 48kHz
- **Bit Depth**: 32-bit float (procesamiento interno)
- **Latencia**: <1ms

### Memoria (v3.0)
- **FLASH**: 102.1KB / 128KB (77.9%)
- **SDRAM**: 1.86MB / 64MB (2.8%)
- **SRAM**: 15.8KB / 512KB (3.0%)

### DSP
- Interpolación Hermite en todos los delays
- Filtros SVF de alta calidad
- Pitch shifter con crossfading
- Limiters con lookahead
- Compresores con soft knee
- 10-voice oscillator bank (Shepard mode)

---

## Consejos de Uso

### Filter+Drive
- Usa **Warm** para saturación sutil tipo válvulas
- Usa **Hard** para distorsión agresiva tipo transistor
- Usa **Destroy** para efectos experimentales y texturas caóticas
- El noise gate elimina ruido de fondo automáticamente

### Space Echo
- Ajusta **Feedback** cerca de 100% para auto-oscilación controlada
- El **Drift** añade variación orgánica al delay time
- Combina **Reverb** alto con **Feedback** bajo para ambientes espaciales
- Usa **Dark** tone para delays dub clásicos

### Shimmer Reverb
- Ajusta **HPF** alto (>350Hz) para evitar distorsión con señales graves
- Usa **+5th** para armonías más sutiles que octavas
- El **Shimmer Amount** controla la cantidad de pitch shifting en el loop
- Combina **Decay** largo con **Shimmer** bajo para pads atmosféricos

### Shepard Tone Generator
- **Speed** bajo (0.01-0.1) para ambientes y drones
- **Speed** medio (0.5-2.0) para build-ups techno
- **Speed** alto (5.0-10.0) para transiciones rápidas
- **Noise** añade textura y carácter
- **Chaos** mode perfecto para techno experimental
- **Clean** mode ideal para capas sutiles bajo otros sonidos

---

## Solución de Problemas

### El filtro no responde
- Verifica que estés en modo Filter (LED ROJO)
- Asegúrate de que el cutoff no esté en los extremos

### El delay distorsiona
- Reduce el **Feedback** por debajo de 100%
- El limiter en el feedback loop previene runaway, pero feedback muy alto puede saturar

### El shimmer suena distorsionado
- Aumenta el **HPF** (knob inferior) para filtrar graves
- Reduce el **Shimmer Amount** (encoder)
- Verifica que la señal de entrada no esté clipping

### Ruido de fondo en Filter+Drive
- El noise gate debería eliminarlo automáticamente
- Si persiste, reduce el **Drive Amount**

### Shepard Tone no se escucha
- Verifica que el **Direction** no esté en Pause (switch centro)
- Aumenta el **Speed** si está muy lento
- Verifica que el **Frequency Range** esté en rango audible (centro del knob)

### Shepard Tone suena "roto"
- Reduce el **Noise Amount** si está muy alto
- Cambia de **Chaos** a **Clean** o **Rich**
- Ajusta el **Frequency Range** para evitar extremos

---

## Créditos

**Firmware Enhancement**: Gemini AI Assistant  
**Original Hardware**: Noise Engineering (Daisy Legio)  
**DSP Libraries**: DaisySP, DaisySP-LGPL  

**Versión**: 3.0 Enhanced  
**Fecha**: Diciembre 2025

---

## Changelog v3.0

### Shepard Tone Generator (NUEVO)
- ✨ 10-voice oscillator bank con crossfade suave
- ✨ 3 modos de carácter (Clean, Rich, Chaos)
- ✨ Control de speed (0.01 - 10.0 oct/sec)
- ✨ Dirección variable (Up/Pause/Down)
- ✨ Generador de noise integrado
- ✨ Stereo imaging con Haas effect
- ✨ Compresión glue profesional
- ✨ Anti-aliasing de alta calidad

### Mejoras Globales
- ✨ Cuarto modo añadido al ciclo
- ✨ LED CYAN para Shepard mode
- ✨ Limiter adaptativo optimizado para 4 modos
- ✨ Input gain staging mejorado

---

## Changelog v2.0

### Filter+Drive
- ✨ Oversampling mejorado con interpolación Hermite
- ✨ Filtro anti-aliasing de 2 polos (24dB/oct)
- ✨ Drive algorithms mejorados con knee más suave
- ✨ Wavefolder de 3 etapas para mayor complejidad

### Space Echo
- ✨ Analog drift emulation (LFO 0.2Hz)
- ✨ Compresor en feedback loop (ratio 3:1)
- ✨ Saturación asimétrica de cinta
- ✨ Soft limiter para prevenir runaway feedback

### Shimmer Reverb
- ✨ HPF variable (150-500Hz) con 2 polos
- ✨ Compresor en shimmer loop (ratio 3:1)
- ✨ Pitch shifter con smoothing mejorado
- ✨ Pre-delay con interpolación Hermite

### Global
- ✨ Limiters adaptativos por modo
- ✨ Stereo widening con procesamiento Mid/Side
- ✨ Crossfade exponencial entre modos
- ✨ Input gain staging optimizado por modo
