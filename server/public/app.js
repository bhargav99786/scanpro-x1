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

    // Format Timestamp
    function formatTime(ts) {
        const date = new Date(ts);
        return date.toLocaleTimeString([], { hour12: false, hour: '2-digit', minute: '2-digit', second:'2-digit' }) + 
               '.' + date.getMilliseconds().toString().padStart(3, '0');
    }

    // State
    let totalScans = 0;
    let cachedInventory = [];
    let cachedUsers = [];
    let cachedTasks = [];

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
            renderUsers(message.data);
        } else if (message.type === 'UPDATE_INVENTORY') {
            renderInventory(message.data);
        }
    };

    function renderDevices(devicesObj) {
        const grid = document.getElementById('devices-grid');
        grid.innerHTML = '';
        Object.entries(devicesObj).forEach(([deviceId, info]) => {
            const card = document.createElement('div');
            card.className = 'stat-card highlight';
            card.innerHTML = `
                <h3 class="stat-title" style="color: var(--accent-cyan)">🟢 Online</h3>
                <div class="stat-value mono" style="font-size: 1.2rem; margin-bottom: 8px">${deviceId}</div>
                <div style="color: var(--text-secondary); font-size: 0.9rem">User: <strong style="color: white">${info.user}</strong></div>
            `;
            grid.appendChild(card);
        });
    }

    const taskTableBody = document.getElementById('task-table-body');
    if (taskTableBody) {
        taskTableBody.addEventListener('click', (e) => {
            const btn = e.target.closest('.delete-task-btn');
            if (btn) {
                const taskId = btn.getAttribute('data-id');
                fetch(`/api/tasks/${taskId}`, { method: 'DELETE' })
                    .then(r => r.json())
                    .then(data => {
                        if (!data.success) alert('Failed to delete task');
                    })
                    .catch(err => console.error('Delete error:', err));
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
            tr.innerHTML = `
                <td class="mono">${task.assignee || task.device_id || 'Unassigned'}</td>
                <td style="font-weight: 600">${task.name}</td>
                <td style="color: var(--text-secondary)">${task.sub}</td>
                <td><span class="badge" style="background: var(--accent-purple, #9d4edd); color: white">${task.items ? task.items.length : 0} items</span></td>
                <td><span class="badge" style="background: var(--accent-blue); color: white">${task.prio}</span></td>
                <td>
                    <button class="edit-task-btn" data-id="${task.id}" style="background: rgba(0,200,255,0.15); border: 1px solid var(--accent-cyan, #00c8ff); color: #00c8ff; cursor: pointer; border-radius: 4px; padding: 4px 10px; font-weight: 600; margin-right: 6px;">Modify</button>
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
    }

    function renderUsers(usersArr) {
        cachedUsers = usersArr;
        populateAssigneeDropdown();
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
                document.getElementById('task-sub').value = task.sub;
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
        const sub = document.getElementById('task-sub').value;
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
            body: JSON.stringify({ assignee, name, sub, prio, items })
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
});
