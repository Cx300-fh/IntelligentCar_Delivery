const MAPS = {
  1: {
    id: 1,
    name: 'THU',
    nodes: {
      1: 'ZJCC',
      2: 'LKL',
      3: 'TSG',
      4: 'SSM',
      5: 'DDCC',
      6: 'XYY',
      7: 'XSSS',
      8: 'DM',
      9: 'DLT',
      10: 'XQH',
      11: 'ZYZL',
      12: 'A',
      13: 'ZLY',
      14: 'KJDL'
    },
    // Straight-line pixel coordinates carried over from the UI prototype's
    // campus map (UI.html NODES) so edge weights below can be computed as
    // Euclidean distance instead of a separately-maintained constant.
    coords: {
      1: { x: 803, y: 30 }, 2: { x: 350, y: 242 }, 3: { x: 560, y: 242 },
      4: { x: 801, y: 242 }, 5: { x: 1154, y: 242 }, 6: { x: 147, y: 465 },
      7: { x: 801, y: 465 }, 8: { x: 1269, y: 465 }, 9: { x: 555, y: 662 },
      10: { x: 801, y: 662 }, 11: { x: 1064, y: 662 }, 12: { x: 1269, y: 662 },
      13: { x: 740, y: 870 }, 14: { x: 1004, y: 870 }
    },
    // 边权重直接抄自车端 dijkstra.cpp init_thu()，是实测的道路长度。
    // 车端跑自己的 Dijkstra、不执行 server_suggested_path，所以后端必须用同一张图，
    // 否则网页上画出来的路线就不是小车实际会走的路线。
    // 不要改回用 coords 算欧氏距离：真实道路要绕 UI 里的隐藏路口（jBumpL/jBumpTL 等）
    // 拐直角，直线距离会把这些 L 型边低估最多 4 倍。
    // 1-4 是故意不加的：它在 UI 里的几何是 zijing -> jBumpTL -> jBumpL -> sushimin，
    // 而 jBumpL 就是图书馆路口，所以这条“路”其实就是 1-3-4，
    // 单独加一条边等于凭空造出一条小车永远不会走的捷径。
    edges: [
      [1, 3, 150], [1, 5, 150], [2, 3, 75], [2, 6, 130],
      [3, 4, 85], [4, 5, 85], [4, 7, 65], [5, 8, 140],
      [6, 9, 205], [7, 10, 65], [8, 12, 65], [9, 10, 85],
      [9, 13, 150], [10, 11, 85], [11, 12, 75], [12, 14, 140],
      [13, 14, 85]
    ]
  },
  2: {
    id: 2,
    name: 'SUTD',
    nodes: {
      1: 'A',
      2: 'B',
      3: 'C',
      4: 'D',
      5: 'E',
      6: 'F',
      7: 'LIB',
      8: 'AUD',
      9: 'SSH',
      10: 'CC',
      11: 'POOL',
      12: 'SRC'
    },
    edges: [
      [1, 2, 209], [1, 3, 149], [1, 10, 80], [2, 11, 114],
      [2, 12, 70], [3, 4, 80], [3, 7, 63], [4, 6, 177],
      [4, 8, 63], [5, 8, 63], [5, 9, 75], [5, 10, 80],
      [5, 12, 65], [6, 9, 70], [6, 11, 74], [7, 10, 63]
    ]
  }
};

function edgeKey(mapId, from, to) {
  const a = Math.min(Number(from), Number(to));
  const b = Math.max(Number(from), Number(to));
  return `${Number(mapId)}:${a}:${b}`;
}

class MapWeights {
  constructor(config) {
    this.config = config;
    this.conditions = new Map();
  }

  loadConditions(conditions) {
    for (const condition of conditions || []) {
      this.conditions.set(
        edgeKey(condition.map_id, condition.from, condition.to),
        {
          map_id: Number(condition.map_id),
          from: Number(condition.from),
          to: Number(condition.to),
          manual_penalty: Number(condition.manual_penalty || 0),
          dynamic_penalty: Number(condition.dynamic_penalty || 0),
          blocked: Boolean(condition.blocked),
          updated_at: condition.updated_at || new Date().toISOString()
        }
      );
    }
  }

  getMaps() {
    return MAPS;
  }

  // 边自带权重时一律优先用它：那是车端实测的道路长度，小车就是按这个规划的。
  // 只有当某条边没带权重时，才退回用 coords 算欧氏距离——那种算法会把绕隐藏路口
  // 的 L 型道路当成直线，是不准的，仅作兜底。
  baseDistance(mapId, from, to, storedDistance) {
    const stored = Number(storedDistance);
    if (Number.isFinite(stored) && stored > 0) return stored;
    const map = MAPS[Number(mapId)];
    const coords = map && map.coords;
    const a = coords && coords[Number(from)];
    const b = coords && coords[Number(to)];
    if (a && b) return Math.hypot(a.x - b.x, a.y - b.y);
    return 0;
  }

  validateEdge(mapId, from, to) {
    const map = MAPS[Number(mapId)];
    if (!map) return false;
    return map.edges.some(([a, b]) => {
      return (a === Number(from) && b === Number(to)) ||
        (a === Number(to) && b === Number(from));
    });
  }

  setEdgeCondition(mapId, edge) {
    if (!this.validateEdge(mapId, edge.from, edge.to)) {
      throw new Error(`invalid edge for map ${mapId}: ${edge.from}-${edge.to}`);
    }

    const condition = {
      map_id: Number(mapId),
      from: Number(edge.from),
      to: Number(edge.to),
      manual_penalty: Number(edge.manual_penalty || 0),
      dynamic_penalty: Number(edge.dynamic_penalty || 0),
      blocked: Boolean(edge.blocked),
      updated_at: new Date().toISOString()
    };

    this.conditions.set(edgeKey(mapId, edge.from, edge.to), condition);
    return condition;
  }

  getEdgeCondition(mapId, from, to) {
    return this.conditions.get(edgeKey(mapId, from, to)) || {
      map_id: Number(mapId),
      from: Number(from),
      to: Number(to),
      manual_penalty: 0,
      dynamic_penalty: 0,
      blocked: false
    };
  }

  listEdgeConditions(mapId) {
    const map = MAPS[Number(mapId)];
    if (!map) return [];
    return map.edges.map(([from, to, storedDistance]) => ({
      ...this.getEdgeCondition(mapId, from, to),
      base_distance: this.baseDistance(mapId, from, to, storedDistance)
    }));
  }

  async refreshDynamicWeights(mapId, context = {}) {
    const map = MAPS[Number(mapId)];
    if (!map) throw new Error(`invalid map_id: ${mapId}`);

    const provider = this.config.trafficProvider === 'mock'
      ? this.mockTrafficProvider.bind(this)
      : this.mockTrafficProvider.bind(this);

    const updates = await provider(map, context);
    return updates.map((update) => {
      const current = this.getEdgeCondition(map.id, update.from, update.to);
      return this.setEdgeCondition(map.id, {
        from: update.from,
        to: update.to,
        manual_penalty: current.manual_penalty,
        dynamic_penalty: update.dynamic_penalty,
        blocked: update.blocked || current.blocked
      });
    });
  }

  async mockTrafficProvider(map, context) {
    const hour = Number.isFinite(Number(context.hour))
      ? Number(context.hour)
      : new Date().getHours();
    const peak = (hour >= 8 && hour <= 10) || (hour >= 17 && hour <= 19);
    const weather = String(context.weather || 'normal').toLowerCase();
    const weatherPenalty = weather === 'rain' || weather === 'storm' ? 25 : 0;

    return map.edges.map(([from, to, storedDistance], index) => {
      const baseDistance = this.baseDistance(map.id, from, to, storedDistance);
      const congestionPenalty = peak && index % 3 === 0 ? Math.round(baseDistance * 0.2) : 0;
      return {
        from,
        to,
        dynamic_penalty: congestionPenalty + weatherPenalty,
        blocked: false
      };
    });
  }
}

module.exports = { MapWeights, MAPS };
