const express = require('express');
const fs = require('fs');
const http = require('http');
const https = require('https');
const path = require('path');
const WebSocket = require('ws');


const HTTP_PORT = 3030;
const HTTPS_PORT = 3031;
const MQTT_PORT = 1883;
const DATA_FILE = path.join(__dirname, 'data.json');

// 1. Database Initialization (JSON File)
let db = {
    scans: [],
    inventory: {
        'SKU-8901234567': { name: 'Widget A', qty: 42 },
        'SKU-8901234981': { name: 'Widget B - steel', qty: 7 },
        'SKU-8901235120': { name: 'Bracket set', qty: 0 }
    },
    active_devices: {},
    tasks: [],
    users: [
        { id: '1', name: 'Operator 1', role: 'Warehouse Operator', pin: '1234' },
        { id: 'admin', name: 'System Admin', role: 'Supervisor', pin: '1234' }
    ]
};

// Load data if exists
if (fs.existsSync(DATA_FILE)) {
    try {
        const raw = fs.readFileSync(DATA_FILE, 'utf8');
        db = JSON.parse(raw);
    } catch (e) {
        console.error('Error reading data.json, starting fresh.', e);
    }
}
// Ensure arrays exist
if (!db.attendance_logs) db.attendance_logs = [];
if (!db.attendance_overrides) db.attendance_overrides = {};
if (!db.tasks) db.tasks = [];
if (!db.users) db.users = [];

// Helper to save data
function saveData() {
    fs.writeFile(DATA_FILE, JSON.stringify(db, null, 2), (err) => {
        if (err) console.error('Error writing data.json', err);
    });
}

// 2. Express Application
const app = express();
app.use(express.json());
app.use(express.static(path.join(__dirname, 'public')));

// API Routes
app.get('/api/scans', (req, res) => {
    // Return last 100 scans, enriched with inventory names
    const enrichedScans = db.scans.slice(-100).reverse().map(scan => {
        const product = db.inventory[scan.sku];
        return {
            ...scan,
            product_name: product ? product.name : 'Unknown Product'
        };
    });
    res.json(enrichedScans);
});

app.get('/api/inventory', (req, res) => {
    const invList = Object.keys(db.inventory).map(sku => ({
        sku,
        name: db.inventory[sku].name,
        qty: db.inventory[sku].qty
    }));
    res.json(invList);
});

app.post('/api/inventory', (req, res) => {
    const { sku, name, qty } = req.body;
    if (!sku) return res.status(400).json({ error: 'SKU required' });
    
    db.inventory[sku] = { name, qty: parseInt(qty) || 0 };
    saveData();
    
    const invList = Object.keys(db.inventory).map(k => ({ sku: k, name: db.inventory[k].name, qty: db.inventory[k].qty }));
    
    // Broadcast UI update
    wss.clients.forEach(c => {
        if (c.readyState === WebSocket.OPEN) {
            c.send(JSON.stringify({ type: 'UPDATE_INVENTORY', data: invList }));
        }
    });
    
    // Publish updated inventory list to MQTT
    mqttClient.publish('config/inventory', JSON.stringify(invList), { retain: true });
    
    res.json({ success: true, sku });
});

app.delete('/api/inventory/:sku', (req, res) => {
    const sku = req.params.sku;
    if (!db.inventory[sku]) return res.status(404).json({ error: 'SKU not found' });
    
    delete db.inventory[sku];
    saveData();
    
    const invList = Object.keys(db.inventory).map(k => ({ sku: k, name: db.inventory[k].name, qty: db.inventory[k].qty }));
    
    // Broadcast UI update
    wss.clients.forEach(c => {
        if (c.readyState === WebSocket.OPEN) {
            c.send(JSON.stringify({ type: 'UPDATE_INVENTORY', data: invList }));
        }
    });
    
    // Publish updated inventory list to MQTT
    mqttClient.publish('config/inventory', JSON.stringify(invList), { retain: true });
    
    res.json({ success: true });
});

app.get('/api/devices', (req, res) => {
    res.json(db.active_devices || {});
});

app.get('/api/tasks', (req, res) => {
    res.json(db.tasks || []);
});

function publishTasksForDevice(deviceId) {
    const activeDev = (db.active_devices || {})[deviceId];
    const devUser = activeDev ? activeDev.user : 'No Login';

    // Find matching user object by name or ID (case-insensitive)
    const userObj = (db.users || []).find(u => 
        (u.name && devUser && u.name.trim().toLowerCase() === devUser.trim().toLowerCase()) ||
        (u.id && devUser && u.id.trim().toLowerCase() === devUser.trim().toLowerCase())
    );
    const userId = userObj ? userObj.id : null;
    const userName = userObj ? userObj.name : null;

    const userTasks = (db.tasks || []).filter(t => {
        if (!t.assignee) return false;
        const ass = t.assignee.trim().toLowerCase();
        if (ass === 'all') return true;
        if (deviceId && ass === deviceId.toLowerCase()) return true;
        if (devUser && devUser !== 'No Login' && ass === devUser.toLowerCase()) return true;
        if (userId && ass === userId.toLowerCase()) return true;
        if (userName && ass === userName.toLowerCase()) return true;
        return false;
    });

    console.log(`[MQTT] Publishing ${userTasks.length} tasks to device/${deviceId}/tasks (user: '${devUser}')`);
    mqttClient.publish(`device/${deviceId}/tasks`, JSON.stringify(userTasks), { retain: true });
}

function publishAllDeviceTasks(specificAssignee = null) {
    const activeDevices = db.active_devices || {};
    const devIds = Object.keys(activeDevices);

    // Always publish to active devices
    devIds.forEach(id => publishTasksForDevice(id));

    // Also publish directly to specific target device if specified or if default device exists
    const targetSet = new Set(['scanpro-test-01']);
    if (specificAssignee) targetSet.add(specificAssignee);
    (db.tasks || []).forEach(t => {
        if (t.assignee && t.assignee !== 'all') targetSet.add(t.assignee);
    });

    targetSet.forEach(targetId => {
        if (!activeDevices[targetId]) {
            publishTasksForDevice(targetId);
        }
    });
}

function publishTasksForAssignee(assignee) {
    publishAllDeviceTasks(assignee);
}

app.post('/api/tasks', (req, res) => {
    const { assignee, name, prio, items } = req.body;
    if (!assignee || !name) return res.status(400).json({ error: 'assignee and name required' });
    
    if (!db.tasks) db.tasks = [];
    const newTask = { id: Date.now().toString(), assignee, name, prio, status: 'active', items: items || [] };
    db.tasks.push(newTask);
    saveData();
    
    // Publish the updated tasks list to all devices in real-time
    publishAllDeviceTasks(assignee);
    
    // Broadcast UI update
    wss.clients.forEach(c => {
        if (c.readyState === WebSocket.OPEN) {
            c.send(JSON.stringify({ type: 'UPDATE_TASKS', data: db.tasks }));
        }
    });
    
    res.json({ success: true, task: newTask });
});

app.put('/api/tasks/:id', (req, res) => {
    const taskId = req.params.id;
    const { assignee, name, prio, items } = req.body;
    if (!db.tasks) return res.status(404).json({ error: 'No tasks found' });
    
    const taskIndex = db.tasks.findIndex(t => t.id === taskId);
    if (taskIndex === -1) return res.status(404).json({ error: 'Task not found' });
    
    const oldAssignee = db.tasks[taskIndex].assignee;
    
    db.tasks[taskIndex] = { ...db.tasks[taskIndex], assignee, name, prio, items: items || [] };
    saveData();
    
    // Publish updated tasks to all devices in real-time
    publishAllDeviceTasks(assignee);
    if (oldAssignee && oldAssignee !== assignee) {
        publishAllDeviceTasks(oldAssignee);
    }
    
    // Broadcast UI update
    wss.clients.forEach(c => {
        if (c.readyState === WebSocket.OPEN) {
            c.send(JSON.stringify({ type: 'UPDATE_TASKS', data: db.tasks }));
        }
    });
    
    res.json({ success: true, task: db.tasks[taskIndex] });
});

app.delete('/api/tasks/:id', (req, res) => {
    const taskId = req.params.id;
    if (!db.tasks) return res.status(404).json({ error: 'No tasks found' });
    
    const taskIndex = db.tasks.findIndex(t => t.id === taskId);
    if (taskIndex === -1) return res.status(404).json({ error: 'Task not found' });
    
    const assignee = db.tasks[taskIndex].assignee || db.tasks[taskIndex].device_id;
    db.tasks.splice(taskIndex, 1);
    saveData();
    
    // Publish updated tasks to all devices in real-time
    publishAllDeviceTasks(assignee);
    
    // Broadcast UI update
    wss.clients.forEach(c => {
        if (c.readyState === WebSocket.OPEN) {
            c.send(JSON.stringify({ type: 'UPDATE_TASKS', data: db.tasks }));
        }
    });
    
    res.json({ success: true });
});

// Users Management API
const activeSessions = {}; // { deviceId: { user: 'Operator 1', loginTime: 12345678 } }

app.get('/api/users', (req, res) => {
    // Inject active session data into the response so frontend can show live timers
    const usersWithSessions = (db.users || []).map(u => {
        const session = Object.values(activeSessions).find(s => s.user === u.name);
        return {
            ...u,
            active_session_start: session ? session.loginTime : null,
            worked_ms: u.worked_ms || 0
        };
    });
    res.json(usersWithSessions);
});

// Attendance Management API
app.get('/api/attendance', (req, res) => {
    // Return historical logs plus any currently active sessions
    const activeSessionLogs = Object.entries(activeSessions).map(([deviceId, session]) => ({
        userId: session.userId,
        userName: session.user,
        loginTime: session.loginTime,
        logoutTime: null,
        durationMs: Date.now() - session.loginTime,
        status: 'Active',
        deviceId
    }));
    
    res.json({
        history: db.attendance_logs || [],
        active: activeSessionLogs,
        overrides: db.attendance_overrides || {}
    });
});

app.post('/api/attendance/override', (req, res) => {
    const { key, durationMs } = req.body;
    if (!key || durationMs === undefined) return res.status(400).json({ error: 'Missing parameters' });
    
    if (!db.attendance_overrides) db.attendance_overrides = {};
    db.attendance_overrides[key] = durationMs;
    saveData();
    
    wss.clients.forEach(c => {
        if (c.readyState === WebSocket.OPEN) {
            c.send(JSON.stringify({ type: 'UPDATE_ATTENDANCE' }));
        }
    });
    
    res.json({ success: true });
});

app.post('/api/users', (req, res) => {
    const { id, name, role, pin } = req.body;
    if (!id || !name) return res.status(400).json({ error: 'User ID and Name required' });
    
    if (!db.users) db.users = [];
    
    // Check if ID already exists
    const idx = db.users.findIndex(u => u.id === id);
    const newUser = { id, name, role: role || 'Operator', pin: pin || '1234' };
    
    if (idx !== -1) {
        db.users[idx] = newUser;
    } else {
        db.users.push(newUser);
    }
    saveData();
    
    // Broadcast UI update
    wss.clients.forEach(c => {
        if (c.readyState === WebSocket.OPEN) {
            c.send(JSON.stringify({ type: 'UPDATE_USERS', data: db.users }));
        }
    });
    
    // Publish updated users list to MQTT
    mqttClient.publish('config/users', JSON.stringify(db.users), { retain: true });
    
    res.json({ success: true, user: newUser });
});

app.delete('/api/users/:id', (req, res) => {
    const userId = req.params.id;
    if (!db.users) return res.status(404).json({ error: 'No users found' });
    
    db.users = db.users.filter(u => u.id !== userId);
    saveData();
    
    // Broadcast UI update
    wss.clients.forEach(c => {
        if (c.readyState === WebSocket.OPEN) {
            c.send(JSON.stringify({ type: 'UPDATE_USERS', data: db.users }));
        }
    });
    
    // Publish updated users list to MQTT
    mqttClient.publish('config/users', JSON.stringify(db.users), { retain: true });
    
    res.json({ success: true });
});

// 3. HTTP Server and WebSockets
const server = http.createServer(app);
const wss = new WebSocket.Server({ noServer: true });
const audioWss = new WebSocket.Server({ noServer: true });

server.on('upgrade', function upgrade(request, socket, head) {
    if (request.url.startsWith('/audio')) {
        audioWss.handleUpgrade(request, socket, head, function done(ws) {
            audioWss.emit('connection', ws, request);
        });
    } else {
        wss.handleUpgrade(request, socket, head, function done(ws) {
            wss.emit('connection', ws, request);
        });
    }
});

// Bidirectional Audio Routing:
//   ESP32 Mic → PC/Browser: VAD-gated frames from ESP32 forwarded to all PC clients.
//   PC/Browser → ESP32:     "Start Talking" frames from PC forwarded to target ESP32 device.
audioWss.on('connection', (ws, request) => {
    const url = new URL(request.url, `http://${request.headers.host}`);
    ws.clientType = url.searchParams.get('client_type') || 'pc';
    ws.deviceId = url.searchParams.get('device_id') || 'unassigned';
    ws.targetDeviceId = url.searchParams.get('target_device') || 'all';
    console.log(`[WS] Audio client connected: type=${ws.clientType}, deviceId=${ws.deviceId}, target=${ws.targetDeviceId}`);

    ws.on('error', (err) => console.error(`[WS Audio Error] ${ws.clientType} (${ws.deviceId}):`, err.message));

    ws.on('message', (message) => {
        if (ws.clientType === 'esp32') {
            // ESP32 Mic → all connected PC/browser clients
            audioWss.clients.forEach(client => {
                if (client !== ws && client.readyState === WebSocket.OPEN && client.clientType === 'pc') {
                    client.send(message, { binary: true });
                }
            });
            process.stdout.write('^'); // uplink from ESP32

        } else if (ws.clientType === 'pc') {
            // PC "Start Talking" → target ESP32 device
            audioWss.clients.forEach(client => {
                if (client !== ws && client.readyState === WebSocket.OPEN && client.clientType === 'esp32') {
                    if (ws.targetDeviceId && ws.targetDeviceId !== 'all') {
                        const activeDev = (db.active_devices || {})[client.deviceId];
                        const devUser = activeDev ? activeDev.user : null;
                        const isMatch = (client.deviceId === ws.targetDeviceId) ||
                                        (devUser === ws.targetDeviceId) ||
                                        (client.deviceId && client.deviceId.includes(ws.targetDeviceId));
                        if (!isMatch) return;
                    }
                    client.send(message, { binary: true });
                }
            });
            process.stdout.write('v'); // downlink to ESP32
        }
    });
});


function broadcastScan(scanData) {
    // Enrich with product name if available
    const product = db.inventory[scanData.sku];
    scanData.product_name = product ? product.name : 'Unknown Product';
    const msg = JSON.stringify({ type: 'NEW_SCAN', data: scanData });
    wss.clients.forEach(client => {
        if (client.readyState === WebSocket.OPEN) {
            client.send(msg);
        }
    });
}

const mqtt = require('mqtt');
const mqttClient = mqtt.connect('mqtt://localhost:1883');

mqttClient.on('connect', () => {
    console.log('[MQTT] Connected to local broker on port 1883');
    mqttClient.subscribe('device/+/scan', (err) => {
        if (!err) console.log('[MQTT] Subscribed to device/+/scan');
    });
    mqttClient.subscribe('device/+/status', (err) => {
        if (!err) console.log('[MQTT] Subscribed to device/+/status');
    });
    mqttClient.subscribe('device/+/task_complete', (err) => {
        if (!err) console.log('[MQTT] Subscribed to device/+/task_complete');
    });
    
    // Publish users as a retained message upon broker connection
    if (db.users) {
        mqttClient.publish('config/users', JSON.stringify(db.users), { retain: true });
    }
    
    // Publish inventory as a retained message upon broker connection
    if (db.inventory) {
        const invList = Object.keys(db.inventory).map(k => ({ sku: k, name: db.inventory[k].name, qty: db.inventory[k].qty }));
        mqttClient.publish('config/inventory', JSON.stringify(invList), { retain: true });
    }
});

mqttClient.on('message', (topic, message) => {
    try {
        const payload = JSON.parse(message.toString());
        
        if (topic.endsWith('/scan')) {
            console.log('Received scan:', payload);
            const exists = db.scans.find(s => s.uuid === payload.uuid);
            if (!exists) {
                db.scans.push(payload);
                if (db.scans.length > 1000) db.scans.shift();
                saveData();
                broadcastScan(payload);
            }
        } else if (topic.endsWith('/status')) {
            console.log('Received status update:', topic, payload);
            const deviceId = topic.split('/')[1];
            if (!db.active_devices) db.active_devices = {};
            
            const prevSession = activeSessions[deviceId];
            const isOnline = payload.status === 'online';
            const newUser = isOnline ? payload.user : null;
            
            // Handle session ending (device goes offline, OR user changes, OR logout)
            const isNoLogin = !newUser || newUser === 'Unassigned' || newUser === 'No Login';
            if (prevSession && (!isOnline || prevSession.user !== newUser || isNoLogin)) {
                const logoutTime = Date.now();
                const duration = logoutTime - prevSession.loginTime;
                
                // Update cumulative worked_ms
                const userObj = db.users.find(u => u.name === prevSession.user);
                if (userObj) {
                    userObj.worked_ms = (userObj.worked_ms || 0) + duration;
                }
                
                // Add to detailed attendance logs
                if (!db.attendance_logs) db.attendance_logs = [];
                db.attendance_logs.push({
                    userId: prevSession.userId || (userObj ? userObj.id : 'Unknown'),
                    userName: prevSession.user,
                    loginTime: prevSession.loginTime,
                    logoutTime: logoutTime,
                    durationMs: duration,
                    deviceId: deviceId
                });
                
                // Keep history trimmed to last 5000 records
                if (db.attendance_logs.length > 5000) db.attendance_logs.shift();
                
                delete activeSessions[deviceId];
            }
            
            if (isOnline) {
                const displayUser = isNoLogin ? 'No Login' : newUser;
                if (!isNoLogin) {
                    if (!activeSessions[deviceId] || activeSessions[deviceId].user !== newUser) {
                        const userObj = db.users.find(u => u.name === newUser);
                        activeSessions[deviceId] = { 
                            user: newUser, 
                            userId: userObj ? userObj.id : 'Unknown',
                            loginTime: Date.now() 
                        };
                    }
                }
                db.active_devices[deviceId] = { user: displayUser, status: 'online', ts: payload.ts || Date.now() };
                publishTasksForDevice(deviceId);
            } else {
                delete db.active_devices[deviceId];
                publishTasksForDevice(deviceId);
            }
            saveData();
            
            // Broadcast UI update
            wss.clients.forEach(c => {
                if (c.readyState === WebSocket.OPEN) {
                    c.send(JSON.stringify({ type: 'UPDATE_DEVICES', data: db.active_devices }));
                    c.send(JSON.stringify({ type: 'UPDATE_USERS' })); // Trigger users table refresh
                }
            });
        } else if (topic.endsWith('/task_complete')) {
            console.log('Received task completion:', payload);
            if (!db.tasks || !db.inventory) return;
            
            const taskIndex = db.tasks.findIndex(t => t.id === payload.task_id);
            if (taskIndex !== -1) {
                const task = db.tasks[taskIndex];
                
                // Deduct inventory
                if (task.items) {
                    task.items.forEach(item => {
                        if (db.inventory[item.sku]) {
                            db.inventory[item.sku].qty = Math.max(0, db.inventory[item.sku].qty - item.target_qty);
                        }
                    });
                }
                
                const assignee = task.assignee;
                // Mark task as complete instead of deleting
                db.tasks[taskIndex].status = 'complete';
                saveData();
                
                // Update specific assignee's tasks
                publishTasksForAssignee(assignee);
                
                // Publish global inventory updates
                const invList = Object.keys(db.inventory).map(k => ({ sku: k, name: db.inventory[k].name, qty: db.inventory[k].qty }));
                mqttClient.publish('config/inventory', JSON.stringify(invList), { retain: true });
                
                // Broadcast UI updates
                wss.clients.forEach(c => {
                    if (c.readyState === WebSocket.OPEN) {
                        c.send(JSON.stringify({ type: 'UPDATE_TASKS', data: db.tasks }));
                        c.send(JSON.stringify({ type: 'UPDATE_INVENTORY', data: invList }));
                    }
                });
            }
        }
    } catch (e) {
        console.error('Error parsing MQTT payload on topic ' + topic, e);
    }
});

// Start Server
server.listen(HTTP_PORT, () => {
    console.log(`[HTTP/WS] Server and Dashboard running on http://localhost:${HTTP_PORT}`);
});

if (fs.existsSync(path.join(__dirname, 'key.pem')) && fs.existsSync(path.join(__dirname, 'cert.pem'))) {
    const options = {
        key: fs.readFileSync(path.join(__dirname, 'key.pem')),
        cert: fs.readFileSync(path.join(__dirname, 'cert.pem'))
    };
    const httpsServer = https.createServer(options, app);
    httpsServer.on('upgrade', (request, socket, head) => {
        if (request.url.startsWith('/audio')) {
            audioWss.handleUpgrade(request, socket, head, (ws) => {
                audioWss.emit('connection', ws, request);
            });
        } else {
            wss.handleUpgrade(request, socket, head, (ws) => {
                wss.emit('connection', ws, request);
            });
        }
    });
    httpsServer.listen(HTTPS_PORT, () => {
        console.log(`[HTTPS/WSS] Secure Dashboard running on https://0.0.0.0:${HTTPS_PORT}`);
    });
}
