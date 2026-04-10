#include "web_server_logic.h"
#include <ArduinoJson.h>
#include "../api/nysse.h"
#include "../sensors/bh1750_sensor.h"
#include "../sensors/inmp441_sensor.h"
#include "../sensors/mq_sensor.h"
#include <WiFi.h>
#include <SD.h>
#include <FS.h>
#include "../config.h"

File uploadFile;

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
        
        /* Tiedostolistaus tyylit */
        table { width: 100%; border-collapse: collapse; margin-top: 10px; }
        th { text-align: left; color: #646e8c; font-size: 0.8em; text-transform: uppercase; padding: 10px 5px; border-bottom: 2px solid #202845; }
        td { padding: 12px 5px; border-bottom: 1px solid #202845; font-size: 0.95em; }
        .file-size { color: #646e8c; font-size: 0.85em; width: 80px; }
        .file-actions { text-align: right; width: 60px; }
        .del-btn { color: #ff4646; background: none; border: 1px solid #ff4646; padding: 4px 8px; border-radius: 4px; cursor: pointer; font-size: 0.8em; transition: 0.2s; }
        .del-btn:hover { background: #ff4646; color: #fff; }
        .empty-msg { text-align: center; padding: 20px; color: #404864; font-style: italic; }
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

        <div class="card">
            <div class="label" style="display: flex; justify-content: space-between; align-items: center;">
                SD-kortin tiedostot 
                <button onclick="updateFileList()" style="background:none; border:none; color:#508cff; cursor:pointer; font-size:0.8em;">Paivita lista</button>
            </div>
            <div id="file-list-container">
                <div class="empty-msg">Ladataan listaa...</div>
            </div>
        </div>

        <div class="card">
            <div class="label">Lataa tiedosto SD-kortille</div>
            <div style="margin-top: 10px;">
                <input type="file" id="file-input" style="display: none;">
                <button onclick="document.getElementById('file-input').click()" style="background: #202845; color: #fff; border: 1px solid #508cff; padding: 10px 20px; border-radius: 8px; cursor: pointer; width: 100%;">Valitse tiedosto...</button>
                <div id="file-name" style="margin-top: 8px; font-size: 0.9em; color: #646e8c; text-align: center;">Ei tiedostoa valittuna</div>
                <button onclick="uploadFile()" id="upload-btn" style="margin-top: 10px; background: #508cff; color: white; border: none; padding: 10px; border-radius: 8px; width: 100%; font-weight: bold; display: none;">Laheta NYT</button>
                <div id="upload-status" style="margin-top: 10px; text-align: center; font-size: 0.9em;"></div>
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
        updateFileList();

        function updateFileList() {
            const container = document.getElementById('file-list-container');
            fetch('/api/list')
                .then(response => response.json())
                .then(files => {
                    if (files.length === 0) {
                        container.innerHTML = '<div class="empty-msg">SD-kortti on tyhja.</div>';
                        return;
                    }
                    let html = '<table><thead><tr><th>Nimi</th><th>Koko</th><th></th></tr></thead><tbody>';
                    files.forEach(f => {
                        const sizeStr = f.size > 1024 * 1024 ? (f.size / (1024 * 1024)).toFixed(1) + ' MB' : (f.size / 1024).toFixed(1) + ' KB';
                        html += `<tr>
                            <td>${f.name}</td>
                            <td class="file-size">${sizeStr}</td>
                            <td class="file-actions"><button class="del-btn" onclick="deleteFile('${f.name}')">Poista</button></td>
                        </tr>`;
                    });
                    html += '</tbody></table>';
                    container.innerHTML = html;
                })
                .catch(err => {
                    container.innerHTML = '<div class="empty-msg" style="color:#ff4646;">Virhe listan haussa.</div>';
                });
        }

        function deleteFile(name) {
            if (!confirm('Haluatko varmasti poistaa tiedoston ' + name + '?')) return;
            fetch('/api/delete?path=/' + name)
                .then(response => {
                    if (response.ok) updateFileList();
                    else alert('Poisto epaonnistui');
                });
        }

        const fileInput = document.getElementById('file-input');
        fileInput.addEventListener('change', function() {
            if (this.files[0]) {
                document.getElementById('file-name').innerText = this.files[0].name;
                document.getElementById('upload-btn').style.display = 'block';
            }
        });

        function uploadFile() {
            const file = fileInput.files[0];
            if (!file) return;
            
            const btn = document.getElementById('upload-btn');
            const status = document.getElementById('upload-status');
            
            btn.disabled = true;
            btn.innerText = 'Lahetetaan...';
            status.innerText = 'Valmistellaan latausta...';
            status.style.color = '#ffca32';

            const formData = new FormData();
            formData.append('file', file);

            fetch('/upload', {
                method: 'POST',
                body: formData
            })
            .then(response => {
                if (response.ok) {
                    status.innerText = 'Ladattu onnistuneesti!';
                    status.style.color = '#3cc864';
                    fileInput.value = '';
                    document.getElementById('file-name').innerText = 'Ei tiedostoa valittuna';
                    btn.style.display = 'none';
                    updateFileList(); // Paivita lista heti latauksen jalkeen
                } else {
                    throw new Error('Lataus epaonnistui');
                }
            })
            .catch(error => {
                status.innerText = 'Virhe: ' + error.message;
                status.style.color = '#ff4646';
            })
            .finally(() => {
                btn.disabled = false;
                btn.innerText = 'Laheta NYT';
            });
        }
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

void handleFileUpload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        String filename = upload.filename;
        if (!filename.startsWith("/")) filename = "/" + filename;
        Serial.print("Ladataan tiedostoa: "); Serial.println(filename);
        uploadFile = SD.open(filename, FILE_WRITE);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (uploadFile) {
            uploadFile.write(upload.buf, upload.currentSize);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (uploadFile) {
            uploadFile.close();
            Serial.print("Lataus valmis. Koko: "); Serial.println(upload.totalSize);
        }
    }
}

void handleFileList() {
    JsonDocument doc;
    JsonArray files = doc.to<JsonArray>();

    File root = SD.open("/");
    if (!root || !root.isDirectory()) {
        server.send(500, "text/plain", "SD error");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            JsonObject f = files.add<JsonObject>();
            f["name"] = String(file.name());
            f["size"] = file.size();
        }
        file = root.openNextFile();
    }
    
    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", output);
}

void handleFileDelete() {
    if (!server.hasArg("path")) {
        server.send(400, "text/plain", "Missing path");
        return;
    }
    String path = server.arg("path");
    Serial.print("Poistetaan tiedosto: "); Serial.println(path);
    if (SD.remove(path)) {
        server.send(200, "text/plain", "Deleted");
    } else {
        server.send(500, "text/plain", "Delete failed");
    }
}

void initWebServer() {
    server.on("/", handleRoot);
    server.on("/api/data", handleData);
    server.on("/api/list", HTTP_GET, handleFileList);
    server.on("/api/delete", HTTP_GET, handleFileDelete);
    
    // Tiedoston lataus (POST)
    server.on("/upload", HTTP_POST, []() {
        server.send(200, "text/plain", "OK");
    }, handleFileUpload);

    server.begin();
    Serial.println("HTTP palvelin kaynnistetty.");
}

void handleWebServer() {
    server.handleClient();
}
