// Mock hpdcache_pkg for standalone FetchFlare testbench compilation.
// Provides the minimal set of types/constants imported by the DUT modules.
package hpdcache_pkg;

    // Physical Memory Attributes (sub-struct of hpdcache_req_t)
    typedef struct packed {
        logic io;
        logic uncacheable;
    } hpdcache_pma_t;

    // Request operation type
    typedef logic [3:0] hpdcache_req_op_t;
    localparam hpdcache_req_op_t HPDCACHE_REQ_CMO_PREFETCH = 4'hF;

endpackage
