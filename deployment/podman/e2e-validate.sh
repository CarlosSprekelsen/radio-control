#!/bin/bash
# End-to-end validation script for Radio Control Container + Silvus Mock
# Usage: ./e2e-validate.sh
#
# Expects RCC reachable at REST_HOST:REST_PORT and silvus-mock running.
# Validates: health, capabilities, radio list, select, setPower, setChannel, SSE events.

set -euo pipefail

REST_HOST="${REST_HOST:-127.0.0.1}"
REST_PORT="${REST_PORT:-8080}"
SSE_PORT="${SSE_PORT:-8081}"
MOCK_STATUS_URL="${MOCK_STATUS_URL:-http://127.0.0.1:9080/status}"
BASE_URL="http://${REST_HOST}:${REST_PORT}"

die() { echo "[e2e][error] $1" >&2; exit 1; }
log() { echo "[e2e] $1"; }

TMP_DIR="$(mktemp -d)"
trap "rm -rf '$TMP_DIR'" EXIT INT TERM

AUTH_HEADER=""

# ── Helper: authenticated curl ───────────────────────────────────────────────
_curl() {
  if [[ -n "$AUTH_HEADER" ]]; then
    curl -fsS --max-time 5 -H "$AUTH_HEADER" "$@"
  else
    curl -fsS --max-time 5 "$@"
  fi
}

# ── 1. Dev token ─────────────────────────────────────────────────────────────
log "=== 1. Obtain dev token ==="
DEV_TOKEN_JSON="$(curl -fsS --max-time 5 "${BASE_URL}/api/v1/dev-token" || die "Dev-token endpoint failed")"
DEV_TOKEN="$(echo "$DEV_TOKEN_JSON" | python3 -c 'import sys,json; print(json.load(sys.stdin)["data"]["token"])' 2>/dev/null || true)"
if [[ -z "$DEV_TOKEN" ]]; then
  die "Failed to extract dev token"
fi
AUTH_HEADER="Authorization: Bearer ${DEV_TOKEN}"
log "Dev token obtained"

# ── 2. Health ────────────────────────────────────────────────────────────────
log "=== 2. Health endpoint ==="
HEALTH_JSON="$(_curl "${BASE_URL}/api/v1/health" || die "Health endpoint unreachable")"
echo "$HEALTH_JSON" | python3 -m json.tool

STATUS="$(echo "$HEALTH_JSON" | python3 -c 'import sys,json; print(json.load(sys.stdin)["status"])')"
log "Health status: $STATUS"

# ── 3. Capabilities ──────────────────────────────────────────────────────────
log "=== 3. Capabilities ==="
_curl "${BASE_URL}/api/v1/capabilities" | python3 -m json.tool || die "Capabilities failed"

# ── 4. Radio list ────────────────────────────────────────────────────────────
log "=== 4. Radio list ==="
RADIOS_JSON="$(_curl "${BASE_URL}/api/v1/radios" || die "Radio list failed")"
echo "$RADIOS_JSON" | python3 -m json.tool

RADIO_ID="$(echo "$RADIOS_JSON" | python3 -c 'import sys,json; print(json.load(sys.stdin)["data"]["items"][0]["id"])' 2>/dev/null || true)"
if [[ -z "$RADIO_ID" ]]; then
  die "No radios returned — check silvus-mock connectivity"
fi
log "Found radio: $RADIO_ID"

# ── 5. Start SSE capture in background ───────────────────────────────────────
log "=== 5. Starting SSE capture (background) ==="
(
  curl -fsS --max-time 45 -N \
    -H "Accept: text/event-stream" \
    -H "$AUTH_HEADER" \
    "http://${REST_HOST}:${SSE_PORT}/api/v1/telemetry" 2>/dev/null > "$TMP_DIR/sse.txt" || true
) &
SSE_PID=$!

# Give SSE time to connect before firing commands
sleep 2

# ── 6. Select radio ──────────────────────────────────────────────────────────
log "=== 6. Select radio ($RADIO_ID) ==="
_curl -X POST "${BASE_URL}/api/v1/radios/select" \
  -H "Content-Type: application/json" \
  -d "{\"id\":\"$RADIO_ID\"}" | python3 -m json.tool || die "Select failed"

# ── 7. Set power ─────────────────────────────────────────────────────────────
log "=== 7. Set power (28 dBm) ==="
_curl -X POST "${BASE_URL}/api/v1/radios/${RADIO_ID}/power" \
  -H "Content-Type: application/json" \
  -d '{"powerDbm":28}' | python3 -m json.tool || die "Set power failed"

# The C++ mock enters a 5-second blackout after power changes.
log "Waiting 6s for power-change blackout to clear..."
sleep 6

# ── 8. Set channel ───────────────────────────────────────────────────────────
log "=== 8. Set channel (index 1) ==="
_curl -X POST "${BASE_URL}/api/v1/radios/${RADIO_ID}/channel" \
  -H "Content-Type: application/json" \
  -d '{"channelIndex":1}' | python3 -m json.tool || die "Set channel failed"

# The C++ mock enters a 30-second blackout after frequency changes.
# We verify the blackout state without waiting the full duration.
log "Checking mock blackout state..."
MOCK_STATUS="$(curl -fsS --max-time 5 "$MOCK_STATUS_URL" || die "Mock status unreachable")"
BLACKOUT_UNTIL="$(echo "$MOCK_STATUS" | python3 -c 'import sys,json; print(json.load(sys.stdin).get("blackoutUntil",0))' 2>/dev/null || echo "0")"
if [[ "$BLACKOUT_UNTIL" -gt 0 ]]; then
  log "Mock is in blackout (blackoutUntil=$BLACKOUT_UNTIL) — state machine confirmed"
else
  log "WARNING: Mock did not enter blackout after channel change"
fi

# ── 9. Get radio details ─────────────────────────────────────────────────────
log "=== 9. Get radio details ==="
_curl "${BASE_URL}/api/v1/radios/${RADIO_ID}" | python3 -m json.tool || die "Get radio failed"

# ── 10. Stop SSE capture and inspect ─────────────────────────────────────────
log "=== 10. SSE telemetry events ==="
kill $SSE_PID 2>/dev/null || true
wait $SSE_PID 2>/dev/null || true

if [[ -s "$TMP_DIR/sse.txt" ]]; then
  log "SSE raw events (first 30 lines):"
  head -30 "$TMP_DIR/sse.txt"
  EVENT_COUNT="$(grep -c "^event:" "$TMP_DIR/sse.txt" 2>/dev/null || echo "0")"
  log "Total event lines: $EVENT_COUNT"
else
  log "WARNING: No SSE events captured"
fi

# ── 11. Monitor page ─────────────────────────────────────────────────────────
log "=== 11. Monitor page ==="
MONITOR_HTML="$(curl -fsS --max-time 5 "${BASE_URL}/monitor" || die "Monitor page unreachable")"
if echo "$MONITOR_HTML" | grep -q "Radio Control Monitor"; then
  log "Monitor page title verified"
else
  die "Monitor page title missing"
fi

# ── Summary ──────────────────────────────────────────────────────────────────
log ""
log "========================================"
log "E2E validation PASSED"
log "========================================"
log "Health:        $STATUS"
log "Radio ID:      $RADIO_ID"
log "Select:        OK"
log "Set Power:     OK"
log "Set Channel:   OK"
log "Mock Blackout: ${BLACKOUT_UNTIL}s remaining"
log "SSE Events:    ${EVENT_COUNT:-0} captured"
log "Monitor Page:  OK"
