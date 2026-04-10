#!/usr/bin/env python3
"""
AIEQ Liquid Intelligence — Design System Validator
===================================================
Tribunale AIEQ+ | Versione 1.0 | 10 Aprile 2026

Script di validazione automatica che Claude deve eseguire dopo ogni Wave
per verificare la conformità del codice al design system "Liquid Intelligence".

Uso:
    python3 aieq_liquid_intelligence_validator.py [path/to/Source]

Se non viene specificato un percorso, lo script cerca ./Source nella directory corrente.
"""

import os
import re
import sys
from pathlib import Path
from dataclasses import dataclass, field
from typing import Optional

# ═══════════════════════════════════════════════════════════════════════════════
# DESIGN SYSTEM REFERENCE (dal rendering Manus "Liquid Intelligence")
# ═══════════════════════════════════════════════════════════════════════════════

AMBER = "0xFFE8A030"
AMBER_BRIGHT = "0xFFF4B84A"
ACCENT_BLUE = "0xFF4A9FD9"  # VIETATO per toggle attivi

# ─── Costanti che DEVONO essere presenti ─────────────────────────────────────

REQUIRED_CONSTANTS = {
    "Logo font size >= 18px": {
        "file_pattern": "PluginEditor.cpp",
        "search": r'logoLabel\.setFont.*withHeight\((\d+\.?\d*)',
        "validator": lambda m: float(m.group(1)) >= 18.0,
        "fix": "Logo font deve essere >= 18.0f (ideale: 20.0f Bold)"
    },
    "Logo color = amber": {
        "file_pattern": "PluginEditor.cpp",
        "search": r'logoLabel\.setColour.*textColourId.*Colors::(\w+)',
        "validator": lambda m: m.group(1) == "amber",
        "fix": "Logo deve usare Colors::amber, non textPrimary o accentBlue"
    },
    "EQ curve main stroke >= 2.0px": {
        "file_pattern": "AdvancedSpectrumDisplay.h",
        "search": r'whiter main stroke.*?PathStrokeType\((\d+\.?\d*f?)',
        "validator": lambda m: float(m.group(1).rstrip('f')) >= 2.0,
        "fix": "Curva EQ principale deve essere >= 2.0f (ideale: 2.5f)"
    },
    "EQ curve glow width >= 8.0px": {
        "file_pattern": "AdvancedSpectrumDisplay.h",
        "search": r'diffuse glow.*?PathStrokeType\((\d+\.?\d*f?)',
        "validator": lambda m: float(m.group(1).rstrip('f')) >= 8.0,
        "fix": "Glow della curva EQ deve essere >= 8.0f (ideale: 10.0f)"
    },
    "EQ curve glow alpha >= 0.12": {
        "file_pattern": "AdvancedSpectrumDisplay.h",
        "search": r'diffuse glow.*?eqCurve\.withAlpha\((\d+\.?\d*f?)\)',
        "validator": lambda m: float(m.group(1).rstrip('f')) >= 0.12,
        "fix": "Alpha del glow EQ deve essere >= 0.12f (ideale: 0.15f)"
    },
    "Band node baseRadius (enabled) >= 12.0": {
        "file_pattern": "AdvancedSpectrumDisplay.h",
        "search": r'float baseRadius\s*=\s*state\.enabled\s*\?\s*(\d+\.?\d*f?)',
        "validator": lambda m: float(m.group(1).rstrip('f')) >= 12.0,
        "fix": "baseRadius enabled deve essere >= 12.0f (ideale: 14.0f per 28px diametro)"
    },
    "Band node baseRadius (disabled) >= 11.0": {
        "file_pattern": "AdvancedSpectrumDisplay.h",
        "search": r'float baseRadius\s*=\s*state\.enabled\s*\?\s*\d+\.?\d*f?\s*:\s*(\d+\.?\d*f?)',
        "validator": lambda m: float(m.group(1).rstrip('f')) >= 11.0,
        "fix": "baseRadius disabled deve essere >= 11.0f (ideale: 12.0f per 24px diametro)"
    },
}

# ─── Pattern che NON DEVONO esistere (violazioni) ────────────────────────────

FORBIDDEN_PATTERNS = {
    "accentBlue su buttonOnColourId (toggle attivi)": {
        "file_pattern": "ModernLookAndFeel.h",
        "search": r'setColour\s*\(\s*juce::TextButton::buttonOnColourId\s*,\s*Colors::accentBlue\s*\)',
        "fix": "Sostituire Colors::accentBlue con Colors::amber per buttonOnColourId"
    },
    "accentBlue hardcoded in drawButtonBackground": {
        "file_pattern": "ModernLookAndFeel.h",
        "search": r'getToggleState\(\)\s*\?\s*Colors::accentBlue',
        "fix": "drawButtonBackground deve usare btn.findColour(buttonOnColourId) o Colors::amber"
    },
    "StackBlur in tooltip (Emendamento #1)": {
        "file_pattern": "AdvancedSpectrumDisplay.h",
        "search": r'applyStackBlur|createComponentSnapshot.*tooltip',
        "fix": "VIETATO: StackBlur e createComponentSnapshot nel tooltip (Emendamento #1 vincolante)"
    },
    "Slider::thumbColourId = accentBlue globale": {
        "file_pattern": "ModernLookAndFeel.h",
        "search": r'setColour\s*\(\s*juce::Slider::thumbColourId\s*,\s*Colors::accentBlue\s*\)',
        "fix": "Slider thumbColourId globale deve essere amber, non accentBlue"
    },
}

# ─── Componenti che DEVONO esistere ──────────────────────────────────────────

REQUIRED_COMPONENTS = {
    "AITooltipState.tooltipBounds (per hit-testing)": {
        "file_pattern": "AdvancedSpectrumDisplay.h",
        "search": r'tooltipBounds|tooltipRect.*member|juce::Rectangle.*tooltip(?!W|H)',
        "fix": "La struct AITooltipState deve avere un campo tooltipBounds per il hit-testing del FIX button"
    },
    "Tooltip hit-testing in mouseMove": {
        "file_pattern": "AdvancedSpectrumDisplay.h",
        "search": r'tooltip.*contains.*e\.position|isMouseInTooltip|tooltipBounds.*contains',
        "fix": "mouseMove deve verificare se il mouse è dentro il tooltip prima di nasconderlo"
    },
    "AI Zone labels (RUMBLE, MUDDY, HARSH)": {
        "file_pattern": "AdvancedSpectrumDisplay.h",
        "search": r'RUMBLE|MUDDY|HARSH|drawText.*zone|drawFittedText.*corr\.',
        "fix": "drawAIMarkers deve renderizzare etichette testuali sotto le zone AI"
    },
    "AIProblemPanel FIX button per riga": {
        "file_pattern": "AIProblemPanel.h",
        "search": r'fixBtn|fixButton|applyBtn.*row|FIX.*TextButton|juce::TextButton.*fix',
        "fix": "Ogni riga del ProblemRowComponent deve avere un pulsante FIX individuale"
    },
    "PillComboBox o preset pill": {
        "file_pattern": "*",
        "search": r'PillComboBox|PillButton|pill.*combo|capsule.*combo',
        "fix": "Creare un componente PillComboBox per la navigazione preset (capsula arrotondata)"
    },
    "Linear Phase pill button": {
        "file_pattern": "*",
        "search": r'LINEAR PHASE|linearPhase.*Pill|linearPhase.*Button|LINEAR_PHASE',
        "fix": "Aggiungere il pulsante 'LINEAR PHASE' nell'header come capsula"
    },
    "DynEQ knobs (non solo testo)": {
        "file_pattern": "DynEQCompactBar.h",
        "search": r'PremiumKnob|Slider.*thresh|Slider.*ratio|Slider.*attack|Slider.*release|rotary.*dyn',
        "fix": "DynEQCompactBar deve avere 4 PremiumKnob (THRESH/RATIO/ATTACK/RELEASE) non solo label"
    },
    "Tooltip drop shadow": {
        "file_pattern": "AdvancedSpectrumDisplay.h",
        "search": r'shadow|Shadow|fillRoundedRectangle.*0x[0-9A-Fa-f]*00|dropShadow',
        "fix": "Il tooltip deve avere un drop shadow (2-3 rect neri sfalsati) per profondità"
    },
}

# ─── Colori dello spettro (conformità al design system) ──────────────────────

SPECTRUM_CHECKS = {
    "Pre-EQ fill = azzurro (non arancio/amber)": {
        "file_pattern": "AdvancedSpectrumDisplay.h",
        "search": r'Pre fill.*?Colors::(\w+)',
        "validator": lambda m: m.group(1) not in ("amber", "accentYellow", "accentOrange"),
        "fix": "Il fill del pre-EQ deve essere azzurro (spectrumFill/accentCyan), non amber"
    },
    "Background gradient (non piatto)": {
        "file_pattern": "PluginEditor.cpp",
        "search": r'ColourGradient.*0xFF0A|Liquid Intelligence background|gradient.*top.*bottom',
        "validator": lambda m: True,  # Basta che esista
        "fix": "Il background deve usare un ColourGradient, non fillAll con colore piatto"
    },
}

# ─── Font scale check ────────────────────────────────────────────────────────

MINIMUM_FONT_SIZES = {
    "ModernLookAndFeel.h": 12.0,  # Font base nel LookAndFeel >= 12px
    "PluginEditor.cpp": 10.0,     # Minimo assoluto nell'editor
}


# ═══════════════════════════════════════════════════════════════════════════════
# VALIDATOR ENGINE
# ═══════════════════════════════════════════════════════════════════════════════

@dataclass
class CheckResult:
    name: str
    passed: bool
    category: str
    details: str = ""
    fix: str = ""
    severity: str = "HIGH"  # HIGH, MED, LOW


class LiquidIntelligenceValidator:
    def __init__(self, source_dir: str):
        self.source_dir = Path(source_dir)
        self.results: list[CheckResult] = []
        self.file_cache: dict[str, str] = {}

    def _read_file(self, pattern: str) -> list[tuple[str, str]]:
        """Read all files matching the pattern and return (path, content) pairs."""
        matches = []
        if pattern == "*":
            for f in self.source_dir.rglob("*.h"):
                matches.append(f)
            for f in self.source_dir.rglob("*.cpp"):
                matches.append(f)
        else:
            for f in self.source_dir.rglob(pattern):
                matches.append(f)
        result = []
        for f in matches:
            key = str(f)
            if key not in self.file_cache:
                try:
                    self.file_cache[key] = f.read_text(encoding="utf-8", errors="replace")
                except Exception:
                    self.file_cache[key] = ""
            result.append((str(f), self.file_cache[key]))
        return result

    def run_all(self):
        """Execute all validation checks."""
        print("=" * 72)
        print("  AIEQ LIQUID INTELLIGENCE — DESIGN SYSTEM VALIDATOR v1.0")
        print("  Tribunale AIEQ+ | Validazione Automatica Post-Wave")
        print("=" * 72)
        print(f"\n  Source directory: {self.source_dir}\n")

        self._check_required_constants()
        self._check_forbidden_patterns()
        self._check_required_components()
        self._check_spectrum_colors()
        self._check_font_sizes()
        self._check_accent_blue_contamination()
        self._check_tooltip_dimensions()
        self._check_layout_proportions()

        self._print_report()

    # ── Required Constants ────────────────────────────────────────────────

    def _check_required_constants(self):
        print("  [1/8] Verifica costanti obbligatorie...")
        for name, spec in REQUIRED_CONSTANTS.items():
            files = self._read_file(spec["file_pattern"])
            found = False
            for fpath, content in files:
                m = re.search(spec["search"], content, re.DOTALL)
                if m:
                    if spec["validator"](m):
                        self.results.append(CheckResult(
                            name=name, passed=True, category="CONSTANTS",
                            details=f"Valore: {m.group(1)} in {Path(fpath).name}"
                        ))
                    else:
                        self.results.append(CheckResult(
                            name=name, passed=False, category="CONSTANTS",
                            details=f"Valore non conforme: {m.group(1)} in {Path(fpath).name}",
                            fix=spec["fix"]
                        ))
                    found = True
                    break
            if not found:
                self.results.append(CheckResult(
                    name=name, passed=False, category="CONSTANTS",
                    details="Pattern non trovato nel file",
                    fix=spec["fix"]
                ))

    # ── Forbidden Patterns ────────────────────────────────────────────────

    def _check_forbidden_patterns(self):
        print("  [2/8] Verifica pattern vietati...")
        for name, spec in FORBIDDEN_PATTERNS.items():
            files = self._read_file(spec["file_pattern"])
            violation_found = False
            for fpath, content in files:
                matches = list(re.finditer(spec["search"], content))
                if matches:
                    lines = []
                    for m in matches:
                        line_num = content[:m.start()].count('\n') + 1
                        lines.append(str(line_num))
                    self.results.append(CheckResult(
                        name=name, passed=False, category="FORBIDDEN",
                        details=f"Trovato in {Path(fpath).name} righe: {', '.join(lines)}",
                        fix=spec["fix"],
                        severity="HIGH"
                    ))
                    violation_found = True
                    break
            if not violation_found:
                self.results.append(CheckResult(
                    name=name, passed=True, category="FORBIDDEN",
                    details="Nessuna violazione trovata"
                ))

    # ── Required Components ───────────────────────────────────────────────

    def _check_required_components(self):
        print("  [3/8] Verifica componenti obbligatori...")
        for name, spec in REQUIRED_COMPONENTS.items():
            files = self._read_file(spec["file_pattern"])
            found = False
            for fpath, content in files:
                if re.search(spec["search"], content, re.IGNORECASE):
                    self.results.append(CheckResult(
                        name=name, passed=True, category="COMPONENTS",
                        details=f"Trovato in {Path(fpath).name}"
                    ))
                    found = True
                    break
            if not found:
                self.results.append(CheckResult(
                    name=name, passed=False, category="COMPONENTS",
                    details="Componente non trovato nel codebase",
                    fix=spec["fix"],
                    severity="HIGH"
                ))

    # ── Spectrum Colors ───────────────────────────────────────────────────

    def _check_spectrum_colors(self):
        print("  [4/8] Verifica colori spettro...")
        for name, spec in SPECTRUM_CHECKS.items():
            files = self._read_file(spec["file_pattern"])
            found = False
            for fpath, content in files:
                m = re.search(spec["search"], content, re.DOTALL)
                if m:
                    if spec["validator"](m):
                        self.results.append(CheckResult(
                            name=name, passed=True, category="SPECTRUM",
                            details=f"Conforme in {Path(fpath).name}"
                        ))
                    else:
                        self.results.append(CheckResult(
                            name=name, passed=False, category="SPECTRUM",
                            details=f"Non conforme in {Path(fpath).name}",
                            fix=spec["fix"]
                        ))
                    found = True
                    break
            if not found:
                self.results.append(CheckResult(
                    name=name, passed=False, category="SPECTRUM",
                    details="Pattern non trovato",
                    fix=spec["fix"],
                    severity="MED"
                ))

    # ── Font Sizes ────────────────────────────────────────────────────────

    def _check_font_sizes(self):
        print("  [5/8] Verifica dimensioni font...")
        for filename, min_size in MINIMUM_FONT_SIZES.items():
            files = self._read_file(filename)
            violations = []
            for fpath, content in files:
                for m in re.finditer(r'withHeight\((\d+\.?\d*)f?\)', content):
                    size = float(m.group(1))
                    if size < min_size:
                        line_num = content[:m.start()].count('\n') + 1
                        violations.append(f"riga {line_num}: {size}px")
            if violations:
                self.results.append(CheckResult(
                    name=f"Font minimo {min_size}px in {filename}",
                    passed=False, category="TYPOGRAPHY",
                    details=f"Violazioni: {'; '.join(violations[:5])}",
                    fix=f"Tutti i font in {filename} devono essere >= {min_size}px",
                    severity="MED"
                ))
            else:
                self.results.append(CheckResult(
                    name=f"Font minimo {min_size}px in {filename}",
                    passed=True, category="TYPOGRAPHY",
                    details="Tutti i font conformi"
                ))

    # ── accentBlue Contamination ──────────────────────────────────────────

    def _check_accent_blue_contamination(self):
        print("  [6/8] Verifica contaminazione accentBlue...")
        # accentBlue è LEGITTIMO solo in: definizione colore, Semantic tab,
        # DynamicEQPanel (colore GR meter), spectrum line.
        # È VIETATO per: toggle attivi, pulsanti header, slider thumb globale.
        allowed_contexts = [
            "inline static const",  # definizione
            "getCurrentAccent",     # helper
            "Semantic",             # tab semantica
            "spectrum",             # spettro
            "GR",                   # gain reduction meter
            "DynamicEQ",           # pannello DynEQ (colore meter)
        ]
        files = self._read_file("*")
        suspicious = []
        for fpath, content in files:
            for m in re.finditer(r'.*accentBlue.*', content):
                line = m.group(0).strip()
                line_num = content[:m.start()].count('\n') + 1
                fname = Path(fpath).name
                is_allowed = any(ctx in line or ctx in fname for ctx in allowed_contexts)
                if not is_allowed:
                    suspicious.append(f"{fname}:{line_num}")
        if suspicious:
            self.results.append(CheckResult(
                name="accentBlue in contesti non autorizzati",
                passed=False, category="COLOR_SYSTEM",
                details=f"{len(suspicious)} occorrenze sospette: {'; '.join(suspicious[:8])}",
                fix="Sostituire accentBlue con amber per toggle, pulsanti header, slider thumb",
                severity="HIGH"
            ))
        else:
            self.results.append(CheckResult(
                name="accentBlue in contesti non autorizzati",
                passed=True, category="COLOR_SYSTEM",
                details="Nessuna contaminazione rilevata"
            ))

    # ── Tooltip Dimensions ────────────────────────────────────────────────

    def _check_tooltip_dimensions(self):
        print("  [7/8] Verifica dimensioni tooltip...")
        files = self._read_file("AdvancedSpectrumDisplay.h")
        for fpath, content in files:
            # Check tooltip width
            m_w = re.search(r'tooltipW\s*=\s*(\d+\.?\d*)', content)
            m_h = re.search(r'tooltipH\s*=\s*(\d+\.?\d*)', content)
            if m_w and m_h:
                w = float(m_w.group(1))
                h = float(m_h.group(1))
                passed = w >= 200.0 and h >= 100.0
                self.results.append(CheckResult(
                    name="Tooltip dimensioni >= 200x100",
                    passed=passed, category="TOOLTIP",
                    details=f"Attuale: {w}x{h}px",
                    fix="Tooltip deve essere almeno 200x100px per contenere titolo, descrizione e FIX button" if not passed else "",
                    severity="MED"
                ))
            else:
                self.results.append(CheckResult(
                    name="Tooltip dimensioni >= 200x100",
                    passed=False, category="TOOLTIP",
                    details="Costanti tooltipW/tooltipH non trovate",
                    fix="Definire tooltipW >= 200 e tooltipH >= 100"
                ))

    # ── Layout Proportions ────────────────────────────────────────────────

    def _check_layout_proportions(self):
        print("  [8/8] Verifica proporzioni layout...")
        files = self._read_file("PluginEditor.h")
        for fpath, content in files:
            # controlH
            m = re.search(r'controlH\s*=\s*(\d+)', content)
            if m:
                val = int(m.group(1))
                passed = val >= 220
                self.results.append(CheckResult(
                    name=f"controlH >= 220 (attuale: {val})",
                    passed=passed, category="LAYOUT",
                    details=f"controlH = {val}px",
                    fix="controlH deve essere >= 220px per ospitare band knobs + DynEQ knobs" if not passed else "",
                    severity="HIGH" if not passed else "LOW"
                ))
            # headerH
            m = re.search(r'headerH\s*=\s*(\d+)', content)
            if m:
                val = int(m.group(1))
                passed = val >= 36
                self.results.append(CheckResult(
                    name=f"headerH >= 36 (attuale: {val})",
                    passed=passed, category="LAYOUT",
                    details=f"headerH = {val}px",
                    fix="headerH deve essere >= 36px" if not passed else ""
                ))

    # ── Report ────────────────────────────────────────────────────────────

    def _print_report(self):
        print("\n" + "=" * 72)
        print("  REPORT DI VALIDAZIONE")
        print("=" * 72)

        categories = {}
        for r in self.results:
            categories.setdefault(r.category, []).append(r)

        total_pass = sum(1 for r in self.results if r.passed)
        total_fail = sum(1 for r in self.results if not r.passed)
        total = len(self.results)

        for cat, checks in categories.items():
            print(f"\n  ── {cat} ──")
            for c in checks:
                icon = "PASS" if c.passed else "FAIL"
                print(f"    [{icon}] {c.name}")
                if c.details:
                    print(f"           {c.details}")
                if not c.passed and c.fix:
                    print(f"           FIX: {c.fix}")

        # Summary
        print("\n" + "=" * 72)
        score = (total_pass / total * 100) if total > 0 else 0
        high_fails = sum(1 for r in self.results if not r.passed and r.severity == "HIGH")

        print(f"  RISULTATO: {total_pass}/{total} check superati ({score:.0f}%)")
        print(f"  Fallimenti HIGH: {high_fails}")
        print(f"  Fallimenti MED:  {total_fail - high_fails}")

        if score >= 95 and high_fails == 0:
            verdict = "CONFORME — Pronto per validazione Tribunale"
        elif score >= 75 and high_fails <= 2:
            verdict = "QUASI CONFORME — Fix minori richiesti prima del push"
        elif score >= 50:
            verdict = "NON CONFORME — Correzioni significative necessarie"
        else:
            verdict = "GRAVEMENTE NON CONFORME — Revisione profonda richiesta"

        print(f"\n  VERDETTO: {verdict}")
        print("=" * 72)

        # Write machine-readable summary
        summary_path = Path(self.source_dir).parent / "validation_report.txt"
        with open(summary_path, "w") as f:
            f.write(f"AIEQ Liquid Intelligence Validator — Report\n")
            f.write(f"Score: {score:.0f}% ({total_pass}/{total})\n")
            f.write(f"HIGH failures: {high_fails}\n")
            f.write(f"Verdict: {verdict}\n\n")
            for r in self.results:
                if not r.passed:
                    f.write(f"[FAIL] {r.name}\n")
                    f.write(f"  Details: {r.details}\n")
                    f.write(f"  Fix: {r.fix}\n\n")
        print(f"\n  Report salvato in: {summary_path}")


# ═══════════════════════════════════════════════════════════════════════════════
# MAIN
# ═══════════════════════════════════════════════════════════════════════════════

if __name__ == "__main__":
    source_dir = sys.argv[1] if len(sys.argv) > 1 else "./Source"

    if not os.path.isdir(source_dir):
        print(f"ERRORE: Directory '{source_dir}' non trovata.")
        print(f"Uso: python3 {sys.argv[0]} [path/to/Source]")
        sys.exit(1)

    validator = LiquidIntelligenceValidator(source_dir)
    validator.run_all()
