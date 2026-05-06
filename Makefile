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

# Run locally with single process on small example dataset (100 events)
run: $(TARGET)
	mpiexec -n 1 ./$(TARGET) data/simple_n100/

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

# Create submission archive: 26-1-Piotrowski-Przezdzik.tar.gz
ARCHIVE = 26-1-Piotrowski-Przezdzik
archive: clean
	@mkdir -p $(ARCHIVE)
	@cp -r src/ $(ARCHIVE)/src/
	@mkdir -p $(ARCHIVE)/data
	@cp -r data/simple_n100/. $(ARCHIVE)/data/
	@mkdir -p $(ARCHIVE)/results
	@cp results_v3/schedule_simple_n100_p1.csv $(ARCHIVE)/results/schedule_example_n1.csv
	@cp results_v3/schedule_simple_n100_p16.csv $(ARCHIVE)/results/schedule_example_n16.csv
	@cp results_v3/unitime_v2_n100.csv $(ARCHIVE)/results/benchmark_summary_n100.csv
	@cp Makefile $(ARCHIVE)/
	@cp README.txt $(ARCHIVE)/
	@if [ -f parallel_ga_timetabling_report.pdf ]; then \
		cp parallel_ga_timetabling_report.pdf $(ARCHIVE)/raport.pdf; \
	elif [ -f ../parallel-ga-timetabling-report/parallel_ga_timetabling_report.pdf ]; then \
		cp ../parallel-ga-timetabling-report/parallel_ga_timetabling_report.pdf $(ARCHIVE)/raport.pdf; \
	else echo "WARNING: PDF raportu nie znaleziony"; fi
	tar czf $(ARCHIVE).tar.gz $(ARCHIVE)/
	@rm -rf $(ARCHIVE)
	@echo "Archive created: $(ARCHIVE).tar.gz"
	@echo "Size: $$(du -h $(ARCHIVE).tar.gz | cut -f1)"

-include $(DEPS)
