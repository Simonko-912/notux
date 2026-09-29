# Makefile snippet for building and installing user applications
# This can be included in the main Makefile

.PHONY: build-user-apps install-user-apps clean-user-apps

# User application directories
USER_APPS := hello init testapp calc filemgr editor shell ps meminfo nfetch notedit pkgman opm gcc_wrapper date echo head wc tree sysinfo
BIN_DIR := build/bin

# Build all user applications
build-user-apps:
	@echo "Building user applications..."
	@mkdir -p $(BIN_DIR)
	@for app in $(USER_APPS); do \
		echo "Building $$app..."; \
		if [ -f "userspace/$$app/Makefile" ]; then \
			$(MAKE) -C userspace/$$app clean install || echo "Failed to build $$app"; \
		else \
			echo "No Makefile found for $$app"; \
		fi; \
	done
	@echo "User application building complete."

# Install user applications to NTFS partition
install-user-apps: build-user-apps
	@echo "Installing applications to NTFS partition..."
	@if [ ! -f "build/data.ntfs" ]; then \
		echo "Error: NTFS image not found. Please run 'make' first."; \
		exit 1; \
	fi

	# Create mount point if it doesn't exist
	@mkdir -p build/mnt

	# Mount the NTFS partition (try read-only first)
	if mount -o loop,ro build/data.ntfs build/mnt 2>/dev/null; then \
		echo "Mounted NTFS partition (read-only)"; \
	elif mount -o loop build/data.ntfs build/mnt; then \
		echo "Mounted NTFS partition"; \
	else \
		echo "Failed to mount NTFS partition"; \
		exit 1; \
	fi

	# Create necessary directories
	@mkdir -p build/mnt/bin
	@mkdir -p build/mnt/etc
	@mkdir -p build/mnt/usr
	@mkdir -p build/mnt/var

	# Copy applications
	for app_bin in $(BIN_DIR)/*; do \
		if [ -f "$$app_bin" ]; then \
			echo "Installing $$(basename $$app_bin)..."; \
			cp "$$app_bin" build/mnt/bin/; \
			chmod 755 "build/mnt/bin/$$(basename $$app_bin)"; \
		else \
			echo "Warning: no binaries in $(BIN_DIR)"; \
			break; \
		fi; \
	done

	# Install default configuration if needed
	if [ ! -f "build/mnt/etc/admin.pass" ]; then \
		echo "admin" > build/mnt/etc/admin.pass; \
		chmod 600 build/mnt/etc/admin.pass; \
		echo "Created default admin password file"; \
	fi

	# Unmount
	umount build/mnt
	@echo "Applications installed to NTFS partition."

clean-user-apps:
	@echo "Cleaning user application build directory..."
	@rm -rf $(BIN_DIR)
	@echo "User applications clean complete."

.PHONY: build-user-apps install-user-apps clean-user-apps
