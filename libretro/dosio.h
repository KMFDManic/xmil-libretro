
extern struct retro_vfs_interface *vfs_interface;

typedef struct {
  FILE *f;
  enum { FILEH_STDIO, FILEH_MEM, FILEH_LIBRETRO } type;
  long memsize;
  long memptr;
  long memalloc;
  int writeable;
  char *mem;
  struct retro_vfs_file_handle *lr;
} *FILEH;
#define	FILEH_INVALID		NULL

#define	FSEEK_SET	SEEK_SET
#define	FSEEK_CUR	SEEK_CUR
#define	FSEEK_END	SEEK_END

enum {
	FILEATTR_READONLY	= 0x01,
	FILEATTR_HIDDEN		= 0x02,
	FILEATTR_SYSTEM		= 0x04,
	FILEATTR_VOLUME		= 0x08,
	FILEATTR_DIRECTORY	= 0x10,
	FILEATTR_ARCHIVE	= 0x20
};

enum {
	FLICAPS_SIZE		= 0x0001,
	FLICAPS_ATTR		= 0x0002,
	FLICAPS_DATE		= 0x0004,
	FLICAPS_TIME		= 0x0008
};

typedef struct {
	UINT16	year;		/* cx */
	UINT8	month;		/* dh */
	UINT8	day;		/* dl */
} DOSDATE;

typedef struct {
	UINT8	hour;		/* ch */
	UINT8	minute;		/* cl */
	UINT8	second;		/* dh */
} DOSTIME;

typedef struct {
	UINT	caps;
	UINT32	size;
	UINT32	attr;
	DOSDATE	date;
	DOSTIME	time;
	char	path[MAX_PATH];
} FLINFO;


#ifdef __cplusplus
extern "C" {
#endif

/* ファイル操作 */
FILEH file_open(const char *path);
FILEH file_open_rb(const char *path);
FILEH file_create(const char *path);
long file_seek(FILEH handle, long pointer, int method);
UINT file_read(FILEH handle, void *data, UINT length);
UINT file_write(FILEH handle, const void *data, UINT length);
short file_close(FILEH handle);
UINT file_getsize(FILEH handle);
short file_getdatetime(FILEH handle, DOSDATE *dosdate, DOSTIME *dostime);

/* カレントファイル操作 */
void file_setcd(const char *exepath);
char *file_getcd(const char *path);
FILEH file_open_c(const char *path);
FILEH file_open_rb_c(const char *path);
FILEH file_create_c(const char *path);

#define file_cpyname(p, n, m)	milstr_ncpy(p, n, m)
#define file_cmpname(p, n)		milstr_cmp(p, n)
void file_catname(char *path, const char *name, int maxlen);
char *file_getname(const char *path);
void file_cutname(char *path);
char *file_getext(const char *path);
void file_cutext(char *path);
void file_cutseparator(char *path);
void file_setseparator(char *path, int maxlen);

FILEH make_readmem_file(void *buf, long size);
FILEH make_writemem_file();

void log_printf(const char *format, ...);

#ifdef	__cplusplus
}
#endif

