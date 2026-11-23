# Plan: guitarra a MIDI polifónico (Strumline)

> **Dónde vive:** este proyecto está en su propia carpeta, `C:/Users/Nico/Documents/GitHub/Strumline` (antes `Gmidi`; decidido el 2026-11-22), fuera del repositorio de
> Schematic Amp Sim. Las partes que dicen "el rack" o "el motor de Schematic Amp Sim" quedaron de cuando se pensaba como un módulo del
> ampli; ahora es un plugin aparte con su propio código. El estado y los resultados de cada fase están al final (sección 8).

## 1. Objetivo

Un módulo del rack que escucha la guitarra y genera notas MIDI **polifónicas** (acordes, arpegios, notas que suenan juntas), en tiempo
real, para tocar sintetizadores o grabar MIDI en la DAW. Lee la señal cruda, antes del ampli y de los efectos, igual que el afinador.

**Criterios de éxito** (medibles, ver la fase 0):

| Qué | Meta |
|---|---|
| Notas correctas en acordes (medida F sobre un conjunto de grabaciones anotadas, ataque a ±50 ms) | ≥ 0,80 en tiempo real (para comparar: la transcripción fuera de línea con el mismo modelo) |
| Latencia desde el ataque hasta la nota MIDI | ≤ 30 ms en las cuerdas agudas, ≤ 60 ms en las graves |
| Notas fantasma (falsos positivos) con una cuerda sonando sola | casi ninguna: el modo monofónico tiene que ser al menos tan bueno como el detector del afinador |
| CPU | ≤ 10 % de un núcleo a 48 kHz, sin asignaciones en el hilo de audio |
| Robustez | ningún cierre con señal nula, ruido, saturación ni con el modelo sin cargar |

## 2. Por qué es difícil (y qué no se puede evitar)

- **Física de las notas graves.** Entre E2 (82,4 Hz) y F2 (87,3 Hz) hay 5 Hz. Separarlas con un análisis de frecuencia clásico pide una
  ventana de unos 200 ms; para leer una sola nota alcanza con 2 o 3 períodos (25 a 40 ms), pero en un acorde los armónicos de una cuerda caen
  sobre los fundamentales de otra. Un acorde grave **siempre** va a tardar más que una nota aguda sola.
- **Armónicos y octavas.** El tercer armónico de A2 es casi un E4; el segundo de cualquier nota es su octava. El método tiene que decidir si
  hay una nota nueva o es un armónico de otra.
- **Ataque y final.** Una nota repetida (la misma cuerda, sin silencio) y una nota que se apaga lentamente son los casos donde todos los
  convertidores fallan; hace falta un detector de ataques aparte del tono.
- **Salida MIDI desde un efecto.** Un efecto VST3 puede emitir MIDI, pero no todas las DAW lo enrutan (REAPER y Cubase sí; Studio One está
  por verificar en la fase 0). El Standalone no tiene ese problema: manda a un puerto MIDI (un driver virtual como loopMIDI para llegar a otro
  programa).

## 3. Caminos para lo polifónico

| Camino | Calidad | Latencia | Qué hace falta |
|---|---|---|---|
| **A. Pastilla hexafónica** (una señal por cuerda, como un GK de 13 pines con su interfaz) | la mejor: cada cuerda es un problema monofónico, el detector del afinador ya lo resuelve | la del modo mono (≈ 2 períodos) | la pastilla y una interfaz con 6 entradas; el plugin con un bus de 6 canales |
| **B. Red neuronal sobre una guitarra común** (del tipo de Basic Pitch de Spotify: un modelo chico, ~17 mil parámetros, sobre una transformada de Q constante con armónicos apilados) | buena en grabaciones, peor en tiempo real (pierde contexto futuro) | de 40 a 100 ms según cuánto "futuro" se le deje mirar | portar el modelo y sus pesos, correrlo por cuadros (≈ 11,6 ms) en un hilo aparte, y un seguidor de notas propio |
| **C. DSP clásico multitono** (estimación iterativa: encontrar el fundamental más fuerte, restar sus armónicos, repetir) | aceptable con 2 o 3 notas, se degrada con acordes completos | ventanas largas en los graves (100 a 200 ms) | solo código propio |

**Decisión:** B como camino principal (guitarra común, sin pastilla hexafónica); A queda fuera por ahora, y C solo como respaldo si B no
cumple la CPU o la licencia. El modo monofónico (el detector del afinador) se hace primero porque valida toda la cadena
MIDI y es útil por sí mismo.

## 4. Arquitectura

```
entrada cruda (antes del rack, como el afinador)
   │  ring buffer sin bloqueo (hilo de audio, costo ≈ 0)
   ▼
hilo de análisis ── frente: transformada de Q constante por cuadros (3 bins por semitono, E2..E6 + armónicos)
   │                ├─ modo mono: PitchDetector (el del afinador) + detector de ataques
   │                ├─ modo poli: red neuronal → probabilidades por cuadro: ataque, nota activa, contorno de tono
   │                └─ modo hexa: 6 detectores mono, uno por cuerda
   ▼
seguidor de notas: umbrales con histéresis, duración mínima, máx. 6 notas (una por cuerda), rango de la afinación del afinador,
                   velocidad desde la fuerza del ataque, pitch bend desde el contorno (por nota → MPE opcional)
   │  cola de eventos sin bloqueo, con la marca de tiempo de cada evento
   ▼
hilo de audio: escribe los eventos en el MidiBuffer del bloque (salida MIDI del plugin), al puerto MIDI del Standalone,
               y al sintetizador interno de monitoreo (opcional)
```

- **Motor en `engine/`, sin JUCE** (como el resto): `sas/guitar_midi.h` con una interfaz `NoteTranscriber` y tres implementaciones
  (`MonoTranscriber`, `NeuralTranscriber`, `HexTranscriber`), el `NoteTracker` y el frente de Q constante. Se puede probar y medir solo.
- **Inferencia:** el modelo es chico, alcanza con convoluciones propias o con RTNeural (BSD-3, ya usado por los plugins de NAM). Sin
  bibliotecas pesadas (nada de ONNX Runtime ni TensorFlow en el plugin).
- **El análisis no corre en el hilo de audio:** corre en un hilo propio del procesador (no en el temporizador del editor como el afinador,
  porque tiene que funcionar con la ventana cerrada). El hilo de audio solo copia muestras y vacía la cola de eventos.
- **Plugin aparte, no un módulo del rack** (decidido el 2026-11-22 a propuesta tuya): un plugin nuevo, "Schematic Guitar MIDI", con su propio
  VST3 y su propio Standalone, con su propio motor en `Strumline/engine/` (sin JUCE: detector de tono, frente de Q constante, seguidor de
  notas, sintetizador). Si más adelante conviene, Schematic Amp Sim puede usar partes (por ejemplo el detector del afinador, que ya existe
  allá y que este plugin puede copiar para el modo de una nota). Por qué:
  - el plugin del ampli no cambia (ni su ID de clase, ni `NEEDS_MIDI_OUTPUT`, ni cómo lo trata cada DAW), así que no hay riesgo para los
    proyectos que ya existen;
  - se coloca donde conviene en la cadena: primero, con la guitarra limpia, pasando el audio sin tocarlo (opcional) hacia el ampli, mientras
    emite el MIDI hacia el instrumento; o en su propia pista;
  - su latencia y su CPU (la red neuronal) no se suman a las del ampli, y quien no lo necesita no lo carga;
  - se puede publicar y corregir por separado, con su propio número de versión;
  - el modelo y sus pesos no engordan el plugin del ampli.
  El precio: dos proyectos que mantener, cada uno con su CMake, su instalador, sus capturas y su README. El afinador de Schematic Amp
  Sim no cambia: sigue usando su detector.
- **Standalone:** selector de puerto de salida MIDI en los ajustes de audio (loopMIDI u otro driver virtual para llegar a otro programa).

**Parámetros previstos:** encendido, modo (mono / poli / hexa), sensibilidad (umbral en dB, como el SENS del afinador), "mirada adelante"
(latencia contra precisión: 0 a 60 ms), canal MIDI, transposición, curva de velocidad, pitch bend (no / por nota MPE / global), rango del
bend, duración mínima de nota, máximo de notas, y monitoreo con el sintetizador interno (nivel).

**Panel:** un teclado o rollo de piano con las notas que están sonando, las cuerdas (reusa el dibujo del afinador), el nivel de entrada y
la latencia medida.

## 5. Fases

### Fase 0 — Investigar y medir (antes de escribir el módulo)
- Confirmar licencias: el modelo y los pesos de Basic Pitch (Apache 2.0, compatible con la AGPL del proyecto), el conjunto de prueba
  GuitarSet (grabaciones de guitarra con las notas anotadas, CC BY 4.0) y RTNeural (BSD-3). Nada de esto se copia al repositorio sin
  revisar la licencia; los datos de prueba se descargan aparte, como JUCE.
- Herramienta `tools/bp_eval.py` (hecha): pasa un transcriptor por las grabaciones de GuitarSet y da la medida F de notas, los falsos positivos y la
  latencia del ataque. Mide primero el modo mono (el detector del afinador) sobre las partes de una sola nota, y el modelo fuera de línea
  como techo.
- Probar en tus DAW (Studio One, REAPER) si un efecto VST3 que emite MIDI se puede enrutar a una pista de instrumento.
- **Salida:** un informe con los números y la decisión de seguir con B tal como está o ajustar.

### Fase 1 — Cadena MIDI y modo mono
- Módulo `GuitarMidi` en el rack, parámetros, estado guardado en el proyecto.
- Salida MIDI del plugin y puerto MIDI del Standalone, sintetizador interno simple (seno / diente de sierra con envolvente) para escuchar.
- Modo mono con el `PitchDetector` + detector de ataques + pitch bend.
- **Hecho cuando:** en tu DAW, una pista de instrumento toca lo que tocás nota por nota, y `sas_midi_eval` da en mono al menos lo mismo que
  el afinador.

### Fase 2 — Polifónico con la red neuronal (el núcleo)
- Frente de Q constante por cuadros, la red portada con sus pesos, comparada número a número con el modelo original sobre las mismas
  entradas.
- Versión de streaming: cuánto contexto futuro usa (la "mirada adelante") como parámetro; medir precisión contra latencia en GuitarSet.
- `NoteTracker` polifónico (histéresis, máx. 6 notas, rango de la afinación, repetición de nota por el ataque, final por caída de la
  probabilidad y del nivel).
- **Hecho cuando:** cumple la tabla de la sección 1 en GuitarSet y suena bien con tu guitarra en acordes abiertos, power chords y arpegios.

### Fase 3 — Hexafónico (fuera del plan por ahora)
- Decidido el 2026-11-22: se usa una guitarra común, sin pastilla hexafónica. Queda como idea para más adelante: bus de 6 entradas en el
  plugin y en el Standalone, un detector mono por cuerda, el mismo seguidor de notas.

### Fase 4 — Interfaz y textos
- Panel del módulo (rollo de piano, cuerdas, nivel, latencia), textos en los 9 idiomas, capturas, README y CHANGELOG.

### Fase 5 — Pulido
- CPU (meta de la sección 1), pruebas de robustez (silencio, ruido, saturación, cambio de frecuencia de muestreo, quitar el módulo en
  marcha), y prueba con el instalador en una DAW limpia.

## 6. Riesgos

| Riesgo | Qué hacemos |
|---|---|
| El modelo en tiempo real pierde mucha precisión sin contexto futuro | la "mirada adelante" como parámetro; si no alcanza, entrenar o ajustar una versión causal (es un trabajo aparte, más largo) |
| La DAW no enruta el MIDI de un efecto | segundo plugin chico como instrumento / efecto MIDI, o el Standalone con puerto virtual |
| CPU del modelo | el modelo es chico; correrlo cada 11,6 ms en su hilo; si hace falta, bajar la resolución del frente |
| Notas fantasma por armónicos | umbrales por nota, máximo de notas, rango de la afinación, y en mono usar el detector del afinador |
| Licencias | revisar todo en la fase 0, antes de copiar código o pesos |

## 7. Preguntas para decidir antes de empezar

1. ~~¿Tenés o vas a usar una pastilla hexafónica?~~ **No: guitarra común.** El camino es B (red neuronal) y la fase 3 queda fuera.
2. ~~¿Para qué lo vas a usar principalmente?~~ **Tocar sintes en vivo: manda la latencia.** La "mirada adelante" arranca corta (0 a
   20 ms) y el seguidor de notas prefiere disparar rápido y corregir el tono con el pitch bend antes que esperar a estar seguro; la meta de
   latencia de la sección 1 es obligatoria, la de precisión se negocia. El sintetizador interno sube de prioridad (escuchar al instante).
3. ~~¿En qué DAW?~~ **REAPER.** REAPER pasa el MIDI que emite un efecto a los efectos que siguen en la misma pista y a otras pistas por un
   envío con MIDI, así que el camino es: la pista de guitarra con Schematic Amp Sim (y este módulo) y un envío MIDI a una pista con el
   instrumento, o el instrumento después del plugin en la misma cadena. Se verifica en la fase 0 con un instrumento de REAPER (ReaSynth).
4. **Sintetizador interno: sí, simple** (decidido por el uso en vivo; se puede apagar). Seno, diente de sierra y cuadrada con envolvente y
   un filtro, para escuchar la conversión sin cargar otro instrumento; no pretende reemplazar un sinte de verdad.

## 8. Estado y resultados

### Fase 0 — Investigar y medir (en curso, 2026-11-22)

**Licencias (confirmadas):**
- Basic Pitch, código: Apache 2.0 ([spotify/basic-pitch](https://github.com/spotify/basic-pitch)). Los pesos: la
  [página del modelo en Hugging Face](https://huggingface.co/spotify/basic-pitch/tree/main) los marca Apache 2.0, aunque ahí no hay un archivo
  LICENSE aparte; el archivo del modelo (`nmp.onnx`, 225 KB) viene dentro del paquete `basic-pitch` de PyPI, que sí lleva su LICENSE.
- GuitarSet: [CC BY 4.0](https://zenodo.org/records/3371780) (citar a Xi et al., ISMIR 2018). RTNeural: BSD-3
  ([jatinchowdhury18/RTNeural](https://github.com/jatinchowdhury18/RTNeural)).

**El modelo (de `models.py` y `constants.py` de Basic Pitch):** entrada de 43 844 muestras a 22 050 Hz (2 s) con salto de 256 muestras
(11,6 ms por fotograma); transformada de Q constante de 3 contenedores por semitono apilada con 8 armónicos; 6 capas convolucionales con relleno
"same" (núcleos de 3, 5, 7, 7, 5 y 3 a lo largo del tiempo); salidas de inicio de nota (onset), nota activa y contorno de tono, 172 fotogramas.
**Campo receptivo en el tiempo: 21 fotogramas, es decir 10 fotogramas (116 ms) de futuro.** El propio Basic Pitch descarta los 15 fotogramas de
cada borde de su ventana porque ahí el resultado no es confiable, y un convertidor en vivo trabaja justo en ese borde.

**Resultados de `tools/bp_eval.py`** (emula el tiempo real: para contestar el fotograma f con L fotogramas de futuro, el modelo recibe solo el
audio hasta f+L; nota correcta = mismo tono y ataque a ±50 ms):

| Futuro (L) | Guitarra sintética (13 notas, acordes de 4) | GuitarSet, acordes (7 archivos, F por archivo) |
|---|---|---|
| 0 (0 ms) | no detecta ninguna nota | 0,00 |
| 3 (35 ms) | 5 de 13 detectadas | 0,05 a 0,49 |
| 6 (70 ms) | 13 de 13, con notas falsas; latencia ≈ 83 ms | 0,46 a 0,82 |
| 10 (116 ms) | igual | 0,39 a 0,84 |
| 15 (174 ms, el modo normal de Basic Pitch) | igual | 0,38 a 0,85 |

**Tabla completa** (`results/bp_eval.txt`: 8 archivos de acordes de GuitarSet, 10 s cada uno, ataque a ±50 ms y mismo tono; latencia = desde el ataque
real hasta que el fotograma está disponible, sin el tiempo que tarda el modelo):

| Futuro (L) | Precisión | Recall | F | Latencia mediana / p90 |
|---|---|---|---|---|
| 0 (0 ms) | 0,00 | 0,00 | 0,00 | no detecta |
| 3 (35 ms) | 0,69 | 0,31 | 0,43 | 41 / 54 ms |
| 6 (70 ms) | 0,63 | 0,63 | 0,63 | 76 / 108 ms |
| 10 (116 ms) | 0,62 | 0,69 | 0,65 | 124 / 158 ms |
| 15 (174 ms) | 0,62 | 0,66 | 0,64 | 181 / 215 ms |

La precisión y el recall de 0,6 a 0,7 son para el seguidor de notas sencillo de `bp_eval.py` (umbral de ataque 0,5); el seguidor de verdad de la
fase 2 puede mejorarlo. La inferencia de una ventana completa tarda unos 6,6 ms con 8 hilos (30 a 66 ms con 1).

**Conclusiones hasta ahora:**
1. **Con los pesos originales no se cumple la meta de latencia** (30 ms en agudas, 60 ms en graves): con 35 ms de futuro el modelo casi no
   sirve, y con 70 ms (la latencia real ≈ 80 a 90 ms contando el pico de la onda de inicio) rinde casi lo mismo que con todo su contexto.
   Para tocar sintes en vivo, 80 a 90 ms se sienten, aunque se puede tocar con eso.
2. **Velocidad de la inferencia:** una ventana completa tarda 30 a 66 ms en un hilo con ONNX Runtime. En el plugin habría que calcularla de
   forma incremental (solo los fotogramas nuevos y su contexto, ≈ 21/172 del costo por fotograma); es trabajo propio de la fase 2.
3. **Camino alternativo para la latencia:** un modelo causal propio (entrenado o ajustado con GuitarSet, que es CC BY 4.0) con 2 a 3
   fotogramas de futuro. Es lo que haría falta para llegar a la meta; es un trabajo aparte y largo, y hay que decidirlo antes de la fase 2.
4. **Modo híbrido posible:** para una sola nota (las líneas de solo), el detector del afinador responde en 25 a 40 ms sin red neuronal;
   la red solo hace falta cuando suenan varias notas a la vez. Un modo "rápido para una nota, red para acordes" puede dar la mejor sensación
   en vivo con el modelo actual.

**Pendiente de la fase 0:** la prueba de la salida MIDI en REAPER (con `probe/` o, mejor, con el botón TEST del cascarón, ver abajo) y la
decisión entre "modelo actual con 80 a 90 ms", "modo híbrido" o "entrenar un modelo causal".

### Fase 1 — Cascarón del plugin (hecho el 2026-11-23, sin el convertidor)
Plugin `Strumline` (VST3 y Standalone, `plugin/`): parámetros, salida MIDI, sintetizador de monitoreo, medidor de nivel e interfaz tal como la
maqueta (`docs/interface.png`). El convertidor todavía no existe: las notas salen de un guion de prueba incorporado (el botón TEST: un arpegio y
un acorde de cinco notas, nueve segundos en bucle), que sirve para probar toda la cadena (MIDI hacia el sinte, monitoreo, dibujo). Motor sin JUCE en
`engine/` (`note_bus.h`, `monitor_synth.h`). `strumline_check` verifica sin host: las notas MIDI con su canal, transposición y curva de velocidad, el
sinte, el interruptor THRU y el all-notes-off al detener (15 comprobaciones, todas pasan). Pendiente del cascarón: MPE real (hoy "MPE" envía como
"Channel"), el pitch bend, la latencia medida (muestra "-- ms") y el motor.

### Fase 1b — Convertidor de una nota (hecho el 2026-11-23)
`engine/pitch_detector.h` (McLeod, el del afinador) y `engine/mono_transcriber.h`: detecta el ataque (subida de más de 8 dB sobre el nivel de 10 a 40 ms
antes, sobre la sensibilidad), busca el tono en el audio desde el ataque apenas caben dos períodos y medio de la nota más grave posible (una nota aguda
se oye a los 10 ms, un Mi grave al aire a los 35 ms), manda la nota cuando tres lecturas seguidas coinciden, sigue el tono como pitch bend (hasta dos
semitonos), detecta el cambio de nota sin ataque nuevo (ligados) por un salto de tono, y termina la nota cuando el nivel cae bajo la sensibilidad.
Conectado al plugin (los tres modos tocan una nota a la vez por ahora); la latencia que muestra el plugin es la medida real.

**Medido** (`strumline_eval`, 8 solos de GuitarSet de 15 s, micrófono acústico, nota correcta = mismo tono, de 50 ms antes a 120 ms después del ataque real):

| Cambio | F | Precisión | Recall | Errores de octava |
|---|---|---|---|---|
| Detector del afinador tal cual (umbral de pico 0,9) | 0,40 | 0,48 | 0,34 | 61 de 148 fallos |
| Umbral de pico 0,6 (elige el pico más corto) | 0,60 | 0,72 | 0,51 | 14 de 80 |
| + detección de cambio de nota por salto de tono | 0,61 | 0,65 | 0,58 | 19 de 127 |

Latencia mediana 40 ms (p90 75 ms). Bajar el umbral de subida (6, 5 o 4 dB) detecta más notas pero agrega falsas (F 0,61 / 0,58 / 0,56).
Con guitarra sintética (seis cuerdas al aire) todas las notas salen bien, de 28 a 36 ms después de pulsar, y el ruido de una interfaz (-80 dBFS)
no dispara nada (`strumline_check`, 20 comprobaciones). Una guitarra eléctrica limpia por interfaz es una señal más fácil que ese micrófono acústico.

**Lo que falta del modo de una nota:** los pasajes muy rápidos y las notas muy suaves se pierden (107 de los ~400 ataques de GuitarSet no se
vieron antes del salto de tono); no hay corrección de octava hacia abajo una vez enviada la nota.

### Fase 2 — Acordes (hecho el 2026-11-23, falta probarlo con tu guitarra)
**Piezas** (`engine/`): `basic_pitch.h` (el modelo `models/nmp.onnx` por ONNX Runtime 1.32, el remuestreo a 22 050 Hz y el seguidor de notas: pico de la
probabilidad de ataque ≥ 0,5, la nota termina cuando su probabilidad queda bajo 0,2 dos fotogramas, máximo 6 voces, no corre mientras no suena nada),
`poly_worker.h` (el hilo propio: el hilo de audio solo copia muestras y vacía una cola) y `note_merger.h` (junta los dos convertidores). La DLL de ONNX
Runtime se carga por su ruta completa desde la carpeta del plugin (`onnxruntime.dll` junto a `Strumline.vst3` y a `Strumline.exe`; Windows trae otra copia
más vieja en System32 y no debe usarla); el modelo va dentro del plugin como un arreglo C que genera CMake. ONNX Runtime no está en el repositorio:
`plugin/CMakeLists.txt` lo busca en `STRUMLINE_ORT_DIR` (por defecto `..\..\SAS-datasets\onnxruntime`, con `include/` y `bin/`; salió del paquete NuGet
Microsoft.ML.OnnxRuntime 1.32.0, porque la página de releases de GitHub ya no responde). Sin eso, todo compila igual y Poly e Hybrid tocan como Mono
(la interfaz avisa).

**Modos.** Mono: el convertidor de una nota. Poly: solo Basic Pitch. Hybrid: la primera nota sale a 30-40 ms; si Basic Pitch tiene la misma nota la
confirma (y desde ahí él decide cuándo termina); las notas que solo él tiene se agregan; una nota del convertidor de una nota que Basic Pitch nunca
confirma (habiendo oído otras del mismo ataque) se retira a los 200 ms; el pitch bend solo pasa mientras suena una nota. Mientras se tocan acordes
(Basic Pitch encontró 4 ataques en 120 ms) el convertidor de una nota exige una lectura de tono muy limpia (claridad ≥ 0,97), porque en un acorde lee
una mezcla de cuerdas. Mirada adelante por defecto: 60 ms (5 fotogramas); el modelo corre cada 35 ms (3 fotogramas).

**Medido** (`strumline_eval`, GuitarSet con micrófono acústico, 8 archivos de 15 s en cada carpeta; nota correcta = mismo tono; para el modo con Basic Pitch el
ataque es el del fotograma, a ±50 ms como en la tabla de la fase 0, y la latencia es cuándo se envió la nota, sin los 15 a 20 ms que tarda el modelo):

| Modo (mirada adelante 5 fotogramas) | Acordes: F (precisión / recall) | Solos: F (precisión / recall) | Latencia mediana / p90 (acordes) |
|---|---|---|---|
| Mono (convertidor de una nota) | 0,15 (0,28 / 0,10) | 0,61 (0,65 / 0,58) | 44 / 100 ms (solo las pocas que acierta) |
| Poly (Basic Pitch) | **0,72** (0,76 / 0,69) | **0,77** (0,68 / 0,87) | 86 / 104 ms |
| Hybrid | 0,68 (0,68 / 0,69) | 0,70 (0,58 / 0,88) | 85 / 103 ms (solos: 68 / 95 ms) |

Con la mirada adelante: 4 fotogramas (46 ms) da F 0,71 en acordes con latencia 81 ms; 6 da 0,73 con 103 ms; 8 da 0,74 con 129 ms. El modelo cada 2
fotogramas baja la latencia mediana a 80 ms (F 0,72) y cuesta el doble de CPU. Umbral de ataque 0,4 / 0,6 en vez de 0,5: peor (F 0,58 / 0,59 con la
mirada de 6 de entonces). **Lo que dice esto:** con estos audios, Poly es lo más exacto y Hybrid ahorra solo ~15 ms de mediana en los solos, a costa de
notas falsas del convertidor de una nota (en acordes y en los solos de micrófono acústico). Una guitarra eléctrica limpia por interfaz es una señal más
fácil, así que la comparación real es con tu guitarra: si Hybrid no se siente más rápido, usar Poly. La meta de latencia de la sección 1 (30-60 ms) sigue
sin cumplirse en acordes (80-100 ms): hace falta un modelo causal propio (conclusión 3 de la fase 0).

**CPU.** Una ventana completa tarda ~18 ms con 2 hilos de ONNX Runtime (30 ms con 1) y corre cada 35 ms mientras suena algo: **mientras se toca**, el
proceso usa del orden de 0,8 a 1 núcleo en total (medido en `strumline_check`: 56 a 58 % de un núcleo en un test de 3 s donde suena ~2 s); con silencio el
modelo no corre (~0). Es más que la meta de la sección 1 (10 %): para bajarlo hace falta el cálculo incremental (solo los fotogramas nuevos), que es
trabajo propio. `strumline_check`: 26 comprobaciones, pasan (agrega: el modelo cargado, un acorde de mi mayor rasgueado sale con 5 de sus 6 notas en Poly y
en Hybrid sin notas de más, y todas llegan en menos de 400 ms; el ruido de una interfaz no toca nada en Hybrid).

**CPU, segunda pasada (2026-11-23).** El perfil del modelo en un hilo: 24 de 30 ms son convoluciones sobre los 172 fotogramas de la ventana; bajarlo de verdad pide
el cálculo incremental (solo los fotogramas nuevos), que exige reescribir el modelo fuera de ONNX Runtime (no hay la biblioteca `onnx` para editar el grafo, y la
entrada es de tamaño fijo). Lo que sí se hizo: el modelo corre a ritmo completo (cada 35 ms) solo durante 0,25 s después de que el nivel sube más de 4 dB
(algo se tocó) y, entre ataques, cada 140 ms nada más, para ver cómo terminan las notas. En GuitarSet (tocado muy denso) las corridas bajan de 28,7 a unas 25
por segundo y la F no cambia (acordes 0,72, solos 0,77); con música menos densa se ahorra más. En `strumline_check` el proceso pasa de 56 % a 33 a 38 % de un núcleo
en el test de 3 s. Otra palanca, sin implementar como ajuste: correr cada 4 fotogramas (46 ms) en vez de 3 baja las corridas a ~18 por segundo (-35 %) y suma
~8 ms de latencia (mediana 92 ms en acordes, F igual). Con 1 hilo de ONNX Runtime el CPU total es parecido y cada corrida tarda el doble: se dejaron 2.

### Interfaz (2026-11-23)
Encabezado: VOICES, NOTE (latencia de la primera nota, convertidor de una nota), CHORD (latencia del acorde: desde donde empezó la nota hasta el note-on, con lo que
tarda el modelo) y MODEL (qué parte del tiempo corre el modelo de acordes). MIDI OUT es una luz que destella con cada nota y bend. Las cuerdas muestran los cents de
desvío cuando suena una sola nota (Mono o Hybrid: un afinador gratis). El rollo ajusta su rango a lo que se toca, pinta las notas más claras cuanto más fuerte se
pulsaron y el carril de PITCH BEND dibuja el bend que se envía. Controles nuevos: selector Fast / Saver (el modelo cada 35 o 46 ms; parámetro `perf`), cuatro presets
(Solo lines, Fast hybrid, Chords, Low CPU), botón PANIC (todas las notas apagadas y los convertidores de cero), marcas en 35, 45 y 60 ms en el knob de LOOK-AHEAD,
tooltips en todos los controles y una franja visible cuando falta `onnxruntime.dll`. Sin probar con el ratón en un host (solo la captura de `strumline_shot`).

**Estilo de la interfaz (2026-11-23):** relieve suave sobre gris carbón (botones y knobs levantados, paneles, rollo y barra hundidos) con el dorado como único acento. Los knobs
llevan un anillo de 31 marcas que se enciende hasta el valor; la barra de nivel va a rayas. Lo que no cambia (fondo, paneles, rótulos, marcas del knob de LOOK-AHEAD) se dibuja
una sola vez en una imagen en caché (`paintStatic`), así que el relieve no suma CPU al redibujar a 30 fps. Todo se dibuja con trazos de JUCE, sin imágenes.

### Linux y macOS (2026-11-23)
Qué se hizo para que el código no dependa de Windows: ONNX Runtime se abre a mano con `dlopen` (Linux y macOS) o `LoadLibraryEx` (Windows), buscando la biblioteca
junto al módulo del plugin (`dladdr` / `GetModuleHandleEx`; en un paquete de macOS es `Contents/MacOS`); el hilo del modelo espera con `std::condition_variable` en
vez de un evento de Windows; CMake elige la biblioteca por sistema (`onnxruntime.dll`, `libonnxruntime.dylib` o `libonnxruntime.so`) y la copia junto a cada binario;
`tools/fetch_onnxruntime.cmake` baja el paquete NuGet y deja cada sistema en `runtimes/<rid>/native`; `build.sh` es el `build.cmd` de Linux y macOS. **Sin probar:** esta
máquina es Windows, no hay WSL, compilador de Linux ni Mac, así que lo de Linux y macOS (la compilación, la carga de la biblioteca, que el paquete NuGet traiga cada sistema
con el nombre esperado, JUCE en esos sistemas) se verifica con `.github/workflows/build.yml` cuando el proyecto esté en GitHub. Pendiente aparte: binario universal de macOS,
firma y notarización, formato Audio Unit, y LV2 en Linux. **Verificado en Windows (2026-11-23):** `tools/fetch_onnxruntime.cmake` baja y ordena el paquete (encontró que el paquete 1.32 no trae `osx-x64`, solo win-x64, linux-x64, linux-arm64 y osx-arm64; los nombres `libonnxruntime.so` y `.dylib` son los esperados) y CMake detecta la carpeta nueva (`runtimes/win-x64/native`). En un Mac Intel el convertidor de acordes queda apagado.
