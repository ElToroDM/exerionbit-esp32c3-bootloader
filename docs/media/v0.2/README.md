# v0.2 Media Assets

Purpose: list the media files included in the v0.2 evidence pack.

Included:
- `waveshare-esp32-c3-zero-custom-bootloader-normal-boot.gif` — animated normal boot preview
- `waveshare-esp32-c3-zero-custom-bootloader-normal-boot.mp4` — normal boot demonstration video
- `waveshare-esp32-c3-zero-custom-bootloader-normal-boot-thumbnail.webp` — still image
- `waveshare-esp32-c3-zero-custom-bootloader-boot-flow.webp` — boot decision overview diagram

Evidence mapping:
- `waveshare-esp32-c3-zero-custom-bootloader-normal-boot.gif` and `waveshare-esp32-c3-zero-custom-bootloader-normal-boot.mp4` come from the same normal boot capture
- `waveshare-esp32-c3-zero-custom-bootloader-normal-boot.mp4` maps to `docs/evidence/v0.2/logs/normal_boot.log`
- `waveshare-esp32-c3-zero-custom-bootloader-boot-flow.webp` summarizes the public boot-path branches and complements `BOOT_SEQUENCE.md`
- Recovery and update behavior remain backed by `docs/evidence/v0.2/logs/recovery_commands.log` and `docs/evidence/v0.2/logs/recovery_update.log`

Note:
- Media complements the serial evidence set. Protocol claims remain grounded in the captured logs.
