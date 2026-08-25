#ifndef __ORDER_SCHEDULER_HPP
#define __ORDER_SCHEDULER_HPP

#include "dijkstra.hpp"
#include <vector>

#define ORDER_DETOUR_PERCENT 15

class NavigationFSM;

enum OrderStatus {
    ORDER_STATUS_NONE = 0,
    ORDER_STATUS_WAITING_PICKUP,
    ORDER_STATUS_TO_PICKUP,
    ORDER_STATUS_WAITING_PUT_ITEM,
    ORDER_STATUS_IN_TRANSIT,
    ORDER_STATUS_WAITING_TAKE_ITEM,
    ORDER_STATUS_COMPLETED,
    ORDER_STATUS_DELAYED,
    ORDER_STATUS_CANCELLED,
    ORDER_STATUS_REJECTED
};

enum RouteStopAction {
    ROUTE_STOP_PICKUP = 0,
    ROUTE_STOP_DROPOFF
};

enum SchedulerEventType {
    SCHED_EVENT_NONE = 0,
    SCHED_EVENT_ORDER_ACCEPTED,
    SCHED_EVENT_ORDER_DELAYED,
    SCHED_EVENT_ORDER_CANCELLED,
    SCHED_EVENT_ROUTE_UPDATED,
    SCHED_EVENT_PICKUP_ARRIVED,
    SCHED_EVENT_DROPOFF_ARRIVED,
    SCHED_EVENT_ORDER_COMPLETED
};

struct DeliveryOrder {
    int order_id;
    int map_id;
    int pickup_node;
    int dropoff_node;
    OrderStatus status;

    DeliveryOrder(int id=0, int map=0, int pickup=0, int dropoff=0)
        : order_id(id), map_id(map), pickup_node(pickup), dropoff_node(dropoff),
          status(ORDER_STATUS_NONE) {}
};

struct RouteStop {
    int order_id;
    int node_id;
    RouteStopAction action;

    RouteStop(int id=0, int node=0, RouteStopAction act=ROUTE_STOP_PICKUP)
        : order_id(id), node_id(node), action(act) {}
};

struct SchedulerEvent {
    SchedulerEventType type;
    int order_id;
    int node_id;
    const char* text_zh;
    const char* text_en;

    SchedulerEvent()
        : type(SCHED_EVENT_NONE), order_id(0), node_id(0),
          text_zh(""), text_en("") {}
};

class OrderScheduler {
public:
    OrderScheduler();

    void init(Dijkstra* planner, NavigationFSM* nav);
    bool submit_order(int order_id, int map_id, int pickup_node, int dropoff_node);
    bool cancel_order(int order_id);
    OrderStatus get_order_status(int order_id) const;
    bool update_edge_condition(int from, int to, int manual_penalty, int dynamic_penalty, bool blocked);

    bool on_arrived_node(int node_id);
    bool confirm_station_action(void);

    int get_current_route(int* out_nodes, int max_len) const;
    int get_current_route_len(void) const;
    const SchedulerEvent& get_last_event(void) const { return last_event; }
    const char* get_pending_action_text_zh(void) const { return pending_text_zh; }
    const char* get_pending_action_text_en(void) const { return pending_text_en; }
    bool has_pending_station_action(void) const { return pending_action; }
    bool has_active_route(void) const { return next_stop_index < (int)route.size(); }

private:
    std::vector<DeliveryOrder> orders;
    std::vector<RouteStop> route;
    int next_stop_index;
    int active_map_id;
    bool pending_action;
    int pending_order_id;
    RouteStopAction pending_stop_action;
    const char* pending_text_zh;
    const char* pending_text_en;
    Dijkstra* dijkstra;
    NavigationFSM* nav_fsm;
    SchedulerEvent last_event;

    int find_order_index(int order_id) const;
    bool is_valid_node(int map_id, int node_id);
    void emit_event(SchedulerEventType type, int order_id, int node_id,
                    const char* text_zh, const char* text_en);
    int route_cost_from(int current_node, const std::vector<RouteStop>& candidate, int start_index);
    bool try_insert_order(const DeliveryOrder& order);
    void append_order_route(const DeliveryOrder& order);
    void start_next_leg(void);
    bool start_next_deferred_order(void);
};

extern OrderScheduler order_scheduler;

const char* get_order_status_name(OrderStatus status);
const char* get_scheduler_event_name(SchedulerEventType event);

#endif
