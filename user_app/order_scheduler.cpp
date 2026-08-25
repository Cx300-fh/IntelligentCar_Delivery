#include "order_scheduler.hpp"
#include "navigation.hpp"

OrderScheduler order_scheduler;

OrderScheduler::OrderScheduler()
    : next_stop_index(0), active_map_id(MAP_NONE), pending_action(false),
      pending_order_id(0), pending_stop_action(ROUTE_STOP_PICKUP),
      pending_text_zh(""), pending_text_en(""),
      dijkstra(nullptr), nav_fsm(nullptr) {
}

void OrderScheduler::init(Dijkstra* planner, NavigationFSM* nav) {
    dijkstra = planner;
    nav_fsm = nav;
    orders.clear();
    route.clear();
    next_stop_index = 0;
    active_map_id = MAP_NONE;
    pending_action = false;
    pending_order_id = 0;
    pending_text_zh = "";
    pending_text_en = "";
    last_event = SchedulerEvent();
}

bool OrderScheduler::submit_order(int order_id, int map_id, int pickup_node, int dropoff_node) {
    if (!dijkstra || !nav_fsm || order_id <= 0 || find_order_index(order_id) >= 0 ||
        !is_valid_node(map_id, pickup_node) || !is_valid_node(map_id, dropoff_node) ||
        pickup_node == dropoff_node) {
        emit_event(SCHED_EVENT_NONE, order_id, 0, "订单无效", "Invalid order");
        return false;
    }

    DeliveryOrder order(order_id, map_id, pickup_node, dropoff_node);
    order.status = ORDER_STATUS_WAITING_PICKUP;
    orders.push_back(order);

    if (!has_active_route() && !pending_action && nav_fsm->is_navigating()) {
        orders.back().status = ORDER_STATUS_DELAYED;
        emit_event(SCHED_EVENT_ORDER_DELAYED, order_id, pickup_node, "订单已延后", "Order delayed");
        return true;
    }

    if (!has_active_route() && !pending_action) {
        active_map_id = map_id;
        append_order_route(order);
        orders.back().status = ORDER_STATUS_TO_PICKUP;
        emit_event(SCHED_EVENT_ORDER_ACCEPTED, order_id, pickup_node, "订单已接收", "Order accepted");
        start_next_leg();
        return true;
    }

    if (map_id == active_map_id && try_insert_order(order)) {
        emit_event(SCHED_EVENT_ROUTE_UPDATED, order_id, pickup_node, "路线已更新", "Route updated");
        return true;
    }

    orders.back().status = ORDER_STATUS_DELAYED;
    emit_event(SCHED_EVENT_ORDER_DELAYED, order_id, pickup_node, "订单已延后", "Order delayed");
    return true;
}

bool OrderScheduler::cancel_order(int order_id) {
    int idx = find_order_index(order_id);
    if (idx < 0) return false;

    orders[idx].status = ORDER_STATUS_CANCELLED;
    for (int i = (int)route.size() - 1; i >= next_stop_index; i--) {
        if (route[i].order_id == order_id) {
            route.erase(route.begin() + i);
        }
    }
    if (pending_order_id == order_id) {
        pending_action = false;
        pending_order_id = 0;
        pending_text_zh = "";
        pending_text_en = "";
    }
    emit_event(SCHED_EVENT_ORDER_CANCELLED, order_id, 0, "订单已取消", "Order cancelled");
    if (has_active_route()) {
        start_next_leg();
    } else if (nav_fsm) {
        nav_fsm->cancel_task();
    }
    return true;
}

OrderStatus OrderScheduler::get_order_status(int order_id) const {
    int idx = find_order_index(order_id);
    if (idx < 0) return ORDER_STATUS_NONE;
    return orders[idx].status;
}

bool OrderScheduler::update_edge_condition(int from, int to, int manual_penalty, int dynamic_penalty, bool blocked) {
    if (!dijkstra) return false;
    if (active_map_id == MAP_THU || active_map_id == MAP_SUTD) {
        dijkstra->set_map(active_map_id);
    }
    bool ok = dijkstra->set_edge_penalty(from, to, manual_penalty, dynamic_penalty, blocked);
    if (ok) {
        emit_event(SCHED_EVENT_ROUTE_UPDATED, 0, 0, "路线权重已更新", "Route weights updated");
        if (has_active_route() && !pending_action) start_next_leg();
    }
    return ok;
}

bool OrderScheduler::on_arrived_node(int node_id) {
    if (!has_active_route() && !pending_action) {
        start_next_deferred_order();
        return false;
    }
    if (!has_active_route() || pending_action) return false;
    RouteStop& stop = route[next_stop_index];
    if (stop.node_id != node_id) return false;

    int idx = find_order_index(stop.order_id);
    if (idx < 0) return false;

    pending_action = true;
    pending_order_id = stop.order_id;
    pending_stop_action = stop.action;

    if (stop.action == ROUTE_STOP_PICKUP) {
        orders[idx].status = ORDER_STATUS_WAITING_PUT_ITEM;
        pending_text_zh = "请放入物品";
        pending_text_en = "Please place the item";
        emit_event(SCHED_EVENT_PICKUP_ARRIVED, stop.order_id, node_id, pending_text_zh, pending_text_en);
    } else {
        orders[idx].status = ORDER_STATUS_WAITING_TAKE_ITEM;
        pending_text_zh = "请取走物品";
        pending_text_en = "Please take the item";
        emit_event(SCHED_EVENT_DROPOFF_ARRIVED, stop.order_id, node_id, pending_text_zh, pending_text_en);
    }

    return true;
}

bool OrderScheduler::confirm_station_action(void) {
    if (!pending_action) return false;

    int idx = find_order_index(pending_order_id);
    if (idx >= 0) {
        if (pending_stop_action == ROUTE_STOP_PICKUP) {
            orders[idx].status = ORDER_STATUS_IN_TRANSIT;
        } else {
            orders[idx].status = ORDER_STATUS_COMPLETED;
            emit_event(SCHED_EVENT_ORDER_COMPLETED, pending_order_id, route[next_stop_index].node_id,
                       "订单已完成", "Order completed");
        }
    }

    pending_action = false;
    pending_order_id = 0;
    pending_text_zh = "";
    pending_text_en = "";
    next_stop_index++;

    if (has_active_route()) {
        start_next_leg();
    } else {
        route.clear();
        next_stop_index = 0;
        active_map_id = MAP_NONE;
        start_next_deferred_order();
    }

    return true;
}

int OrderScheduler::get_current_route(int* out_nodes, int max_len) const {
    if (!out_nodes || max_len <= 0) return 0;
    int n = 0;
    for (int i = next_stop_index; i < (int)route.size() && n < max_len; i++) {
        out_nodes[n++] = route[i].node_id;
    }
    return n;
}

int OrderScheduler::get_current_route_len(void) const {
    int len = (int)route.size() - next_stop_index;
    return len > 0 ? len : 0;
}

int OrderScheduler::find_order_index(int order_id) const {
    for (int i = 0; i < (int)orders.size(); i++) {
        if (orders[i].order_id == order_id) return i;
    }
    return -1;
}

bool OrderScheduler::is_valid_node(int map_id, int node_id) {
    if (!dijkstra || node_id <= 0) return false;
    if (map_id != MAP_THU && map_id != MAP_SUTD) return false;
    int old_map = dijkstra->get_map();
    dijkstra->set_map(map_id);
    bool valid = node_id > 0 && node_id < dijkstra->get_node_count();
    dijkstra->set_map(old_map);
    return valid;
}

void OrderScheduler::emit_event(SchedulerEventType type, int order_id, int node_id,
                                const char* text_zh, const char* text_en) {
    last_event.type = type;
    last_event.order_id = order_id;
    last_event.node_id = node_id;
    last_event.text_zh = text_zh ? text_zh : "";
    last_event.text_en = text_en ? text_en : "";
    printf("[Order] %s: order=%d node=%d %s / %s\n",
           get_scheduler_event_name(type), order_id, node_id,
           last_event.text_zh, last_event.text_en);
}

int OrderScheduler::route_cost_from(int current_node, const std::vector<RouteStop>& candidate, int start_index) {
    if (!dijkstra || current_node <= 0) return INF;
    if (active_map_id == MAP_THU || active_map_id == MAP_SUTD) {
        dijkstra->set_map(active_map_id);
    }
    int total = 0;
    int from = current_node;

    for (int i = start_index; i < (int)candidate.size(); i++) {
        int dist = dijkstra->find_shortest_path(from, candidate[i].node_id);
        if (dist < 0) return INF;
        total += dist;
        if (total >= INF) return INF;
        from = candidate[i].node_id;
    }
    return total;
}

bool OrderScheduler::try_insert_order(const DeliveryOrder& order) {
    if (!dijkstra || !nav_fsm || pending_action || !has_active_route()) return false;

    const NavStatus& status = nav_fsm->get_status();
    int current_node = status.current_id;
    if (current_node <= 0) return false;

    int old_next = route[next_stop_index].node_id;
    int base_cost = route_cost_from(current_node, route, next_stop_index);
    if (base_cost >= INF) return false;

    int best_cost = INF;
    std::vector<RouteStop> best_route;

    for (int pickup_pos = next_stop_index; pickup_pos <= (int)route.size(); pickup_pos++) {
        for (int drop_pos = pickup_pos + 1; drop_pos <= (int)route.size() + 1; drop_pos++) {
            std::vector<RouteStop> candidate = route;
            candidate.insert(candidate.begin() + pickup_pos,
                             RouteStop(order.order_id, order.pickup_node, ROUTE_STOP_PICKUP));
            candidate.insert(candidate.begin() + drop_pos,
                             RouteStop(order.order_id, order.dropoff_node, ROUTE_STOP_DROPOFF));
            int cost = route_cost_from(current_node, candidate, next_stop_index);
            if (cost < best_cost) {
                best_cost = cost;
                best_route = candidate;
            }
        }
    }

    if (best_cost >= INF) return false;
    if (best_cost * 100 > base_cost * (100 + ORDER_DETOUR_PERCENT)) return false;

    route = best_route;
    int idx = find_order_index(order.order_id);
    if (idx >= 0) orders[idx].status = ORDER_STATUS_WAITING_PICKUP;

    if (has_active_route() && route[next_stop_index].node_id != old_next) {
        start_next_leg();
    }
    return true;
}

void OrderScheduler::append_order_route(const DeliveryOrder& order) {
    route.push_back(RouteStop(order.order_id, order.pickup_node, ROUTE_STOP_PICKUP));
    route.push_back(RouteStop(order.order_id, order.dropoff_node, ROUTE_STOP_DROPOFF));
}

void OrderScheduler::start_next_leg(void) {
    if (!nav_fsm || !has_active_route() || pending_action) return;

    RouteStop& stop = route[next_stop_index];
    int idx = find_order_index(stop.order_id);
    if (idx >= 0) {
        orders[idx].status = (stop.action == ROUTE_STOP_PICKUP) ?
            ORDER_STATUS_TO_PICKUP : ORDER_STATUS_IN_TRANSIT;
        active_map_id = orders[idx].map_id;
    }

    nav_fsm->start_leg(active_map_id, stop.node_id);
}

bool OrderScheduler::start_next_deferred_order(void) {
    for (int i = 0; i < (int)orders.size(); i++) {
        if (orders[i].status == ORDER_STATUS_DELAYED) {
            active_map_id = orders[i].map_id;
            append_order_route(orders[i]);
            orders[i].status = ORDER_STATUS_TO_PICKUP;
            emit_event(SCHED_EVENT_ORDER_ACCEPTED, orders[i].order_id, orders[i].pickup_node,
                       "延后订单开始执行", "Delayed order started");
            start_next_leg();
            return true;
        }
    }
    return false;
}

const char* get_order_status_name(OrderStatus status) {
    switch (status) {
        case ORDER_STATUS_NONE: return "NONE";
        case ORDER_STATUS_WAITING_PICKUP: return "WAITING_PICKUP";
        case ORDER_STATUS_TO_PICKUP: return "TO_PICKUP";
        case ORDER_STATUS_WAITING_PUT_ITEM: return "WAITING_PUT_ITEM";
        case ORDER_STATUS_IN_TRANSIT: return "IN_TRANSIT";
        case ORDER_STATUS_WAITING_TAKE_ITEM: return "WAITING_TAKE_ITEM";
        case ORDER_STATUS_COMPLETED: return "COMPLETED";
        case ORDER_STATUS_DELAYED: return "DELAYED";
        case ORDER_STATUS_CANCELLED: return "CANCELLED";
        case ORDER_STATUS_REJECTED: return "REJECTED";
        default: return "UNKNOWN";
    }
}

const char* get_scheduler_event_name(SchedulerEventType event) {
    switch (event) {
        case SCHED_EVENT_NONE: return "NONE";
        case SCHED_EVENT_ORDER_ACCEPTED: return "ORDER_ACCEPTED";
        case SCHED_EVENT_ORDER_DELAYED: return "ORDER_DELAYED";
        case SCHED_EVENT_ORDER_CANCELLED: return "ORDER_CANCELLED";
        case SCHED_EVENT_ROUTE_UPDATED: return "ROUTE_UPDATED";
        case SCHED_EVENT_PICKUP_ARRIVED: return "ORDER_PICKUP_ARRIVED";
        case SCHED_EVENT_DROPOFF_ARRIVED: return "ORDER_DROPOFF_ARRIVED";
        case SCHED_EVENT_ORDER_COMPLETED: return "ORDER_COMPLETED";
        default: return "UNKNOWN";
    }
}
