# Premium Color Systems

Sistemi cromatici predefiniti per interfacce audio professionali e prodotti digitali.
Ogni sistema è completo e auto-contenuto: background, superfici, testo, accenti e stati.

## Regole Universali

1. **Massimo 3 accenti** per sistema (esclusi neutri, grigi e stati semantici).
2. **Contrasto minimo** testo-sfondo: 4.5:1 (WCAG AA).
3. **Temperatura coerente:** ogni sistema ha una temperatura dominante (calda, fredda, neutra).
4. **Accento primario** = colore firma del prodotto, usato per stati attivi e feature principali.
5. **Accento secondario** = complementare o analogo, usato per informazioni secondarie.
6. **Accento terziario** = usato con parsimonia per highlight o stati speciali.

## Struttura di un Color System

Ogni sistema definisce questi ruoli:

| Ruolo | Uso | Note |
|-------|-----|------|
| `bg-deep` | Background principale dell'applicazione | Il colore più scuro, mai nero puro |
| `bg-surface` | Card, pannelli, aree elevate | Leggermente più chiaro di bg-deep |
| `bg-elevated` | Tooltip, dropdown, modal | Ancora più chiaro, con possibile blur |
| `border` | Bordi sottili di separazione | Molto sottile, quasi invisibile |
| `text-primary` | Testo principale, valori | Alto contrasto con bg-deep |
| `text-secondary` | Label, descrizioni | Contrasto medio |
| `text-muted` | Hint, placeholder, disabilitato | Basso contrasto |
| `accent-1` | Colore firma, stati attivi, feature AI | Il colore più riconoscibile |
| `accent-2` | Informazioni secondarie, grafici | Complementare ad accent-1 |
| `accent-3` | Highlight speciali, badge | Usato con parsimonia |
| `state-success` | Conferme, fix applicati | Verde o teal |
| `state-warning` | Avvertimenti, attenzione | Ambra o giallo |
| `state-error` | Errori, problemi critici | Rosso o rosa |
| `knob-body` | Corpo dei potenziometri | Metallico o neutro |
| `knob-indicator` | Indicatore di posizione del knob | Spesso uguale ad accent-1 |
| `meter-low` | Parte bassa dei meter | Colore freddo |
| `meter-high` | Parte alta dei meter (clipping) | Colore caldo |

---

## 1. Electric Blue

**Temperatura:** Fredda. **Feeling:** Tecnologico, preciso, futuristico.

| Ruolo | Hex | Anteprima |
|-------|-----|-----------|
| bg-deep | `#060810` | Nero-blu profondo |
| bg-surface | `#0C1020` | Navy scurissimo |
| bg-elevated | `#121830` | Navy con trasparenza |
| border | `#1A2A40` | Blu-grigio sottile |
| text-primary | `#E8EDF5` | Bianco freddo |
| text-secondary | `#8899B0` | Grigio-blu |
| text-muted | `#4A5A70` | Grigio-blu scuro |
| accent-1 | `#0088FF` | Blu elettrico puro |
| accent-2 | `#00D4FF` | Cyan brillante |
| accent-3 | `#5BC4D6` | Teal ghiaccio |
| state-success | `#00CC88` | Verde-teal |
| state-warning | `#FFAA33` | Ambra calda |
| state-error | `#FF4466` | Rosa-rosso |
| knob-body | `#C8D0DC` | Argento freddo |
| knob-indicator | `#0088FF` | Blu elettrico |
| meter-low | `#003366` | Blu profondo |
| meter-high | `#00D4FF` | Cyan brillante |

---

## 2. Violet Haze

**Temperatura:** Fredda-neutra. **Feeling:** Elegante, misterioso, AI-forward.

| Ruolo | Hex |
|-------|-----|
| bg-deep | `#08090D` |
| bg-surface | `#0D0F18` |
| bg-elevated | `#141828` |
| border | `#2A2D38` |
| text-primary | `#E8E4F0` |
| text-secondary | `#9088A8` |
| text-muted | `#5A5270` |
| accent-1 | `#7B5EFF` |
| accent-2 | `#C9A96E` |
| accent-3 | `#5BC4B0` |
| state-success | `#5BC4B0` |
| state-warning | `#E8A838` |
| state-error | `#C47A7A` |
| knob-body | `#D0C8D8` |
| knob-indicator | `#7B5EFF` |
| meter-low | `#3A2A6B` |
| meter-high | `#C9A96E` |

---

## 3. Emerald Night

**Temperatura:** Fredda-naturale. **Feeling:** Organico, profondo, sofisticato.

| Ruolo | Hex |
|-------|-----|
| bg-deep | `#060D0A` |
| bg-surface | `#0A1510` |
| bg-elevated | `#102018` |
| border | `#1A3028` |
| text-primary | `#E0F0E8` |
| text-secondary | `#80A890` |
| text-muted | `#4A6A58` |
| accent-1 | `#00CC77` |
| accent-2 | `#88DDAA` |
| accent-3 | `#D4AA55` |
| state-success | `#00CC77` |
| state-warning | `#D4AA55` |
| state-error | `#CC4455` |
| knob-body | `#C0D0C8` |
| knob-indicator | `#00CC77` |
| meter-low | `#004422` |
| meter-high | `#88DDAA` |

---

## 4. Warm Gold

**Temperatura:** Calda. **Feeling:** Lussuoso, classico, hardware vintage.

| Ruolo | Hex |
|-------|-----|
| bg-deep | `#0A0908` |
| bg-surface | `#141210` |
| bg-elevated | `#1E1A16` |
| border | `#302A22` |
| text-primary | `#F0E8D8` |
| text-secondary | `#A89878` |
| text-muted | `#6A5A48` |
| accent-1 | `#D4A853` |
| accent-2 | `#E8C878` |
| accent-3 | `#8B7EC8` |
| state-success | `#88BB66` |
| state-warning | `#E8A838` |
| state-error | `#CC5544` |
| knob-body | `#D8C8A8` |
| knob-indicator | `#D4A853` |
| meter-low | `#4A3A18` |
| meter-high | `#E8C878` |

---

## 5. Rose Platinum

**Temperatura:** Calda-neutra. **Feeling:** Moderno, sofisticato, editorial.

| Ruolo | Hex |
|-------|-----|
| bg-deep | `#0A0809` |
| bg-surface | `#141012` |
| bg-elevated | `#1E181A` |
| border | `#302828` |
| text-primary | `#F0E8EC` |
| text-secondary | `#A89098` |
| text-muted | `#6A5860` |
| accent-1 | `#D4708A` |
| accent-2 | `#E8A0B0` |
| accent-3 | `#88AAC8` |
| state-success | `#88BB88` |
| state-warning | `#D4AA55` |
| state-error | `#CC4455` |
| knob-body | `#D8C8D0` |
| knob-indicator | `#D4708A` |
| meter-low | `#4A2838` |
| meter-high | `#E8A0B0` |

---

## 6. Arctic Silver

**Temperatura:** Fredda-neutra. **Feeling:** Clinico, preciso, scientifico.

| Ruolo | Hex |
|-------|-----|
| bg-deep | `#08090A` |
| bg-surface | `#10121A` |
| bg-elevated | `#181C28` |
| border | `#282C38` |
| text-primary | `#E8ECF0` |
| text-secondary | `#8890A0` |
| text-muted | `#505868` |
| accent-1 | `#A0B8D0` |
| accent-2 | `#D0D8E0` |
| accent-3 | `#6888A8` |
| state-success | `#66BBAA` |
| state-warning | `#CCAA55` |
| state-error | `#CC5566` |
| knob-body | `#D0D4DC` |
| knob-indicator | `#A0B8D0` |
| meter-low | `#2A3848` |
| meter-high | `#D0D8E0` |

---

## 7. Sunset Amber

**Temperatura:** Calda. **Feeling:** Energetico, creativo, analogico.

| Ruolo | Hex |
|-------|-----|
| bg-deep | `#0A0806` |
| bg-surface | `#14100C` |
| bg-elevated | `#1E1812` |
| border | `#302818` |
| text-primary | `#F0E8D0` |
| text-secondary | `#A89060` |
| text-muted | `#6A5838` |
| accent-1 | `#FF8833` |
| accent-2 | `#FFBB55` |
| accent-3 | `#CC5533` |
| state-success | `#88BB55` |
| state-warning | `#FFBB55` |
| state-error | `#CC3333` |
| knob-body | `#D0C0A0` |
| knob-indicator | `#FF8833` |
| meter-low | `#4A2808` |
| meter-high | `#FFBB55` |

---

## 8. Deep Crimson

**Temperatura:** Calda-scura. **Feeling:** Potente, drammatico, aggressivo.

| Ruolo | Hex |
|-------|-----|
| bg-deep | `#0A0608` |
| bg-surface | `#140C10` |
| bg-elevated | `#1E1218` |
| border | `#301820` |
| text-primary | `#F0E0E4` |
| text-secondary | `#A87880` |
| text-muted | `#6A4850` |
| accent-1 | `#CC2244` |
| accent-2 | `#E85070` |
| accent-3 | `#D4A853` |
| state-success | `#66BB88` |
| state-warning | `#D4AA55` |
| state-error | `#CC2244` |
| knob-body | `#D0B8C0` |
| knob-indicator | `#CC2244` |
| meter-low | `#4A0818` |
| meter-high | `#E85070` |
