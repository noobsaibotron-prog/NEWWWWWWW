# REV8 — ROUND 2.3 — RESTRICTED RED-TEAM MICRO-AMEND BALLOT

**Stato:** `SIGNED_BY_SCIENTIFIC_AUTHORITY`

## 1. Artefatti vincolati

```text
Predecessore firmato:
docs/REV8_MATCHING_FORMALIZATION_ROUND2_3_NORMATIVE_SIGNED_DRAFT.md
SHA-256:
8f5857a8f0ebd6aa7483a68127d31e58f024ca25a8eebd59abacf0734db3c041

Report red-team:
docs/REV8_ROUND2_3_RESTRICTED_RED_TEAM_REPORT.md
SHA-256:
6f905589e08740fb12ca8cc446365938f217ef2acc48b958cd9441a8a4051841

Amended draft sottoposto a firma:
docs/REV8_MATCHING_FORMALIZATION_ROUND2_3_NORMATIVE_AMENDED_DRAFT.md
SHA-256:
7167d787f6c7940d53baa97cd29c07555d31e31504308110332d3f79cd042011
```

Il ballot è limitato ai finding `RT-001`–`RT-005`. Non riapre le altre
decisioni scientifiche e non autorizza codice, schema live, candidate REV8,
runtime, training, `G1 PASS` o `REV8 SPEC GO`.

---

## 2. Decisioni sottoposte a firma

### R23A_01 — Compound transcendental rounding

**Finding:** `RT-001`.

**Decisione proposta:**

```text
mean_center_error_oct64(R,K1) =
  correctly_round_binary64(log2(R)/K1)
```

L’interval refinement copre l’intera espressione. È vietato arrotondare
`log2(R)` a binary64 prima della divisione.

**Golden vincolante:**

```text
R=9/8, K1=3 -> f64:3fad0022f7fa735f
double-rounded vietato -> f64:3fad0022f7fa7360
```

```text
Decisione R23A_01: APPROVATA
```

### R23A_02 — Scientific K6 signature vs diagnostic replay

**Finding:** `RT-002`.

**Decisione proposta:**

```text
S_can    = min_{M in M*} S(M)
M_replay = unique argmin_{M in M*} (S(M),D(M))
```

`S_can` è l’output scientifico K6. `M_replay` è esclusivamente diagnostico.
Severity, confidence, actionable, ID e hash possono influire su `D` e
`M_replay`, ma non su eligibility, `V*`, `M*`, `S`, `S_can`, metriche o gate.

```text
Decisione R23A_02: APPROVATA
```

### R23A_03 — Byte-authoritative mean hierarchy

**Finding:** `RT-003`.

**Decisione proposta:**

```text
mean64(entries):
  N=0 -> N/A
  N>0 ->
    ordered = sort(entries, by=normative_entry_key)
    s = sum_pairwise64(value64(e) for e in ordered)
    return RN64(exact_rational(s)/N)
```

La divisione avviene una sola volta dopo la riduzione. Le medie di unità,
gruppo e macro usano ricorsivamente `mean64`; la macro-AP usa `mean64` sugli
`AP_group64` ordinati per `group_id`.

**Golden vincolante:**

```text
mean64([0.2,0.3,1.0]) -> f64:3fe0000000000000
divide-first vietato  -> f64:3fdfffffffffffff
```

```text
Decisione R23A_03: APPROVATA
```

### R23A_04 — Unreachable Resonance equality fixture

**Finding:** `RT-004`.

**Decisione proposta:** il requisito “esattamente sul bordo” si applica
soltanto alle uguaglianze raggiungibili nel dominio ammesso. Per Resonance,
`q^3=2` è irraggiungibile per `q` razionale; la suite usa la prova di
irraggiungibilità e due N64 adiacenti che racchiudono il bordo.

**Golden vincolante:**

```text
min center = f64:408f400000000000
inside     = f64:4093afaf27b421db
outside    = f64:4093afaf27b421dc
```

```text
Decisione R23A_04: APPROVATA
```

### R23A_05 — Exact scalar bit-length semantics

**Finding:** `RT-005`.

**Decisione proposta:**

```text
int_bit_length(0) = 0
int_bit_length(z) = floor(log2(abs(z)))+1, se z != 0

rational_bit_length(n/d) =
  int_bit_length(n) + int_bit_length(d)
```

Il razionale è ridotto, con denominatore positivo; il segno non conta. La
misura riguarda valori matematici normativi, non temporanei
implementation-specific. Il ceiling `65536` resta provvisorio: l’addendum
O-09 dovrà attivare atomicamente bound di preflight, scope, soglia, fixture e
blast radius.

**Golden vincolante:**

```text
q = 2^39999/(2^39999+1)
rational_bit_length(q) = 80000
```

```text
Decisione R23A_05: APPROVATA
```

---

## 3. Firma dell’autorità scientifica

La firma approva o respinge il pacchetto atomico `R23A_01`–`R23A_05`. Una
firma parziale non chiude il `BLOCK`.

```text
Decisione complessiva: APPROVO
Firma/nome: Marco
Data: 2026-07-29
SHA-256 amended draft verificato:
7167d787f6c7940d53baa97cd29c07555d31e31504308110332d3f79cd042011
```

Se approvato, il solo passo successivo consentito è il recheck ristretto sullo
SHA firmato. L’approvazione non costituisce `REV8 SPEC GO` e non autorizza
implementazione o patch del candidate.
