class RoutePlanner {
  constructor(mapWeights) {
    this.mapWeights = mapWeights;
  }

  validateNode(mapId, nodeId) {
    const map = this.mapWeights.getMaps()[Number(mapId)];
    return Boolean(map && map.nodes[Number(nodeId)]);
  }

  shortestPath(mapId, startNode, endNode) {
    const map = this.mapWeights.getMaps()[Number(mapId)];
    const start = Number(startNode);
    const end = Number(endNode);
    if (!map || !map.nodes[start] || !map.nodes[end]) {
      return { reachable: false, distance: Infinity, path: [] };
    }
    if (start === end) {
      return { reachable: true, distance: 0, path: [start] };
    }

    const graph = new Map();
    for (const node of Object.keys(map.nodes).map(Number)) graph.set(node, []);
    for (const [from, to, baseDistance] of map.edges) {
      const condition = this.mapWeights.getEdgeCondition(mapId, from, to);
      if (condition.blocked) continue;
      const weight = Math.max(0, Number(baseDistance)) +
        Math.max(0, Number(condition.manual_penalty || 0)) +
        Math.max(0, Number(condition.dynamic_penalty || 0));
      graph.get(from).push({ to, weight });
      graph.get(to).push({ to: from, weight });
    }

    const distances = new Map();
    const previous = new Map();
    const unvisited = new Set(graph.keys());
    for (const node of unvisited) distances.set(node, Infinity);
    distances.set(start, 0);

    while (unvisited.size > 0) {
      let current = null;
      let currentDistance = Infinity;
      for (const node of unvisited) {
        const value = distances.get(node);
        if (value < currentDistance) {
          current = node;
          currentDistance = value;
        }
      }
      if (current == null || currentDistance === Infinity) break;
      unvisited.delete(current);
      if (current === end) break;
      for (const edge of graph.get(current)) {
        if (!unvisited.has(edge.to)) continue;
        const candidate = currentDistance + edge.weight;
        if (candidate < distances.get(edge.to)) {
          distances.set(edge.to, candidate);
          previous.set(edge.to, current);
        }
      }
    }

    if (distances.get(end) === Infinity) {
      return { reachable: false, distance: Infinity, path: [] };
    }
    const path = [];
    for (let node = end; node != null; node = previous.get(node)) {
      path.unshift(node);
      if (node === start) break;
    }
    return { reachable: path[0] === start, distance: distances.get(end), path };
  }

  pathCost(mapId, path) {
    const map = this.mapWeights.getMaps()[Number(mapId)];
    const nodes = Array.isArray(path) ? path.map(Number) : [];
    if (!map || nodes.length === 0) {
      return { reachable: false, distance: Infinity };
    }
    if (nodes.length === 1) {
      return { reachable: Boolean(map.nodes[nodes[0]]), distance: 0 };
    }

    let distance = 0;
    for (let index = 0; index < nodes.length - 1; index++) {
      const from = nodes[index];
      const to = nodes[index + 1];
      const edge = map.edges.find(([left, right]) =>
        (Number(left) === from && Number(right) === to) ||
        (Number(left) === to && Number(right) === from)
      );
      if (!edge) return { reachable: false, distance: Infinity };
      const condition = this.mapWeights.getEdgeCondition(mapId, from, to);
      if (condition.blocked) return { reachable: false, distance: Infinity };
      distance += Math.max(0, Number(edge[2])) +
        Math.max(0, Number(condition.manual_penalty || 0)) +
        Math.max(0, Number(condition.dynamic_penalty || 0));
    }
    return { reachable: true, distance };
  }

  routeCost(mapId, startNode, stops) {
    let current = Number(startNode);
    let distance = 0;
    const fullPath = [current];
    for (const stop of stops) {
      const segment = this.shortestPath(mapId, current, stop.node_id);
      if (!segment.reachable) {
        return { reachable: false, distance: Infinity, path: [] };
      }
      distance += segment.distance;
      fullPath.push(...segment.path.slice(1));
      current = Number(stop.node_id);
    }
    return { reachable: true, distance, path: fullPath };
  }
}

module.exports = { RoutePlanner };
