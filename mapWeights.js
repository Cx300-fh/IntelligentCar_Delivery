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
    edges: [
      [1, 3], [1, 5], [2, 3], [2, 6],
      [4, 5], [4, 7], [5, 8], [6, 9],
      [9, 10], [9, 13], [7, 10], [10, 11],
      [11, 12], [8, 12], [12, 14], [13, 14],
      // These two roads are real on the campus map (through jBumpL /
      // jBumpTL — see UI.html's EDGES) but were missing here, which forced
      // every route near SSM through DDCC instead of the actual short way.
      [3, 4], [1, 4]
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

  // Euclidean distance between the two nodes' mapped coordinates when
  // available (currently THU / map 1); otherwise falls back to the
  // edge's own stored constant (older maps that carry no coordinates).
  baseDistance(mapId, from, to, storedDistance) {
    const map = MAPS[Number(mapId)];
    const coords = map && map.coords;
    const a = coords && coords[Number(from)];
    const b = coords && coords[Number(to)];
    if (a && b) return Math.hypot(a.x - b.x, a.y - b.y);
    return Math.max(0, Number(storedDistance || 0));
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
