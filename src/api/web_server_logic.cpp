#include "web_server_logic.h"
#include <ArduinoJson.h>
#include "../api/nysse.h"
#include "../sensors/bh1750_sensor.h"
#include "../sensors/inmp441_sensor.h"
#include "../sensors/mq_sensor.h"
#include "../sensors/dust_sensor.h"
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
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Nysse Dashboard - Remote</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
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

        /* Drag-and-drop tyylit */
        .drop-zone {
            border: 2px dashed #202845;
            border-radius: 12px;
            padding: 20px;
            transition: 0.3s;
        }
        .drop-zone--over {
            border-color: #508cff;
            background: rgba(80, 140, 255, 0.05);
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Dashboard Remote</h1>
        
        <div class="card" id="departures-card">
            <div class="label">Pysäkki: <span id="stop-name">Ladataan...</span></div>
            <div id="departures-list"></div>
        </div>

        <div class="sensor-grid">
            <div class="card sensor">
                <div class="label">Valoisuus</div>
                <div class="val" id="val-lux">-- lx</div>
            </div>
            <div class="card sensor">
                <div class="label">Ääni (VU)</div>
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

        <div class="card sensor">
            <div class="label">Pöly (PM2.5 Sharp)</div>
            <div class="val" id="val-dust">Ladataan...</div>
            <div style="height: 10px; background: #0a0c14; border-radius: 5px; margin-top: 10px; overflow: hidden;">
                <div id="dust-bar" style="height: 100%; width: 0%; background: #3cc864; transition: width 0.5s;"></div>
            </div>
        </div>

        <div class="card">
            <div class="label" style="display: flex; justify-content: space-between; align-items: center;">
                SD-kortin tiedostot 
                <button onclick="updateFileList()" style="background:none; border:none; color:#508cff; cursor:pointer; font-size:0.8em;">Päivitä lista</button>
            </div>
            <div id="file-list-container">
                <div class="empty-msg">Ladataan listaa...</div>
            </div>
        </div>

        <div class="card">
            <div class="label" style="display: flex; justify-content: space-between; align-items: center;">
                Anturihistoria (log.csv)
                <div>
                    <a href="/log.csv" download="historia.csv" style="display:inline-block; background:#202845; color:#fff; border:1px solid #508cff; text-decoration:none; padding:4px 10px; border-radius:4px; font-size:0.8em; margin-right:8px; cursor:pointer;">Lataa CSV</a>
                    <button onclick="updateChart()" style="background:none; border:none; color:#508cff; cursor:pointer; font-size:0.8em;">Päivitä data</button>
                </div>
            </div>
            <div style="position: relative; height: 300px; width: 100%; margin-top: 10px;">
                <canvas id="historyChart"></canvas>
            </div>
        </div>

        <div class="card drop-zone" id="drop-zone">
            <div class="label" style="margin-bottom: 10px; text-align: center;">Lataa tiedosto SD-kortille</div>
            <div style="text-align: center;">
                <input type="file" id="file-input" style="display: none;">
                
                <div id="upload-prompt">
                    <div style="color: #404864; margin-bottom: 10px;">
                        <svg width="40" height="40" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="opacity: 0.5;"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"></path><polyline points="17 8 12 3 7 8"></polyline><line x1="12" y1="3" x2="12" y2="15"></line></svg>
                        <p style="margin: 5px 0; font-size: 0.9em;">Raahaa tiedosto tähän tai</p>
                    </div>
                    <button onclick="document.getElementById('file-input').click()" style="background: #202845; color: #fff; border: 1px solid #508cff; padding: 8px 16px; border-radius: 6px; cursor: pointer; font-size: 0.9em;">Valitse tiedosto</button>
                </div>

                <div id="file-info" style="display: none;">
                    <div id="file-name" style="font-weight: bold; color: #fff; margin-bottom: 10px; word-break: break-all;"></div>
                    <div style="display: flex; gap: 10px;">
                        <button onclick="cancelUpload()" style="flex: 1; background: #0a0c14; color: #ff4646; border: 1px solid #ff4646; padding: 10px; border-radius: 8px; cursor: pointer; font-weight: bold;">Peruuta</button>
                        <button onclick="uploadFile()" id="upload-btn" style="flex: 2; background: #508cff; color: white; border: none; padding: 10px; border-radius: 8px; cursor: pointer; font-weight: bold;">Lähetä NYT</button>
                    </div>
                </div>

                <div id="upload-status" style="margin-top: 15px; font-size: 0.9em;"></div>
            </div>
        </div>

        <div class="footer">
            IP: <span id="ip">--</span> | Päivitetty: <span id="time">--</span>
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
                    document.getElementById('val-mq').innerText = data.mq + ' VOC Indeksi';
                    document.getElementById('val-dust').innerText = data.dust.toFixed(1) + ' µg/m³';
                    document.getElementById('ip').innerText = data.ip;
                    document.getElementById('time').innerText = new Date().toLocaleTimeString();

                    const mqPct = Math.min(100, (data.mq / 4095) * 100);
                    const bar = document.getElementById('mq-bar');
                    bar.style.width = mqPct + '%';
                    bar.style.background = data.mq > 2500 ? '#ff4646' : (data.mq > 1200 ? '#ffca32' : '#3cc864');

                    const dustPct = Math.min(100, (data.dust / 250) * 100);
                    const dBar = document.getElementById('dust-bar');
                    dBar.style.width = dustPct + '%';
                    dBar.style.background = data.dust > 75 ? '#ff4646' : (data.dust > 35 ? '#ffca32' : '#3cc864');

                    let html = '';
                    if (data.departures.length === 0) html = '<div class="departure">Ei lähtöjä juuri nyt.</div>';
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
        setInterval(updateData, 1000);
        updateData();
        updateFileList();

        let historyChart = null;
        
        function initChart() {
            const ctx = document.getElementById('historyChart').getContext('2d');
            historyChart = new Chart(ctx, {
                type: 'line',
                data: {
                    labels: [],
                    datasets: [
                        { label: 'Pöly (µg/m³)', borderColor: '#508cff', backgroundColor: 'rgba(80, 140, 255, 0.1)', data: [], yAxisID: 'y', tension: 0.3, fill: true, borderWidth: 2, pointRadius: 0 },
                        { label: 'Valoisuus', borderColor: '#ffca32', data: [], yAxisID: 'y1', tension: 0.3, borderWidth: 2, pointRadius: 0 },
                        { label: 'Ääni (VU)', borderColor: '#3cc864', data: [], yAxisID: 'y', tension: 0.3, borderWidth: 2, pointRadius: 0 },
                        { label: 'Ilmanlaatu (MQ)', borderColor: '#ff4646', data: [], yAxisID: 'y1', tension: 0.3, borderWidth: 2, pointRadius: 0 }
                    ]
                },
                options: {
                    responsive: true, maintainAspectRatio: false,
                    interaction: { mode: 'index', intersect: false },
                    scales: {
                        x: { grid: { color: '#202845' }, ticks: { color: '#646e8c', maxTicksLimit: 10 } },
                        y: { type: 'linear', display: true, position: 'left', suggestedMin: 0, suggestedMax: 100, grid: { color: '#202845' }, ticks: { color: '#646e8c' } },
                        y1: { type: 'linear', display: true, position: 'right', suggestedMin: 0, suggestedMax: 2000, grid: { drawOnChartArea: false }, ticks: { color: '#646e8c' } }
                    },
                    plugins: { legend: { labels: { color: '#e0e5f0' } } }
                }
            });
            updateChart();
        }

        function updateChart() {
            fetch('/log.csv')
                .then(response => {
                    if (!response.ok) throw new Error('Ei lokitietoja');
                    return response.text();
                })
                .then(csv => {
                    const lines = csv.trim().split('\n');
                    const limit = 200; // Rajataan 200 viimeiseen näytteeseen
                    const startIdx = Math.max(0, lines.length - limit);
                    
                    const labels = [];
                    const dustData = [];
                    const luxData = [];
                    const volData = [];
                    const mqData = [];
                    
                    // Format: time,lux,vol,mq,dust
                    for (let i = startIdx; i < lines.length; i++) {
                        const parts = lines[i].split(',');
                        if (parts.length >= 5 && parts[0] !== 'Time') {
                            labels.push(parts[0]);
                            luxData.push(parseFloat(parts[1]));
                            volData.push(parseFloat(parts[2]));
                            mqData.push(parseFloat(parts[3]));
                            dustData.push(parseFloat(parts[4]));
                        }
                    }
                    if (historyChart) {
                        historyChart.data.labels = labels;
                        historyChart.data.datasets[0].data = dustData;
                        historyChart.data.datasets[1].data = luxData;
                        historyChart.data.datasets[2].data = volData;
                        historyChart.data.datasets[3].data = mqData;
                        historyChart.update();
                    }
                }).catch(e => console.log('Chart error:', e));
        }
        setTimeout(initChart, 500); // Ladataan chart vähän viiveellä

        function updateFileList() {
            const container = document.getElementById('file-list-container');
            fetch('/api/list')
                .then(response => response.json())
                .then(files => {
                    if (files.length === 0) {
                        container.innerHTML = '<div class="empty-msg">SD-kortti on tyhjä.</div>';
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
                    else alert('Poisto epäonnistui');
                });
        }

        const fileInput = document.getElementById('file-input');
        const dropZone = document.getElementById('drop-zone');
        const uploadPrompt = document.getElementById('upload-prompt');
        const fileInfo = document.getElementById('file-info');
        const fileNameDisplay = document.getElementById('file-name');
        let selectedFile = null;

        // Drag and drop tapahtumat
        ['dragover', 'dragleave', 'drop'].forEach(evt => {
            dropZone.addEventListener(evt, e => {
                e.preventDefault();
                e.stopPropagation();
            });
        });

        dropZone.addEventListener('dragover', () => dropZone.classList.add('drop-zone--over'));
        ['dragleave', 'drop'].forEach(evt => dropZone.addEventListener(evt, () => dropZone.classList.remove('drop-zone--over')));

        dropZone.addEventListener('drop', e => {
            const files = e.dataTransfer.files;
            if (files.length > 0) handleFileSelect(files[0]);
        });

        fileInput.addEventListener('change', function() {
            if (this.files[0]) handleFileSelect(this.files[0]);
        });

        function handleFileSelect(file) {
            selectedFile = file;
            fileNameDisplay.innerText = file.name;
            uploadPrompt.style.display = 'none';
            fileInfo.style.display = 'block';
            document.getElementById('upload-status').innerText = '';
        }

        function cancelUpload() {
            selectedFile = null;
            fileInput.value = '';
            uploadPrompt.style.display = 'block';
            fileInfo.style.display = 'none';
        }

        function uploadFile() {
            if (!selectedFile) return;
            
            const btn = document.getElementById('upload-btn');
            const status = document.getElementById('upload-status');
            
            btn.disabled = true;
            btn.innerText = 'Lähetetään...';
            status.innerText = 'Valmistellaan latausta...';
            status.style.color = '#ffca32';

            const formData = new FormData();
            formData.append('file', selectedFile);

            fetch('/upload', {
                method: 'POST',
                body: formData
            })
            .then(response => {
                if (response.ok) {
                    status.innerText = 'Ladattu onnistuneesti!';
                    status.style.color = '#3cc864';
                    setTimeout(cancelUpload, 2000); // Nollaa nakyma hetken kuluttua
                    updateFileList();
                } else {
                    throw new Error('Lataus epäonnistui');
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
    doc["dust"] = getDustDensity();
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
    
    // Palvele CSV suoraan SD-kortilta
    server.on("/log.csv", HTTP_GET, []() {
        File file = SD.open("/log.csv", FILE_READ);
        if (!file) {
            server.send(404, "text/plain", "Log file not found");
            return;
        }
        server.streamFile(file, "text/csv");
        file.close();
    });
    
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
