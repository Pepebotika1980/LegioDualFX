# LegioDualFX

**Firmware de alta calidad para módulo Eurorack Daisy Legio**

![Version](https://img.shields.io/badge/version-1.1.1-blue)
![Platform](https://img.shields.io/badge/platform-Daisy%20Seed-green)
![License](https://img.shields.io/badge/license-MIT-orange)

---

## 🎛️ Descripción

LegioDualFX es un firmware multi-efecto profesional para el módulo Daisy Legio, ofreciendo 4 modos de procesamiento de audio de alta calidad:

1. **Filter/Drive** - Filtro resonante 24dB/oct con 3 modos de distorsión
2. **Space Echo** - Delay estéreo con flutter analógico y reverb spring
3. **Shimmer Reverb** - Reverb lush con pitch shifting y pre-delay
4. **Shepard Tone** - Generador de tonos Shepard con reverb integrado

---

## ✨ Características

### Audio
- ✅ **Procesamiento estéreo completo** en todos los modos
- ✅ **Crossfade suave** entre modos sin clicks
- ✅ **Limiters adaptativos** por modo para headroom óptimo
- ✅ **Stereo widening** con procesamiento Mid/Side
- ✅ **Auto-recovery** ante condiciones de error (NaN protection)

### Rendimiento
- ✅ **Optimizado para CPU** (~4-6% de headroom liberado en v1.1.1)
- ✅ **Uso eficiente de memoria** (FLASH: 76%, SDRAM: 3.4%)
- ✅ **Sin artefactos** de audio ni clicks

### Código
- ✅ **67 constantes nombradas** para fácil ajuste
- ✅ **Código limpio** sin magic numbers
- ✅ **Bien documentado** con comentarios inline
- ✅ **Modular** - fácil de extender

---

## 🎚️ Controles

### Globales
- **Encoder (Press)**: Cambiar modo (Filter → Echo → Shimmer → Shepard)
- **LEDs**: Indicador de modo actual (Rojo/Verde/Blanco/Cian)

### Por Modo

#### Mode 1: Filter/Drive (LED Rojo)
- **Knob Top**: Cutoff frequency (5Hz - 18kHz)
- **Knob Bottom**: Resonance
- **Encoder Turn**: Drive amount
- **Switch Left**: Drive type (Warm/Hard/Destroy)
- **Switch Right**: Filter type (HP/BP/LP)

#### Mode 2: Space Echo (LED Verde)
- **Knob Top**: Delay time
- **Knob Bottom**: Feedback
- **Encoder Turn**: Reverb amount
- **Switch Left**: Head mode (Short/Med/Long)
- **Switch Right**: Tone (Bright/Normal/Dark)

#### Mode 3: Shimmer Reverb (LED Blanco)
- **Knob Top**: Decay time
- **Knob Bottom**: High-pass filter
- **Encoder Turn**: Shimmer amount
- **Switch Left**: Pitch interval (+1oct/+5th/-1oct)
- **Switch Right**: Tone (Bright/Normal/Dark)

#### Mode 4: Shepard Tone (LED Cian)
- **Knob Top**: Speed
- **Knob Bottom**: Tone/Brightness
- **Encoder Turn**: Reverb amount
- **Switch Left**: Direction (Up/Pause/Down)
- **Switch Right**: Range (Low/Mid/High)

---

## 🚀 Instalación

### Requisitos
- Daisy Legio hardware
- ARM GNU Toolchain 14.3.rel1
- libDaisy
- DaisySP

### Compilación
```bash
cd LegioDualFX
make clean
make
```

### Flasheo
```bash
# Usando dfu-util
make program-dfu

# O manualmente
dfu-util -a 0 -s 0x08000000:leave -D build/LegioDualFX.bin
```

---

## 📊 Changelog

### v1.1.1 (2026-02-05)
- ✅ Optimización adicional de ModeShepardTone (~1% CPU)
- ✅ Movido SetFreq de filtros fuera del loop de audio
- ✅ Documentación completa añadida

### v1.1 (2026-02-05)
- ✅ Optimizaciones de rendimiento (~3-5% CPU)
- ✅ Wavefolder mejorado con interpolación cúbica
- ✅ Flutter orgánico en delay con generador de ruido
- ✅ 67 constantes nombradas añadidas
- ✅ Código muerto eliminado
- ✅ NaN auto-recovery implementado

### v1.0
- ✅ Release inicial con 4 modos

Ver [CHANGELOG.md](CHANGELOG.md) para detalles completos.

---

## 📖 Documentación

- **[CHANGELOG.md](CHANGELOG.md)** - Historial detallado de cambios
- **[OPTIMIZATION_REPORT.md](OPTIMIZATION_REPORT.md)** - Informe técnico de optimizaciones
- **[manual_usuario.md](manual_usuario.md)** - Manual de usuario completo
- **[walkthrough.md](walkthrough.md)** - Guía de desarrollo

---

## 🔧 Arquitectura Técnica

### Estructura de Archivos
```
LegioDualFX/
├── main.cpp                  # Loop principal y gestión de modos
├── ModeFilterDrive.h         # Modo 1: Filtro + Drive
├── ModeSpaceEcho.h           # Modo 2: Delay + Reverb
├── ModeShimmerReverb.h       # Modo 3: Shimmer Reverb
├── ModeShepardTone.h         # Modo 4: Shepard Tone
├── PlateReverb.h             # Reverb auxiliar
├── Makefile                  # Configuración de compilación
└── build/                    # Binarios compilados
```

### Gestión de Memoria
- **SRAM**: Variables globales y stack
- **SDRAM**: Buffers de delay/reverb grandes
- **FLASH**: Código del programa (76% usado)

### Optimizaciones Clave
1. **Cálculos fuera del loop**: Parámetros mode-specific calculados 1 vez por buffer
2. **Constantes nombradas**: Todas las magic numbers reemplazadas
3. **Interpolación mejorada**: Cubic en wavefolder, Hermite en delays
4. **Noise generation**: LCG para flutter orgánico

---

## 🎵 Calidad de Audio

### Especificaciones
- **Sample Rate**: 48kHz
- **Bit Depth**: 32-bit float interno
- **Latency**: ~1ms (buffer size dependent)
- **THD+N**: <0.1% (modos clean)
- **Dynamic Range**: >100dB

### Características DSP
- **Oversampling**: 2x Hermite en drive
- **Anti-aliasing**: Filtros LPF de 24dB/oct
- **Limiting**: Adaptativo por modo
- **Stereo**: True stereo con Mid/Side processing

---

## 🤝 Contribuciones

Las contribuciones son bienvenidas! Por favor:

1. Fork el repositorio
2. Crea una rama para tu feature (`git checkout -b feature/AmazingFeature`)
3. Commit tus cambios (`git commit -m 'Add some AmazingFeature'`)
4. Push a la rama (`git push origin feature/AmazingFeature`)
5. Abre un Pull Request

---

## 📝 Licencia

Este proyecto está bajo licencia MIT. Ver archivo `LICENSE` para detalles.

---

## 🙏 Créditos

- **Desarrollo**: Xavi SolNig
- **Optimización**: Antigravity AI
- **Platform**: Electrosmith Daisy
- **Hardware**: Noise Engineering Legio

---

## 📧 Contacto

- **GitHub**: [@Pepebotika1980](https://github.com/Pepebotika1980)
- **Repository**: [LegioDualFX](https://github.com/Pepebotika1980/LegioDualFX)

---

## 🔗 Links Útiles

- [Daisy Documentation](https://electro-smith.github.io/libDaisy/)
- [DaisySP Documentation](https://electro-smith.github.io/DaisySP/)
- [Noise Engineering](https://noiseengineering.us/)
- [Electrosmith](https://www.electro-smith.com/)

---

**Hecho con ❤️ y mucho cuidado para la comunidad Eurorack**
