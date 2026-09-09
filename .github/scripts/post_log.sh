#!/bin/sh
# Posts the tail of a log file as a commit comment, so a failing CI job can be
# diagnosed from anywhere that can reach api.github.com.
#
#   post_log.sh <logfile> <label>

LOG="$1"
LABEL="$2"

[ -f "$LOG" ] || { echo "no log file $LOG"; exit 0; }

TMP="$(mktemp)"
python3 - "$LOG" "$LABEL" > "$TMP" <<'PYEOF'
import json
import sys

log_path, label = sys.argv[1], sys.argv[2]
with open(log_path, errors="replace") as f:
    tail = f.read().splitlines()[-120:]
body = "**%s** failed, gradle log tail:\n\n```\n%s\n```\n" % (label, "\n".join(tail))
sys.stdout.write(json.dumps({"body": body}))
PYEOF

CODE=$(curl -sS -o /tmp/comment_resp.json -w "%{http_code}" -X POST \
    "https://api.github.com/repos/$GITHUB_REPOSITORY/commits/$GITHUB_SHA/comments" \
    -H "Authorization: Bearer $GITHUB_TOKEN" \
    -H "Accept: application/vnd.github+json" \
    -d @"$TMP" 2>/dev/null || echo 000)

echo "commit comment HTTP $CODE"
cat /tmp/comment_resp.json 2>/dev/null || true
rm -f "$TMP"

# Fallback that needs no token permissions: workflow-command annotations are
# visible through the checks API.
tail -n 30 "$LOG" | while IFS= read -r line; do
    esc=$(printf '%s' "$line" | sed -e 's/%/%25/g' -e 's/\r/%0D/g')
    echo "::error title=gradle log ($LABEL)::$esc"
done
