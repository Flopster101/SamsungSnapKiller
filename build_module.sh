#!/bin/bash
# Script to generate minimal Magisk/KSU module structure

MODULE_DIR="$1"
MODULE_ID="$2"
MODULE_NAME="$3"
MODULE_VERSION="$4"
MODULE_VERSION_CODE="$5"
MODULE_AUTHOR="$6"

# Create module.prop
cat > "$MODULE_DIR/module.prop" << EOF
id=$MODULE_ID
name=$MODULE_NAME
version=$MODULE_VERSION
versionCode=$MODULE_VERSION_CODE
author=$MODULE_AUTHOR
description=Automatically kills Samsung's battery-draining vendor.samsung.hardware.snap-service whenever it starts.
EOF

# Create service.sh (runs on boot)
cat > "$MODULE_DIR/service.sh" << 'EOF'
#!/system/bin/sh
MODDIR="${0%/*}"
BINARY="$MODDIR/monitor_snap"

[ -f "$BINARY" ] || exit 0
chmod 755 "$BINARY" 2>/dev/null || true

# Avoid duplicates
if ! pidof monitor_snap >/dev/null 2>&1; then
  "$BINARY" >/dev/kmsg 2>&1 &
else
  echo "monitor_snap: already running" >/dev/kmsg 2>&1
fi
EOF

chmod +x "$MODULE_DIR/service.sh"

echo "Module structure created in $MODULE_DIR"
