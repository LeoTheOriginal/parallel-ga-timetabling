# Parallel GA -- University Timetabling
# ======================================
#
# Before compiling, source the MPICH environment:
#   source /opt/nfs/config/source_mpich500.sh
#   source /opt/nfs/config/source_cuda121.sh
#   export MPIR_CVAR_ENABLE_GPU=0

CC       = mpicc
CFLAGS   = -std=c99 -Wall -Wextra -Wpedantic -O2
LDFLAGS  = -lm -Wl,--allow-shlib-undefined

SRC_DIR  = src
OBJ_DIR  = obj
SRCS     = $(wildcard $(SRC_DIR)/*.c)
OBJS     = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
DEPS     = $(OBJS:.o=.d)
TARGET   = timetable_ga

.PHONY: all run run-parallel benchmark plot report archive clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Run locally with single process (backward-compatible, no cluster needed)
run: $(TARGET)
	mpiexec -n 1 ./$(TARGET) data/

# Run with 4 MPI processes (island model)
run-parallel: $(TARGET)
	mpiexec -n 4 ./$(TARGET) data/

# Run with UniTime v2 dataset (filtered, multi-slot, ~1058 events)
run-unitime: $(TARGET)
	mpiexec -n 1 ./$(TARGET) data/unitime_v2_nogrupa/

# Run UniTime v2 with 4 MPI processes
run-unitime-parallel: $(TARGET)
	mpiexec -n 4 ./$(TARGET) data/unitime_v2_nogrupa/

# Run small subset (100 events, quick test)
run-unitime-100: $(TARGET)
	mpiexec -n 1 ./$(TARGET) data/unitime_v2_n100/

# Convert UniTime data with various subset sizes
convert-unitime:
	python3 scripts/convert_unitime_v2.py data/unitime/events_raw.csv data/unitime_v2_nogrupa/ --no-grupa --all-rooms --stats
	python3 scripts/convert_unitime_v2.py data/unitime/events_raw.csv data/unitime_v2/ --all-rooms --stats
	@for N in 100 200 400 600; do \
		python3 scripts/convert_unitime_v2.py data/unitime/events_raw.csv data/unitime_v2_n$${N}/ --no-grupa --all-rooms --subset $$N; \
	done
	@echo "All datasets generated."

# Run benchmark suite (5 runs per configuration: n=1,2,4,8,16)
benchmark: $(TARGET)
	bash benchmark.sh

# Generate performance charts
plot:
	gnuplot plots/speedup_v2.gp
	gnuplot plots/convergence_v2.gp
	gnuplot plots/quality_scaling.gp
	@echo "Charts generated in plots/"

# Start webapp (local demo)
demo:
	bash demo.sh

# Run on cluster
demo-cluster:
	bash demo.sh cluster

# Generate simple dataset
convert:
	python3 scripts/convert_simple.py data/unitime/events_raw.csv data/simple/ --no-grupa --stats

# Restore directory to initial state (build artifacts + generated outputs)
clean:
	rm -rf $(OBJ_DIR) $(TARGET) timetable.txt schedule.csv convergence_rank*.csv nodes

# Build PDF report (requires pdflatex)
report:
	$(MAKE) -C ../report

# Create submission archive: 26-2-piotrowski-przezdzik.tar.gz
ARCHIVE = 26-2-piotrowski-przezdzik
archive: clean
	@mkdir -p $(ARCHIVE)
	@cp -r src/ $(ARCHIVE)/src/
	@cp -r data/simple/ $(ARCHIVE)/data/
	@cp -r results_v3/ $(ARCHIVE)/results/
	@cp Makefile $(ARCHIVE)/
	@cp README.txt $(ARCHIVE)/
	@if [ -f raport.pdf ]; then cp raport.pdf $(ARCHIVE)/; else echo "WARNING: raport.pdf not found — copy it manually"; fi
	tar czf $(ARCHIVE).tar.gz $(ARCHIVE)/
	@rm -rf $(ARCHIVE)
	@echo "Archive created: $(ARCHIVE).tar.gz"
	@echo "Size: $$(du -h $(ARCHIVE).tar.gz | cut -f1)"

-include $(DEPS)
