const express = require('express');
const fs = require('fs');
const http = require('http');
const path = require('path');
const WebSocket = require('ws');


const HTTP_PORT = 3030;
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

function publishTasksForAssignee(assignee) {
    const activeDevices = db.active_devices || {};
    const userTasks = (db.tasks || []).filter(t => t.assignee === assignee);
    
    for (const [deviceId, data] of Object.entries(activeDevices)) {
        if (data.user === assignee) {
            console.log(`[MQTT] Publishing tasks to device/${deviceId}/tasks for user ${assignee}`);
            mqttClient.publish(`device/${deviceId}/tasks`, JSON.stringify(userTasks), { retain: true });
        }
    }
}

app.post('/api/tasks', (req, res) => {
    const { assignee, name, sub, prio, items } = req.body;
    if (!assignee || !name) return res.status(400).json({ error: 'assignee and name required' });
    
    if (!db.tasks) db.tasks = [];
    const newTask = { id: Date.now().toString(), assignee, name, sub, prio, items: items || [] };
    db.tasks.push(newTask);
    saveData();
    
    // Publish the updated tasks list to any devices the assignee is using
    publishTasksForAssignee(assignee);
    
    // Broadcast UI update
    wss.clients.forEach(c => {
        if (c.readyState === WebSocket.OPEN) {
            c.send(JSON.stringify({ type: 'UPDATE_TASKS', data: db.tasks }));
        }
    });
    
    res.json({ success: true, task: newTask });
});

app.delete('/api/tasks/:id', (req, res) => {
    const taskId = req.params.id;
    if (!db.tasks) return res.status(404).json({ error: 'No tasks found' });
    
    const taskIndex = db.tasks.findIndex(t => t.id === taskId);
    if (taskIndex === -1) return res.status(404).json({ error: 'Task not found' });
    
    const assignee = db.tasks[taskIndex].assignee || db.tasks[taskIndex].device_id;
    db.tasks.splice(taskIndex, 1);
    saveData();
    
    // Publish updated tasks to any devices the assignee is using
    publishTasksForAssignee(assignee);
    
    // Broadcast UI update
    wss.clients.forEach(c => {
        if (c.readyState === WebSocket.OPEN) {
            c.send(JSON.stringify({ type: 'UPDATE_TASKS', data: db.tasks }));
        }
    });
    
    res.json({ success: true });
});

// Users Management API
app.get('/api/users', (req, res) => {
    res.json(db.users || []);
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
const wss = new WebSocket.Server({ server });

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
            
            if (payload.status === 'online') {
                db.active_devices[deviceId] = { user: payload.user, ts: payload.ts || Date.now() };
                // Send tasks for this user directly to the newly connected device
                const userTasks = (db.tasks || []).filter(t => t.assignee === payload.user);
                mqttClient.publish(`device/${deviceId}/tasks`, JSON.stringify(userTasks), { retain: true });
            } else {
                delete db.active_devices[deviceId];
                // Clear tasks from the device screen
                mqttClient.publish(`device/${deviceId}/tasks`, JSON.stringify([]), { retain: true });
            }
            saveData();
            
            // Broadcast UI update
            wss.clients.forEach(c => {
                if (c.readyState === WebSocket.OPEN) {
                    c.send(JSON.stringify({ type: 'UPDATE_DEVICES', data: db.active_devices }));
                }
            });
        }
    } catch (e) {
        console.error('Error parsing MQTT payload on topic ' + topic, e);
    }
});

// Start Server
server.listen(HTTP_PORT, () => {
    console.log(`[HTTP/WS] Server and Dashboard running on http://localhost:${HTTP_PORT}`);
});
