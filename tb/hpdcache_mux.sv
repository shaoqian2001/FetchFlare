// Mock one-hot multiplexer matching the interface used by fetchflare_arb.
module hpdcache_mux #(
    parameter int NINPUT      = 4,
    parameter int DATA_WIDTH  = 128,
    parameter bit ONE_HOT_SEL = 1'b1
)(
    input  logic [NINPUT-1:0][DATA_WIDTH-1:0] data_i,
    input  logic [NINPUT-1:0]                 sel_i,
    output logic [DATA_WIDTH-1:0]             data_o
);

    always_comb begin
        data_o = '0;
        for (int i = 0; i < NINPUT; i++)
            if (sel_i[i])
                data_o = data_i[i];
    end

endmodule
