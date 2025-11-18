# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++23 -Wall -Wextra -O2
DEBUG_FLAGS = -g -DDEBUG
RELEASE_FLAGS = -O3 -DNDEBUG

# Directories
SRC_DIR = src
INCLUDE_DIR = include
TEST_DIR = tests
SCRIPT_DIR = scripts
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
TEST_OBJ_DIR = $(BUILD_DIR)/test_obj

# Output executables
MAIN_TARGET = monitor_app.out
TEST_CPU_TARGET = test_cpu.out
TEST_MEMORY_TARGET = test_memory.out
TEST_IO_TARGET = test_io.out

# Source files
MAIN_SOURCES = $(SRC_DIR)/main.cpp \
               $(SRC_DIR)/monitor.cpp \
               $(SRC_DIR)/cpu_monitor.cpp \
               $(SRC_DIR)/memory_monitor.cpp \
               $(SRC_DIR)/io_monitor.cpp \
               $(SRC_DIR)/network_monitor.cpp \
               $(SRC_DIR)/cgroup_analyzer.cpp \
               $(SRC_DIR)/namespace_analyzer.cpp

# Object files for main application
MAIN_OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(MAIN_SOURCES))

# Library sources (everything except main.cpp)
LIB_SOURCES = $(filter-out $(SRC_DIR)/main.cpp,$(MAIN_SOURCES))
LIB_OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(LIB_SOURCES))

# Test sources
TEST_CPU_SOURCES = $(TEST_DIR)/test_cpu.cpp
TEST_MEMORY_SOURCES = $(TEST_DIR)/test_memory.cpp
TEST_IO_SOURCES = $(TEST_DIR)/test_io.cpp

# Test objects
TEST_CPU_OBJECTS = $(patsubst $(TEST_DIR)/%.cpp,$(TEST_OBJ_DIR)/%.o,$(TEST_CPU_SOURCES))
TEST_MEMORY_OBJECTS = $(patsubst $(TEST_DIR)/%.cpp,$(TEST_OBJ_DIR)/%.o,$(TEST_MEMORY_SOURCES))
TEST_IO_OBJECTS = $(patsubst $(TEST_DIR)/%.cpp,$(TEST_OBJ_DIR)/%.o,$(TEST_IO_SOURCES))

# Include path
INCLUDES = -I$(INCLUDE_DIR)

# Libraries
LIBS = -pthread

# Default target
.PHONY: all
all: $(MAIN_TARGET)

# Create build directories
$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(TEST_OBJ_DIR):
	@mkdir -p $(TEST_OBJ_DIR)

# Main application
$(MAIN_TARGET): $(MAIN_OBJECTS) | $(OBJ_DIR)
	@echo "Linking main application..."
	$(CXX) $(CXXFLAGS) $(MAIN_OBJECTS) -o $@ $(LIBS)
	@echo "Built: $(MAIN_TARGET)"

# Library objects
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Test executables
$(TEST_CPU_TARGET): $(TEST_CPU_OBJECTS) $(LIB_OBJECTS) | $(TEST_OBJ_DIR)
	@echo "Linking CPU test..."
	$(CXX) $(CXXFLAGS) $(TEST_CPU_OBJECTS) $(LIB_OBJECTS) -o $@ $(LIBS)
	@echo "Built: $(TEST_CPU_TARGET)"

$(TEST_MEMORY_TARGET): $(TEST_MEMORY_OBJECTS) $(LIB_OBJECTS) | $(TEST_OBJ_DIR)
	@echo "Linking Memory test..."
	$(CXX) $(CXXFLAGS) $(TEST_MEMORY_OBJECTS) $(LIB_OBJECTS) -o $@ $(LIBS)
	@echo "Built: $(TEST_MEMORY_TARGET)"

$(TEST_IO_TARGET): $(TEST_IO_OBJECTS) $(LIB_OBJECTS) | $(TEST_OBJ_DIR)
	@echo "Linking I/O test..."
	$(CXX) $(CXXFLAGS) $(TEST_IO_OBJECTS) $(LIB_OBJECTS) -o $@ $(LIBS)
	@echo "Built: $(TEST_IO_TARGET)"

# Test objects
$(TEST_OBJ_DIR)/%.o: $(TEST_DIR)/%.cpp | $(TEST_OBJ_DIR)
	@echo "Compiling test $<..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Build all tests
.PHONY: tests
tests: $(TEST_CPU_TARGET) $(TEST_MEMORY_TARGET) $(TEST_IO_TARGET)
	@echo "All tests built successfully!"

# Debug build
.PHONY: debug
debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: clean $(MAIN_TARGET)
	@echo "Debug build completed"

# Release build
.PHONY: release
release: CXXFLAGS += $(RELEASE_FLAGS)
release: clean $(MAIN_TARGET)
	@echo "Release build completed"

# Run the main application
.PHONY: run
run: $(MAIN_TARGET)
	@echo "Running main application..."
	./$(MAIN_TARGET)

# Run tests
.PHONY: run-tests
run-tests: tests
	@echo "Running CPU test..."
	./$(TEST_CPU_TARGET)
	@echo "Running Memory test..."
	./$(TEST_MEMORY_TARGET)
	@echo "Running I/O test..."
	./$(TEST_IO_TARGET)

# Check for memory leaks with valgrind
.PHONY: valgrind
valgrind: debug
	@echo "Running valgrind memory check..."
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(MAIN_TARGET)

# Check code style and warnings
.PHONY: check
check:
	@echo "Checking code style..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -fsyntax-only $(MAIN_SOURCES)
	@echo "Syntax check passed!"

# Static analysis with cppcheck (if available)
.PHONY: static-analysis
static-analysis:
	@echo "Running static analysis..."
	@if command -v cppcheck >/dev/null 2>&1; then \
		cppcheck --enable=all --inconclusive --std=c++23 $(SRC_DIR) $(INCLUDE_DIR); \
	else \
		echo "cppcheck not found, skipping static analysis"; \
	fi

# Generate documentation (if doxygen is available)
.PHONY: docs
docs:
	@echo "Generating documentation..."
	@if command -v doxygen >/dev/null 2>&1; then \
		doxygen Doxyfile; \
	else \
		echo "doxygen not found, skipping documentation generation"; \
	fi

# Performance profiling with perf (requires root)
.PHONY: profile
profile: $(MAIN_TARGET)
	@echo "Running performance profiling..."
	@if command -v perf >/dev/null 2>&1; then \
		sudo perf record -g ./$(MAIN_TARGET); \
		sudo perf report; \
	else \
		echo "perf not found, skipping profiling"; \
	fi

# Install (copy to /usr/local/bin)
.PHONY: install
install: $(MAIN_TARGET)
	@echo "Installing $(MAIN_TARGET) to /usr/local/bin..."
	sudo cp $(MAIN_TARGET) /usr/local/bin/
	@echo "Installation completed"

# Uninstall
.PHONY: uninstall
uninstall:
	@echo "Removing $(MAIN_TARGET) from /usr/local/bin..."
	sudo rm -f /usr/local/bin/$(MAIN_TARGET)
	@echo "Uninstallation completed"

# Clean build files
.PHONY: clean
clean:
	@echo "Cleaning build files..."
	rm -rf $(BUILD_DIR)
	rm -f $(MAIN_TARGET) $(TEST_CPU_TARGET) $(TEST_MEMORY_TARGET) $(TEST_IO_TARGET)
	rm -f *.out
	@echo "Clean completed"

# Deep clean (including documentation and profiling data)
.PHONY: distclean
distclean: clean
	@echo "Deep cleaning..."
	rm -rf docs/html docs/latex
	rm -f perf.data perf.data.old
	rm -f *.log *.tmp
	@echo "Deep clean completed"

# Show help
.PHONY: help
help:
	@echo "Available targets:"
	@echo "  all          - Build main application (default)"
	@echo "  tests        - Build all test executables"
	@echo "  debug        - Build with debug flags"
	@echo "  release      - Build with release optimization"
	@echo "  run          - Build and run main application"
	@echo "  run-tests    - Build and run all tests"
	@echo "  valgrind     - Run with valgrind memory checker"
	@echo "  check        - Check syntax and warnings"
	@echo "  static-analysis - Run cppcheck static analysis"
	@echo "  docs         - Generate documentation with doxygen"
	@echo "  profile      - Run performance profiling with perf"
	@echo "  install      - Install to /usr/local/bin"
	@echo "  uninstall    - Remove from /usr/local/bin"
	@echo "  clean        - Remove build files"
	@echo "  distclean    - Remove all generated files"
	@echo "  help         - Show this help message"

# Print variables (for debugging the Makefile)
.PHONY: print-vars
print-vars:
	@echo "CXX: $(CXX)"
	@echo "CXXFLAGS: $(CXXFLAGS)"
	@echo "INCLUDES: $(INCLUDES)"
	@echo "LIBS: $(LIBS)"
	@echo "MAIN_SOURCES: $(MAIN_SOURCES)"
	@echo "MAIN_OBJECTS: $(MAIN_OBJECTS)"
	@echo "LIB_OBJECTS: $(LIB_OBJECTS)"