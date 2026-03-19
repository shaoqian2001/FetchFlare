// Verilator C++ testbench for CTA-aware FetchFlare prefetcher.
//
// Tests:
//   1. Reset: outputs are deasserted after reset
//   2. Stride training: 7 snooped accesses with consistent stride produce prefetches
//   3. CTA isolation: two CTAs with same tag but different strides train independently
//   4. CTA invalidation: cta_done clears training state, preventing prefetches

#include "Vtb_top.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

#include <cstdio>
#include <cstdlib>
#include <cstdint>

// ---------------------------------------------------------------------------
//  Globals
// ---------------------------------------------------------------------------
static Vtb_top*       top      = nullptr;
static VerilatedVcdC* tfp      = nullptr;
static uint64_t       sim_time = 0;

// Simple 1-cycle-latency response generator
static bool    pending_rsp     = false;
static uint8_t pending_rsp_tid = 0;

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------

// Advance one clock cycle (negedge then posedge).
// Automatically generates cache responses for accepted requests.
static void tick() {
    // Drive response from previous cycle's accepted request
    if (pending_rsp) {
        top->hpdcache_rsp_valid_i = 1;
        top->hpdcache_rsp_tid_i   = pending_rsp_tid;
    } else {
        top->hpdcache_rsp_valid_i = 0;
    }
    pending_rsp = false;

    // Falling edge
    top->clk_i = 0;
    top->eval();
    if (tfp) tfp->dump(sim_time++);

    // Rising edge
    top->clk_i = 1;
    top->eval();
    if (tfp) tfp->dump(sim_time++);

    // Record accepted request for next-cycle response
    if (top->hpdcache_req_valid_o && top->hpdcache_req_ready_i) {
        pending_rsp     = true;
        pending_rsp_tid = top->hpdcache_req_tid_o;
    }
}

// Hold reset for several cycles, then release.
static void reset_dut() {
    top->rst_ni                        = 0;
    top->snoop_valid_i                 = 0;
    top->snoop_addr_i                  = 0;
    top->snoop_cta_id_i                = 0;
    top->hpdc_valid_i                  = 0;
    top->hpdc_prefetcher_cachelines_i  = 0;
    top->hpdc_prefetcher_inflight_i    = 0;
    top->hpdc_prefetcher_wait_i        = 0;
    top->hpdc_prefetcher_page_size_i   = 0;
    top->cta_done_valid_i              = 0;
    top->cta_done_id_i                 = 0;
    top->hpdcache_req_ready_i          = 1;
    top->hpdcache_rsp_valid_i          = 0;
    top->hpdcache_rsp_tid_i            = 0;
    top->hpdcache_req_sid_i            = 0;
    pending_rsp = false;

    for (int i = 0; i < 5; i++) tick();

    top->rst_ni = 1;
    // Wait for engine_rst_n to go high (1 cycle after wrapper reset release)
    for (int i = 0; i < 3; i++) tick();
}

// Issue a single snoop pulse for one cycle, then idle for one cycle.
static void snoop(uint32_t addr, uint8_t cta_id) {
    top->snoop_valid_i  = 1;
    top->snoop_addr_i   = addr;
    top->snoop_cta_id_i = cta_id;
    tick();
    top->snoop_valid_i = 0;
    tick();
}

// Run for up to max_cycles; return the number of prefetch requests observed.
static int run_and_count_prefetches(int max_cycles) {
    int count = 0;
    for (int i = 0; i < max_cycles; i++) {
        tick();
        if (top->hpdcache_req_valid_o && top->hpdcache_req_ready_i)
            count++;
    }
    return count;
}

// ---------------------------------------------------------------------------
//  Test cases
// ---------------------------------------------------------------------------
static int test_pass = 0;
static int test_fail = 0;

static void report(const char* name, bool pass) {
    if (pass) {
        printf("  [PASS] %s\n", name);
        test_pass++;
    } else {
        printf("  [FAIL] %s\n", name);
        test_fail++;
    }
}

// Test 1: After reset, no prefetch requests should be active.
static void test_reset() {
    reset_dut();
    bool ok = (top->hpdcache_req_valid_o == 0);
    report("Reset clears req_valid", ok);
}

// Test 2: Feed 7 snooped accesses with stride 0x40 (64B = 1 cache line)
// to the same tag/CTA. The RPT should progress through the training states
// (INITIAL -> STRIDE_DETECTION -> HIT1 -> HIT2 -> HIT3 -> PREFETCHING)
// and ultimately push a prefetch entry into the FIFO. An engine should
// then issue at least one prefetch request.
static void test_stride_training() {
    reset_dut();

    const uint32_t BASE   = 0xABCDE100u; // tag=0xABCDE, index=0x100
    const uint32_t STRIDE = 0x40u;       // 64 bytes
    const uint8_t  CTA    = 0;

    for (int i = 0; i < 7; i++)
        snoop(BASE + i * STRIDE, CTA);

    int prefetches = run_and_count_prefetches(100);
    report("Stride training generates prefetches", prefetches > 0);
}

// Test 3: Two CTAs access the SAME tag with DIFFERENT strides.
// With CTA-aware matching each CTA should train independently.
// Both should eventually trigger prefetches (we verify that by
// seeing more total prefetch requests than a single CTA would produce,
// or by seeing requests from distinct engine TIDs).
static void test_cta_isolation() {
    reset_dut();

    const uint32_t BASE     = 0x12345100u; // tag=0x12345, index=0x100
    const uint32_t STRIDE_A = 0x40u;       // CTA 0 stride
    const uint32_t STRIDE_B = 0x80u;       // CTA 1 stride
    const uint8_t  CTA_A    = 0;
    const uint8_t  CTA_B    = 1;

    // Interleave 7 accesses per CTA
    for (int i = 0; i < 7; i++) {
        snoop(BASE + i * STRIDE_A, CTA_A);
        snoop(BASE + i * STRIDE_B, CTA_B);
    }

    // Collect which engine TIDs issue requests
    bool tid_seen[16] = {};
    int  total = 0;
    for (int i = 0; i < 200; i++) {
        tick();
        if (top->hpdcache_req_valid_o && top->hpdcache_req_ready_i) {
            tid_seen[top->hpdcache_req_tid_o & 0xF] = true;
            total++;
        }
    }

    int distinct_tids = 0;
    for (int i = 0; i < 16; i++)
        if (tid_seen[i]) distinct_tids++;

    report("CTA isolation: both CTAs trigger prefetches (distinct TIDs >= 2)",
           distinct_tids >= 2);
}

// Test 4: Train CTA 2 for 5 accesses (reaches HIT2 state), then signal
// cta_done. Continue with 2 more accesses. Because the RPT entry was
// invalidated, training restarts from INITIAL and 2 accesses is nowhere
// near enough to reach PREFETCHING. No prefetch should appear.
static void test_cta_invalidation() {
    reset_dut();

    const uint32_t BASE   = 0x54321200u; // tag=0x54321, index=0x200
    const uint32_t STRIDE = 0x40u;
    const uint8_t  CTA    = 2;

    // 5 accesses: allocate + INITIAL + STRIDE_DETECTION + HIT1 + HIT2
    for (int i = 0; i < 5; i++)
        snoop(BASE + i * STRIDE, CTA);

    // Signal CTA completion
    top->cta_done_valid_i = 1;
    top->cta_done_id_i    = CTA;
    tick();
    top->cta_done_valid_i = 0;
    tick();

    // 2 more accesses with same tag/CTA (restarts from INITIAL, only reaches
    // STRIDE_DETECTION - far from PREFETCHING)
    for (int i = 0; i < 2; i++)
        snoop(BASE + i * STRIDE, CTA);

    int prefetches = run_and_count_prefetches(100);
    report("CTA invalidation prevents prefetches", prefetches == 0);
}

// ---------------------------------------------------------------------------
//  Main
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    top = new Vtb_top;

    // Optional VCD tracing (pass +trace on command line to enable)
    Verilated::traceEverOn(true);
    tfp = new VerilatedVcdC;
    top->trace(tfp, 99);
    tfp->open("tb_fetchflare.vcd");

    printf("=== FetchFlare CTA-aware prefetcher testbench ===\n");

    test_reset();
    test_stride_training();
    test_cta_isolation();
    test_cta_invalidation();

    printf("\nResults: %d passed, %d failed\n", test_pass, test_fail);

    tfp->close();
    delete tfp;
    delete top;

    return test_fail ? EXIT_FAILURE : EXIT_SUCCESS;
}
