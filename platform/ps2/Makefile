# Keep every generated object, executable and report under the ignored build tree.
PYTHON ?= python3
ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
OUT ?= $(ROOT)/build/ps2-bringup
BACKEND ?= docker
ARGS ?=

.PHONY: all build smoke report core-probe
all: build

build:
	$(PYTHON) "$(ROOT)/platform/ps2/build_ps2.py" build --backend "$(BACKEND)" --output "$(OUT)" $(ARGS)

smoke:
	$(PYTHON) "$(ROOT)/platform/ps2/build_ps2.py" smoke --output "$(OUT)" $(ARGS)

report:
	$(PYTHON) "$(ROOT)/platform/ps2/build_ps2.py" report --backend "$(BACKEND)" --output "$(OUT)" $(ARGS)

# CORE_MANIFEST names an ignored JSON build specification for a real core probe.
# Its ELF is a development measurement; it is not a playable game package.
core-probe:
	$(PYTHON) "$(ROOT)/platform/ps2/build_ps2.py" build --target core-probe --core-manifest "$(CORE_MANIFEST)" --backend "$(BACKEND)" --output "$(OUT)" $(ARGS)
