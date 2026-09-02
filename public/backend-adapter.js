(function () {
  'use strict';

  const UI_TO_SERVER_NODE = Object.freeze({
    zijing: 1, like: 2, library: 3, sushimin: 4, dongda: 5,
    hospital: 6, dorm: 7, dongmen: 8, hall: 9, xinqinghua: 10,
    zhongyang: 11, aBuild: 12, zhaolan: 13, keji: 14
  });
  const SERVER_TO_UI_NODE = Object.freeze(Object.fromEntries(
    Object.entries(UI_TO_SERVER_NODE).map(([uiNode, serverNode]) => [serverNode, uiNode])
  ));

  // Fixed geometry for each THU server edge. This translates an authoritative
  // server edge into the original UI's hidden junctions; it never chooses a route.
  const EDGE_GEOMETRY = Object.freeze({
    '1-3': ['zijing', 'jBumpTL', 'jBumpL', 'library'],
    '1-5': ['zijing', 'jBumpTR', 'jBumpR', 'dongda'],
    '2-3': ['like', 'library'],
    '2-6': ['like', 'j1', 'hospital'],
    '4-5': ['sushimin', 'jBumpR', 'dongda'],
    '4-7': ['sushimin', 'dorm'],
    '5-8': ['dongda', 'j3', 'dongmen'],
    '6-9': ['hospital', 'j4', 'hall'],
    '7-10': ['dorm', 'xinqinghua'],
    '8-12': ['dongmen', 'aBuild'],
    '9-10': ['hall', 'xinqinghua'],
    '9-13': ['hall', 'j7', 'zhaolan'],
    '10-11': ['xinqinghua', 'zhongyang'],
    '11-12': ['zhongyang', 'aBuild'],
    '12-14': ['aBuild', 'j8', 'keji'],
    '13-14': ['zhaolan', 'keji'],
    '3-4': ['library', 'jBumpL', 'sushimin'],
    '1-4': ['zijing', 'jBumpTL', 'jBumpL', 'sushimin']
  });

  const adapter = {
    snapshot: null,
    snapshotVersion: 0,
    preview: null,
    previewToken: 0,
    routeVisuals: [],
    currentVisual: null,
    currentRunKey: null,
    displayedProgress: 0,
    targetProgress: 0,
    lastAnimationAt: 0,
    animationFrame: null,
    socket: null,
    reconnectTimer: null,
    submitting: false,
    pendingRequestId: null
  };

  const originalRenderSvgOverlay = renderSvgOverlay;
  const originalOnPinClick = onPinClick;
  const originalSwapSelection = swapSelection;
  const originalCancelSelection = cancelSelection;

  function locationIdForUiNode(uiNode) {
    return `1:${UI_TO_SERVER_NODE[uiNode]}`;
  }

  function safeText(value) {
    return String(value == null ? '' : value)
      .replaceAll('&', '&amp;')
      .replaceAll('<', '&lt;')
      .replaceAll('>', '&gt;')
      .replaceAll('"', '&quot;')
      .replaceAll("'", '&#39;');
  }

  async function apiJson(url, options = {}) {
    const response = await fetch(url, {
      ...options,
      headers: { 'content-type': 'application/json', ...(options.headers || {}) }
    });
    const payload = await response.json().catch(() => ({}));
    if (!response.ok || payload.ok === false) {
      const error = new Error(payload.error || `请求失败（${response.status}）`);
      error.code = payload.error_code || 'HTTP_ERROR';
      throw error;
    }
    return payload;
  }

  function edgeGeometry(fromNode, toNode) {
    const from = Number(fromNode);
    const to = Number(toNode);
    const low = Math.min(from, to);
    const high = Math.max(from, to);
    const base = EDGE_GEOMETRY[`${low}-${high}`];
    if (!base) throw new Error(`服务器路径包含未知边 ${from}-${to}`);
    const oriented = from === low ? base.slice() : base.slice().reverse();
    if (oriented[0] !== SERVER_TO_UI_NODE[from] || oriented.at(-1) !== SERVER_TO_UI_NODE[to]) {
      throw new Error(`路径边 ${from}-${to} 与 UI 几何不匹配`);
    }
    return oriented;
  }

  function buildServerPath(serverNodes) {
    const path = Array.isArray(serverNodes) ? serverNodes.map(Number) : [];
    if (path.length === 0 || !SERVER_TO_UI_NODE[path[0]]) return null;
    const waypoints = [SERVER_TO_UI_NODE[path[0]]];
    const serverBreakIndices = [0];
    for (let index = 0; index < path.length - 1; index++) {
      const geometry = edgeGeometry(path[index], path[index + 1]);
      waypoints.push(...geometry.slice(1));
      serverBreakIndices.push(waypoints.length - 1);
    }
    const built = buildRun(waypoints);
    if (built.rawPath.length !== waypoints.length ||
        built.rawPath.some((node, index) => node !== waypoints[index])) {
      throw new Error('服务器边几何不连续');
    }
    return {
      ...built,
      serverNodes: path,
      serverBreakLen: serverBreakIndices.map((index) => built.breakLen[index])
    };
  }

  function statusPresentation(order) {
    const statusCode = Number(order.status);
    if (statusCode === 5) return { base: '已送达', label: '已送达', phase: null };
    if (statusCode === 1 && order.dispatch_state === 'UNASSIGNED') {
      return { base: '排队中', label: '排队中', phase: null };
    }
    const label = {
      1: '前往取件', 2: '等待装载', 3: '配送中', 4: '等待取件'
    }[statusCode] || '处理中';
    const phase = statusCode === 1 ? 'to_pickup' : (statusCode === 3 ? 'in_transit' : null);
    return { base: '配送中', label, phase };
  }

  function transformOrders(snapshot) {
    const activeOrderIds = new Set(snapshot.active_trip?.order_ids || []);
    const grouped = activeOrderIds.size >= 2 ? `trip-${snapshot.active_trip.trip_id}` : null;
    return (snapshot.orders || [])
      .filter((order) => Number(order.map_id) === 1)
      .map((order, index) => {
        const from = SERVER_TO_UI_NODE[order.pickup?.node_id];
        const to = SERVER_TO_UI_NODE[order.dropoff?.node_id];
        if (!from || !to) return null;
        const presentation = statusPresentation(order);
        const displayNumber = Number(order.display_no);
        return {
          id: Number.isFinite(displayNumber) ? displayNumber : index + 1,
          serverOrderId: order.order_id,
          statusCode: Number(order.status),
          name: safeText(order.nickname),
          from,
          to,
          status: presentation.base,
          statusLabel: presentation.label,
          phase: presentation.phase,
          color: ORDER_COLORS[index % ORDER_COLORS.length],
          groupId: grouped && activeOrderIds.has(order.order_id) && Number(order.status) !== 5 ? grouped : null,
          ts: order.created_at ? Date.parse(order.created_at) : Date.now(),
          version: Number(order.version)
        };
      })
      .filter(Boolean);
  }

  function buildRouteVisuals(snapshot) {
    const visuals = [];
    for (const leg of snapshot.route_plan?.legs || []) {
      try {
        const built = buildServerPath(leg.route_nodes);
        if (!built) continue;
        visuals.push({ ...leg, ...built });
      } catch (error) {
        console.warn(error.message);
      }
    }
    return visuals;
  }

  function progressFromSnapshot(snapshot, visual) {
    if (!visual || visual.totalLen <= 0) return 1;
    const vehicle = snapshot.vehicle || {};
    const progress = snapshot.navigation_progress;
    if (progress && progress.stop_id === visual.stop_id && progress.motion_state === 'MOVING') {
      const index = Number(progress.path_index);
      if (Number.isInteger(index) && index >= 0 && index < visual.serverBreakLen.length - 1) {
        const start = visual.serverBreakLen[index];
        const end = visual.serverBreakLen[index + 1];
        return Math.max(0, Math.min(1,
          (start + (end - start) * Number(progress.segment_progress)) / visual.totalLen
        ));
      }
    }
    if (Number(vehicle.current_node) === Number(visual.node_id) && vehicle.motion_state !== 'MOVING') return 1;
    const currentNodeIndex = visual.serverNodes.lastIndexOf(Number(vehicle.current_node));
    if (currentNodeIndex >= 0) {
      return Math.max(0, Math.min(1, visual.serverBreakLen[currentNodeIndex] / visual.totalLen));
    }
    return 0;
  }

  function updateVisualModel(snapshot) {
    adapter.routeVisuals = buildRouteVisuals(snapshot);
    adapter.currentVisual = adapter.routeVisuals.find((leg) => leg.is_current) || null;
    const key = adapter.currentVisual
      ? `${snapshot.route_plan.trip_id}:${adapter.currentVisual.stop_id}:${snapshot.active_trip?.command_version || 0}`
      : null;
    const nextProgress = progressFromSnapshot(snapshot, adapter.currentVisual);
    if (key !== adapter.currentRunKey) {
      adapter.currentRunKey = key;
      adapter.displayedProgress = nextProgress;
      adapter.targetProgress = nextProgress;
    } else {
      adapter.targetProgress = Math.max(adapter.displayedProgress, nextProgress);
    }
    ensureAnimation();
  }

  function applySnapshot(snapshot) {
    if (!snapshot || snapshot.type !== 'snapshot') return;
    const version = Number(snapshot.snapshot_version || 0);
    if (version < adapter.snapshotVersion) return;
    adapter.snapshotVersion = version;
    adapter.snapshot = snapshot;
    state.orders = transformOrders(snapshot);
    state.run = null;
    const vehicleNode = SERVER_TO_UI_NODE[snapshot.vehicle?.current_node];
    if (vehicleNode) state.robotPos = vehicleNode;
    updateVisualModel(snapshot);
    renderSvgOverlay();
    renderPanel();
    updateConnectionHint();
  }

  function updateConnectionHint(message = null) {
    if (state.selecting) return;
    const hint = document.getElementById('hintChip');
    if (!hint || !adapter.snapshot) return;
    if (message) {
      hint.textContent = message;
    } else if (adapter.snapshot.vehicle?.online_status !== 'ONLINE') {
      hint.textContent = '小车离线 · 订单将在上线后调度';
    } else if (adapter.snapshot.route_plan?.legs?.some((leg) => !leg.reachable)) {
      hint.textContent = '部分后续路线暂时不可达 · 等待服务器重新规划';
    }
  }

  function currentLegClass(leg) {
    return leg.is_current && leg.action === 'PICKUP' ? 'route-repositioning' : 'route-track';
  }

  function renderServerOverlay() {
    if (!adapter.snapshot) return originalRenderSvgOverlay();
    const svg = document.getElementById('routeSvg');
    let markup = '';
    for (const leg of adapter.routeVisuals) {
      if (leg.points.length > 1) {
        markup += `<path d="${pointsToPathD(leg.points)}" class="${currentLegClass(leg)}" />`;
      }
    }
    const current = adapter.currentVisual;
    if (current && current.points.length > 1 && adapter.displayedProgress > 0) {
      const travelled = tracePartial(current.points, adapter.displayedProgress);
      if (travelled.length > 1) {
        markup += `<path d="${pointsToPathD(travelled)}" class="route-travelled" />`;
      }
    }
    if (state.selecting?.from && state.selecting?.to && previewMatchesSelection()) {
      markup += `<path d="${pointsToPathD(adapter.preview.points)}" class="route-preview" />`;
    }
    svg.innerHTML = markup;

    const robot = document.getElementById('robotMarker');
    if (current?.points?.length) {
      const point = pointAtFraction(current.points, adapter.displayedProgress);
      robot.style.left = `${point.x}px`;
      robot.style.top = `${point.y}px`;
    } else {
      const resting = NODES[state.robotPos] || NODES.hall;
      robot.style.left = `${resting.x}px`;
      robot.style.top = `${resting.y}px`;
    }
    robot.style.opacity = 1;

    const markerLayer = document.getElementById('markerLayer');
    markerLayer.innerHTML = '';
    if (state.selecting) {
      if (state.selecting.from) markerLayer.appendChild(buildEndpointMarker(state.selecting.from, 'from'));
      if (state.selecting.to) markerLayer.appendChild(buildEndpointMarker(state.selecting.to, 'to'));
      return;
    }
    const stopsByNode = new Map();
    for (const leg of adapter.routeVisuals) {
      const aggregate = stopsByNode.get(leg.node_id) || { ...leg, operations: [] };
      aggregate.operations.push(...(leg.operations || []));
      stopsByNode.set(leg.node_id, aggregate);
    }
    let markerNumber = 0;
    for (const leg of stopsByNode.values()) {
      markerNumber += 1;
      const uiNode = SERVER_TO_UI_NODE[leg.node_id];
      if (!uiNode) continue;
      const operationText = (leg.operations || [])
        .map((operation) => `${operation.action === 'PICKUP' ? '取件' : '送件'} ${operation.order_id}`)
        .join('、');
      const order = state.orders.find((item) =>
        (leg.operations || []).some((operation) => operation.order_id === item.serverOrderId)
      );
      let iconMode = null;
      if (leg.is_current && order) {
        if (order.statusCode === 2) iconMode = 'load';
        else if (order.statusCode === 4) iconMode = 'unload';
      }
      const marker = buildStopMarker(uiNode, markerNumber, order?.color, iconMode);
      marker.title = operationText;
      marker.setAttribute('aria-label', operationText);
      markerLayer.appendChild(marker);
    }
  }

  function updateProgressBars() {
    for (const order of state.orders) {
      const fill = document.querySelector(`[data-order="${order.id}"] .progress-fill`);
      if (fill) fill.style.width = `${orderProgressPercent(order)}%`;
    }
  }

  function animationStep(now) {
    adapter.animationFrame = null;
    const elapsed = adapter.lastAnimationAt ? Math.min(100, now - adapter.lastAnimationAt) : 16;
    adapter.lastAnimationAt = now;
    if (adapter.displayedProgress < adapter.targetProgress) {
      const ratio = Math.min(1, elapsed / 240);
      adapter.displayedProgress += (adapter.targetProgress - adapter.displayedProgress) * ratio;
      if (adapter.targetProgress - adapter.displayedProgress < 0.0005) {
        adapter.displayedProgress = adapter.targetProgress;
      }
      renderSvgOverlay();
      updateProgressBars();
    }
    if (adapter.displayedProgress < adapter.targetProgress) ensureAnimation();
  }

  function ensureAnimation() {
    if (adapter.animationFrame == null && adapter.displayedProgress < adapter.targetProgress) {
      adapter.animationFrame = requestAnimationFrame(animationStep);
    }
  }

  function previewMatchesSelection() {
    return Boolean(adapter.preview && state.selecting &&
      adapter.preview.from === state.selecting.from && adapter.preview.to === state.selecting.to);
  }

  function clearPreview() {
    adapter.previewToken += 1;
    adapter.preview = null;
  }

  async function requestPreview() {
    const selection = state.selecting;
    if (!selection?.from || !selection?.to) return null;
    const token = ++adapter.previewToken;
    const from = selection.from;
    const to = selection.to;
    try {
      const payload = await apiJson('/api/route-preview', {
        method: 'POST',
        body: JSON.stringify({
          pickup_location_id: locationIdForUiNode(from),
          dropoff_location_id: locationIdForUiNode(to)
        })
      });
      if (token !== adapter.previewToken || !state.selecting ||
          state.selecting.from !== from || state.selecting.to !== to) return null;
      const visual = buildServerPath(payload.route_nodes);
      adapter.preview = {
        from,
        to,
        points: visual.points,
        distance: Number(payload.distance),
        waitMinutes: Number(payload.estimated_wait_minutes),
        deliveryMinutes: Number(payload.estimated_delivery_minutes),
        queuedOrderCount: Number(payload.queued_order_count)
      };
      renderSvgOverlay();
      renderPanel();
      return adapter.preview;
    } catch (error) {
      if (token === adapter.previewToken) {
        adapter.preview = null;
        updateConnectionHint(error.message || '路线暂时不可达');
        showToast(`🚫 ${safeText(error.message || '路线暂时不可达')}`);
      }
      return null;
    }
  }

  async function submitSelection() {
    if (adapter.submitting || !state.selecting?.from || !state.selecting?.to) return;
    adapter.submitting = true;
    const button = document.getElementById('confirmSelBtn');
    if (button) {
      button.disabled = true;
      button.textContent = '提交中…';
    }
    try {
      if (!previewMatchesSelection()) {
        const preview = await requestPreview();
        if (!preview) throw new Error('服务器未返回可用路线');
      }
      const selection = state.selecting;
      const name = (selection.name || '').trim() || peekName();
      adapter.pendingRequestId ||= (crypto.randomUUID ? crypto.randomUUID() : `browser-${Date.now()}`);
      const payload = await apiJson('/api/orders', {
        method: 'POST',
        body: JSON.stringify({
          client_request_id: adapter.pendingRequestId,
          nickname: name,
          pickup_location_id: locationIdForUiNode(selection.from),
          dropoff_location_id: locationIdForUiNode(selection.to),
          item_summary: '网页订单',
          item_count: 1,
          note: ''
        })
      });
      adapter.pendingRequestId = null;
      nameCursor += 1;
      state.selecting = null;
      clearPreview();
      clearPinStates();
      state.tab = 'active';
      showToast(`订单 #${safeText(payload.order.display_no)} 已加入配送队列`);
      renderSvgOverlay();
      renderPanel();
      await loadSnapshot();
    } catch (error) {
      showToast(`🚫 ${safeText(error.message || '下单失败')}`);
      renderPanel();
    } finally {
      adapter.submitting = false;
    }
  }

  async function loadSnapshot() {
    try {
      applySnapshot(await apiJson('/api/snapshot'));
    } catch (error) {
      updateConnectionHint('无法连接服务器');
    }
  }

  function connectWebSocket() {
    clearTimeout(adapter.reconnectTimer);
    const protocol = location.protocol === 'https:' ? 'wss:' : 'ws:';
    const socket = new WebSocket(`${protocol}//${location.host}`);
    adapter.socket = socket;
    socket.addEventListener('open', () => {
      socket.send(JSON.stringify({ type: 'get_snapshot' }));
    });
    socket.addEventListener('message', (event) => {
      try {
        const message = JSON.parse(event.data);
        if (message.type === 'snapshot') applySnapshot(message);
        if (message.type === 'car_error') showToast(`🚫 ${safeText(message.error || '小车消息异常')}`);
      } catch (error) {
        console.warn('invalid websocket message', error);
      }
    });
    socket.addEventListener('close', () => {
      if (adapter.socket === socket) adapter.socket = null;
      updateConnectionHint('服务器连接已断开 · 正在重连');
      adapter.reconnectTimer = setTimeout(() => {
        loadSnapshot();
        connectWebSocket();
      }, 2000);
    });
  }

  renderSvgOverlay = renderServerOverlay;
  confirmSelection = submitSelection;
  tryPoolOrStartOrder = function () { return false; };
  startNextRun = function () {};
  animateRun = function () {};
  orderProgressPercent = function (order) {
    if (Number(order.statusCode) === 5) return 100;
    const current = adapter.currentVisual;
    const belongsToCurrent = current?.operations?.some((operation) =>
      operation.order_id === order.serverOrderId
    );
    return belongsToCurrent ? Math.round(adapter.displayedProgress * 100) : 0;
  };
  etaInfo = function (from, to) {
    if (adapter.preview?.from === from && adapter.preview?.to === to) {
      return {
        distM: adapter.preview.distance,
        travelMin: Math.max(1, adapter.preview.deliveryMinutes - adapter.preview.waitMinutes),
        waitMin: adapter.preview.waitMinutes,
        totalMin: adapter.preview.deliveryMinutes,
        queueAhead: adapter.preview.queuedOrderCount
      };
    }
    return { distM: '—', travelMin: 0, waitMin: 0, totalMin: '—', queueAhead: totalOpen() };
  };
  onPinClick = function (nodeId) {
    clearPreview();
    originalOnPinClick(nodeId);
    requestPreview();
  };
  swapSelection = function () {
    clearPreview();
    originalSwapSelection();
    requestPreview();
  };
  cancelSelection = function () {
    clearPreview();
    adapter.pendingRequestId = null;
    originalCancelSelection();
  };

  document.addEventListener('DOMContentLoaded', () => {
    window.deliveryBackendAdapter = adapter;
    loadSnapshot();
    connectWebSocket();
  });
})();
