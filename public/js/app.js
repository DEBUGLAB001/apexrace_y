// ApexRace Motorsport Race Control System — Multi-Page Station Controller
(function() {
  'use strict';

  // Global State Cache
  let currentRaceState = null;
  let selectedDriverId = 'drv_ver';
  let activeInvestigationIncident = null;

  // DOM Elements: Top Strip
  const stripEventName = document.getElementById('strip-event-name');
  const stripCircuit = document.getElementById('strip-circuit');
  const stripSessionType = document.getElementById('strip-session-type');
  const stripLapCounter = document.getElementById('strip-lap-counter');
  const stripSessionClock = document.getElementById('strip-session-clock');
  const stripTrackStatus = document.getElementById('strip-track-status');
  const stripStatusText = document.getElementById('strip-status-text');
  const trackStatusSelect = document.getElementById('track-status-select');
  const btnAdvanceLap = document.getElementById('btn-advance-lap');
  const btnResetSession = document.getElementById('btn-reset-session');

  // Navigation Links & Page Views
  const navLinks = document.querySelectorAll('.nav-link');
  const pageViews = document.querySelectorAll('.page-view');

  // Timing Tower
  const timingTbody = document.getElementById('timing-tbody');
  const driverInspector = document.getElementById('driver-inspector');
  const btnCloseInspector = document.getElementById('btn-close-inspector');
  const inspNumber = document.getElementById('insp-number');
  const inspName = document.getElementById('insp-name');
  const inspTeam = document.getElementById('insp-team');
  const inspPos = document.getElementById('insp-pos');
  const inspLaps = document.getElementById('insp-laps');
  const inspPenalty = document.getElementById('insp-penalty');
  const inspTire = document.getElementById('insp-tire');
  const inspHistoryTbody = document.getElementById('insp-history-tbody');
  const recordLapForm = document.getElementById('record-lap-form');

  // Radio Comms
  const radioFeedList = document.getElementById('radio-feed-list');
  const radioSearchInput = document.getElementById('radio-search-input');
  const radioCategoryFilter = document.getElementById('radio-category-filter');
  const radioDriverFilter = document.getElementById('radio-driver-filter');
  const newRadioForm = document.getElementById('new-radio-form');

  // Pit Strategy (Linked List)
  const pitStrategyBoard = document.getElementById('pit-strategy-board');
  const newPitForm = document.getElementById('new-pit-form');

  // Incident Control (FIFO Queue)
  const btnProcessNextIncident = document.getElementById('btn-process-next-incident');
  const pendingIncidentsContainer = document.getElementById('pending-incidents-container');
  const resolvedIncidentsContainer = document.getElementById('resolved-incidents-container');
  const activeInvestigationBox = document.getElementById('active-investigation-box');
  const newIncidentForm = document.getElementById('new-incident-form');

  // Steward Decisions (Stack Undo)
  const btnUndoDecision = document.getElementById('btn-undo-decision');
  const decisionHistoryFeed = document.getElementById('decision-history-feed');
  const newDecisionForm = document.getElementById('new-decision-form');
  const decDriverSelect = document.getElementById('dec-driver-select');

  // Footer & Toasts
  const footerLogTicker = document.getElementById('footer-log-ticker');
  const toastContainer = document.getElementById('toast-container');

  // ==========================================
  // MULTI-PAGE VIEW ROUTING
  // ==========================================
  function switchView(viewName) {
    const validViews = ['timing', 'radio', 'pit', 'incidents', 'stewards'];
    if (!validViews.includes(viewName)) viewName = 'timing';

    // Update active nav link
    navLinks.forEach(link => {
      if (link.dataset.view === viewName) {
        link.classList.add('active');
      } else {
        link.classList.remove('active');
      }
    });

    // Update active page view
    pageViews.forEach(page => {
      if (page.id === `view-${viewName}`) {
        page.classList.add('active');
      } else {
        page.classList.remove('active');
      }
    });

    // Hash sync
    if (window.location.hash !== `#${viewName}`) {
      window.location.hash = viewName;
    }

    // Refresh context if needed
    if (viewName === 'radio') {
      renderRadioFeed();
    }
  }

  // Handle nav clicks
  navLinks.forEach(link => {
    link.addEventListener('click', (e) => {
      e.preventDefault();
      const targetView = link.dataset.view;
      switchView(targetView);
    });
  });

  // Handle browser back/forward navigation
  window.addEventListener('hashchange', () => {
    const hash = window.location.hash.replace('#', '');
    if (hash) switchView(hash);
  });

  // ==========================================
  // FORMATTING HELPERS
  // ==========================================
  function formatLapTime(sec) {
    if (!sec || sec <= 0) return '--:--.---';
    const m = Math.floor(sec / 60);
    const s = (sec % 60).toFixed(3);
    return `${m}:${s < 10 ? '0' : ''}${s}`;
  }

  function formatClock(seconds) {
    const hrs = Math.floor(seconds / 3600);
    const mins = Math.floor((seconds % 3600) / 60);
    const secs = seconds % 60;
    return `${hrs < 10 ? '0' : ''}${hrs}:${mins < 10 ? '0' : ''}${mins}:${secs < 10 ? '0' : ''}${secs}`;
  }

  function showToast(message, type = 'info') {
    const toast = document.createElement('div');
    toast.className = `toast toast-${type}`;
    toast.textContent = message;
    toastContainer.appendChild(toast);
    setTimeout(() => {
      toast.style.opacity = '0';
      setTimeout(() => toast.remove(), 300);
    }, 3500);
  }

  // ==========================================
  // API FETCH & STATE SYNC
  // ==========================================
  async function fetchRaceState() {
    try {
      const res = await fetch('api/state');
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      currentRaceState = await res.json();
      renderAll();
    } catch (err) {
      console.error('[ApexRace] State fetch failed:', err);
    }
  }

  function renderAll() {
    if (!currentRaceState) return;
    renderTopStrip(currentRaceState.session);
    renderTimingTower(currentRaceState.drivers);
    renderDriverInspector();
    renderRadioFeed();
    renderIncidents(currentRaceState.incidents);
    renderPitStrategy(currentRaceState.pitStrategy);
    renderDecisions(currentRaceState.decisions, currentRaceState.undoStackDepth);
    renderFooterLogs(currentRaceState.session.eventLogs);
    updateDriverSelectDropdowns(currentRaceState.drivers);
  }

  // ==========================================
  // TOP STRIP RENDERING
  // ==========================================
  function renderTopStrip(session) {
    stripEventName.textContent = session.eventName;
    stripCircuit.textContent = `${session.circuit} (${session.circuitLengthKm.toFixed(3)} km)`;
    stripSessionType.textContent = session.sessionType.toUpperCase();
    stripLapCounter.textContent = `${session.currentLap} / ${session.totalLaps}`;
    stripSessionClock.textContent = formatClock(session.sessionClockSeconds);

    stripTrackStatus.className = `track-status-pill status-${session.trackStatus.toLowerCase()}`;
    if (session.trackStatus === 'GREEN') stripStatusText.textContent = 'TRACK CLEAR';
    else if (session.trackStatus === 'YELLOW') stripStatusText.textContent = 'YELLOW SECTOR';
    else if (session.trackStatus === 'SAFETY_CAR') stripStatusText.textContent = 'SAFETY CAR';
    else if (session.trackStatus === 'RED_FLAG') stripStatusText.textContent = 'SESSION SUSPENDED';

    trackStatusSelect.value = session.trackStatus;
  }

  // ==========================================
  // PAGE 1: TIMING TOWER
  // ==========================================
  function renderTimingTower(drivers) {
    timingTbody.innerHTML = '';
    if (!drivers || drivers.length === 0) {
      timingTbody.innerHTML = '<tr><td colspan="13" style="text-align:center; padding: 24px;">No driver timing data available</td></tr>';
      return;
    }

    document.getElementById('nav-badge-drivers').textContent = `${drivers.length} CARS`;

    drivers.forEach(d => {
      const tr = document.createElement('tr');
      tr.className = `timing-row ${d.id === selectedDriverId ? 'selected' : ''}`;
      tr.onclick = () => selectDriver(d.id);

      const posClass = d.position === 1 ? 'pos-p1' : (d.position === 2 ? 'pos-p2' : (d.position === 3 ? 'pos-p3' : ''));
      const gapDisplay = d.position === 1 ? 'LEADER' : (d.gapToLeader > 0 ? `+${d.gapToLeader.toFixed(3)}s` : '--');
      const intDisplay = d.position === 1 ? '--' : (d.intervalToAhead > 0 ? `+${d.intervalToAhead.toFixed(3)}s` : '--');

      const bestLapClass = d.isFastestLap ? 'timing-purple-val' : 'timing-green-val';
      const tireCompoundClass = `tire-${(d.tireCompound || 'medium').toLowerCase()}`;

      tr.innerHTML = `
        <td class="col-pos"><span class="pos-val ${posClass}">P${d.position}</span></td>
        <td class="col-car"><span class="car-badge">#${d.carNumber}</span></td>
        <td class="col-driver">
          <div class="driver-cell">
            <span class="driver-abbr">${d.name.substring(0, 3).toUpperCase()}</span>
            <span class="driver-fullname">${d.name}</span>
            ${d.totalPenaltySeconds > 0 ? `<span class="penalty-tag">+${d.totalPenaltySeconds}s</span>` : ''}
            ${d.isDisqualified ? `<span class="penalty-tag">DSQ</span>` : ''}
          </div>
        </td>
        <td class="col-team"><span class="team-text">${d.team}</span></td>
        <td class="col-gap mono-cell">${gapDisplay}</td>
        <td class="col-int mono-cell">${intDisplay}</td>
        <td class="col-last mono-cell">${formatLapTime(d.lastLapTime)}</td>
        <td class="col-best mono-cell ${bestLapClass}">${formatLapTime(d.bestLapTime)}</td>
        <td class="col-s1 sector-cell">${d.s1Time > 0 ? d.s1Time.toFixed(3) : '--'}</td>
        <td class="col-s2 sector-cell">${d.s2Time > 0 ? d.s2Time.toFixed(3) : '--'}</td>
        <td class="col-s3 sector-cell">${d.s3Time > 0 ? d.s3Time.toFixed(3) : '--'}</td>
        <td class="col-tire">
          <span class="tire-pill ${tireCompoundClass}">${d.tireCompound} (${d.tireAgeLaps}L)</span>
        </td>
        <td class="col-pit">
          <span class="pit-status-text">${d.pitStatus}</span>
        </td>
      `;

      timingTbody.appendChild(tr);
    });
  }

  function selectDriver(driverId) {
    selectedDriverId = driverId;
    renderTimingTower(currentRaceState.drivers);
    renderDriverInspector();
  }

  function renderDriverInspector() {
    if (!currentRaceState || !selectedDriverId) {
      driverInspector.style.display = 'none';
      return;
    }

    const d = currentRaceState.drivers.find(x => x.id === selectedDriverId);
    if (!d) {
      driverInspector.style.display = 'none';
      return;
    }

    driverInspector.style.display = 'block';
    inspNumber.textContent = `#${d.carNumber}`;
    inspName.textContent = d.name;
    inspTeam.textContent = d.team;
    inspPos.textContent = `P${d.position}`;
    inspLaps.textContent = `${d.completedLaps}`;
    inspPenalty.textContent = `${d.totalPenaltySeconds}s (${d.warningsCount} W)`;
    inspTire.textContent = `${d.tireCompound} (${d.tireAgeLaps} Laps)`;

    inspHistoryTbody.innerHTML = '';
    if (d.lapHistory && d.lapHistory.length > 0) {
      d.lapHistory.slice(-6).reverse().forEach(lh => {
        const tr = document.createElement('tr');
        const bestCls = lh.isOverallBest ? 'timing-purple-val' : (lh.isPersonalBest ? 'timing-green-val' : '');
        tr.innerHTML = `
          <td>L${lh.lapNumber}</td>
          <td class="${bestCls}">${formatLapTime(lh.lapTime)}</td>
          <td>${lh.s1.toFixed(3)}</td>
          <td>${lh.s2.toFixed(3)}</td>
          <td>${lh.s3.toFixed(3)}</td>
          <td>${lh.isPersonalBest ? 'PB' : (lh.isOverallBest ? 'PURPLE' : '--')}</td>
        `;
        inspHistoryTbody.appendChild(tr);
      });
    } else {
      inspHistoryTbody.innerHTML = '<tr><td colspan="6">No lap records logged yet</td></tr>';
    }
  }

  // ==========================================
  // PAGE 2: RACE RADIO COMMS
  // ==========================================
  async function renderRadioFeed() {
    if (!currentRaceState) return;

    const query = radioSearchInput.value.trim();
    const category = radioCategoryFilter.value;
    const carNumber = radioDriverFilter.value;

    let url = 'api/radio?';
    if (query) url += `query=${encodeURIComponent(query)}&`;
    if (category && category !== 'ALL') url += `category=${encodeURIComponent(category)}&`;
    if (carNumber && carNumber !== '0') url += `carNumber=${encodeURIComponent(carNumber)}&`;

    try {
      const res = await fetch(url);
      const messages = await res.json();

      radioFeedList.innerHTML = '';
      document.getElementById('radio-badge').textContent = messages.length;

      if (messages.length === 0) {
        radioFeedList.innerHTML = '<div style="color:var(--text-muted); padding: 24px; text-align: center; background:var(--bg-surface-2); border-radius:4px;">No radio communications matched filter criteria.</div>';
        return;
      }

      messages.forEach(msg => {
        const card = document.createElement('div');
        card.className = `radio-msg-card ${msg.isFlagged ? 'flagged' : ''}`;

        const catClass = `cat-${msg.category.toLowerCase()}`;
        card.innerHTML = `
          <div class="radio-msg-header">
            <div class="radio-sender-group">
              <span class="radio-sender">${msg.sender}</span>
              ${msg.carNumber > 0 ? `<span class="car-badge">#${msg.carNumber}</span>` : ''}
              <span class="radio-cat-tag ${catClass}">${msg.category}</span>
            </div>
            <span class="radio-time">${msg.timestamp}</span>
          </div>
          <p class="radio-text">${msg.text}</p>
          <div class="radio-actions">
            <button class="btn-flag ${msg.isFlagged ? 'active' : ''}" onclick="toggleRadioFlag('${msg.id}', ${!msg.isFlagged})">
              ${msg.isFlagged ? '🚩 FLAGGED FOR REVIEW' : '⚑ FLAG FOR INCIDENT'}
            </button>
          </div>
        `;
        radioFeedList.appendChild(card);
      });
    } catch (err) {
      console.error('[ApexRace] Radio fetch failed:', err);
    }
  }

  window.toggleRadioFlag = async function(messageId, flagged) {
    try {
      const res = await fetch('api/radio', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ flagAction: true, messageId, flagged })
      });
      if (res.ok) {
        showToast(`Radio message ${flagged ? 'flagged for incident review' : 'unflagged'}`);
        renderRadioFeed();
      }
    } catch (err) {
      showToast('Failed to update radio flag', 'error');
    }
  };

  // ==========================================
  // PAGE 3: PIT STRATEGY WALL (LINKED LIST)
  // ==========================================
  function renderPitStrategy(stops) {
    pitStrategyBoard.innerHTML = '';
    const count = stops ? stops.length : 0;
    document.getElementById('pit-badge').textContent = count;
    document.getElementById('pit-stops-count-badge').textContent = `${count} STOPS IN LINKED LIST`;

    if (!stops || stops.length === 0) {
      pitStrategyBoard.innerHTML = '<div style="color:var(--text-muted); padding: 24px; text-align:center; background:var(--bg-surface-2); border-radius:4px;">No pit stops planned in linked list. Use form to insert a stop.</div>';
      return;
    }

    stops.forEach((stop, index) => {
      const card = document.createElement('div');
      card.className = 'pit-stop-node-card';
      const tireCompoundClass = `tire-${(stop.stopType || 'medium').toLowerCase()}`;

      card.innerHTML = `
        <span class="node-seq-index">#${index + 1}</span>
        <div class="node-details">
          <span class="node-car-badge">CAR #${stop.carNumber}</span>
          <span class="node-lap-text">PLANNED LAP: <strong>${stop.plannedLap}</strong></span>
          <span class="tire-pill ${tireCompoundClass}">${stop.stopType}</span>
          <span style="font-size:12px; color:var(--text-muted); font-weight:600;">${stop.status}</span>
        </div>
        <div class="node-actions">
          <button class="btn-icon-small" title="Move stop earlier in linked list" onclick="reorderPitStop('${stop.id}', 'move_earlier')">▲ Earlier</button>
          <button class="btn-icon-small" title="Move stop later in linked list" onclick="reorderPitStop('${stop.id}', 'move_later')">▼ Later</button>
          <button class="btn-icon-small" style="color:#ff6666;" title="Remove stop from linked list" onclick="deletePitStop('${stop.id}')">✕ Remove</button>
        </div>
      `;
      pitStrategyBoard.appendChild(card);
    });
  }

  window.reorderPitStop = async function(stopId, action) {
    try {
      const res = await fetch(`api/pit-strategy/stops/${stopId}`, {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ action: action })
      });
      if (res.ok) {
        showToast(`Pit stop sequence updated: ${action}`);
        fetchRaceState();
      }
    } catch (err) {
      showToast('Failed to update stop order', 'error');
    }
  };

  window.deletePitStop = async function(stopId) {
    try {
      const res = await fetch(`api/pit-strategy/stops/${stopId}`, {
        method: 'DELETE'
      });
      if (res.ok) {
        showToast('Pit stop removed from linked list');
        fetchRaceState();
      }
    } catch (err) {
      showToast('Failed to delete pit stop', 'error');
    }
  };

  // ==========================================
  // PAGE 4: INCIDENT DISPATCH (FIFO QUEUE)
  // ==========================================
  function renderIncidents(incidents) {
    if (!incidents) return;

    const count = incidents.pending ? incidents.pending.length : 0;
    document.getElementById('incident-badge').textContent = count;
    document.getElementById('incident-count-label').textContent = `${count} PENDING`;

    pendingIncidentsContainer.innerHTML = '';
    if (!incidents.pending || incidents.pending.length === 0) {
      pendingIncidentsContainer.innerHTML = '<div style="color:var(--text-muted); padding: 16px; background:var(--bg-surface-2); border-radius:4px;">No pending incidents in queue.</div>';
    } else {
      incidents.pending.forEach(inc => {
        const card = document.createElement('div');
        const sevClass = inc.severity.toLowerCase();
        card.className = `incident-card ${sevClass}`;

        card.innerHTML = `
          <div class="incident-card-top">
            <span class="inc-seq-badge">ARRIVAL QUEUE #${inc.arrivalSequence}</span>
            <span class="radio-cat-tag cat-incident">${inc.category} (${inc.severity})</span>
          </div>
          <div class="inc-loc">LAP ${inc.lap} — ${inc.location} [Cars: ${inc.involvedCars.join(', ')}]</div>
          <p class="inc-desc">${inc.description}</p>
        `;
        pendingIncidentsContainer.appendChild(card);
      });
    }

    resolvedIncidentsContainer.innerHTML = '';
    if (!incidents.resolved || incidents.resolved.length === 0) {
      resolvedIncidentsContainer.innerHTML = '<div style="color:var(--text-muted); padding: 12px;">No resolved incident history.</div>';
    } else {
      incidents.resolved.slice().reverse().forEach(inc => {
        const card = document.createElement('div');
        card.className = 'incident-card low';
        card.innerHTML = `
          <div class="incident-card-top">
            <span class="inc-seq-badge">${inc.id}</span>
            <span class="radio-cat-tag cat-technical">${inc.status}</span>
          </div>
          <div class="inc-loc">Lap ${inc.lap}: ${inc.location}</div>
          <p class="inc-desc">${inc.description}</p>
          <div style="font-size:11px; color:var(--timing-green); margin-top:4px;">Resolution: ${inc.reviewNotes || 'Action recorded'}</div>
        `;
        resolvedIncidentsContainer.appendChild(card);
      });
    }
  }

  function renderActiveInvestigation(inc) {
    activeInvestigationIncident = inc;
    if (!inc) {
      activeInvestigationBox.className = 'investigation-empty-state';
      activeInvestigationBox.innerHTML = '<p>No active incident under direct review. Click "REVIEW NEXT INCIDENT" above to dequeue the arrival-order incident.</p>';
      return;
    }

    activeInvestigationBox.className = 'active-incident-card';
    activeInvestigationBox.innerHTML = `
      <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:8px;">
        <span class="callout-tag">UNDER DIRECT INVESTIGATION: ${inc.id}</span>
        <span class="inc-seq-badge">Queue Arrival #${inc.arrivalSequence}</span>
      </div>
      <div style="font-weight:700; color:#fff; font-size:13px; margin-bottom:6px;">
        Lap ${inc.lap} — ${inc.location} (Cars involved: ${inc.involvedCars.join(', ')})
      </div>
      <p style="font-size:12px; margin-bottom:12px; color:var(--text-primary); line-height:1.4;">${inc.description}</p>
      <div style="display:flex; gap:10px;">
        <button class="action-btn btn-primary" onclick="resolveActiveIncident('${inc.id}', 'RESOLVED_NO_FURTHER_ACTION')">NO FURTHER ACTION</button>
        <button class="action-btn btn-alert" onclick="resolveActiveIncident('${inc.id}', 'PENALTY_REQUIRED')">SEND TO STEWARDS</button>
      </div>
    `;
  }

  window.resolveActiveIncident = async function(incidentId, status) {
    try {
      const res = await fetch(`api/incidents/${incidentId}/resolve`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ status: status, reviewNotes: `Reviewed and marked ${status}` })
      });
      if (res.ok) {
        showToast(`Incident ${incidentId} marked as ${status}`);
        renderActiveInvestigation(null);
        fetchRaceState();
      }
    } catch (err) {
      showToast('Failed to resolve incident', 'error');
    }
  };

  // ==========================================
  // PAGE 5: STEWARD DECISIONS (STACK UNDO)
  // ==========================================
  function renderDecisions(decisions, undoStackDepth) {
    decisionHistoryFeed.innerHTML = '';
    const count = decisions ? decisions.length : 0;
    document.getElementById('stewards-badge').textContent = count;
    document.getElementById('undo-depth-counter').textContent = undoStackDepth || 0;

    if (!decisions || decisions.length === 0) {
      decisionHistoryFeed.innerHTML = '<div style="color:var(--text-muted); padding: 24px; text-align:center; background:var(--bg-surface-2); border-radius:4px;">No steward decisions active.</div>';
      return;
    }

    decisions.slice().reverse().forEach(dec => {
      const card = document.createElement('div');
      card.className = 'decision-card';
      card.innerHTML = `
        <div class="decision-top">
          <div style="display:flex; align-items:center; gap:10px;">
            <span class="decision-type-badge">${dec.actionType}</span>
            <span class="car-badge">#${dec.carNumber}</span>
          </div>
          <span style="font-family:var(--font-mono); font-size:11px; color:var(--text-muted);">${dec.timestamp}</span>
        </div>
        <div style="font-size:13px; color:#fff; font-weight:600; margin-top:4px;">${dec.reason}</div>
        <div style="font-size:11px; color:var(--text-secondary); margin-top:2px;">
          Sanction Details: ${dec.penaltySeconds > 0 ? `+${dec.penaltySeconds}s time penalty` : dec.actionType} | Action Ref: ${dec.decisionId}
        </div>
      `;
      decisionHistoryFeed.appendChild(card);
    });
  }

  // ==========================================
  // FOOTER EVENT LOGS
  // ==========================================
  function renderFooterLogs(logs) {
    if (!logs || logs.length === 0) return;
    footerLogTicker.textContent = logs.slice(-4).join('  ///  ');
  }

  function updateDriverSelectDropdowns(drivers) {
    if (!drivers) return;

    // Filter dropdown in radio
    const currentCarFilter = radioDriverFilter.value;
    radioDriverFilter.innerHTML = '<option value="0">All Cars</option>';
    drivers.forEach(d => {
      const opt = document.createElement('option');
      opt.value = d.carNumber;
      opt.textContent = `#${d.carNumber} - ${d.name}`;
      radioDriverFilter.appendChild(opt);
    });
    radioDriverFilter.value = currentCarFilter;

    // Target driver dropdown in stewards form
    const currentSelectedDriver = decDriverSelect.value;
    decDriverSelect.innerHTML = '';
    drivers.forEach(d => {
      const opt = document.createElement('option');
      opt.value = d.id;
      opt.textContent = `#${d.carNumber} ${d.name} (${d.team})`;
      decDriverSelect.appendChild(opt);
    });
    if (currentSelectedDriver) decDriverSelect.value = currentSelectedDriver;
  }

  // ==========================================
  // EVENT LISTENERS & FORM SUBMISSIONS
  // ==========================================

  // Session Controls
  btnAdvanceLap.addEventListener('click', async () => {
    try {
      btnAdvanceLap.disabled = true;
      const res = await fetch('api/session/advance', { method: 'POST' });
      if (res.ok) {
        showToast('Lap advanced across all cars');
        fetchRaceState();
      }
    } catch (err) {
      showToast('Failed to advance lap', 'error');
    } finally {
      btnAdvanceLap.disabled = false;
    }
  });

  btnResetSession.addEventListener('click', async () => {
    try {
      const res = await fetch('api/reset', { method: 'POST' });
      if (res.ok) {
        showToast('Session reset to seeded state');
        activeInvestigationIncident = null;
        renderActiveInvestigation(null);
        fetchRaceState();
      }
    } catch (err) {
      showToast('Reset failed', 'error');
    }
  });

  trackStatusSelect.addEventListener('change', async (e) => {
    try {
      const status = e.target.value;
      const res = await fetch('api/session/status', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ trackStatus: status })
      });
      if (res.ok) {
        showToast(`Track status changed to ${status}`);
        fetchRaceState();
      }
    } catch (err) {
      showToast('Failed to update track status', 'error');
    }
  });

  btnCloseInspector.addEventListener('click', () => {
    selectedDriverId = null;
    driverInspector.style.display = 'none';
    renderTimingTower(currentRaceState.drivers);
  });

  // Manual Lap Telemetry Injection Form
  recordLapForm.addEventListener('submit', async (e) => {
    e.preventDefault();
    if (!selectedDriverId) return;

    const lapTime = parseFloat(document.getElementById('input-lap-time').value);
    const s1 = parseFloat(document.getElementById('input-s1').value);
    const s2 = parseFloat(document.getElementById('input-s2').value);
    const s3 = parseFloat(document.getElementById('input-s3').value);

    try {
      const res = await fetch('api/laps', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ driverId: selectedDriverId, lapTime, s1, s2, s3 })
      });
      if (res.ok) {
        showToast(`Recorded lap ${formatLapTime(lapTime)} for selected driver`);
        recordLapForm.reset();
        fetchRaceState();
      } else {
        const err = await res.json();
        showToast(`Failed to record lap: ${err.error || 'Server error'}`, 'error');
      }
    } catch (err) {
      showToast('Network error while recording lap', 'error');
    }
  });

  // Radio Filters
  radioSearchInput.addEventListener('input', renderRadioFeed);
  radioCategoryFilter.addEventListener('change', renderRadioFeed);
  radioDriverFilter.addEventListener('change', renderRadioFeed);

  // New Radio Message Form
  newRadioForm.addEventListener('submit', async (e) => {
    e.preventDefault();
    const sender = document.getElementById('radio-sender-input').value.trim();
    const carNumber = parseInt(document.getElementById('radio-car-input').value, 10) || 0;
    const category = document.getElementById('radio-category-input').value;
    const text = document.getElementById('radio-text-input').value.trim();

    try {
      const res = await fetch('api/radio', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ sender, carNumber, category, text })
      });
      if (res.ok) {
        showToast('Radio transmission broadcasted');
        document.getElementById('radio-text-input').value = '';
        fetchRaceState();
      }
    } catch (err) {
      showToast('Failed to send radio', 'error');
    }
  });

  // Process Next Incident (FIFO queue)
  btnProcessNextIncident.addEventListener('click', async () => {
    try {
      const res = await fetch('api/incidents/process-next', { method: 'POST' });
      if (res.ok) {
        const data = await res.json();
        renderActiveInvestigation(data.selectedIncident);
        showToast(`Dequeued incident ${data.selectedIncident.id} for review`);
        fetchRaceState();
      } else {
        const err = await res.json();
        showToast(err.error || 'No pending incidents to review', 'error');
      }
    } catch (err) {
      showToast('Incident queue operation failed', 'error');
    }
  });

  // Report New Incident Form
  newIncidentForm.addEventListener('submit', async (e) => {
    e.preventDefault();
    const lap = parseInt(document.getElementById('inc-lap-input').value, 10);
    const location = document.getElementById('inc-loc-input').value.trim();
    const carsStr = document.getElementById('inc-cars-input').value.trim();
    const involvedCars = carsStr.split(',').map(s => parseInt(s.trim(), 10)).filter(n => !isNaN(n));
    const category = document.getElementById('inc-cat-input').value;
    const severity = document.getElementById('inc-sev-input').value;
    const description = document.getElementById('inc-desc-input').value.trim();

    try {
      const res = await fetch('api/incidents', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ lap, location, involvedCars, category, severity, description })
      });
      if (res.ok) {
        showToast('Incident reported and enqueued');
        newIncidentForm.reset();
        fetchRaceState();
      }
    } catch (err) {
      showToast('Failed to report incident', 'error');
    }
  });

  // Schedule Pit Stop Form
  newPitForm.addEventListener('submit', async (e) => {
    e.preventDefault();
    const carNumber = parseInt(document.getElementById('pit-car-input').value, 10);
    const plannedLap = parseInt(document.getElementById('pit-lap-input').value, 10);
    const stopType = document.getElementById('pit-type-input').value;

    try {
      const res = await fetch('api/pit-strategy/stops', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ carNumber, plannedLap, stopType })
      });
      if (res.ok) {
        showToast(`Pit stop for Car #${carNumber} added to schedule`);
        newPitForm.reset();
        fetchRaceState();
      } else {
        const err = await res.json();
        showToast(`Validation Failed: ${err.error || 'Invalid stop plan'}`, 'error');
      }
    } catch (err) {
      showToast('Network error adding pit stop', 'error');
    }
  });

  // Apply Steward Decision Form
  newDecisionForm.addEventListener('submit', async (e) => {
    e.preventDefault();
    const driverId = decDriverSelect.value;
    const actionTypeRaw = document.getElementById('dec-action-select').value;
    const reason = document.getElementById('dec-reason-input').value.trim();

    let actionType = 'WARNING';
    let penaltySeconds = 0;

    if (actionTypeRaw === 'TIME_PENALTY') {
      actionType = 'TIME_PENALTY';
      penaltySeconds = 5;
    } else if (actionTypeRaw === 'TIME_PENALTY_10') {
      actionType = 'TIME_PENALTY';
      penaltySeconds = 10;
    } else if (actionTypeRaw === 'DISQUALIFICATION') {
      actionType = 'DISQUALIFICATION';
    } else {
      actionType = 'WARNING';
    }

    try {
      const res = await fetch('api/decisions', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ driverId, actionType, penaltySeconds, reason, reversible: true })
      });
      if (res.ok) {
        showToast(`Steward decision executed and logged`);
        document.getElementById('dec-reason-input').value = '';
        fetchRaceState();
      } else {
        const err = await res.json();
        showToast(`Error: ${err.error || 'Could not apply decision'}`, 'error');
      }
    } catch (err) {
      showToast('Failed to apply steward decision', 'error');
    }
  });

  // Undo Decision (Stack)
  btnUndoDecision.addEventListener('click', async () => {
    try {
      const res = await fetch('api/decisions/undo', { method: 'POST' });
      if (res.ok) {
        const data = await res.json();
        showToast(`Undone action for Car #${data.undoneDecision.carNumber} — driver state restored`);
        fetchRaceState();
      } else {
        const err = await res.json();
        showToast(err.error || 'No reversible decisions on the undo stack', 'error');
      }
    } catch (err) {
      showToast('Failed to execute undo', 'error');
    }
  });

  // Initial routing and data load
  const initialHash = window.location.hash.replace('#', '') || 'timing';
  switchView(initialHash);
  fetchRaceState();

})();
