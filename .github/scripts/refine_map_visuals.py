from pathlib import Path
import re

path = Path("docs/index.html")
text = path.read_text(encoding="utf-8")

if "Map visual hierarchy v2" in text:
    raise SystemExit("map refinement already applied")

# Add background groups and an HTML tooltip layer to the map stage.
old_groups = '''            <g id="baseRoads"></g>
            <g id="oldRoute"></g>'''
new_groups = '''            <g id="mapZones"></g>
            <g id="baseRoads"></g>
            <g id="roadJunctions"></g>
            <g id="oldRoute"></g>'''
if old_groups not in text:
    raise SystemExit("map group anchor not found")
text = text.replace(old_groups, new_groups, 1)

old_svg_end = '''            <circle class="vehicle-dot" id="vehicleDot" r="8" cx="0" cy="0" visibility="hidden"></circle>
          </svg>
        </div>'''
new_svg_end = '''            <circle class="vehicle-dot" id="vehicleDot" r="8" cx="0" cy="0" visibility="hidden"></circle>
          </svg>
          <div class="map-tooltip" id="mapTooltip" role="status" aria-live="polite" aria-hidden="true">
            <strong></strong><span></span>
          </div>
        </div>'''
if old_svg_end not in text:
    raise SystemExit("map svg end anchor not found")
text = text.replace(old_svg_end, new_svg_end, 1)

css = r'''

    /* ============================================================
       Map visual hierarchy v2
       Road hierarchy, spatial zones, semantic nodes, restrained motion.
       ============================================================ */
    .map-stage {
      background:
        linear-gradient(rgba(188,214,198,.015) 1px, transparent 1px),
        linear-gradient(90deg, rgba(188,214,198,.015) 1px, transparent 1px),
        radial-gradient(circle at 50% 44%, rgba(53,101,79,.055), transparent 66%),
        #09110d;
      background-size: 28px 28px, 28px 28px, auto, auto;
      border-color: rgba(195,220,205,.10);
      border-radius: 18px;
    }

    .base-road-bed {
      stroke: rgba(1,7,5,.82);
      stroke-width: 12.5;
      stroke-linecap: round;
      stroke-linejoin: round;
    }

    .base-road {
      stroke: rgba(151,174,158,.34);
      stroke-width: 1.7;
      stroke-linecap: round;
      stroke-linejoin: round;
    }

    .road-junction {
      fill: #0a130f;
      stroke: rgba(181,207,190,.13);
      stroke-width: 1;
      opacity: .82;
      pointer-events: none;
    }

    .campus-zone {
      fill: rgba(92,130,107,.022);
      stroke: rgba(151,185,163,.105);
      stroke-width: 1;
      stroke-dasharray: 6 9;
      rx: 12px;
      ry: 12px;
      pointer-events: none;
    }

    .campus-zone-label {
      fill: rgba(183,207,190,.24);
      font-family: "Prism", "Arial Narrow", sans-serif;
      font-size: 9px;
      letter-spacing: .17em;
      text-transform: uppercase;
      pointer-events: none;
    }

    .route-road {
      stroke-linecap: round;
      stroke-linejoin: round;
      filter: none;
    }

    .route-future {
      stroke: rgba(121,162,92,.28);
      stroke-width: 3.1;
    }

    .route-traveled {
      stroke: rgba(141,190,80,.52);
      stroke-width: 3.35;
      filter: drop-shadow(0 0 2px rgba(118,185,0,.10));
    }

    .route-current-flow {
      stroke: rgba(180,224,112,.68);
      stroke-width: 2;
      stroke-dasharray: 9 18;
      animation: mapRouteDirection 5.4s linear infinite;
      filter: none;
    }

    @keyframes mapRouteDirection {
      to { stroke-dashoffset: -54; }
    }

    .old-road {
      stroke-width: 2.5;
      stroke: rgba(210,166,76,.45);
      stroke-dasharray: 8 10;
      filter: none;
    }

    .anomaly-road {
      stroke-width: 3.3;
      stroke: rgba(240,169,58,.72);
      filter: drop-shadow(0 0 3px rgba(240,169,58,.15));
    }

    .map-node {
      fill: #08120e;
      stroke: rgba(171,211,119,.58);
      stroke-width: 1.8;
      transition: stroke .2s ease, fill .2s ease, opacity .2s ease;
    }

    .map-node.node-on-route {
      stroke: rgba(185,226,125,.82);
      fill: #0a150f;
    }

    .map-node.selected-start {
      fill: #0b1711;
      stroke: rgba(217,239,196,.94);
      stroke-width: 2.2;
    }

    .map-node.selected-end {
      fill: #101b0d;
      stroke: rgba(164,222,73,.98);
      stroke-width: 2.3;
      animation: targetNodeBreath 4.8s ease-in-out infinite;
      transform-box: fill-box;
      transform-origin: center;
    }

    @keyframes targetNodeBreath {
      0%, 72%, 100% { transform: scale(1); filter: drop-shadow(0 0 0 rgba(118,185,0,0)); }
      84% { transform: scale(1.12); filter: drop-shadow(0 0 5px rgba(118,185,0,.24)); }
    }

    .map-node.selectable:hover {
      r: 5px;
      fill: #0d1a13;
      stroke: rgba(188,232,124,.96);
      stroke-width: 2.1;
    }

    .map-label {
      fill: rgba(205,218,208,.55);
      font-size: 11.5px;
      font-weight: 620;
      paint-order: stroke;
      stroke: rgba(6,14,10,.92);
      stroke-width: 3px;
      stroke-linejoin: round;
      transition: fill .28s ease, opacity .28s ease;
    }

    .map-label.major,
    .map-label.minor {
      font-size: 11.5px;
      font-weight: 620;
    }

    .map-label.map-label-muted { fill: rgba(199,213,203,.50); }
    .map-label.map-label-route { fill: rgba(221,232,223,.75); }
    .map-label.map-label-endpoint { fill: rgba(242,247,243,.96); }

    .map-label.target-label {
      animation: targetLabelBreath 4.8s ease-in-out infinite;
    }

    @keyframes targetLabelBreath {
      0%,72%,100% { fill: rgba(242,247,243,.92); }
      84% { fill: rgba(226,247,199,1); }
    }

    .map-landmark-icon {
      opacity: .42;
      pointer-events: none;
    }

    .map-landmark-icon path,
    .map-landmark-icon circle,
    .map-landmark-icon rect {
      fill: none;
      stroke: rgba(191,217,198,.58);
      stroke-width: 1.25;
      stroke-linecap: round;
      stroke-linejoin: round;
    }

    .map-tooltip {
      position: absolute;
      z-index: 8;
      min-width: 142px;
      max-width: 190px;
      padding: 9px 11px 10px;
      border: 1px solid rgba(174,215,137,.22);
      border-radius: 10px;
      background: rgba(7,15,11,.92);
      box-shadow: 0 14px 36px rgba(0,0,0,.26);
      backdrop-filter: blur(12px);
      -webkit-backdrop-filter: blur(12px);
      pointer-events: none;
      opacity: 0;
      transform: translate(8px, 8px) scale(.97);
      transition: opacity .16s ease, transform .16s ease;
    }

    .map-tooltip.is-visible {
      opacity: 1;
      transform: translate(8px, 8px) scale(1);
    }

    .map-tooltip strong,
    .map-tooltip span {
      display: block;
    }

    .map-tooltip strong {
      margin-bottom: 4px;
      color: rgba(242,247,243,.94);
      font-size: 12px;
      font-weight: 680;
    }

    .map-tooltip span {
      color: rgba(181,203,188,.62);
      font-size: 10px;
      letter-spacing: .02em;
    }

    .vehicle-dot {
      fill: rgba(154,207,77,.92);
      stroke: rgba(235,243,236,.88);
      stroke-width: 1.35;
      filter: drop-shadow(0 0 4px rgba(118,185,0,.22));
      animation: vehicleSoftPulse 6.2s ease-in-out infinite;
    }

    @keyframes vehicleSoftPulse {
      0%,72%,100% { filter: drop-shadow(0 0 3px rgba(118,185,0,.16)); stroke-width: 1.35; }
      84% { filter: drop-shadow(0 0 7px rgba(118,185,0,.28)); stroke-width: 1.75; }
    }

    @media (prefers-reduced-motion: reduce) {
      .route-current-flow,
      .map-node.selected-end,
      .map-label.target-label,
      .vehicle-dot {
        animation: none !important;
      }
    }
'''

style_end = "\n  </style>\n</head>"
if style_end not in text:
    raise SystemExit("style end anchor not found")
text = text.replace(style_end, css + style_end, 1)

# Replace road rendering with zone + junction rendering helpers.
road_block = r'''    function campusZonesForMap() {
      if (state.map === "thu") {
        return [
          {x:105, y:220, w:210, h:128, label:"WEST ACADEMIC"},
          {x:382, y:220, w:205, h:138, label:"CENTRAL CAMPUS"},
          {x:680, y:220, w:112, h:138, label:"EAST CAMPUS"},
          {x:392, y:78, w:216, h:68, label:"SPORTS NORTH"},
          {x:398, y:427, w:354, h:58, label:"SOUTH CAMPUS"}
        ];
      }
      return [
        {x:220, y:118, w:365, h:165, label:"ACADEMIC CORE"},
        {x:250, y:330, w:245, h:118, label:"CAMPUS CENTRE"},
        {x:520, y:205, w:185, h:105, label:"RESIDENTIAL"},
        {x:585, y:315, w:150, h:112, label:"SPORTS"}
      ];
    }

    function drawCampusZones(group) {
      campusZonesForMap().forEach(zone => {
        group.appendChild(svgEl("rect", {
          x: zone.x, y: zone.y, width: zone.w, height: zone.h,
          rx: 12, ry: 12, class: "campus-zone"
        }));
        const label = svgEl("text", {
          x: zone.x + 12,
          y: zone.y + 18,
          class: "campus-zone-label"
        });
        label.textContent = zone.label;
        group.appendChild(label);
      });
    }

    function nodeMeta(node) {
      const label = String(node.label || "").toLowerCase();
      if (label.includes("医院") || label.includes("hospital")) return {category:"Medical", icon:"medical"};
      if (label.includes("图书馆") || label.includes("library")) return {category:"Library", icon:"library"};
      if (label.includes("宿舍") || label.includes("housing")) return {category:"Residence", icon:"home"};
      if (label.includes("操场") || label.includes("sports") || label.includes("track")) return {category:"Sports", icon:"sports"};
      if (label.includes("楼") || label.includes("学堂") || label.includes("书院") || label.includes("auditorium")) return {category:"Academic", icon:null};
      return {category:"Campus node", icon:null};
    }

    function drawLandmarkIcon(group, node) {
      const meta = nodeMeta(node);
      if (!meta.icon) return;
      const g = svgEl("g", {
        class: "map-landmark-icon",
        transform: `translate(${node.x + 11} ${node.y + 11})`,
        "aria-hidden": "true"
      });

      if (meta.icon === "medical") {
        g.appendChild(svgEl("path", {d:"M -4 0 H 4 M 0 -4 V 4"}));
      } else if (meta.icon === "library") {
        g.appendChild(svgEl("path", {d:"M -5 -4 Q -2 -5 0 -3 V 4 Q -2 2 -5 3 Z M 5 -4 Q 2 -5 0 -3 V 4 Q 2 2 5 3 Z"}));
      } else if (meta.icon === "home") {
        g.appendChild(svgEl("path", {d:"M -5 0 L 0 -4 L 5 0 V 5 H -5 Z M -1 5 V 1 H 2 V 5"}));
      } else if (meta.icon === "sports") {
        g.appendChild(svgEl("path", {d:"M -5 0 C -5 -3 -2 -4 0 -4 C 3 -4 5 -3 5 0 C 5 3 2 4 0 4 C -3 4 -5 3 -5 0 Z M -2 0 H 2"}));
      }
      group.appendChild(g);
    }

    function estimateNodeMinutes(map, nodeId) {
      if (nodeId === state.pickup) return 0;
      const pathIds = dijkstra(map, state.pickup, nodeId, null);
      if (pathIds.length < 2) return null;
      const distance = pathIds.slice(1).reduce((sum, id, i) => {
        const edge = edgeByNodes(map, pathIds[i], id);
        return sum + (edge ? getEdgeWeight(map, edge, null) : 0);
      }, 0);
      return Math.max(1, Math.round(distance / 80));
    }

    function positionMapTooltip(event, node, map) {
      const tooltip = document.getElementById("mapTooltip");
      if (!tooltip) return;
      const rect = mapStage.getBoundingClientRect();
      const meta = nodeMeta(node);
      const mins = estimateNodeMinutes(map, node.id);
      tooltip.querySelector("strong").textContent = node.label;
      tooltip.querySelector("span").textContent = mins === 0
        ? `${meta.category} · Current location`
        : `${meta.category} · ETA ${mins ?? "—"} min`;
      tooltip.style.left = `${Math.min(rect.width - 190, Math.max(8, event.clientX - rect.left + 10))}px`;
      tooltip.style.top = `${Math.min(rect.height - 74, Math.max(8, event.clientY - rect.top + 10))}px`;
      tooltip.classList.add("is-visible");
      tooltip.setAttribute("aria-hidden", "false");
    }

    function hideMapTooltip() {
      const tooltip = document.getElementById("mapTooltip");
      if (!tooltip) return;
      tooltip.classList.remove("is-visible");
      tooltip.setAttribute("aria-hidden", "true");
    }

    function drawRoadNetwork(map) {
      const zones = document.getElementById("mapZones");
      const junctions = document.getElementById("roadJunctions");
      clear(zones);
      clear(junctions);
      drawCampusZones(zones);
      map.roads.forEach((d) => appendPath(baseRoads, d, "base-road-bed"));
      map.roads.forEach((d) => appendPath(baseRoads, d, "base-road"));
      map.nodes.forEach(node => {
        junctions.appendChild(svgEl("circle", {
          cx: node.x, cy: node.y, r: 3.8, class: "road-junction"
        }));
      });
    }
'''

text, n = re.subn(
    r'    function drawRoadNetwork\(map\) \{.*?\n    \}\n\n(?=    function drawNodes)',
    road_block + "\n",
    text,
    count=1,
    flags=re.S,
)
if n != 1:
    raise SystemExit(f"drawRoadNetwork replacement count={n}")

nodes_block = r'''    function drawNodes(map, activeIds) {
      const activeSet = new Set(activeIds);

      map.nodes.forEach((node) => {
        const isStart = node.id === state.pickup;
        const isEnd = node.id === state.dropoff;
        const isRoute = activeSet.has(node.id);

        let cls = "map-node";
        if (node.selectable) cls += " selectable";
        if (isRoute) cls += " node-on-route";
        if (isStart) cls += " selected-start";
        if (isEnd) cls += " selected-end";

        const circle = svgEl("circle", {
          cx: node.x,
          cy: node.y,
          r: isStart || isEnd ? 6.2 : (isRoute ? 4.2 : 3.7),
          class: cls,
          tabindex: node.selectable ? 0 : -1,
          role: node.selectable ? "button" : "img",
          "aria-label": node.selectable ? `${node.label}，点击设置${state.clickMode === "pickup" ? "起点" : "终点"}` : node.label
        });

        if (node.selectable) {
          circle.addEventListener("click", () => handleNodePick(node.id));
          circle.addEventListener("keydown", (event) => {
            if (event.key === "Enter" || event.key === " ") {
              event.preventDefault();
              handleNodePick(node.id);
            }
          });
          circle.addEventListener("pointerenter", event => positionMapTooltip(event, node, map));
          circle.addEventListener("pointermove", event => positionMapTooltip(event, node, map));
          circle.addEventListener("pointerleave", hideMapTooltip);
          circle.addEventListener("blur", hideMapTooltip);
        }

        nodesGroup.appendChild(circle);
        drawLandmarkIcon(nodesGroup, node);

        let labelClass = `map-label ${node.cls || ""}`.trim();
        if (isStart || isEnd) labelClass += " map-label-endpoint";
        else if (isRoute) labelClass += " map-label-route";
        else labelClass += " map-label-muted";
        if (isEnd) labelClass += " target-label";

        appendText(
          nodesGroup,
          node.x + node.dx,
          node.y + node.dy,
          node.label,
          labelClass
        );
      });
    }
'''

text, n = re.subn(
    r'    function drawNodes\(map, activeIds\) \{.*?\n    \}\n\n(?=    function handleNodePick)',
    nodes_block + "\n",
    text,
    count=1,
    flags=re.S,
)
if n != 1:
    raise SystemExit(f"drawNodes replacement count={n}")

animate_block = r'''    function animateVehicle(path, token, traveledPath = null) {
      cancelAnimationFrame(routeAnimationFrame);

      const length = path.getTotalLength();
      if (!Number.isFinite(length) || length <= 1) return;

      const duration = Math.max(9000, Math.min(20000, length * 24));
      const startTime = performance.now();

      function frame(now) {
        if (token !== routeAnimationToken) return;

        const progress = ((now - startTime) % duration) / duration;
        const point = path.getPointAtLength(progress * length);

        vehicleDot.setAttribute("cx", point.x.toFixed(2));
        vehicleDot.setAttribute("cy", point.y.toFixed(2));

        if (traveledPath) {
          const traveled = Math.max(0, progress * length);
          traveledPath.style.strokeDasharray = `${traveled.toFixed(2)} ${length.toFixed(2)}`;
          traveledPath.style.strokeDashoffset = "0";
        }

        routeAnimationFrame = requestAnimationFrame(frame);
      }

      routeAnimationFrame = requestAnimationFrame(frame);
    }
'''

text, n = re.subn(
    r'    function animateVehicle\(path, token\) \{.*?\n    \}\n\n(?=    function animateRouteDraw)',
    animate_block + "\n",
    text,
    count=1,
    flags=re.S,
)
if n != 1:
    raise SystemExit(f"animateVehicle replacement count={n}")

render_block = r'''    function renderMap() {
      const map = maps[state.map];
      const blockedEdge = state.congested ? state.congestionEdge : null;
      const nodePath = dijkstra(map, state.pickup, state.dropoff, blockedEdge);

      routeAnimationToken += 1;
      const token = routeAnimationToken;

      routeSvg.setAttribute("viewBox", map.viewBox);
      clear(baseRoads);
      clear(currentRoute);
      clear(oldRoute);
      clear(anomalyRoad);
      clear(nodesGroup);
      hideMapTooltip();

      drawRoadNetwork(map);

      if (nodePath.length < 2) {
        if (routeText) routeText.textContent = "—";
        costText.textContent = "—";
        etaText.textContent = "—";
        routeSummary.textContent = "当前两点之间没有可用路径。";
        vehicleDot.setAttribute("visibility", "hidden");
        drawNodes(map, []);
        return;
      }

      const d = combineRouteGeometry(map, nodePath);
      const futurePath = appendPath(currentRoute, d, "route-road route-future");
      const traveledPath = appendPath(currentRoute, d, "route-road route-traveled");
      appendPath(currentRoute, d, "route-road route-current-flow");

      const routeLength = futurePath.getTotalLength();
      traveledPath.style.strokeDasharray = `0 ${routeLength.toFixed(2)}`;
      traveledPath.style.strokeDashoffset = "0";

      if (state.congested && state.congestionEdge) {
        if (state.preCongestionPath && state.preCongestionPath.length >= 2) {
          const oldD = combineRouteGeometry(map, state.preCongestionPath);
          if (oldD) appendPath(oldRoute, oldD, "old-road");
        }

        const [a,b] = state.congestionEdge;
        const edge = edgeByNodes(map, a, b);
        if (edge) {
          appendPath(anomalyRoad, orientedEdgeD(map, edge, a, b), "anomaly-road");
        }
      }

      drawNodes(map, nodePath);
      mapStage.classList.toggle("congested", state.congested);

      const startNode = nodeById(map, state.pickup);
      const endNode = nodeById(map, state.dropoff);

      const totalCost = nodePath.slice(1).reduce((sum, id, i) => {
        const edge = edgeByNodes(map, nodePath[i], id);
        return sum + getEdgeWeight(map, edge, null);
      }, 0);

      if (routeText) routeText.textContent = nodePath.join("-");
      if (heroRoute) heroRoute.textContent = nodePath.join(" -> ");
      costText.textContent = Math.round(totalCost);
      etaText.textContent = `${Math.max(1.2, totalCost / 80).toFixed(1)}m`;
      edgeStatus.textContent = state.congested && state.congestionEdge
        ? `Rerouted · blocked ${state.congestionEdge[0]} → ${state.congestionEdge[1]}`
        : "Normal traffic";
      routeSummary.textContent = `${startNode.label} → ${endNode.label} · ${nodePath.length} nodes`;

      vehicleDot.setAttribute("cx", startNode.x);
      vehicleDot.setAttribute("cy", startNode.y);
      vehicleDot.setAttribute("visibility", "visible");

      if (!window.matchMedia("(prefers-reduced-motion: reduce)").matches) {
        animateVehicle(futurePath, token, traveledPath);
      }
    }
'''

text, n = re.subn(
    r'    function renderMap\(\) \{.*?\n    \}\n\n(?=    pickupSelect\.addEventListener)',
    render_block + "\n",
    text,
    count=1,
    flags=re.S,
)
if n != 1:
    raise SystemExit(f"renderMap replacement count={n}")

path.write_text(text, encoding="utf-8")
