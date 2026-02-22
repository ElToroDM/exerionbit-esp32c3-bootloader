# Add boot_core subdirectory to make it discoverable as a component
# This allows the custom bootloader component to be found even though it's named
# boot_core instead of the default 'main' name
add_subdirectory(boot_core)
