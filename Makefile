# Parallel GA -- University Timetabling
# ======================================
#
# Build & run a single-host MPI process: just `make` then `make run`.
# For a multi-node cluster: source your cluster's MPI environment first,
# create a `nodes` hostfile, then `mpiexec -f nodes -n N ./timetable_ga DATA_DIR/`.

CC       = mpicc
CFLAGS   = -std=c99 -Wall -Wextra -Wpedantic -O2
LDFLAGS  = -lm -Wl,--allow-shlib-undefined

SRC_DIR  = src
OBJ_DIR  = obj
SRCS     = $(wildcard $(SRC_DIR)/*.c)
OBJS     = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
DEPS     = $(OBJS:.o=.d)
TARGET   = timetable_ga

.PHONY: all run run-parallel benchmark plot demo archive clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Run locally with a single MPI process on the small example dataset (100 events)
run: $(TARGET)
	mpiexec -n 1 ./$(TARGET) data/simple_n100/

# Run with 4 MPI processes (island model, single host)
run-parallel: $(TARGET)
	mpiexec -n 4 ./$(TARGET) data/simple_n100/

# Run benchmark suite (5 runs per configuration: n=1,2,4,8,16)
benchmark: $(TARGET)
	bash benchmark.sh

# Regenerate the speedup / quality_scaling charts used by the report
plot:
	python3 plots/generate_real_pngs.py
	@echo "Charts regenerated in plots/ (speedup_real.png, quality_scaling_real.png)"

# Local demo: convert dataset, build SQLite, start the visualisation webapp
demo:
	bash demo.sh

# Restore directory to initial state (build artifacts + generated outputs)
clean:
	rm -rf $(OBJ_DIR) $(TARGET) timetable.txt schedule.csv convergence_rank*.csv nodes

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
