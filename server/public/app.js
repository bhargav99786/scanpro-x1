document.addEventListener('DOMContentLoaded', () => {
    // Navigation Logic
    const navItems = document.querySelectorAll('.nav-item');
    const views = document.querySelectorAll('.view');

    navItems.forEach(item => {
        item.addEventListener('click', () => {
            // Update active nav
            navItems.forEach(n => n.classList.remove('active'));
            item.classList.add('active');

            // Update active view
            const targetId = item.getAttribute('data-target');
            views.forEach(v => {
                if (v.id === targetId) {
                    v.classList.add('active');
                } else {
                    v.classList.remove('active');
                }
            });
        });
    });

    let cachedTasks = [];
    let cachedUsers = [];
    let cachedInventory = [];
    let cachedDevices = {};
    let totalScans = 0;

    // Format Timestamp
    function formatTime(ts) {
        const date = new Date(ts);
        return date.toLocaleTimeString([], { hour12: false, hour: '2-digit', minute: '2-digit', second:'2-digit' }) + 
               '.' + date.getMilliseconds().toString().padStart(3, '0');
    }

    // WebSocket Connection
    const ws = new WebSocket(`ws://${window.location.host}`);
    const statusIndicator = document.querySelector('.status-indicator');
    const wsStatusText = document.getElementById('ws-status');

    ws.onopen = () => {
        statusIndicator.classList.add('connected');
        wsStatusText.textContent = 'Live Feed Active';
    };

    ws.onclose = () => {
        statusIndicator.classList.remove('connected');
        wsStatusText.textContent = 'Disconnected';
    };

    ws.onmessage = (event) => {
        const message = JSON.parse(event.data);
        if (message.type === 'NEW_SCAN') {
            addScanToTable(message.data, true);
            updateStats(message.data);
        } else if (message.type === 'UPDATE_DEVICES') {
            renderDevices(message.data);
        } else if (message.type === 'UPDATE_TASKS') {
            renderTasks(message.data);
        } else if (message.type === 'UPDATE_USERS') {
            fetch('/api/users').then(r => r.json()).then(renderUsers);
            if (typeof loadAttendance === 'function') loadAttendance();
        } else if (message.type === 'UPDATE_INVENTORY') {
            renderInventory(message.data);
        }
    };

    function renderDevices(devicesObj) {
        cachedDevices = devicesObj;
        const grid = document.getElementById('devices-grid');
        if (!grid) return;
        grid.innerHTML = '';
        
        const targetSelect = document.getElementById('intercom-target-device');
        if (targetSelect) {
            const currentVal = targetSelect.value;
            targetSelect.innerHTML = '<option value="all">Broadcast (All Devices)</option>';
            Object.keys(devicesObj).forEach(dId => {
                const rawUser = devicesObj[dId].user;
                const devUser = (rawUser && rawUser !== 'Unassigned' && rawUser !== 'No Login') ? rawUser : 'No Login';
                targetSelect.innerHTML += `<option value="${dId}">${dId} (${devUser})</option>`;
            });
            if (Array.from(targetSelect.options).some(opt => opt.value === currentVal)) {
                targetSelect.value = currentVal;
            }
        }
        
        Object.entries(devicesObj).forEach(([deviceId, info]) => {
            const card = document.createElement('div');
            card.className = 'stat-card highlight';
            card.style.position = 'relative';
            const userDisplay = (info.user && info.user !== 'Unassigned' && info.user !== 'No Login')
                ? `<strong style="color: white">${info.user}</strong>`
                : `<span style="color: var(--accent-saffron); font-weight: 600; background: rgba(255,170,0,0.15); padding: 2px 8px; border-radius: 4px;">No Login</span>`;

            card.innerHTML = `
                <h3 class="stat-title" style="color: var(--accent-cyan)">🟢 Online</h3>
                <div class="stat-value mono" style="font-size: 1.2rem; margin-bottom: 8px">${deviceId}</div>
                <div style="color: var(--text-secondary); font-size: 0.9rem; margin-bottom: 12px">User: ${userDisplay}</div>
                <button class="talk-to-device-btn btn-primary" data-device="${deviceId}" style="width: 100%; padding: 6px 12px; font-size: 0.85rem; background: var(--accent-cyan); color: #040812; font-weight: 700; border-radius: 6px; border: none; cursor: pointer; display: flex; align-items: center; justify-content: center; gap: 6px;">
                    <svg viewBox="0 0 24 24" width="16" height="16" stroke="currentColor" stroke-width="2" fill="none"><path d="M12 2a3 3 0 0 0-3 3v7a3 3 0 0 0 6 0V5a3 3 0 0 0-3-3Z"></path><path d="M19 10v2a7 7 0 0 1-14 0v-2"></path><line x1="12" y1="19" x2="12" y2="23"></line></svg>
                    Talk to Device
                </button>
            `;
            grid.appendChild(card);
        });
        
        populateAssigneeDropdown(); // Update task assignment dropdown with latest devices
    }

    // Devices Grid Talk Button Listener
    const devicesGridEl = document.getElementById('devices-grid');
    if (devicesGridEl) {
        devicesGridEl.addEventListener('click', (e) => {
            const btn = e.target.closest('.talk-to-device-btn');
            if (btn) {
                const devId = btn.getAttribute('data-device');
                const targetSelect = document.getElementById('intercom-target-device');
                if (targetSelect) {
                    targetSelect.value = devId;
                    connectAudioWs(devId);
                }
                const helpBox = document.getElementById('help-box');
                if (helpBox) helpBox.classList.remove('hidden');
            }
        });
    }

    function renderTasks(tasksArr) {
        cachedTasks = tasksArr;
        const tbody = document.getElementById('task-table-body');
        if (!tbody) return;
        tbody.innerHTML = '';
        [...tasksArr].reverse().forEach(task => {
            const tr = document.createElement('tr');
            
            const isComplete = task.status === 'complete';
            const statusHtml = isComplete 
                ? '<span class="badge" style="background: var(--accent-success, #00e676); color: white">Completed</span>'
                : `<span class="badge" style="background: var(--accent-blue); color: white">${task.prio}</span>`;
                
            tr.innerHTML = `
                <td class="mono">${task.assignee || task.device_id || 'Unassigned'}</td>
                <td style="font-weight: 600; ${isComplete ? 'text-decoration: line-through; opacity: 0.6;' : ''}">${task.name}</td>
                <td><span class="badge" style="background: var(--accent-purple, #9d4edd); color: white">${task.items ? task.items.length : 0} items</span></td>
                <td>${statusHtml}</td>
                <td>
                    <button class="edit-task-btn" data-id="${task.id}" style="background: rgba(0,200,255,0.15); border: 1px solid var(--accent-cyan, #00c8ff); color: #00c8ff; cursor: pointer; border-radius: 4px; padding: 4px 10px; font-weight: 600; margin-right: 6px;" ${isComplete ? 'disabled style="opacity: 0.5; cursor: not-allowed;"' : ''}>Modify</button>
                    <button class="delete-task-btn" data-id="${task.id}" style="background: rgba(255,51,85,0.15); border: 1px solid var(--accent-danger, #ff3355); color: #ff3355; cursor: pointer; border-radius: 4px; padding: 4px 10px; font-weight: 600;">Delete</button>
                </td>
            `;
            tbody.appendChild(tr);
        });
    }

    function populateAssigneeDropdown() {
        const select = document.getElementById('task-assignee');
        if (!select) return;
        select.innerHTML = '<option value="">-- Select User --</option>';
        cachedUsers.forEach(u => {
            select.innerHTML += `<option value="${u.name}">${u.name} (${u.role})</option>`;
        });
        
        select.innerHTML += '<option disabled>──────────</option>';
        select.innerHTML += '<option disabled>-- Active Devices --</option>';
        
        for (const devId in cachedDevices) {
            const devUser = cachedDevices[devId].user;
            select.innerHTML += `<option value="${devId}">${devId} (${devUser})</option>`;
        }
    }

    function formatDuration(ms) {
        if (!ms || ms < 0) return '0m';
        const totalMinutes = Math.floor(ms / 60000);
        const hours = Math.floor(totalMinutes / 60);
        const minutes = totalMinutes % 60;
        if (hours > 0) return `${hours}h ${minutes}m`;
        return `${minutes}m`;
    }

    let activeSessionInterval = null;

    function renderUsers(usersArr) {
        cachedUsers = usersArr;
        populateAssigneeDropdown();
        
        // Populate Attendance User Filter if it exists
        const attFilterUser = document.getElementById('att-filter-user');
        if (attFilterUser) {
            const currentVal = attFilterUser.value;
            attFilterUser.innerHTML = '<option value="all">All Users</option>';
            usersArr.forEach(u => {
                attFilterUser.innerHTML += `<option value="${u.name}">${u.name}</option>`;
            });
            attFilterUser.value = currentVal;
        }

        const tbody = document.getElementById('user-table-body');
        if (!tbody) return;
        tbody.innerHTML = '';
        usersArr.forEach(u => {
            const tr = document.createElement('tr');
            tr.innerHTML = `
                <td class="mono" style="font-weight: 600">${u.id}</td>
                <td>${u.name}</td>
                <td><span class="badge" style="background: var(--accent-cyan); color: white">${u.role || 'Operator'}</span></td>
                <td class="mono">${u.pin || '1234'}</td>
                <td>
                    <button class="edit-user-btn" data-id="${u.id}" data-name="${u.name}" data-role="${u.role || 'Operator'}" data-pin="${u.pin || '1234'}" style="background: rgba(0,200,255,0.15); border: 1px solid var(--accent-cyan, #00c8ff); color: #00c8ff; cursor: pointer; border-radius: 4px; padding: 4px 10px; font-weight: 600; margin-right: 6px;">Modify</button>
                    <button class="delete-user-btn" data-id="${u.id}" style="background: rgba(255,51,85,0.15); border: 1px solid var(--accent-danger, #ff3355); color: #ff3355; cursor: pointer; border-radius: 4px; padding: 4px 10px; font-weight: 600;">Delete</button>
                </td>
            `;
            tbody.appendChild(tr);
        });
    }

    const userTableBody = document.getElementById('user-table-body');
    if (userTableBody) {
        userTableBody.addEventListener('click', (e) => {
            const deleteBtn = e.target.closest('.delete-user-btn');
            if (deleteBtn) {
                const userId = deleteBtn.getAttribute('data-id');
                fetch(`/api/users/${userId}`, { method: 'DELETE' })
                    .then(r => r.json())
                    .then(data => {
                        if (!data.success) alert('Failed to delete user');
                    });
                return;
            }

            const editBtn = e.target.closest('.edit-user-btn');
            if (editBtn) {
                document.getElementById('user-id').value = editBtn.getAttribute('data-id');
                document.getElementById('user-name').value = editBtn.getAttribute('data-name');
                document.getElementById('user-role').value = editBtn.getAttribute('data-role');
                document.getElementById('user-pin').value = editBtn.getAttribute('data-pin');
                
                const title = document.getElementById('user-form-title');
                if (title) title.textContent = `Modify User (${editBtn.getAttribute('data-id')})`;
                const btn = document.getElementById('user-submit-btn');
                if (btn) btn.textContent = 'Update User';
            }
        });
    }

    const userResetBtn = document.getElementById('user-form-reset');
    if (userResetBtn) {
        userResetBtn.addEventListener('click', () => {
            document.getElementById('user-form').reset();
            const title = document.getElementById('user-form-title');
            if (title) title.textContent = 'Create New User';
            const btn = document.getElementById('user-submit-btn');
            if (btn) btn.textContent = 'Save User';
        });
    }

    // Fetch initial devices, tasks, users
    fetch('/api/devices').then(r => r.json()).then(renderDevices);
    fetch('/api/tasks').then(r => r.json()).then(renderTasks);
    fetch('/api/users').then(r => r.json()).then(renderUsers);

    // ==========================================
    // ATTENDANCE & LOGS LOGIC
    // ==========================================
    let cachedAttendance = { history: [], active: [] };
    let attendanceInterval = null;

    function loadAttendance() {
        fetch('/api/attendance')
            .then(r => r.json())
            .then(data => {
                cachedAttendance = data;
                renderAttendance();
            });
    }

    function renderAttendance() {
        const tbody = document.getElementById('attendance-table-body');
        if (!tbody) return;
        
        const filterPeriod = document.getElementById('att-filter-period').value;
        const filterUser = document.getElementById('att-filter-user').value;
        
        const now = new Date();
        let cutoffTime = 0;
        if (filterPeriod === 'today') {
            cutoffTime = new Date(now.getFullYear(), now.getMonth(), now.getDate()).getTime();
        } else if (filterPeriod === 'week') {
            const day = now.getDay() || 7; // Get current day number, converting Sun. to 7
            if(day !== 1) now.setHours(-24 * (day - 1)); // Set to previous Monday
            cutoffTime = new Date(now.getFullYear(), now.getMonth(), now.getDate()).getTime();
        } else if (filterPeriod === 'month') {
            cutoffTime = new Date(now.getFullYear(), now.getMonth(), 1).getTime();
        }
        
        // Combine history and active
        let allLogs = [...cachedAttendance.active, ...cachedAttendance.history];
        
        // Sort descending by loginTime
        allLogs.sort((a, b) => b.loginTime - a.loginTime);
        
        let filteredLogs = allLogs.filter(log => {
            if (filterUser !== 'all' && log.userName !== filterUser) return false;
            if (filterPeriod !== 'all' && log.loginTime < cutoffTime) return false;
            return true;
        });

        let totalMs = 0;
        let hasActive = false;
        
        // Group logs by Date and User
        const groupedLogs = {};
        filteredLogs.forEach(log => {
            const dateStr = new Date(log.loginTime).toLocaleDateString();
            const key = dateStr + '_' + log.userName;
            
            let duration = log.durationMs;
            let isActive = false;
            if (log.status === 'Active') {
                duration = Date.now() - log.loginTime;
                isActive = true;
                hasActive = true;
            }
            totalMs += duration;
            
            if (!groupedLogs[key]) {
                groupedLogs[key] = {
                    dateStr: dateStr,
                    userName: log.userName,
                    sessions: 0,
                    totalDuration: 0,
                    isActive: false,
                    latestLogin: log.loginTime
                };
            }
            
            groupedLogs[key].sessions++;
            groupedLogs[key].totalDuration += duration;
            if (isActive) groupedLogs[key].isActive = true;
            if (log.loginTime > groupedLogs[key].latestLogin) {
                groupedLogs[key].latestLogin = log.loginTime;
            }
        });
        
        // Apply Overrides
        const overrides = cachedAttendance.overrides || {};
        Object.keys(groupedLogs).forEach(key => {
            if (overrides[key] !== undefined) {
                groupedLogs[key].totalDuration = overrides[key];
                groupedLogs[key].isAdjusted = true;
            }
        });
        
        // Convert back to array and sort by latest login
        const summaryRows = Object.values(groupedLogs).sort((a, b) => b.latestLogin - a.latestLogin);
        
        tbody.innerHTML = '';
        summaryRows.forEach(summary => {
            const tr = document.createElement('tr');
            
            let activeIndicator = '';
            let statusBadge = '<span class="badge" style="background: var(--bg-dark); color: var(--text-secondary); border: 1px solid var(--border-color);">Completed</span>';
            let durationText = formatDuration(summary.totalDuration);
            
            if (summary.isAdjusted) {
                durationText += ' <span style="font-size: 0.7em; color: var(--text-secondary);">(Adjusted)</span>';
            }
            
            if (summary.isActive) {
                activeIndicator = `<span class="status-indicator connected" style="display:inline-block; margin-right:6px; width:8px; height:8px; animation: pulse 2s infinite;" title="Online"></span>`;
                statusBadge = '<span class="badge" style="background: rgba(0, 212, 255, 0.15); color: var(--accent-cyan); border: 1px solid var(--accent-cyan);">Active</span>';
            }
            
            tr.innerHTML = `
                <td>${summary.dateStr}</td>
                <td>${activeIndicator}${summary.userName}</td>
                <td class="mono">${summary.sessions}</td>
                <td class="mono" style="color: var(--accent-saffron); font-weight: 600;">${durationText}</td>
                <td>${statusBadge}</td>
                <td>
                    <button class="adjust-att-btn" data-key="${summary.dateStr}_${summary.userName}" style="background: rgba(0,200,255,0.15); border: 1px solid var(--accent-cyan, #00c8ff); color: #00c8ff; cursor: pointer; border-radius: 4px; padding: 4px 10px; font-weight: 600;">Adjust</button>
                </td>
            `;
            tbody.appendChild(tr);
        });
        
        const totalSessionsEl = document.getElementById('att-total-sessions');
        const totalHoursEl = document.getElementById('att-total-hours');
        if (totalSessionsEl) totalSessionsEl.textContent = filteredLogs.length;
        if (totalHoursEl) totalHoursEl.textContent = formatDuration(totalMs);
        
        if (attendanceInterval) clearInterval(attendanceInterval);
        if (hasActive) {
            attendanceInterval = setInterval(renderAttendance, 60000);
        }
    }

    const attFilterPeriod = document.getElementById('att-filter-period');
    const attFilterUser = document.getElementById('att-filter-user');
    if (attFilterPeriod) attFilterPeriod.addEventListener('change', renderAttendance);
    if (attFilterUser) attFilterUser.addEventListener('change', renderAttendance);
    
    // Attendance Adjustment Listener
    const attendanceTableBody = document.getElementById('attendance-table-body');
    if (attendanceTableBody) {
        attendanceTableBody.addEventListener('click', (e) => {
            const adjustBtn = e.target.closest('.adjust-att-btn');
            if (adjustBtn) {
                const key = adjustBtn.getAttribute('data-key');
                const parts = key.split('_');
                const newMins = prompt(`Enter new total worked time in minutes for ${parts[1]} on ${parts[0]}:`);
                if (newMins !== null && !isNaN(newMins) && newMins !== '') {
                    const durationMs = parseInt(newMins, 10) * 60000;
                    fetch('/api/attendance/override', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify({ key, durationMs })
                    })
                    .then(r => r.json())
                    .then(data => {
                        if (data.success) {
                            loadAttendance(); // reload data to show adjustment
                        } else {
                            alert('Failed to adjust attendance');
                        }
                    });
                }
            }
        });
    }

    loadAttendance();

    // Fetch initial scans
    fetch('/api/scans')
        .then(res => res.json())
        .then(data => {
            totalScans = data.length;
            document.getElementById('stat-total').textContent = totalScans;
            if (data.length > 0) {
                document.getElementById('stat-last-sku').textContent = data[0].sku;
                // Since data is ordered DESC, we can just append them
                data.forEach(scan => addScanToTable(scan, false));
            }
        });

    // Fetch initial inventory
    function loadInventory() {
        fetch('/api/inventory')
            .then(res => res.json())
            .then(renderInventory);
    }
    loadInventory();

    function renderInventory(data) {
        cachedInventory = data;
        const tbody = document.getElementById('inventory-table-body');
        if (!tbody) return;
        tbody.innerHTML = '';
        data.forEach(item => {
            const tr = document.createElement('tr');
            tr.innerHTML = `
                <td class="mono" style="font-weight: 600">${item.sku}</td>
                <td>${item.name}</td>
                <td><span class="badge" style="background: var(--accent-cyan); color: white">${item.qty}</span></td>
                <td>
                    <button class="edit-inv-btn" data-sku="${item.sku}" data-name="${item.name}" data-qty="${item.qty}" style="background: rgba(0,200,255,0.15); border: 1px solid var(--accent-cyan, #00c8ff); color: #00c8ff; cursor: pointer; border-radius: 4px; padding: 4px 10px; font-weight: 600; margin-right: 6px;">Modify</button>
                    <button class="delete-inv-btn" data-sku="${item.sku}" style="background: rgba(255,51,85,0.15); border: 1px solid var(--accent-danger, #ff3355); color: #ff3355; cursor: pointer; border-radius: 4px; padding: 4px 10px; font-weight: 600;">Delete</button>
                </td>
            `;
            tbody.appendChild(tr);
        });
    }

    const inventoryTableBody = document.getElementById('inventory-table-body');
    if (inventoryTableBody) {
        inventoryTableBody.addEventListener('click', (e) => {
            const deleteBtn = e.target.closest('.delete-inv-btn');
            if (deleteBtn) {
                const sku = deleteBtn.getAttribute('data-sku');
                fetch(`/api/inventory/${sku}`, { method: 'DELETE' })
                    .then(r => r.json())
                    .then(data => {
                        if (!data.success) alert('Failed to delete item');
                    });
                return;
            }

            const editBtn = e.target.closest('.edit-inv-btn');
            if (editBtn) {
                document.getElementById('sku').value = editBtn.getAttribute('data-sku');
                document.getElementById('name').value = editBtn.getAttribute('data-name');
                document.getElementById('qty').value = editBtn.getAttribute('data-qty');
                
                const btn = document.querySelector('#inventory-form button[type="submit"]');
                if (btn) btn.textContent = 'Update Product';
            }
        });
    }

    function addScanToTable(scan, animate) {
        const tbody = document.getElementById('scan-table-body');
        const tr = document.createElement('tr');
        if (animate) tr.classList.add('new-row');
        
        tr.innerHTML = `
            <td class="mono">${formatTime(scan.ts)}</td>
            <td class="mono">${scan.device_id}</td>
            <td class="mono" style="color: var(--accent-cyan); font-weight: 600;">${scan.sku}</td>
            <td>${scan.product_name || 'Unknown Product'}</td>
            <td class="mono" style="color: var(--text-secondary); font-size: 0.8rem;">${scan.uuid}</td>
        `;
        
        tbody.insertBefore(tr, tbody.firstChild);

        // Keep table size manageable
        if (tbody.children.length > 100) {
            tbody.removeChild(tbody.lastChild);
        }
    }

    function updateStats(scan) {
        totalScans++;
        document.getElementById('stat-total').textContent = totalScans;
        document.getElementById('stat-last-sku').textContent = scan.sku;
    }

    // Inventory Form Submit
    document.getElementById('inventory-form').addEventListener('submit', (e) => {
        e.preventDefault();
        const sku = document.getElementById('sku').value;
        const name = document.getElementById('name').value;
        const qty = parseInt(document.getElementById('qty').value) || 0;

        fetch('/api/inventory', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ sku, name, qty })
        })
        .then(res => res.json())
        .then(data => {
            if (data.success) {
                // Reset form
                document.getElementById('inventory-form').reset();
                
                // Show a quick success visual
                const btn = e.target.querySelector('button');
                const origText = btn.textContent;
                btn.textContent = 'Saved!';
                btn.style.background = 'var(--accent-saffron)';
                setTimeout(() => {
                    btn.textContent = 'Save Product';
                    btn.style.background = '';
                }, 1500);
            }
        });
    });

    // Task Items Logic
    const addTaskItemBtn = document.getElementById('add-task-item-btn');
    const taskItemsContainer = document.getElementById('task-items-container');
    
    if (addTaskItemBtn && taskItemsContainer) {
        addTaskItemBtn.addEventListener('click', () => {
            const row = document.createElement('div');
            row.className = 'task-item-row';
            row.style.display = 'flex';
            row.style.gap = '8px';
            
            let options = '<option value="">-- Select Item --</option>';
            cachedInventory.forEach(inv => {
                options += `<option value="${inv.sku}">${inv.name} (${inv.sku})</option>`;
            });
            
            row.innerHTML = `
                <select class="item-sku" style="flex: 1; padding: 6px; border-radius: 4px; border: 1px solid var(--border-color); background: rgba(0,0,0,0.2); color: white;" required>
                    ${options}
                </select>
                <div style="display:flex; flex-direction:column; gap:2px;">
                    <input type="number" class="item-qty" placeholder="Qty" min="1" value="1" style="width: 80px; padding: 6px; border-radius: 4px; border: 1px solid var(--border-color); background: rgba(0,0,0,0.2); color: white;" required>
                    <span class="qty-hint" style="font-size: 10px; color: var(--text-secondary);"></span>
                </div>
                <button type="button" class="remove-item-btn" style="background: rgba(255,51,85,0.15); border: 1px solid var(--accent-danger); color: var(--accent-danger); cursor: pointer; border-radius: 4px; padding: 4px 8px;">X</button>
            `;
            
            const skuSelect = row.querySelector('.item-sku');
            const qtyInput = row.querySelector('.item-qty');
            const qtyHint = row.querySelector('.qty-hint');
            
            skuSelect.addEventListener('change', () => {
                const selectedSku = skuSelect.value;
                const invItem = cachedInventory.find(i => i.sku === selectedSku);
                if (invItem) {
                    qtyInput.max = invItem.qty;
                    qtyHint.textContent = `Max: ${invItem.qty}`;
                    if (parseInt(qtyInput.value) > parseInt(invItem.qty)) {
                        qtyInput.value = invItem.qty;
                    }
                } else {
                    qtyInput.removeAttribute('max');
                    qtyHint.textContent = '';
                }
            });
            
            row.querySelector('.remove-item-btn').addEventListener('click', () => row.remove());
            taskItemsContainer.appendChild(row);
        });
    }

    const taskResetBtn = document.getElementById('task-form-reset');
    
    function resetTaskForm() {
        document.getElementById('task-form').reset();
        document.getElementById('task-id').value = '';
        document.getElementById('task-items-container').innerHTML = '';
        const btn = document.getElementById('task-submit-btn');
        if (btn) btn.textContent = 'Assign Task';
        if (taskResetBtn) taskResetBtn.style.display = 'none';
    }
    if (taskResetBtn) taskResetBtn.addEventListener('click', resetTaskForm);

    const taskTableBody = document.getElementById('task-table-body');
    if (taskTableBody) {
        taskTableBody.addEventListener('click', (e) => {
            const deleteBtn = e.target.closest('.delete-task-btn');
            if (deleteBtn) {
                const taskId = deleteBtn.getAttribute('data-id');
                fetch(`/api/tasks/${taskId}`, { method: 'DELETE' })
                    .then(r => r.json())
                    .then(data => {
                        if (!data.success) alert('Failed to delete task');
                    });
                return;
            }

            const editBtn = e.target.closest('.edit-task-btn');
            if (editBtn) {
                const taskId = editBtn.getAttribute('data-id');
                const task = cachedTasks.find(t => t.id === taskId);
                if (!task) return;
                
                document.getElementById('task-id').value = task.id;
                document.getElementById('task-assignee').value = task.assignee || task.device_id || '';
                document.getElementById('task-name').value = task.name;
                document.getElementById('task-prio').value = task.prio;
                
                // Clear items
                taskItemsContainer.innerHTML = '';
                
                if (task.items) {
                    task.items.forEach(item => {
                        addTaskItemBtn.click();
                        const newRow = taskItemsContainer.lastElementChild;
                        const skuSelect = newRow.querySelector('.item-sku');
                        skuSelect.value = item.sku;
                        skuSelect.dispatchEvent(new Event('change'));
                        
                        const qtyInput = newRow.querySelector('.item-qty');
                        qtyInput.value = item.target_qty;
                    });
                }
                
                const btn = document.getElementById('task-submit-btn');
                if (btn) btn.textContent = 'Update Task';
                if (taskResetBtn) taskResetBtn.style.display = 'block';
            }
        });
    }

    // Task Form Submit
    document.getElementById('task-form').addEventListener('submit', (e) => {
        e.preventDefault();
        const taskId = document.getElementById('task-id').value;
        const assignee = document.getElementById('task-assignee').value;
        const name = document.getElementById('task-name').value;
        const prio = document.getElementById('task-prio').value;

        const items = [];
        const itemRows = document.querySelectorAll('.task-item-row');
        itemRows.forEach(row => {
            const sku = row.querySelector('.item-sku').value;
            const qty = parseInt(row.querySelector('.item-qty').value) || 1;
            const invItem = cachedInventory.find(i => i.sku === sku);
            if (sku && invItem) {
                items.push({ sku, name: invItem.name, target_qty: qty, picked_qty: 0 });
            }
        });

        const url = taskId ? `/api/tasks/${taskId}` : '/api/tasks';
        const method = taskId ? 'PUT' : 'POST';

        fetch(url, {
            method: method,
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ assignee, name, prio, items })
        })
        .then(res => res.json())
        .then(data => {
            if (data.success) {
                resetTaskForm();
                const btn = document.getElementById('task-submit-btn');
                const origText = btn.textContent;
                btn.textContent = taskId ? 'Updated!' : 'Assigned!';
                btn.style.background = 'var(--accent-saffron)';
                setTimeout(() => {
                    btn.textContent = origText;
                    btn.style.background = '';
                }, 1500);
            }
        });
    });

    // User Form Submit
    const userForm = document.getElementById('user-form');
    if (userForm) {
        userForm.addEventListener('submit', (e) => {
            e.preventDefault();
            const id = document.getElementById('user-id').value;
            const name = document.getElementById('user-name').value;
            const role = document.getElementById('user-role').value;
            const pin = document.getElementById('user-pin').value;

            fetch('/api/users', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ id, name, role, pin })
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) {
                    const btn = e.target.querySelector('button[type="submit"]');
                    const origText = btn.textContent;
                    btn.textContent = 'Saved!';
                    btn.style.background = 'var(--accent-saffron)';
                    setTimeout(() => {
                        btn.textContent = 'Save User';
                        btn.style.background = '';
                        const title = document.getElementById('user-form-title');
                        if (title) title.textContent = 'Create New User';
                    }, 1500);
                }
            });
        });
    }
    // ==========================================
    // INTERCOM (WALKIE-TALKIE) LOGIC
    // ==========================================
    const pttBtn = document.getElementById('ptt-btn');
    const intercomStatus = document.getElementById('intercom-status-text');
    const intercomIndicator = document.getElementById('intercom-status-indicator');
    const helpTag = document.getElementById('help-tag');
    const helpBox = document.getElementById('help-box');
    const closeHelpBox = document.getElementById('close-help-box');
    
    if (helpTag) {
        helpTag.addEventListener('click', () => {
            helpBox.classList.remove('hidden');
            helpTag.classList.add('hidden');
            if (audioContext && audioContext.state === 'suspended') {
                audioContext.resume();
            }
        });
    }
    
    if (closeHelpBox) {
        closeHelpBox.addEventListener('click', () => {
            helpBox.classList.add('hidden');
            helpTag.classList.remove('hidden');
        });
    }
    
    let audioWs = null;
    let audioContext = null;
    let mediaStream = null;
    let microphoneNode = null;
    let scriptProcessor = null;
    let isRecording = false;

    // Connect to dedicated audio websock
    function connectAudioWs(targetDevId) {
        const wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const target = targetDevId || document.getElementById('intercom-target-device')?.value || 'all';
        if (audioWs) {
            audioWs.onclose = null; // Prevent reconnect loop during explicit switch
            audioWs.close();
        }
        audioWs = new WebSocket(`${wsProtocol}//${window.location.host}/audio?client_type=pc&target_device=${encodeURIComponent(target)}`);
        audioWs.binaryType = 'arraybuffer';
        
        audioWs.onopen = () => {
            const devLabel = target === 'all' ? 'All Devices' : target;
            intercomStatus.textContent = `Ready: ${devLabel}`;
            intercomIndicator.style.background = 'var(--accent-cyan)';
            intercomIndicator.style.boxShadow = 'var(--glow-cyan)';
            if (pttBtn) pttBtn.disabled = false;
        };
        
        audioWs.onclose = () => {
            intercomStatus.textContent = "Reconnecting audio...";
            intercomIndicator.style.background = 'var(--accent-saffron)';
            intercomIndicator.style.boxShadow = 'none';
            if (pttBtn) pttBtn.disabled = false; // Always keep button clickable for mic setup
            setTimeout(connectAudioWs, 3000);
        };
        
        let nextPlayTime = 0;
        
        // Browsers block audio until a user interaction occurs.
        document.addEventListener('click', () => {
            if (audioContext && audioContext.state === 'suspended') {
                audioContext.resume().then(() => {
                    console.log("[AUDIO] Context resumed by user click");
                });
            }
        }, { once: true });
        
        // ESP32 Mic → PC Speaker: receive raw Mono 16-bit PCM at 16kHz and play it.
        audioWs.onmessage = async (event) => {
            if (!audioContext) {
                audioContext = new (window.AudioContext || window.webkitAudioContext)({ sampleRate: 16000 });
            }
            if (audioContext.state === 'suspended') {
                try {
                    await audioContext.resume();
                } catch (e) {
                    intercomStatus.textContent = "Click Page to Unmute!";
                    intercomStatus.style.color = "var(--accent-saffron)";
                    return;
                }
            }

            // Flash indicator: ESP32 mic frame arriving
            intercomIndicator.style.background = 'var(--accent-cyan)';
            setTimeout(() => { intercomIndicator.style.background = 'var(--text-secondary)'; }, 60);
            intercomStatus.textContent = "Receiving from ESP32...";
            intercomStatus.style.color = "var(--text-primary)";

            // Decode: raw Mono 16-bit little-endian PCM → Web Audio float32
            const monoBuffer = new Int16Array(event.data);
            const frameLength = monoBuffer.length;
            const audioBuffer = audioContext.createBuffer(1, frameLength, 16000);
            const channelData = audioBuffer.getChannelData(0);
            for (let i = 0; i < frameLength; i++) {
                channelData[i] = monoBuffer[i] / 32768.0;
            }

            const source = audioContext.createBufferSource();
            source.buffer = audioBuffer;
            source.connect(audioContext.destination);

            // Jitter buffer: 120ms head start to absorb Wi-Fi fluctuations
            if (nextPlayTime < audioContext.currentTime) {
                nextPlayTime = audioContext.currentTime + 0.120;
            }
            source.start(nextPlayTime);
            nextPlayTime += audioBuffer.duration;

            // Reset status label after silence
            clearTimeout(window.intercomResetTimeout);
            window.intercomResetTimeout = setTimeout(() => {
                intercomStatus.textContent = "Listening...";
            }, 600);
        };
    }
    connectAudioWs();

    // Setup microphone
    async function setupMicrophone() {
        if (mediaStream) return true;
        if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) {
            console.error("Microphone API blocked on insecure HTTP context!");
            intercomStatus.textContent = "Mic Blocked (Switch to HTTPS:3031)";
            intercomStatus.style.color = "var(--accent-danger)";
            const httpsUrl = `https://${location.hostname}:3031`;
            if (confirm("Microphone access is BLOCKED by your browser on HTTP IP addresses.\n\nWould you like to open the Secure HTTPS Dashboard on Port 3031 to enable your microphone?")) {
                location.href = httpsUrl;
            }
            return false;
        }
        try {
            mediaStream = await navigator.mediaDevices.getUserMedia({
                audio: {
                    echoCancellation: true,
                    noiseSuppression: true,
                    autoGainControl: true
                }
            });
            if (!audioContext) audioContext = new (window.AudioContext || window.webkitAudioContext)({ sampleRate: 16000 });
            microphoneNode = audioContext.createMediaStreamSource(mediaStream);
            
            scriptProcessor = audioContext.createScriptProcessor(4096, 1, 1);
            microphoneNode.connect(scriptProcessor);
            
            const dummyGain = audioContext.createGain();
            dummyGain.gain.value = 0;
            scriptProcessor.connect(dummyGain);
            dummyGain.connect(audioContext.destination);
            
            scriptProcessor.onaudioprocess = (e) => {
                if (!isRecording || !audioWs || audioWs.readyState !== WebSocket.OPEN) return;
                
                const inputData = e.inputBuffer.getChannelData(0);
                const inputSampleRate = e.inputBuffer.sampleRate || 16000;
                const targetSampleRate = 16000;
                const ratio = inputSampleRate / targetSampleRate;
                const outputLength = Math.floor(inputData.length / ratio);
                const outputBuffer = new Int16Array(outputLength);
                const gain = 1.5;
                
                for (let i = 0; i < outputLength; i++) {
                    const inputIdx = Math.floor(i * ratio);
                    const sample = inputData[inputIdx] * gain;
                    outputBuffer[i] = Math.max(-32768, Math.min(32767, Math.floor(sample * 32768)));
                }
                
                audioWs.send(outputBuffer.buffer);
            };
            return true;
        } catch (err) {
            console.error("Microphone access error:", err);
            intercomStatus.textContent = "Mic Blocked (Switch to HTTPS:3031)";
            intercomStatus.style.color = "var(--accent-danger)";
            
            if (location.protocol === 'http:' && location.hostname !== 'localhost' && location.hostname !== '127.0.0.1') {
                const httpsUrl = `https://${location.hostname}:3031`;
                alert("BROWSER SECURITY POLICY:\n\nChrome & Edge NEVER show microphone popups on http://" + location.hostname + ":3030.\n\nYou MUST use the HTTPS port (3031) for microphone access.\n\nClick OK to switch to https://" + location.hostname + ":3031 now.");
                location.href = httpsUrl;
            } else {
                alert("Microphone permission denied! Please click the camera/mic icon in your browser address bar and select 'Allow'.");
            }
            return false;
        }
    }

    if (pttBtn) {
        let streamingActive = false;

        const startStreaming = async () => {
            // Ensure AudioContext is running (also enables ESP32→PC playback)
            if (!audioContext) {
                audioContext = new (window.AudioContext || window.webkitAudioContext)({ sampleRate: 16000 });
            }
            if (audioContext.state === 'suspended') await audioContext.resume();

            const ok = await setupMicrophone();
            if (!ok) return;

            isRecording = true;
            streamingActive = true;
            pttBtn.textContent = "🔴 Stop Talking";
            pttBtn.classList.add("recording");
            intercomStatus.textContent = "Talking to ESP32...";
        };

        const stopStreaming = () => {
            isRecording = false;
            streamingActive = false;
            pttBtn.textContent = "🎙️ Start Talking";
            pttBtn.classList.remove("recording");
            intercomStatus.textContent = "Ready";
        };

        pttBtn.textContent = "🎙️ Start Talking";

        pttBtn.addEventListener('click', async () => {
            if (streamingActive) stopStreaming();
            else await startStreaming();
        });

        // Touch support
        pttBtn.addEventListener('touchstart', async (e) => {
            e.preventDefault();
            if (streamingActive) stopStreaming();
            else await startStreaming();
        });
    }

    // Check for HTTP IP connection and show warning banner
    const httpsBanner = document.getElementById('https-warning-banner');
    const httpsSwitchLink = document.getElementById('https-switch-link');
    if (location.protocol === 'http:' && location.hostname !== 'localhost' && location.hostname !== '127.0.0.1') {
        if (httpsBanner) httpsBanner.style.display = 'block';
        if (httpsSwitchLink) {
            httpsSwitchLink.href = `https://${location.hostname}:3031`;
        }
    }

    const targetSelectEl = document.getElementById('intercom-target-device');
    if (targetSelectEl) {
        targetSelectEl.addEventListener('change', (e) => {
            const targetDev = e.target.value;
            console.log("[INTERCOM] Switching audio target to:", targetDev);
            connectAudioWs(targetDev);
        });
    }
});
