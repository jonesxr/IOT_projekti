#include "web_server_logic.h"
#include <ArduinoJson.h>
#include "../api/nysse.h"
#include "../sensors/bh1750_sensor.h"
#include "../sensors/inmp441_sensor.h"
#include "../sensors/mq_sensor.h"
#include <WiFi.h>

WebServer server(80);

const char INDEX_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Nysse Dashboard - Remote</title>
    <style>
        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #0a0c14; color: #e0e5f0; margin: 0; padding: 20px; }
        .container { max-width: 600px; margin: auto; }
        h1 { color: #508cff; border-bottom: 2px solid #14192d; padding-bottom: 10px; }
        .card { background: #161c30; padding: 15px; border-radius: 12px; margin-bottom: 15px; box-shadow: 0 4px 15px rgba(0,0,0,0.3); }
        .departure { display: flex; align-items: center; justify-content: space-between; padding: 10px 0; border-bottom: 1px solid #202845; }
        .departure:last-child { border-bottom: none; }
        .route { background: #508cff; color: white; padding: 5px 12px; border-radius: 6px; font-weight: bold; min-width: 40px; text-align: center; }
        .dest { flex-grow: 1; margin-left: 15px; font-size: 1.1em; }
        .time { font-weight: bold; color: #fff; }
        .sensor-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
        .sensor { text-align: center; }
        .val { font-size: 1.5em; font-weight: bold; color: #ffca32; margin-top: 5px; }
        .label { color: #646e8c; font-size: 0.9em; text-transform: uppercase; }
        .footer { text-align: center; color: #404864; font-size: 0.8em; margin-top: 20px; }
        .realtime { width: 8px; height: 8px; background: #3cc864; border-radius: 50%; display: inline-block; margin-left: 5px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Dashboard Remote</h1>
        
        <div class="card" id="departures-card">
            <div class="label">Pysakki: <span id="stop-name">Ladataan...</span></div>
            <div id="departures-list"></div>
        </div>

        <div class="sensor-grid">
            <div class="card sensor">
                <div class="label">Valoisuus</div>
                <div class="val" id="val-lux">-- lx</div>
            </div>
            <div class="card sensor">
                <div class="label">Aani (VU)</div>
                <div class="val" id="val-vol">0</div>
            </div>
        </div>

        <div class="card sensor">
            <div class="label">Ilmanlaatu (MQ-135)</div>
            <div class="val" id="val-mq">Ladataan...</div>
            <div style="height: 10px; background: #0a0c14; border-radius: 5px; margin-top: 10px; overflow: hidden;">
                <div id="mq-bar" style="height: 100%; width: 0%; background: #3cc864; transition: width 0.5s;"></div>
            </div>
        </div>

        <div class="footer">
            IP: <span id="ip">--</span> | Paivitetty: <span id="time">--</span>
        </div>
    </div>

    <script>
        function updateData() {
            fetch('/api/data')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('stop-name').innerText = data.stop;
                    document.getElementById('val-lux').innerText = data.lux.toFixed(1) + ' lx';
                    document.getElementById('val-vol').innerText = data.vol;
                    document.getElementById('val-mq').innerText = data.mq;
                    document.getElementById('ip').innerText = data.ip;
                    document.getElementById('time').innerText = new Date().toLocaleTimeString();

                    const mqPct = Math.min(100, (data.mq / 4095) * 100);
                    const bar = document.getElementById('mq-bar');
                    bar.style.width = mqPct + '%';
                    bar.style.background = data.mq > 2500 ? '#ff4646' : (data.mq > 1200 ? '#ffca32' : '#3cc864');

                    let html = '';
                    if (data.departures.length === 0) html = '<div class="departure">Ei lahtoja juuri nyt.</div>';
                    data.departures.forEach(d => {
                        html += `<div class="departure">
                            <div class="route">${d.route}</div>
                            <div class="dest">${d.headsign}${d.realtime ? '<span class="realtime"></span>' : ''}</div>
                            <div class="time">${d.time}</div>
                        </div>`;
                    });
                    document.getElementById('departures-list').innerHTML = html;
                });
        }
        setInterval(updateData, 5000);
        updateData();
    </script>
</body>
</html>
)=====";

void handleRoot() {
    server.send_P(200, "text/html", INDEX_HTML);
}

void handleData() {
    JsonDocument doc;
    doc["stop"] = stopName;
    doc["lux"] = getLightLevelLux();
    doc["vol"] = getMicrophoneVolume();
    doc["mq"] = getMQLevel();
    doc["ip"] = WiFi.localIP().toString();

    JsonArray deps = doc["departures"].to<JsonArray>();
    for (int i = 0; i < departureCount; i++) {
        JsonObject d = deps.add<JsonObject>();
        d["route"] = departures[i].route;
        
        // Puretaan aika ja määränpää headsign-kentästä (muotoa "12:34 Kohde")
        char timePart[8] = "";
        const char* rawDest = departures[i].headsign;
        if (strlen(departures[i].headsign) > 5 && departures[i].headsign[2] == ':') {
            strncpy(timePart, departures[i].headsign, 5);
            timePart[5] = 0;
            rawDest = departures[i].headsign + 6;
        }
        
        d["headsign"] = rawDest;
        d["time"] = timePart;
        d["realtime"] = departures[i].realtime;
    }

    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", output);
}

void initWebServer() {
    server.on("/", handleRoot);
    server.on("/api/data", handleData);
    server.begin();
    Serial.println("HTTP palvelin kaynnistetty.");
}

void handleWebServer() {
    server.handleClient();
}
