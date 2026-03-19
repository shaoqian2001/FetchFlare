// Testbench top for fetchflare_wrapper (run with Verilator).
// Provides concrete type parameters and exposes flat ports for C++ driving.

`define PREFETCHER_TABLE_SIZE 8
`define HPDC_PREFETCHER_FIFO  4
`define CTA_ID_WIDTH          4

module tb_top
import hpdcache_pkg::*;
import fetchflare_pkg::*;
(
    input  logic        clk_i,
    input  logic        rst_ni,

    // CSR configuration
    input  logic        hpdc_valid_i,
    input  logic [15:0] hpdc_prefetcher_cachelines_i,
    input  logic [15:0] hpdc_prefetcher_inflight_i,
    input  logic [15:0] hpdc_prefetcher_wait_i,
    input  logic [12:0] hpdc_prefetcher_page_size_i,

    // Snoop interface (flat)
    input  logic        snoop_valid_i,
    input  logic [31:0] snoop_addr_i,
    input  logic [3:0]  snoop_cta_id_i,

    // CTA lifecycle
    input  logic        cta_done_valid_i,
    input  logic [3:0]  cta_done_id_i,

    // DCache request output (flat)
    input  logic [3:0]  hpdcache_req_sid_i,
    output logic        hpdcache_req_valid_o,
    input  logic        hpdcache_req_ready_i,
    output logic [31:0] hpdcache_req_addr_o,
    output logic [3:0]  hpdcache_req_tid_o,

    // DCache response input (flat)
    input  logic        hpdcache_rsp_valid_i,
    input  logic [3:0]  hpdcache_rsp_tid_i
);

    // ---------------------------------------------------------------
    //  Concrete type definitions (TAG=20, INDEX=12, ADDR=32)
    // ---------------------------------------------------------------
    localparam int TB_TAG_W   = 20;
    localparam int TB_INDEX_W = 12;

    typedef logic [TB_TAG_W-1:0]   tb_tag_t;
    typedef logic [TB_INDEX_W-1:0] tb_offset_t;
    typedef logic [63:0]           tb_data_t;
    typedef logic [7:0]            tb_be_t;
    typedef logic [3:0]            tb_sid_t;
    typedef logic [3:0]            tb_tid_t;
    typedef logic [31:0]           tb_nline_t;
    typedef logic [7:0]            tb_set_t;

    typedef struct packed {
        hpdcache_pma_t    pma;          //  2b
        tb_tag_t          addr_tag;     // 20b
        logic             phys_indexed; //  1b
        logic             need_rsp;     //  1b
        tb_tid_t          tid;          //  4b
        tb_sid_t          sid;          //  4b
        logic [2:0]       size;         //  3b
        tb_be_t           be;           //  8b
        hpdcache_req_op_t op;           //  4b
        tb_data_t         wdata;        // 64b
        tb_offset_t       addr_offset;  // 12b
    } tb_req_t;                         // total 123b

    typedef struct packed {
        tb_tid_t tid;                   // 4b
    } tb_rsp_t;

    // ---------------------------------------------------------------
    //  Internal wiring
    // ---------------------------------------------------------------
    tb_req_t hpdcache_req;
    tb_rsp_t hpdcache_rsp;

    assign hpdcache_rsp.tid = hpdcache_rsp_tid_i;

    // Expose useful fields to C++ as flat outputs
    assign hpdcache_req_addr_o = {hpdcache_req.addr_tag, hpdcache_req.addr_offset};
    assign hpdcache_req_tid_o  = hpdcache_req.tid;

    // Pack array-typed snoop ports (NUM_SNOOP_PORTS=1)
    localparam type tb_addr_t = logic [TB_TAG_W + TB_INDEX_W - 1 : 0];

    logic       [0:0] snoop_valid_arr;
    tb_addr_t   [0:0] snoop_addr_arr;
    cta_id_t    [0:0] snoop_cta_id_arr;

    assign snoop_valid_arr[0]  = snoop_valid_i;
    assign snoop_addr_arr[0]   = snoop_addr_i;
    assign snoop_cta_id_arr[0] = snoop_cta_id_i;

    // ---------------------------------------------------------------
    //  DUT instantiation
    // ---------------------------------------------------------------
    fetchflare_wrapper #(
        .NUM_HW_PREFETCH     (4),
        .NUM_SNOOP_PORTS     (1),
        .CACHE_LINE_BYTES    (64),
        .hpdcache_tag_t      (tb_tag_t),
        .hpdcache_req_offset_t (tb_offset_t),
        .hpdcache_req_data_t (tb_data_t),
        .hpdcache_req_be_t   (tb_be_t),
        .hpdcache_req_sid_t  (tb_sid_t),
        .hpdcache_req_tid_t  (tb_tid_t),
        .hpdcache_req_t      (tb_req_t),
        .hpdcache_rsp_t      (tb_rsp_t),
        .hpdcache_nline_t    (tb_nline_t),
        .hpdcache_set_t      (tb_set_t)
    ) dut (
        .clk_i,
        .rst_ni,

        .hwpf_stride_base_o  (),  // status output, left unconnected
        .hpdc_valid_i,
        .hpdc_prefetcher_cachelines_i,
        .hpdc_prefetcher_inflight_i,
        .hpdc_prefetcher_wait_i,
        .hpdc_prefetcher_page_size_i,

        .snoop_valid_i       (snoop_valid_arr),
        .snoop_addr_i        (snoop_addr_arr),
        .snoop_cta_id_i      (snoop_cta_id_arr),

        .cta_done_valid_i,
        .cta_done_id_i,

        .hpdcache_req_sid_i  (tb_sid_t'(hpdcache_req_sid_i)),
        .hpdcache_req_valid_o,
        .hpdcache_req_ready_i,
        .hpdcache_req_o      (hpdcache_req),

        .hpdcache_rsp_valid_i,
        .hpdcache_rsp_i      (hpdcache_rsp)
    );

endmodule
