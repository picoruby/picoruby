#include <stdlib.h>
#include <string.h>

#include "../../lib/ff14b/source/ff.h"
#include "../../lib/ff14b/source/diskio.h"

#include "disk.h"
#include <stdio.h>

void
FILE_physical_address(FIL *fp, uint8_t **addr)
{
}

int
FILE_sector_size(void)
{
  return FLASH_SECTOR_SIZE;
}
