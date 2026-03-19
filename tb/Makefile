# Verilator testbench for CTA-aware FetchFlare prefetcher
#
# Usage:
#   make        # build
#   make run    # build and run
#   make wave   # open waveform (requires gtkwave)
#   make clean  # remove build artifacts

VERILATOR  ?= verilator
TOP_MODULE  = tb_top
OBJ_DIR     = obj_dir
SRC_DIR     = ../src
TB_DIR      = .

# Source file order matters: packages first, then leaf modules, then top
SV_SOURCES = \
    $(TB_DIR)/hpdcache_pkg.sv \
    $(SRC_DIR)/fetchflare_pkg.sv \
    $(SRC_DIR)/fetchflare_bram_based_fifo.sv \
    $(SRC_DIR)/fetchflare_thermo_gen_pref.sv \
    $(SRC_DIR)/fetchflare_arbiter_pref.sv \
    $(TB_DIR)/hpdcache_rrarb.sv \
    $(TB_DIR)/hpdcache_mux.sv \
    $(SRC_DIR)/fetchflare.sv \
    $(SRC_DIR)/fetchflare_arb.sv \
    $(SRC_DIR)/fetchflare_wrapper.sv \
    $(TB_DIR)/tb_top.sv

CPP_SOURCES = $(TB_DIR)/sim_main.cpp

# Use small table/FIFO for faster simulation (overrides defaults in SV)
DEFINES = \
    -DPREFETCHER_TABLE_SIZE=8 \
    -DHPDC_PREFETCHER_FIFO=4 \
    -DCTA_ID_WIDTH=4

VERILATOR_FLAGS = \
    --cc --exe --trace --build -j 0 \
    --top-module $(TOP_MODULE) \
    -Wno-fatal \
    $(DEFINES)

.PHONY: all build run wave clean

all: build

build:
	$(VERILATOR) $(VERILATOR_FLAGS) $(SV_SOURCES) $(CPP_SOURCES)

run: build
	./$(OBJ_DIR)/V$(TOP_MODULE)

wave: run
	gtkwave tb_fetchflare.vcd &

clean:
	rm -rf $(OBJ_DIR) tb_fetchflare.vcd
