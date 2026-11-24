(flow_vertex (flow_vertex_id) @name . (_)) @definition.node

(flow_stmt_subgraph (flow_vertex_id) @name) @definition.module

(flow_stmt_vertice (flow_node (flow_vertex (flow_vertex_id) @name)) [(flow_link_simplelink) (flow_link_arrowtext) (flow_link_middletext)]) @reference.edge

(flow_stmt_vertice [(flow_link_simplelink) (flow_link_arrowtext) (flow_link_middletext)] (flow_node (flow_vertex (flow_vertex_id) @name))) @reference.edge

(sequence_stmt_participant (sequence_actor) @name) @definition.node

(sequence_stmt_actor (sequence_actor) @name) @definition.node

(sequence_stmt_signal (sequence_actor) @name) @reference.edge

(class_stmt_class (class_name) @name) @definition.class

(class_stmt_relation (class_name) @name) @reference.edge

(state_stmt_simple (state_name) @name) @definition.node

(state_stmt_composite (state_name) @name) @definition.node

(state_stmt_arrow (state_name) @name) @reference.edge

(er_stmt_entity_block (er_entity_name) @name) @definition.class

(er_stmt_entity_relation (er_entity_name) @name) @reference.edge
