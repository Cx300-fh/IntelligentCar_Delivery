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
    edges: [
      [1, 3, 65], [1, 5, 65], [2, 3, 75], [2, 6, 92],
      [4, 5, 85], [4, 7, 65], [5, 8, 96], [6, 9, 162],
      [9, 10, 85], [9, 13, 65], [7, 10, 65], [10, 11, 85],
      [11, 12, 75], [8, 12, 65], [12, 14, 75], [13, 14, 85]
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
    return map.edges.map(([from, to, base_distance]) => ({
      ...this.getEdgeCondition(mapId, from, to),
      base_distance
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

    return map.edges.map(([from, to, baseDistance], index) => {
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
