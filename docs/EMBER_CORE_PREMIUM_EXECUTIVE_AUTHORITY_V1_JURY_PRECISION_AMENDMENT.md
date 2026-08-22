# Ember Core Premium — Micro-amend Jury e precisione biquad

## Autorità e stato

```text
Documento sostituito, limitatamente a §2.2 = EMBER_CORE_PREMIUM_EXECUTIVE_AUTHORITY_V1.md
Commit autorità principale                  = 586c6613c3314f2968aeeb5ea93acf3fbcf15947
Decisione autorità                          = APPROVATA
Autorità scientifica                        = Marco
Data                                        = 2026-08-22
Ambito                                      = Jury, precisione biquad, dominio HQ
Tutto il resto dell'autorità V1             = INVARIATO
```

Il margine Jury assoluto `1e-6` è revocato. A frequenze basse e sample rate
interni elevati rifiuta sezioni matematicamente stabili; con coefficienti
`float`, a 768 kHz alcuni denominatori marginali collassano invece realmente
sul confine di stabilità. Non è ammesso risolvere il problema mediante bypass
silenzioso.

## §2.2 sostitutivo — Biquad, Jury e precisione

Per i coefficienti normalizzati effettivamente memorizzati:

\[
J_1=1+a_1+a_2,
\qquad
J_2=1-a_1+a_2,
\qquad
J_3=1-a_2.
\]

La tolleranza usata per valutare numericamente le disuguaglianze è:

\[
\tau=32\,\epsilon_{\mathrm{double}}
\left(1+|a_1|+|a_2|\right).
\]

Una sezione è valida se e soltanto se:

```text
b0, b1, b2, a1, a2 sono finiti
J1 > tau
J2 > tau
J3 > tau
```

`tau` è una tolleranza di valutazione numerica. Non rappresenta una distanza
fisica minima imposta ai poli rispetto al cerchio unitario.

### Routing della precisione

Il percorso storico `float` è conservato esclusivamente nel dominio Legacy
Zero Latency a sample rate host, dove è richiesta la compatibilità congelata.

Usano coefficienti e stati `double`:

- HP/LP e tutte le relative cascate;
- Notch, BandPass e AllPass quando `fs_processing > 192 kHz`;
- qualunque biquad RBJ quando `fs_processing > 192 kHz`;
- ogni sezione esplicitamente appartenente al percorso Natural/HQ.

Surgical Bell e Shelf verranno trasferiti al TPT-SVF `double` nella tranche
successiva. Natural/HQ non può ricadere su coefficienti `float` marginali.

Il dominio resta:

```text
fs_host       in [32 kHz, 192 kHz]
oversampling  in {1, 2, 4}
fs_processing = fs_host * oversampling <= 768 kHz
```

Il limite frequenziale utente è derivato da `fs_host`; i coefficienti sono
calcolati usando `fs_processing`.

### Failure policy

- una sezione invalida invalida l'intera banda;
- la banda deve pubblicare un fault/reason code diagnostico;
- nessuna impostazione valida del profilo ufficiale può essere bypassata;
- un input non finito produce zero e reset;
- uno stato o output non finito con input finito produce il dry sample, reset e
  fault counter.

### HP/LP

```text
resonance = 1  -> Butterworth canonico
altro valore   -> Resonant Cascade
```

Una cascata viene pubblicata soltanto se tutte le sue sezioni sono finite e
superano la guardia Jury sostitutiva.

## Test normativi

Devono essere coperti almeno:

1. HP 30 Hz e LP 18 kHz, 48 dB/oct, a 384 e 768 kHz: attivi, finiti e stabili;
2. Bell, Low Shelf, High Shelf, Notch e BandPass a 20 Hz e ai limiti superiori,
   a 384 e 768 kHz;
3. Jury runtime confrontata con un oracolo multiprecisione/root-pole;
4. impulso lungo e risposta DC/Nyquist bounded;
5. nessun bypass silenzioso del profilo ufficiale a 96 kHz x4 e 192 kHz x4;
6. null test Legacy Zero Latency bit-identico nel dominio congelato;
7. mutation test che ripristina il margine assoluto `1e-6`;
8. mutation test che ripristina coefficienti `float` nel percorso high-rate.

L'oracolo multiprecisione aumenta la precisione finché classificazione dei poli
e arrotondamento sono univoci. `long double` non è un'autorità portabile.

## Sequenza autorizzata

1. freeze docs-only di questa appendice;
2. commit atomico dei tipi/coefficienti `double` e rimozione del bypass oltre
   192 kHz;
3. test numerici e regressivi;
4. soltanto dopo: TPT/Surgical e Dynamic EQ.

Assunzione congelata: il claim di bit-identità Legacy riguarda il percorso
Zero Latency protetto. Natural/HQ privilegia stabilità e precisione `double`.
