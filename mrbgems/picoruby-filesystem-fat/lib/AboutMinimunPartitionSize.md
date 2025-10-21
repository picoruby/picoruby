# Why is the minimum FAT partition size 192 sectors?

Investigated by Claude Code.

## ANSWER:

- The 128-sector minimum is a **practical/conventional limit** rather than a structural requirement.
- Including MBR offset 63, 192-sector is the minimum limit.

## Evidence from FatFS source code (ff.c):

- Line 5914: if (sz_vol < 128) LEAVE_MKFS(FR_MKFS_ABORTED);
    Comment: "Check if volume size is >=128s"

- Line 3266: && (ld_word(fs->win + BPB_TotSec16) >= 128 || ...)
    Comment: "Properness of volume sectors (>=128)"

The code enforces this limit but doesn't explain WHY.

## Structural minimum (theoretical):

For FAT12 with 4KB sectors, 1 FAT, 512 root entries, 1-sector clusters:

```
  - Boot sector:      1 sector
  - FAT:              1 sector
  - Root directory:   4 sectors (512 entries * 32 bytes / 4096)
  - Data area:       16 sectors (minimum per line 6145)
  ------------------------------------------------
  TOTAL:             22 sectors (88 KB with 4KB sectors)
```

So technically, 22 sectors would be sufficient!

## Historical context:

1. Early DOS/FAT history (512-byte sectors):
   - 128 sectors * 512 bytes = 64 KB
   - This was a reasonable minimum for early storage media
   - Smaller volumes were impractical for actual use

2. FAT specification compatibility:
   - Microsoft FAT specification likely defines this as minimum
   - Ensures consistent behavior across implementations
   - Avoids edge cases with very small filesystems

3. Validation check:
   - Line 3266 checks BPB_TotSec16 >= 128 when mounting
   - This suggests it's part of FAT volume validation rules
   - Volume < 128 sectors may not be considered valid FAT

## Why FatFS enforces 128 and not lower:

1. **Compatibility**: Follows Microsoft FAT specification
2. **Validation**: Ensures created volumes can be mounted/recognized
3. **Practical**: Prevents degenerate edge cases
4. **Conservative**: Better to reject too-small volumes than create invalid ones

## Can it be changed?

**Technically**: Yes, the check could be reduced to ~22 sectors
**Practically**: NO - would break FAT spec compliance

If you modify FatFS to allow < 128 sectors:

- ✗ Volume may not mount on Windows/Linux/other systems
- ✗ May not be recognized as valid FAT filesystem
- ✗ May fail validation checks in other FAT implementations

# Conclusion for RP2040 case:

With 4KB sectors and MBR partitioning:

- Minimum drive size = 63 (MBR offset) + 128 (partition) = 191 sectors
- Rounded up to 192 sectors
- This equals 192 * 4096 = 768 KB

The 128-sector limit is **not arbitrary code in FatFS**, but rather
**compliance with FAT filesystem specification** for interoperability.
