# Original WC2 recovery, 2026-11-05

WC7 physical feedback remains unsuccessful. No WC8 control patch was made.
The original archived WC2 AXF was verified against its release SHA256:
37402F1A84FB0883C6713D43333945791FA68AD3089DDE48281606EF863DBD4C

Keil project: demo/mdk/WC2_original_download.uvprojx
Target: nor_sdram_zf_dtcm
Action: Flash > Download ONLY. Do not Build or Rebuild this download-only project.
Output: .build/wc2-original/Objects/rt1064.axf, exact copy of archived WC2.
The project has no source files and a deliberate build blocker; it cannot compile WC7 into this output. The output copy is read-only. Existing Keil debug options were copied. No hardware download or start was performed; actual programmer connectivity is unverified.

If an accidental Rebuild removes the output, tools/restore_wc2_original.ps1 recreates and verifies it from the unchanged archive. A command-line Rebuild check confirmed the source build is blocked, but UV4 may remove executable output during failed builds; this is why Download is the intended operation. The verified firmware copy was restored after these checks.

This is a complete binary rollback, not an entry-only approximation. Previous WC7 source and artifacts remain separate. Successful physical entry with current installation is still unverified. Do not treat host simulation totals or successful compilation as track acceptance.