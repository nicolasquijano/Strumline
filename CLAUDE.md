# Strumline: guitarra a MIDI

> Renombrado a **Strumline** el 2026-11-23 (antes Gmidi), también la carpeta del repositorio (`C:\Users\Nico\Documents\GitHub\Strumline`). El espacio de nombres `gm` del código no tiene relación con el nombre viejo. Si algo (un MCP registrado por carpeta, el historial de sesiones) sigue apuntando a la ruta vieja `...\GitHub\Gmidi`, hay que volver a registrarlo.

Plugin (VST3 y Standalone, Windows, C++20 + JUCE 8.0.16) que escucha una guitarra eléctrica común y toca lo que oye como MIDI, en tiempo real. Proyecto
aparte de Schematic Amp Sim (`../SchematicAmpSim`, otro repositorio), con el que solo comparte la versión de JUCE y el estilo de la interfaz.

**Antes de hacer nada, leé `docs/PLAN_GUITAR_MIDI.md` (sobre todo la sección 8: estado y resultados) y el `README.md`.**

## Cómo trabajar con el usuario
- Escribe en español. Respuestas cortas y concretas; cuando algo no funciona o no se probó, decirlo.
- No commitear ni subir nada si no lo pide ("commit"). Antes de descargar archivos o instalar paquetes, pedir permiso diciendo qué, de dónde y cuánto pesa.
- No operar la pantalla del usuario mientras la usa. El usuario toca en REAPER.
- Para compilar: `build.cmd` (usa las Build Tools de Visual Studio ya instaladas); `build.cmd target <nombre>`. Para ver resultados: `strumline_check` (PASS/FAIL
  sin ventana), `strumline_eval` (contra GuitarSet), `strumline_shot` (captura de la interfaz). Lo compilado se instala copiando a
  `%LOCALAPPDATA%\\Programs\\Common\\VST3\\Strumline.vst3` (con REAPER cerrado o sin el plugin cargado) y `%LOCALAPPDATA%\\Programs\\Strumline\\Strumline.exe`.

## Estado (2026-11-23, acordes hechos)
- Linux y macOS: el código quedó portable (dlopen, std::condition_variable, CMake por sistema, `build.sh`, `tools/fetch_onnxruntime.cmake`, workflow de GitHub Actions) pero SIN PROBAR fuera de Windows (no hay WSL ni compilador de Linux/Mac acá).
- Interfaz ampliada (ver "Interfaz" al final del plan): latencia de nota y de acorde, carga del modelo, cents en las cuerdas, carril de bend, presets, PANIC, Fast/Saver.
- Hecho: interfaz (como la maqueta), parámetros, salida MIDI, sinte de monitoreo, medidor, convertidor de UNA nota (`engine/mono_transcriber.h`, 28-40 ms de
  latencia, pitch bend, GuitarSet solos F 0,61) conectado al plugin, `strumline_check` con 26 comprobaciones que pasan.
- Decisiones del usuario: guitarra común (sin hexafónica), tocar sintes EN VIVO (manda la latencia), DAW = REAPER, plugin aparte, sinte de monitoreo simple.
- ACORDES hechos (fase 2 del plan, sección 8): `engine/basic_pitch.h`, `poly_worker.h`, `note_merger.h`; modos Mono / Poly / Hybrid. Medido: acordes de GuitarSet F 0,72 (Poly) / 0,68 (Hybrid), latencia ~85 ms; CPU ~0,8-1 núcleo mientras se toca (el modelo corre cada 35 ms). Instalado en el VST3 y el Standalone (con `onnxruntime.dll` al lado).
- Pendiente: que el usuario pruebe los tres modos con su guitarra y en REAPER (sobre todo si Hybrid se siente más rápido que Poly: en GuitarSet casi no gana) y el modo
  de una nota; bajar el CPU (cálculo incremental de Basic Pitch) y la latencia (modelo causal propio); MPE real y pitch bend por nota; la latencia del plugin que ve el host.

## Datos y herramientas fuera del repositorio
- `C:\Users\Nico\Documents\SAS-datasets\guitarset`: GuitarSet (CC BY 4.0), con `extracted_solo` y `extracted_comp` (wav + txt de notas) para `strumline_eval`.
- `C:\Users\Nico\Documents\SAS-datasets\venv`: Python con onnxruntime, numpy y `basic-pitch` (instalado con `--no-deps`, para su `nmp.onnx`); lo usa `tools/bp_eval.py`.
  No hay `python` en el PATH del shell: usar `SAS-datasets\venv\Scripts\python.exe`.
- ONNX Runtime 1.32.0 (MIT): `C:\Users\Nico\Documents\SAS-datasets\onnxruntime` (`include/`, `bin/onnxruntime.dll`, `lib/`), sacado del paquete NuGet Microsoft.ML.OnnxRuntime (la
  página de releases de GitHub dio 404). CMake lo busca ahí (`STRUMLINE_ORT_DIR`) y copia las DLL junto a cada ejecutable y al VST3.
- `strumline_eval <carpeta> <seg> <sens> <clave> <subida> <hold> <mono|poly|hybrid> <mirada en fotogramas> <umbral de ataque> <claridad solo> <claridad acorde> <hop>`.
- MCP de REAPER (xDarkzx/Reaper-MCP, commit 525b62c, ya auditado): registrado solo para esta carpeta (`claude mcp get reaper`), en
  `C:\Users\Nico\Documents\Tools`. El usuario carga a mano `reaper_scripts\reaper_mcp_server.lua` en REAPER; no lee el MIDI en vivo de un plugin.

## Trampas conocidas
- Al editar archivos con scripts de Python desde el shell, un `\n` dentro de una cadena C++ se vuelve un salto de línea real y rompe la compilación: usar
  cadenas crudas o la herramienta de edición (también pasa con `sed`).
- `juce_add_plugin` crea el objetivo `Strumline`; los ejecutables de prueba se enlazan con `Strumline` (no con `Strumline_SharedCode`).
- `onnxruntime.dll` se carga por ruta completa desde la carpeta del módulo (Windows trae otra copia vieja en System32); el VST3 instalado necesita las dos DLL junto a `Strumline.vst3`.
