#include <Arduino.h>
#include <WebServer.h>
#include <uri/UriBraces.h>
#include <ArduinoJson.h>

#include "api_server.h"
#include "status_engine.h"
#include "condition_registry.h"
#include "heartbeat_monitor.h"
#include "network.h"
#include "config.h"
#include "secrets.h"

namespace {
    WebServer server(80);
    LightController* lights = nullptr;
    TalariaStatus currentStatus = TalariaStatus::OFFLINE;
    ConditionRegistry conditions(MAX_ACTIVE_CONDITIONS);
    HeartbeatMonitor heartbeat(HEARTBEAT_TIMEOUT_MS);

    // The last state actually pushed to LightController, so refreshDisplay()
    // can skip re-applying it when nothing changed. LightController::setState()
    // resets the flash timer on every call, so calling it unconditionally
    // from a periodic tick (needed for heartbeat staleness) would freeze
    // every flashing pattern mid-flash.
    SignalState lastAppliedState { SignalColor::NONE, SignalPattern::OFF, SignalPriority::NORMAL };
    bool hasAppliedState = false;

    // What the light should actually show: the higher-priority of the base
    // Talaria status (open/closed/warning/critical/offline) and whatever
    // structured condition (POST /api/v1/signals) currently ranks highest.
    // Clearing the top condition falls back to the next one, or the base
    // status if none are left -- Talaria never has to re-send the others.
    // If Talaria has gone quiet past HEARTBEAT_TIMEOUT_MS, the base status
    // is forced to OFFLINE (an active condition can still override that,
    // same as it would override a real OFFLINE).
    SignalState resolveDisplayState() {
        // Drop anything that's outlived its ttlSeconds before deciding
        // what to show, so an expired condition can never win the pick.
        conditions.pruneExpired(millis());

        TalariaStatus effectiveStatus = heartbeat.isStale(millis()) ? TalariaStatus::OFFLINE : currentStatus;
        SignalState base = signalForStatus(effectiveStatus);
        const Condition* top = conditions.highestPriority();

        if (top != nullptr) {
            SignalState overlay = signalForSeverity(top->severity);
            if (static_cast<int>(overlay.priority) >= static_cast<int>(base.priority)) {
                return overlay;
            }
        }

        return base;
    }

    void refreshDisplay() {
        // Never fight a mode that already owns the lights (boot animation,
        // self-test, all-on hold, or a forced-off) -- this used to only be
        // checked by apiServerTick()'s periodic call, but any caller
        // (handleHeartbeat() in particular) could still blow one of these
        // away by calling refreshDisplay() directly. Checking it here
        // once, for every caller, closes that off for good.
        if (lights->bootSequenceActive() || lights->selfTestActive() || lights->allOnTestActive() ||
            lights->forcedOffActive()) {
            return;
        }

        SignalState next = resolveDisplayState();

        if (hasAppliedState &&
            next.color == lastAppliedState.color &&
            next.pattern == lastAppliedState.pattern &&
            next.priority == lastAppliedState.priority) {
            return;
        }

        lights->setState(next);
        lastAppliedState = next;
        hasAppliedState = true;
    }

    void applyStatus(TalariaStatus status) {
        currentStatus = status;
        heartbeat.markContact(millis());
        refreshDisplay();
    }

    void sendJson(int code, JsonDocument& doc) {
        String body;
        serializeJson(doc, body);

        // Permissive CORS so the dashboard (or a laptop browser during
        // bench testing) can be served from anywhere and still call the API.
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(code, "application/json", body);
    }

    bool isAuthorized() {
        return server.hasHeader("X-Api-Key") && server.header("X-Api-Key") == API_KEY;
    }

    // Wraps a state-changing handler so it requires a valid X-Api-Key
    // header. Read-only GET routes are deliberately left unwrapped -- the
    // whole point of a status light is that its state is easy to see.
    WebServer::THandlerFunction requireAuth(WebServer::THandlerFunction handler) {
        return [handler]() {
            if (!isAuthorized()) {
                JsonDocument err;
                err["error"] = "unauthorized";
                err["hint"] = "send the shared secret as the X-Api-Key header";
                sendJson(401, err);
                return;
            }

            handler();
        };
    }

    void fillStatusDoc(JsonDocument& doc) {
        SignalState state = lights->currentState();

        doc["status"] = toString(currentStatus);
        doc["color"] = toString(state.color);
        doc["pattern"] = toString(state.pattern);
        doc["priority"] = static_cast<int>(state.priority);
        doc["selfTest"] = lights->selfTestActive();
        doc["allOnTest"] = lights->allOnTestActive();
        doc["forcedOff"] = lights->forcedOffActive();
        doc["booting"] = lights->bootSequenceActive();
        doc["activeSignals"] = conditions.count();
        doc["stale"] = heartbeat.isStale(millis());
        doc["msSinceContact"] = heartbeat.msSinceContact(millis());
    }

    void handleHealth() {
        JsonDocument doc;
        doc["status"] = "ok";
        doc["device"] = DEVICE_NAME;
        doc["uptimeMs"] = millis();
        doc["ethernet"] = ethernetReady();
        doc["freeHeapBytes"] = ESP.getFreeHeap();
        doc["lowHeapRebootThresholdBytes"] = LOW_HEAP_REBOOT_THRESHOLD_BYTES;
        sendJson(200, doc);
    }

    void handleGetStatus() {
        JsonDocument doc;
        fillStatusDoc(doc);
        sendJson(200, doc);
    }

    void handleSetStatus(TalariaStatus status) {
        applyStatus(status);

        JsonDocument doc;
        fillStatusDoc(doc);
        sendJson(200, doc);
    }

    void handleSelfTest() {
        lights->startSelfTest();
        // Self-test restores state via its own internal setState() call
        // when it finishes, bypassing refreshDisplay() -- invalidate the
        // cache so the next tick reconciles for real instead of trusting
        // a comparison against what it thinks is still showing.
        hasAppliedState = false;
        // Counts as contact: someone actively driving the device from the
        // dashboard/API is clearly not an abandoned/unreachable Talaria --
        // without this, a test run that happens to land >30s after the
        // last real command restores into a stale-triggered OFFLINE
        // flash instead of the status that was actually active.
        heartbeat.markContact(millis());

        JsonDocument doc;
        doc["result"] = "self-test started";
        doc["note"] = "full diagnostic: all-on, each color solid, each color flashing alone, "
                       "then multi-channel combinations";
        doc["durationMs"] = lights->selfTestTotalMs();
        sendJson(200, doc);
    }

    void handleAllOnTestStart() {
        lights->startAllOnTest();
        // These bypass refreshDisplay(), so its dedup cache doesn't know
        // about them -- invalidate it so the next tick always reconciles
        // against reality instead of possibly comparing against a stale
        // idea of what's currently showing.
        hasAppliedState = false;
        heartbeat.markContact(millis());

        JsonDocument doc;
        doc["result"] = "all lights on";
        doc["note"] = "call POST /test/all-off to turn them back off";
        sendJson(200, doc);
    }

    void handleAllOnTestStop() {
        lights->stopAllOnTest();
        hasAppliedState = false;
        heartbeat.markContact(millis());

        JsonDocument doc;
        doc["result"] = "all lights off";
        sendJson(200, doc);
    }

    void handleGetSignals() {
        const unsigned long now = millis();
        conditions.pruneExpired(now);

        JsonDocument doc;
        doc["count"] = conditions.count();
        doc["maxEntries"] = conditions.maxEntries();

        JsonArray arr = doc["signals"].to<JsonArray>();
        for (const Condition& c : conditions.all()) {
            JsonObject item = arr.add<JsonObject>();
            item["source"] = c.source.c_str();
            item["condition"] = c.condition.c_str();
            item["severity"] = toString(c.severity);
            item["message"] = c.message.c_str();
            item["ttlSeconds"] = c.ttlSeconds;

            if (c.ttlSeconds > 0) {
                const unsigned long ageMs = now - c.updatedAtMs;
                const unsigned long ttlMs = c.ttlSeconds * 1000UL;
                item["expiresInMs"] = ageMs >= ttlMs ? 0UL : (ttlMs - ageMs);
            } else {
                item["expiresInMs"] = -1; // never expires
            }
        }

        sendJson(200, doc);
    }

    void handlePostSignal() {
        if (!server.hasArg("plain")) {
            JsonDocument err;
            err["error"] = "missing JSON body";
            sendJson(400, err);
            return;
        }

        JsonDocument doc;
        DeserializationError parseError = deserializeJson(doc, server.arg("plain"));
        if (parseError) {
            JsonDocument err;
            err["error"] = "invalid JSON body";
            err["detail"] = parseError.c_str();
            sendJson(400, err);
            return;
        }

        const std::string source(doc["source"] | "");
        const std::string conditionName(doc["condition"] | "");
        const std::string severityText(doc["severity"] | "");
        const std::string message(doc["message"] | "");
        const unsigned long ttlSeconds = doc["ttlSeconds"] | 0UL;

        if (source.empty() || conditionName.empty() || severityText.empty()) {
            JsonDocument err;
            err["error"] = "source, condition, and severity are required";
            sendJson(400, err);
            return;
        }

        SignalSeverity severity;
        if (!severityFromString(severityText, severity)) {
            JsonDocument err;
            err["error"] = "unknown severity";
            err["severity"] = severityText;
            JsonArray allowed = err["allowed"].to<JsonArray>();
            allowed.add("info");
            allowed.add("warning");
            allowed.add("critical");
            allowed.add("emergency");
            sendJson(400, err);
            return;
        }

        if (ttlSeconds > MAX_TTL_SECONDS) {
            JsonDocument err;
            err["error"] = "ttlSeconds too large";
            err["ttlSeconds"] = ttlSeconds;
            err["maxTtlSeconds"] = MAX_TTL_SECONDS;
            sendJson(400, err);
            return;
        }

        Condition condition;
        condition.source = source;
        condition.condition = conditionName;
        condition.severity = severity;
        condition.message = message;
        condition.ttlSeconds = ttlSeconds;

        conditions.pruneExpired(millis());
        if (!conditions.upsert(condition, millis())) {
            JsonDocument err;
            err["error"] = "too many active conditions";
            err["maxEntries"] = conditions.maxEntries();
            err["hint"] = "clear or wait for an existing condition to expire before adding a new one";
            sendJson(507, err);
            return;
        }

        heartbeat.markContact(millis());
        refreshDisplay();

        JsonDocument res;
        res["result"] = "ok";
        res["source"] = condition.source.c_str();
        res["condition"] = condition.condition.c_str();
        res["severity"] = toString(condition.severity);
        fillStatusDoc(res);
        sendJson(200, res);
    }

    void handleDeleteSignal() {
        const std::string source(server.pathArg(0).c_str());
        const std::string conditionName(server.pathArg(1).c_str());

        if (!conditions.remove(source, conditionName)) {
            JsonDocument err;
            err["error"] = "not found";
            err["source"] = source.c_str();
            err["condition"] = conditionName.c_str();
            sendJson(404, err);
            return;
        }

        heartbeat.markContact(millis());
        refreshDisplay();

        JsonDocument res;
        res["result"] = "removed";
        res["source"] = source.c_str();
        res["condition"] = conditionName.c_str();
        fillStatusDoc(res);
        sendJson(200, res);
    }

    void handleHeartbeat() {
        heartbeat.markContact(millis());
        refreshDisplay();

        JsonDocument doc;
        doc["result"] = "ok";
        fillStatusDoc(doc);
        sendJson(200, doc);
    }

    // One-button "get back to a known, neutral state" -- clears every
    // active /api/v1/signals condition, resets the base status to
    // OFFLINE, and force-holds all lights off (same persistent off as
    // /test/all-off), so nothing lingers to reassert itself later.
    void handleReset() {
        conditions.clear();
        currentStatus = TalariaStatus::OFFLINE;
        lights->stopAllOnTest();
        hasAppliedState = false;
        heartbeat.markContact(millis());

        JsonDocument doc;
        doc["result"] = "reset";
        doc["note"] = "all signals cleared, status reset to offline, lights forced off";
        fillStatusDoc(doc);
        sendJson(200, doc);
    }

    void handleNotFound() {
        JsonDocument doc;
        doc["error"] = "not found";
        doc["path"] = server.uri();
        sendJson(404, doc);
    }

    // Minimal built-in control panel: lets you exercise the API and bench-test
    // the physical lights from a browser, with no other tooling required.
    const char DASHBOARD_HTML[] = R"HTML(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Talaria Stack Light</title>
<style>
  body { font-family: system-ui, sans-serif; max-width: 420px; margin: 2rem auto; padding: 0 1rem; color: #1a1a1a; }
  h1 { font-size: 1.25rem; margin-bottom: 0; }
  .sub { color: #666; margin-top: 0.25rem; margin-bottom: 1.5rem; font-size: 0.9rem; }
  .panel { border: 1px solid #ddd; border-radius: 8px; padding: 1rem; margin-bottom: 1rem; }
  .row { display: flex; align-items: center; gap: 1.1rem; }
  .stack { display: flex; flex-direction: column; gap: 0.5rem; background: #222; padding: 0.6rem; border-radius: 10px; flex-shrink: 0; }
  .lamp {
    width: 36px; height: 36px; border-radius: 50%;
    border: 2px solid rgba(0,0,0,0.35);
    filter: brightness(0.32) saturate(0.5);
    box-shadow: none;
    transition: filter 0.1s linear;
  }
  .lamp.red    { background: #ff3b30; color: #ff3b30; }
  .lamp.yellow { background: #ffcc00; color: #ffcc00; }
  .lamp.green  { background: #34c759; color: #34c759; }
  .lamp.on { filter: brightness(1.15) saturate(1.3); box-shadow: 0 0 14px 3px currentColor; }
  /* Duration is a full on+off cycle -- twice the firmware's toggle
     interval (SLOW_FLASH_MS/FAST_FLASH_MS), which is time-between-toggles,
     not a full period. */
  .lamp.flash-slow { animation: lampBlink 2s step-start infinite; }
  .lamp.flash-fast { animation: lampBlink 0.5s step-start infinite; }
  .lamp.chase { animation: lampBlink 1.4s step-start infinite; }
  @keyframes lampBlink {
    0%, 49%   { filter: brightness(1.15) saturate(1.3); box-shadow: 0 0 14px 3px currentColor; }
    50%, 100% { filter: brightness(0.32) saturate(0.5); box-shadow: none; }
  }
  .fields { font-size: 0.9rem; color: #444; line-height: 1.5; }
  .fields b { color: #111; }
  button { display: block; width: 100%; padding: 0.6rem; margin-bottom: 0.5rem; border: 1px solid #ccc; border-radius: 6px; background: #fafafa; font-size: 0.95rem; cursor: pointer; }
  button:hover { background: #f0f0f0; }
  button.primary { background: #1a1a1a; color: white; border-color: #1a1a1a; }
  button.primary:hover { background: #333; }
  button.danger { background: #d32f2f; color: white; border-color: #d32f2f; font-weight: 600; }
  button.danger:hover { background: #b71c1c; }
  .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 0.5rem; }
  #msg { font-size: 0.85rem; color: #666; min-height: 1.2em; }
  .field { display: block; width: 100%; margin-bottom: 0.4rem; padding: 0.5rem; border: 1px solid #ccc; border-radius: 6px; font-size: 0.9rem; box-sizing: border-box; }
  .small-btn { width: auto; display: inline-block; margin: 0; padding: 0.2rem 0.6rem; font-size: 0.8rem; }
  .signal-row { display: flex; justify-content: space-between; align-items: center; padding: 0.3rem 0; border-bottom: 1px solid #eee; font-size: 0.85rem; }
  .panel-label { font-size: 0.85rem; color: #666; margin-bottom: 0.5rem; }
</style>
</head>
<body>
  <h1>Talaria Stack Light</h1>
  <div class="sub" id="device">talaria-stack-01</div>

  <div class="panel">
    <div class="row">
      <div class="stack">
        <div class="lamp red" id="lamp-red"></div>
        <div class="lamp yellow" id="lamp-yellow"></div>
        <div class="lamp green" id="lamp-green"></div>
      </div>
      <div class="fields">
        <div>status: <b id="f-status">-</b></div>
        <div>color: <b id="f-color">-</b> &middot; pattern: <b id="f-pattern">-</b></div>
        <div>priority: <b id="f-priority">-</b> &middot; self-test: <b id="f-selftest">-</b></div>
        <div>all-on test: <b id="f-allontest">-</b> &middot; forced off: <b id="f-forcedoff">-</b></div>
        <div>booting: <b id="f-booting">-</b></div>
        <div>talaria contact: <b id="f-stale">-</b></div>
      </div>
    </div>
  </div>

  <div class="panel">
    <div class="panel-label">API key -- required for every button below;
      stored only in this browser (localStorage), never sent anywhere but
      this device</div>
    <input id="api-key" class="field" type="password" placeholder="X-Api-Key">
  </div>

  <div class="panel">
    <button class="danger" onclick="resetAll()">Reset / Kill All</button>
    <div class="panel-label" style="margin-top:0.4rem; margin-bottom:0;">
      Clears every active signal, resets status to offline, and force-holds
      all lights off. The one button to get back to a known, blank state.
    </div>
    <div id="reset-msg"></div>
  </div>

  <div class="panel">
    <div class="grid">
      <button onclick="setStatus('open')">Open (green)</button>
      <button onclick="setStatus('closed')">Closed (red)</button>
      <button onclick="setStatus('warning')">Warning (yellow)</button>
      <button onclick="setStatus('critical')">Critical (red flash)</button>
      <button onclick="setStatus('offline')">Offline (test)</button>
      <button class="primary" onclick="runSelfTest()">Run Light Self-Test</button>
    </div>
    <div id="msg"></div>
  </div>

  <div class="panel">
    <div class="panel-label">Heartbeat -- if nothing below is sent within
      the configured timeout, the display forces itself to offline</div>
    <button onclick="sendHeartbeat()">Send Heartbeat</button>
    <div id="hb-msg"></div>
  </div>

  <div class="panel">
    <div class="grid">
      <button onclick="allOnTest()">Hold All Lights On</button>
      <button onclick="allOffTest()">All Lights Off</button>
    </div>
  </div>

  <div class="panel">
    <div class="panel-label">Post a test condition (POST /api/v1/signals)</div>
    <input id="sig-source" class="field" placeholder="source (e.g. printer-front-01)">
    <input id="sig-condition" class="field" placeholder="condition (e.g. PAPER_JAM)">
    <select id="sig-severity" class="field">
      <option value="info">info</option>
      <option value="warning" selected>warning</option>
      <option value="critical">critical</option>
      <option value="emergency">emergency</option>
    </select>
    <input id="sig-message" class="field" placeholder="message (optional)">
    <button class="primary" onclick="postSignal()">Post Signal</button>
    <div id="sig-msg"></div>
    <div id="sig-list"></div>
  </div>

  <script>
    const API_KEY_STORAGE_KEY = 'talaria-api-key';

    function getApiKey() {
      try {
        return localStorage.getItem(API_KEY_STORAGE_KEY) || '';
      } catch (e) {
        return '';
      }
    }

    function authHeaders(extra) {
      return Object.assign({ 'X-Api-Key': getApiKey() }, extra || {});
    }

    // Runs a mutating fetch and reports a clear message on failure --
    // 401 in particular usually just means the API key field is empty or
    // wrong, not that anything is actually broken.
    async function sendAction(msgEl, path, options, busyText, doneText) {
      msgEl.textContent = busyText;
      try {
        const res = await fetch(path, options);
        if (!res.ok) {
          let detail = res.status;
          try { detail = (await res.json()).error || detail; } catch (e) {}
          msgEl.textContent = res.status === 401
            ? 'Unauthorized -- check the API key above.'
            : ('Error: ' + detail);
          return false;
        }
        msgEl.textContent = doneText || '';
        return true;
      } catch (e) {
        msgEl.textContent = 'Could not reach device: ' + e;
        return false;
      }
    }

    async function refresh() {
      try {
        const res = await fetch('/status');
        const s = await res.json();
        document.getElementById('f-status').textContent = s.status;
        document.getElementById('f-color').textContent = s.color;
        document.getElementById('f-pattern').textContent = s.pattern;
        document.getElementById('f-priority').textContent = s.priority;
        document.getElementById('f-selftest').textContent = s.selfTest ? 'running' : 'idle';
        document.getElementById('f-allontest').textContent = s.allOnTest ? 'holding on' : 'idle';
        document.getElementById('f-forcedoff').textContent = s.forcedOff ? 'yes (holding off)' : 'no';
        document.getElementById('f-booting').textContent = s.booting ? 'yes' : 'no';
        document.getElementById('f-stale').textContent = s.stale
          ? 'STALE (forcing offline)'
          : (s.msSinceContact < 0 ? 'never contacted' : Math.round(s.msSinceContact / 1000) + 's ago');

        updateStack(s);
      } catch (e) {
        document.getElementById('msg').textContent = 'Could not reach device: ' + e;
      }
    }

    // Drives the red/yellow/green lamp stack: dim = off, bright + glow =
    // on, with a hard on/off blink (no fade) matching the relay's actual
    // step-function timing for flashing patterns.
    function updateStack(s) {
      const lamps = {
        red: document.getElementById('lamp-red'),
        yellow: document.getElementById('lamp-yellow'),
        green: document.getElementById('lamp-green')
      };

      Object.values(lamps).forEach((el) => {
        el.classList.remove('on', 'flash-slow', 'flash-fast', 'chase');
        el.style.animationDelay = '';
      });

      if (s.booting || s.selfTest) {
        // No per-step detail over the API for these -- approximate with a
        // staggered chase across all three rather than guessing exactly.
        let i = 0;
        for (const color of ['red', 'yellow', 'green']) {
          lamps[color].classList.add('chase');
          lamps[color].style.animationDelay = (i * 0.467) + 's'; // 1.4s / 3
          i++;
        }
        return;
      }

      if (s.allOnTest) {
        Object.values(lamps).forEach((el) => el.classList.add('on'));
        return;
      }

      if (s.pattern === 'alternating') {
        lamps.red.classList.add('flash-fast');
        lamps.yellow.classList.add('flash-fast');
        lamps.yellow.style.animationDelay = '0.25s';
        return;
      }

      const active = lamps[s.color];
      if (!active || s.pattern === 'off') {
        return;
      }

      if (s.pattern === 'solid') {
        active.classList.add('on');
      } else if (s.pattern === 'slow_flash') {
        active.classList.add('flash-slow');
      } else if (s.pattern === 'fast_flash') {
        active.classList.add('flash-fast');
      }
    }

    async function setStatus(name) {
      const msg = document.getElementById('msg');
      await sendAction(msg, '/status/' + name, { method: 'POST', headers: authHeaders() }, 'Setting ' + name + '...');
      refresh();
    }

    async function runSelfTest() {
      const msg = document.getElementById('msg');
      msg.textContent = 'Running self-test...';
      try {
        const res = await fetch('/test/cycle', { method: 'POST', headers: authHeaders() });
        const data = await res.json();
        if (!res.ok) {
          msg.textContent = res.status === 401 ? 'Unauthorized -- check the API key above.' : ('Error: ' + data.error);
          return;
        }
        const seconds = Math.round((data.durationMs || 0) / 1000);
        msg.textContent = 'Self-test running (~' + seconds + 's) -- full diagnostic, watch the lights.';
        setTimeout(() => { msg.textContent = ''; refresh(); }, data.durationMs || 2000);
      } catch (e) {
        msg.textContent = 'Could not reach device: ' + e;
      }
    }

    async function allOnTest() {
      const msg = document.getElementById('msg');
      await sendAction(msg, '/test/all-on', { method: 'POST', headers: authHeaders() }, 'Turning all lights on...', '');
      refresh();
    }

    async function allOffTest() {
      const msg = document.getElementById('msg');
      await sendAction(msg, '/test/all-off', { method: 'POST', headers: authHeaders() }, 'Turning all lights off...', '');
      refresh();
    }

    async function sendHeartbeat() {
      const hbMsg = document.getElementById('hb-msg');
      const ok = await sendAction(hbMsg, '/heartbeat', { method: 'POST', headers: authHeaders() }, 'Sending...', 'Heartbeat sent.');
      refresh();
      if (ok) {
        setTimeout(() => { hbMsg.textContent = ''; }, 1500);
      }
    }

    async function resetAll() {
      const msg = document.getElementById('reset-msg');
      const ok = await sendAction(msg, '/reset', { method: 'POST', headers: authHeaders() }, 'Resetting...', 'All clear.');
      refresh();
      refreshSignals();
      if (ok) {
        setTimeout(() => { msg.textContent = ''; }, 1500);
      }
    }

    // Keeps the device from thinking Talaria has gone silent just because
    // this dashboard is open and idle -- if someone's actively looking at
    // it, that's reasonable proof it isn't abandoned.
    function backgroundHeartbeat() {
      if (!getApiKey()) return;
      fetch('/heartbeat', { method: 'POST', headers: authHeaders() }).catch(() => {});
    }

    async function postSignal() {
      const body = {
        source: document.getElementById('sig-source').value,
        condition: document.getElementById('sig-condition').value,
        severity: document.getElementById('sig-severity').value,
        message: document.getElementById('sig-message').value
      };

      const msg = document.getElementById('sig-msg');
      await sendAction(msg, '/api/v1/signals', {
        method: 'POST',
        headers: authHeaders({ 'Content-Type': 'application/json' }),
        body: JSON.stringify(body)
      }, 'Sending...', 'Posted.');

      refresh();
      refreshSignals();
    }

    async function deleteSignal(source, condition) {
      const msg = document.getElementById('sig-msg');
      await sendAction(
        msg,
        '/api/v1/signals/' + encodeURIComponent(source) + '/' + encodeURIComponent(condition),
        { method: 'DELETE', headers: authHeaders() },
        'Clearing...',
        'Cleared.'
      );
      refresh();
      refreshSignals();
    }

    async function refreshSignals() {
      try {
        const res = await fetch('/api/v1/signals');
        const data = await res.json();
        const list = document.getElementById('sig-list');
        list.innerHTML = '';

        if (!data.signals || data.signals.length === 0) {
          list.innerHTML = '<i>No active signals</i>';
          return;
        }

        data.signals.forEach((s) => {
          const row = document.createElement('div');
          row.className = 'signal-row';

          const label = document.createElement('span');
          label.textContent = s.source + ' / ' + s.condition + ' (' + s.severity + ')';

          const btn = document.createElement('button');
          btn.className = 'small-btn';
          btn.textContent = 'clear';
          btn.dataset.source = s.source;
          btn.dataset.condition = s.condition;

          row.appendChild(label);
          row.appendChild(btn);
          list.appendChild(row);
        });
      } catch (e) {
        // Device unreachable -- leave the last-known list in place.
      }
    }

    document.getElementById('sig-list').addEventListener('click', (e) => {
      const btn = e.target.closest('button[data-source]');
      if (!btn) return;
      deleteSignal(btn.dataset.source, btn.dataset.condition);
    });

    const apiKeyInput = document.getElementById('api-key');
    apiKeyInput.value = getApiKey();
    apiKeyInput.addEventListener('change', (e) => {
      try { localStorage.setItem(API_KEY_STORAGE_KEY, e.target.value); } catch (err) {}
    });

    refresh();
    refreshSignals();
    backgroundHeartbeat();
    setInterval(() => { refresh(); refreshSignals(); }, 2000);
    setInterval(backgroundHeartbeat, 10000); // well under HEARTBEAT_TIMEOUT_MS (30s default)
  </script>
</body>
</html>
)HTML";

    void handleDashboard() {
        server.send(200, "text/html", DASHBOARD_HTML);
    }
}

void setupApiServer(LightController& lightController) {
    lights = &lightController;

    const char* collectedHeaders[] = { "X-Api-Key" };
    server.collectHeaders(collectedHeaders, 1);

    // Read-only routes: left open so status is easy to check without a
    // key -- the whole point of a status light is that it's easy to see.
    server.on("/", HTTP_GET, handleDashboard);
    server.on("/health", HTTP_GET, handleHealth);
    server.on("/status", HTTP_GET, handleGetStatus);
    server.on("/api/v1/signals", HTTP_GET, handleGetSignals);

    // Everything else changes device state and requires X-Api-Key.
    server.on("/status/open", HTTP_POST, requireAuth([]() { handleSetStatus(TalariaStatus::OPEN); }));
    server.on("/status/closed", HTTP_POST, requireAuth([]() { handleSetStatus(TalariaStatus::CLOSED); }));
    server.on("/status/warning", HTTP_POST, requireAuth([]() { handleSetStatus(TalariaStatus::WARNING); }));
    server.on("/status/critical", HTTP_POST, requireAuth([]() { handleSetStatus(TalariaStatus::CRITICAL); }));
    // Normally OFFLINE is set by the device itself when it loses contact
    // with Talaria, but it's exposed here too so the pattern can be bench-tested.
    server.on("/status/offline", HTTP_POST, requireAuth([]() { handleSetStatus(TalariaStatus::OFFLINE); }));

    server.on("/test/cycle", HTTP_POST, requireAuth(handleSelfTest));
    server.on("/test/all-on", HTTP_POST, requireAuth(handleAllOnTestStart));
    server.on("/test/all-off", HTTP_POST, requireAuth(handleAllOnTestStop));

    server.on("/api/v1/signals", HTTP_POST, requireAuth(handlePostSignal));
    server.on(UriBraces("/api/v1/signals/{}/{}"), HTTP_DELETE, requireAuth(handleDeleteSignal));

    // Talaria should ping this periodically -- well under
    // HEARTBEAT_TIMEOUT_MS -- even when nothing has changed, so the device
    // knows it's still there.
    server.on("/heartbeat", HTTP_POST, requireAuth(handleHeartbeat));

    // One-button "known, blank state" -- clears every active signal,
    // resets status to offline, and force-holds all lights off.
    server.on("/reset", HTTP_POST, requireAuth(handleReset));

    server.onNotFound(handleNotFound);

    server.begin();
    Serial.println("[API] HTTP server listening on port 80");
}

void handleApiServer() {
    server.handleClient();
}

void apiServerTick() {
    // refreshDisplay() itself now checks whether a test mode owns the
    // lights before doing anything, so this is just the periodic nudge
    // that catches heartbeat staleness and TTL expiry between requests.
    refreshDisplay();
}
