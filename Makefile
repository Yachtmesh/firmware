.PHONY: build test tag release

# Board environment to build (see platformio.ini): esp32dev, esp32s3, native
ENV ?= esp32s3

build:
	pio run -e $(ENV)

test:
	pio test -e native

# Create an annotated git tag for a release, e.g.:
#   make tag VERSION=v0.0.4
# Firmware version is derived from the tag via `git describe` (see
# .github/workflows/ci.yml), so no version file needs updating.
tag:
ifndef VERSION
	$(error VERSION is required, e.g. make tag VERSION=v0.0.4)
endif
	@case "$(VERSION)" in \
		v[0-9]*.[0-9]*.[0-9]*) ;; \
		*) echo "VERSION must look like vX.Y.Z, got '$(VERSION)'"; exit 1 ;; \
	esac
	@git diff --quiet && git diff --cached --quiet || \
		(echo "Working tree not clean, commit or stash changes first" && exit 1)
	@if git rev-parse $(VERSION) >/dev/null 2>&1; then \
		echo "Tag $(VERSION) already exists"; \
		exit 1; \
	fi
	git tag -a $(VERSION) -m "Release $(VERSION)"
	@echo "Created tag $(VERSION). Run 'make release VERSION=$(VERSION)' to push it and trigger CI."

# Push an existing tag to origin, e.g.:
#   make release VERSION=v0.0.4
# Requires the tag to already exist locally (run 'make tag' first). Pushes
# only that tag, not all local tags. CI builds esp32dev + esp32s3 and
# publishes a GitHub Release with firmware binaries, checksums, and an OTA
# manifest for any pushed tag matching v* (see .github/workflows/ci.yml).
release:
ifndef VERSION
	$(error VERSION is required, e.g. make release VERSION=v0.0.4)
endif
	@git rev-parse $(VERSION) >/dev/null 2>&1 || \
		(echo "Tag $(VERSION) does not exist locally. Run 'make tag VERSION=$(VERSION)' first." && exit 1)
	git push origin $(VERSION)
