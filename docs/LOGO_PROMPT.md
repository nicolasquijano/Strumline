# Strumline: propuesta de logo y prompts

## Qué tiene que comunicar
- **Guitarra a MIDI en vivo**: una cuerda que vibra y se convierte en notas.
- **Strum** (rasgueo): varias cuerdas a la vez, o sea acordes, que es lo que distingue al plugin.
- Estilo de la interfaz: fondo oscuro (`#16171a`) y dorado (`#e0b24a`), el mismo de Schematic Amp Sim.

## Concepto recomendado: "seis cuerdas que se vuelven notas"
Un ícono cuadrado: seis líneas horizontales finas y paralelas (las cuerdas). Del lado izquierdo son líneas continuas y onduladas (la señal de la guitarra). Hacia la derecha cada línea se corta en pequeños bloques rectangulares de distinto largo, como las notas de un piano roll, que forman un acorde escalonado (el rasgueo: cada cuerda empieza apenas después de la anterior). Todo en dorado sobre un cuadrado de esquinas redondeadas casi negro.

Otras dos direcciones, si esta no convence:
- **Letra S como cuerda**: una "S" dibujada con una sola línea que vibra, con una púa pequeña en el extremo.
- **Púa + onda**: una púa de guitarra con una onda de audio que sale de la punta y termina en barras de piano roll.

## Prompt principal (para un generador de imágenes)
```
Minimalist app icon logo for an audio plugin called "Strumline" (guitar to MIDI converter).
Flat vector style, a rounded square icon with a near-black charcoal background (#16171a).
Six thin parallel horizontal gold lines (#e0b24a) that start on the left as smooth wavy guitar string
vibrations and, moving to the right, break up into small rounded gold rectangles of different lengths,
like notes in a piano roll, arranged in a staggered diagonal cascade as if a chord is being strummed.
Clean geometry, generous negative space, no gradients, no glow, no 3D, no text, no shadows,
high contrast, centered composition, looks sharp at 32x32 pixels. 1:1 aspect ratio.
```

## Variante con el nombre (para web y portada)
```
Horizontal logo lockup for "Strumline", an audio plugin. On the left the icon: six thin gold (#e0b24a)
lines that turn from a wavy string into staggered piano-roll note blocks. On the right the word
"STRUMLINE" in a clean, bold geometric sans-serif, all caps, wide letter spacing, gold on a near-black
(#16171a) background. Flat vector, no gradients, no effects, plenty of margin, crisp edges.
```

## Variante en una sola tinta (para ícono pequeño, 16 a 32 px)
```
Single-color version of a logo: six horizontal lines that start wavy on the left and become small
staggered rectangles on the right (a strummed chord as a piano roll). Pure white shapes on a transparent
background, uniform thick line weight, no gradients, simple enough to read at 16x16 pixels, vector style.
```

## Prompt negativo (si el generador lo acepta)
```
text, letters, watermark, guitar photo, realistic, 3D, gradient, glow, neon, shadow, blur,
cluttered, ornate, cartoon, human hands, extra strings, low contrast
```

## Consejos para usarlo
- Pedí 4 a 8 variantes y elegí la que se lea mejor **reducida a 32 px**: el ícono de un plugin se ve chico en el DAW.
- Pedí el nombre aparte (el texto lo escriben mal casi todos los generadores de imágenes) y componelo después con una fuente: por ejemplo Inter Bold o Montserrat Bold, en mayúsculas.
- Si querés un resultado exacto y editable, pedí un **SVG** (a un generador vectorial) o dibujá el concepto a mano con rectángulos en Figma.
- Antes de usar el logo comercialmente, verificá su licencia (la del generador) y que no se parezca a ningún logo existente.
