// Mock round-robin arbiter matching the interface used by fetchflare_arb.
module hpdcache_rrarb #(
    parameter int N = 4
)(
    input  logic         clk_i,
    input  logic         rst_ni,
    input  logic [N-1:0] req_i,
    output logic [N-1:0] gnt_o,
    input  logic         ready_i
);

    logic [N-1:0] mask_q;

    always_ff @(posedge clk_i or negedge rst_ni) begin
        if (!rst_ni)
            mask_q <= {N{1'b1}};
        else if (|gnt_o && ready_i)
            mask_q <= {gnt_o[N-2:0], 1'b0};
    end

    // Priority encode masked requests (round-robin fairness)
    logic [N-1:0] masked_req, gnt_masked, gnt_unmasked;

    assign masked_req = req_i & mask_q;

    always_comb begin
        gnt_masked = '0;
        for (int i = 0; i < N; i++)
            if (masked_req[i] && gnt_masked == '0)
                gnt_masked[i] = 1'b1;
    end

    always_comb begin
        gnt_unmasked = '0;
        for (int i = 0; i < N; i++)
            if (req_i[i] && gnt_unmasked == '0)
                gnt_unmasked[i] = 1'b1;
    end

    assign gnt_o = (|gnt_masked) ? gnt_masked : gnt_unmasked;

endmodule
