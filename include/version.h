#if !defined(__VERSION_H__)
#define __VERSION_H__

#define PICORUBY_VERSION "4.0.0"

const char* picorb_version(void);
const char* picorb_version_with_build_info(void);
void picorb_description(const char *platform_name, char *description, unsigned int description_size);

#endif
