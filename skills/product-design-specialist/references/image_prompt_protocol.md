# Image Prompt Protocol

Protocollo per la generazione di prompt immagine strutturati e ad alta fedeltà per mockup di interfacce audio e prodotti digitali.

## Principi Fondamentali

1. **Specificità assoluta:** Ogni elemento deve avere colore hex, dimensione relativa e posizione.
2. **Struttura per zone:** Il prompt è organizzato per aree dell'interfaccia, non per concetti astratti.
3. **Materiali espliciti:** Ogni superficie deve avere un materiale dichiarato (flat, glass, metal, ecc.).
4. **Reference obbligatoria:** Usare sempre l'immagine originale come reference per mantenere il layout.
5. **Aspect ratio coerente:** Usare 16:9 per plugin desktop, 9:16 per mobile, 4:3 per iPad.

## Struttura del Prompt

Ogni prompt deve seguire questa struttura ordinata:

```
[HEADER] — Tipo di prodotto e nome
[COLOR THEME] — Tema cromatico con hex primari
[BACKGROUND] — Colore, texture, gradiente del background
[TOP BAR] — Navigazione, bottoni, toggle
[MAIN AREA] — Contenuto principale (grafico, display, ecc.)
[SIDE PANEL] — Pannelli laterali (se presenti)
[BOTTOM BAR] — Controlli, knob, meter
[OVERALL AESTHETIC] — Feeling generale, materiali, filosofia
```

## Template per Plugin Audio EQ

```
Hyper-premium luxury audio plugin interface for '[NOME PLUGIN]'.
[TIPO]: professional audio equalizer with AI-powered analysis.
macOS window with red/yellow/green traffic lights top-left.

COLOR THEME: [NOME TEMA]. Accent system built around [ACCENT-1 hex],
[ACCENT-2 hex], [ACCENT-3 hex]. Background [BG-DEEP hex].

BACKGROUND: [BG-DEEP hex] with [texture description].
[Ambient light description if applicable].

TOP NAVIGATION BAR: [Material: frosted glass / flat / etc.].
Left: [navigation elements with colors].
Center: [toggle/mode elements with active state color].
Right: [utility buttons, AI badge with ACCENT-1 glow].

MAIN EQUALIZER DISPLAY (dominant center area):
- Grid: [grid line color and thickness]
- Frequency axis: [font style, color, range]
- dB axis: [font style, color, range]
- Main EQ curve: [line color, glow, thickness]
- Spectrum analyzer: [gradient description with hex values]
- Secondary spectrum: [color, transparency]
- Band nodes: [size, glow, colors for each band]
- Tooltip: [material, content, typography]
- Collision indicators: [shape, color, size]

RIGHT PANEL — AI ANALYSIS:
- Panel material: [glassmorphism / flat / etc.]
- Tab styling: [active indicator color and style]
- Alert cards: [border color, typography hierarchy]
- Action buttons: [shape, border, hover state]
- Dynamic EQ knobs: [material, indicator color]

BOTTOM CONTROL BAR:
- Material: [frosted glass / flat / etc.]
- Band selector: [styling]
- Main knobs (FREQ/GAIN/Q): [material, size, indicator color, shadow]
- Secondary knobs (DRY/WET, OUTPUT): [material, size]
- AUTO button: [physical switch style, glow color]
- Level meter: [gradient from-to colors]

OVERALL: [1-2 sentences on the feeling and philosophy].
[Material summary]. [Color restriction]. [Typography note].
```

## Regole per Varianti Tematiche

Quando si genera una variante cromatica di un design esistente:

1. **Usare l'immagine precedente come reference** per mantenere layout e struttura.
2. **Sostituire TUTTI i colori accento**, non solo il primario.
3. **Adattare il background** alla temperatura del nuovo tema.
4. **Adattare i materiali** se necessario (es. tema caldo → metallo spazzolato, tema freddo → cromo).
5. **Mantenere la stessa gerarchia visiva** e lo stesso livello di densità.

## Checklist Pre-Generazione

Prima di inviare il prompt al generatore:
- [ ] Tutti i colori hanno valori hex espliciti?
- [ ] Ogni superficie ha un materiale dichiarato?
- [ ] L'immagine originale è inclusa come reference?
- [ ] L'aspect ratio è corretto per il tipo di prodotto?
- [ ] Il prompt è organizzato per zone (non per concetti)?
- [ ] Il feeling generale è descritto in modo concreto (non vago)?
- [ ] I knob hanno materiale, dimensione relativa e colore indicatore?

## Anti-Pattern da Evitare

- "Make it look premium" → Troppo vago. Specificare QUALI elementi rendono premium.
- "Use nice colors" → Specificare hex, ruoli e relazioni.
- "Add some effects" → Specificare QUALE effetto, su QUALE elemento, con QUALI parametri.
- "Modern design" → Specificare materiali, spacing, tipografia, palette.
- Prompt sotto le 200 parole → Quasi certamente troppo generico per un mockup di qualità.
