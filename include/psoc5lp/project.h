 #define CyGlobalIntEnable           {__asm("CPSIE   i");}
 #define CyGlobalIntDisable          {__asm("CPSID   i");}

#define PM_SLEEP_SRC_NONE               (0x0000u)
#define PM_SLEEP_TIME_NONE              (0x00u)
#define PM_ALT_ACT_SRC_NONE             (0x0000u)
#define PM_ALT_ACT_TIME_NONE            (0x0000u)

/* Wake up sources for Sleep mode */
#define PM_SLEEP_SRC_COMPARATOR0        (0x0001u)
#define PM_SLEEP_SRC_COMPARATOR1        (0x0002u)
#define PM_SLEEP_SRC_COMPARATOR2        (0x0004u)
#define PM_SLEEP_SRC_COMPARATOR3        (0x0008u)
#define PM_SLEEP_SRC_PICU               (0x0040u)
#define PM_SLEEP_SRC_I2C                (0x0080u)
#define PM_SLEEP_SRC_BOOSTCONVERTER     (0x0200u)
#define PM_SLEEP_SRC_VD                 (0x0400u)
#define PM_SLEEP_SRC_CTW                (0x0800u)
#define PM_SLEEP_SRC_ONE_PPS            (0x0800u)
#define PM_SLEEP_SRC_LCD                (0x1000u)

typedef unsigned char   uint8;
typedef unsigned short  uint16;
typedef unsigned long   uint32;
typedef signed   char   int8;
typedef signed   short  int16;
typedef signed   long   int32;
typedef          float  float32;

/***************************************
*    Function Prototypes
***************************************/
void CyPmSaveClocks(void) ;
void CyPmRestoreClocks(void) ;
void CyPmAltAct(uint16 wakeupTime, uint16 wakeupSource) ;
void CyPmSleep(uint8 wakeupTime, uint16 wakeupSource) ;
void CyPmHibernate(void) ;
void CyPmHibernateEx(uint16 wakeupSource) ;
