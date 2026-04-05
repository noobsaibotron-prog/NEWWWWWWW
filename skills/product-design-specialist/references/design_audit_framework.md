# Design Audit Framework

Framework strutturato per l'analisi forense di interfacce di plugin audio e prodotti digitali.
Ogni audit produce evidenze concrete che alimentano la Fase 2 (Design Direction).

## Pre-Audit: Raccolta Artefatti

Prima di iniziare, raccogliere:
1. Screenshot dell'interfaccia completa (stato di default).
2. Screenshot di stati alternativi (hover, active, modal aperti) se disponibili.
3. Documentazione esistente del design system (se presente).
4. Riferimenti competitivi citati dall'utente.

## Audit 1 — Palette Audit

**Obiettivo:** Valutare la coerenza e la qualità del sistema cromatico.

**Checklist:**
- Contare i colori distinti usati (escludendo variazioni di opacità dello stesso colore).
- Classificare ogni colore per ruolo: background, surface, text-primary, text-secondary, accent-1, accent-2, accent-3, success, warning, error, info.
- Verificare il rapporto di contrasto testo/sfondo (WCAG AA minimo 4.5:1 per testo normale).
- Identificare colori che competono per attenzione (due o più accenti saturi nella stessa zona).
- Verificare se esiste una temperatura cromatica dominante (calda, fredda, neutra) o se è incoerente.

**Output:** Tabella colori con hex, ruolo, area di utilizzo e giudizio (coerente/incoerente/ridondante).

**Soglia critica:** Più di 5 colori accento distinti = palette caotica. Meno di 2 = palette monotona.

## Audit 2 — Hierarchy Audit

**Obiettivo:** Valutare se l'occhio dell'utente è guidato correttamente.

**Checklist:**
- Identificare l'elemento visivamente dominante (dovrebbe essere il contenuto principale, es. il grafico EQ).
- Verificare che i controlli secondari abbiano peso visivo inferiore al contenuto principale.
- Controllare la gerarchia tipografica: quanti livelli di dimensione/peso font sono usati?
- Verificare lo spacing: gli elementi correlati sono raggruppati? C'è separazione tra gruppi diversi?
- Identificare "punti di competizione": zone dove due o più elementi hanno lo stesso peso visivo.

**Output:** Mappa della gerarchia con livelli (L1 dominante → L5 terziario) e anomalie.

**Soglia critica:** Più di 3 punti di competizione = gerarchia rotta.

## Audit 3 — Material Audit

**Obiettivo:** Valutare la coerenza dei materiali virtuali e delle superfici.

**Classificazione materiali:**
| Materiale | Caratteristiche | Esempio |
|-----------|----------------|---------|
| Flat | Nessuna profondità, colori solidi | Bottoni CSS standard |
| Soft Shadow | Ombre leggere, leggera profondità | Card Material Design |
| Neumorphism | Ombre interne ed esterne, effetto "premuto" | Toggle iOS-style |
| Glassmorphism | Trasparenza, blur, bordi sottili | Panel macOS Big Sur |
| Skeuomorphism | Simulazione materiali reali (metallo, legno) | Knob metallici 3D |
| Hybrid | Mix di due o più stili | Flat + knob skeuomorph |

**Checklist:**
- Classificare ogni superficie/componente nel materiale corrispondente.
- Verificare se lo stile è coerente (tutto flat, tutto glass, ecc.) o ibrido.
- Se ibrido, verificare se il mix è intenzionale e armonioso o casuale.

**Output:** Lista componenti con materiale assegnato e giudizio di coerenza.

**Soglia critica:** Più di 3 stili materiali diversi senza logica = incoerenza materica.

## Audit 4 — Density Audit

**Obiettivo:** Valutare il rapporto tra contenuto e spazio respirabile.

**Checklist:**
- Dividere l'interfaccia in zone (top bar, main area, side panel, bottom bar).
- Per ogni zona, stimare la percentuale di spazio occupato da elementi vs spazio vuoto.
- Identificare zone "soffocate" (>85% di contenuto) e zone "vuote" (>70% di spazio vuoto).
- Verificare il padding interno dei componenti (troppo stretto = claustrofobico, troppo largo = dispersivo).
- Controllare la distanza tra gruppi di controlli correlati.

**Output:** Mappa di densità per zona con percentuali e giudizio.

**Soglia critica:** Densità media >80% = interfaccia affollata. Densità media <30% = interfaccia dispersiva.

## Audit 5 — Consistency Audit

**Obiettivo:** Verificare l'uniformità degli elementi di design.

**Checklist:**
- Border radius: tutti i componenti usano lo stesso raggio o ci sono variazioni?
- Ombre: stile e dimensione delle ombre sono coerenti?
- Font: quanti font family diversi sono usati? (ideale: 1-2).
- Dimensioni dei controlli: i bottoni hanno altezze coerenti? I knob hanno dimensioni proporzionate?
- Iconografia: le icone seguono lo stesso stile (outline, filled, duotone)?
- Spaziatura: il padding/margin segue una griglia coerente (es. multipli di 4px o 8px)?

**Output:** Lista delle incongruenze con gravità (minore/maggiore/critica).

**Soglia critica:** Più di 5 incongruenze maggiori = design system assente.

## Sintesi dell'Audit

Dopo aver completato tutti e 5 gli audit, produrre una sintesi con:

```
## Design Audit Summary — [Nome Prodotto]
- **Date:** [Data]
- **Artifact:** [Screenshot/File analizzato]
- **Overall Score:** [1-10]
- **Critical Issues:** [Lista dei problemi con gravità critica]
- **Strengths:** [Cosa funziona bene]
- **Primary Recommendation:** [Tier di redesign suggerito: T1-T5]
- **Design Direction Hint:** [Suggerimento iniziale per la Fase 2]
```
