#!/usr/bin/env bash
# Ember Core History Extraction — run this in /Users/marco/Desktop/NEWWWWWWW
# Output goes to /tmp/ember-history-$(date +%s)/

set -euo pipefail

OUT="/tmp/ember-history-$(date +%s)"
mkdir -p "$OUT"

echo ">>> Extracting to $OUT"

# 1. Tutti i commit cronologici (hash|data|subject)
git log --all --format='%h|%ad|%s' --date=short --reverse > "$OUT/01-all-commits-chrono.txt"
echo "  01-all-commits-chrono.txt: $(wc -l < "$OUT/01-all-commits-chrono.txt") commit"

# 2. Commit con messaggi completi (per contesto milestone)
git log --all --format='%h|%ad|%s%n%b%n---' --date=short --reverse > "$OUT/02-all-commits-full.txt"
echo "  02-all-commits-full.txt: $(wc -l < "$OUT/02-all-commits-full.txt") righe"

# 3. Commit "significativi" per keyword milestone/gate/decisione
git log --all --format='%h|%ad|%s' --date=short --reverse \
  | grep -iE "milestone|gate|GO/NO-GO|NO-GO|freeze|congel|release|ship|PASS|FAIL|contract|kill|verdict|close|chius|verdict" \
  > "$OUT/03-milestone-commits.txt"
echo "  03-milestone-commits.txt: $(wc -l < "$OUT/03-milestone-commits.txt") commit"

# 4. Branch di lunga vita (non merged, o merged da tempo)
git branch -a --format='%(refname:short)|%(committerdate:short)|%(subject)' \
  | sort -t'|' -k2 > "$OUT/04-branches.txt"
echo "  04-branches.txt: $(wc -l < "$OUT/04-branches.txt") branch"

# 5. Worktree attivi
git worktree list --porcelain > "$OUT/05-worktrees.txt"
echo "  05-worktrees.txt: $(grep -c '^worktree ' "$OUT/05-worktrees.txt") worktree"

# 6. File chiave che documentano la storia (PLAN, report, ARCHITECTURE, ecc.)
KEY_FILES=(
  "PLAN.md"
  "docs/PLAN.md"
  "docs/ARCHITECTURE.md"
  "docs/ROADMAP.md"
  "CHANGELOG.md"
  "README.md"
)
for f in "${KEY_FILES[@]}"; do
  if [ -f "$f" ]; then
    cp "$f" "$OUT/06-keyfiles-$(basename "$f")"
    echo "  06-keyfiles-$(basename "$f"): OK ($(wc -l < "$f") righe)"
  else
    echo "  06-keyfiles-$(basename "$f"): NOT FOUND"
  fi
done

# 7. Tutto ciò che è in docs/ e PROMPTS/ (indice)
find docs PROMPTS -type f -name "*.md" 2>/dev/null | head -100 | while read f; do
  echo "$f|$(wc -l < "$f")|$(stat -f%z "$f" 2>/dev/null || stat -c%s "$f" 2>/dev/null)" >> "$OUT/07-docs-index.txt"
done
echo "  07-docs-index.txt: $(wc -l < "$OUT/07-docs-index.txt" 2>/dev/null || echo 0) file"

# 8. Skill/discipline agenti (la linea tribunale/surgeon/protocol)
git log --all --format='%h|%ad|%s' --date=short --reverse \
  | grep -iE "skill|tribunal|surgeon|war.?room|protocol|directive|cheat.?sheet|governor|veridic" \
  > "$OUT/08-agent-discipline-commits.txt"
echo "  08-agent-discipline-commits.txt: $(wc -l < "$OUT/08-agent-discipline-commits.txt") commit"

# 9. File skill attuali se esistono
if [ -d "skills/ACTIVE_SKILLS" ]; then
  find skills/ACTIVE_SKILLS -type f | while read f; do
    echo "$f|$(wc -l < "$f")" >> "$OUT/09-active-skills-index.txt"
  done
  echo "  09-active-skills-index.txt: $(wc -l < "$OUT/09-active-skills-index.txt") file"
fi

# 10. Attività per mese (forma del progetto)
git log --all --format='%ad' --date=format:'%Y-%m' | sort | uniq -c \
  | awk '{printf "%s|%d\n", $2, $1}' > "$OUT/10-monthly-activity.txt"
echo "  10-monthly-activity.txt: $(wc -l < "$OUT/10-monthly-activity.txt") mesi"

# 11. Commit per autore (chi ha fatto cosa)
git log --all --format='%an|%h|%ad|%s' --date=short --reverse > "$OUT/11-commits-by-author.txt"
echo "  11-commits-by-author.txt: $(wc -l < "$OUT/11-commits-by-author.txt") commit"

# 12. Diffstat degli ultimi 50 commit (cosa è cambiato di recente)
git log --all --format='%h' --reverse | tail -50 | while read h; do
  git show --stat --format='%h|%ad|%s' --date=short "$h" | head -30 >> "$OUT/12-recent-diffstat.txt"
done
echo "  12-recent-diffstat.txt: done"

# 13. File più toccati (hotspots)
git log --all --pretty=format: --name-only | grep -v '^$' | sort | uniq -c | sort -rn | head -50 > "$OUT/13-hotspot-files.txt"
echo "  13-hotspot-files.txt: top 50 file"

# 14. Zip tutto per consegna facile
cd /tmp
ZIP="ember-history-$(date +%s).zip"
zip -r "$ZIP" "$(basename "$OUT")" >/dev/null
echo
echo ">>> DONE. Zip pronto: /tmp/$ZIP"
echo ">>> Mandami quel file (o incollami i .txt che vuoi)"