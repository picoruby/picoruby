#ifndef FAT_DEFINED_H_
#define FAT_DEFINED_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef USE_FAT_SD_DISK
void c_FAT__init_spi();
#endif

#ifdef __cplusplus
}
#endif

#endif /* FAT_DEFINED_H_ */

