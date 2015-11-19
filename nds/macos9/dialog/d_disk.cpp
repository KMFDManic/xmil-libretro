#include	"compiler.h"
#include	"strres.h"
#include	"resource.h"
#include	"dosio.h"
#include	"dialog.h"
#include	"dialogs.h"
#include	"pccore.h"
#include	"fddfile.h"
#include	"diskdrv.h"
#include	"newdisk.h"


void dialog_changefdd(BYTE drv) {

	char	fname[MAX_PATH];

	if (drv < 4) {
		if (dlgs_selectfile(fname, sizeof(fname))) {
			diskdrv_setfdd(drv, fname, 0);
		}
	}
}


// ---- newdisk

static const char str_newdisk[] = "newdisk.d88";

typedef struct {
	BYTE	fdtype;
	char	label[16+1];
} NEWDISK;

static int NewdiskDlgProc(NEWDISK *newdisk) {

	DialogPtr		hDlg;
	int				media;
	int				done;
	short			item;
	Str255			disklabel;
	ControlHandle	btn[2];

	hDlg = GetNewDialog(IDD_NEWFDDDISK, NULL, (WindowPtr)-1);
	if (!hDlg) {
		return(IDCANCEL);
	}

	SelectDialogItemText(hDlg, IDC_DISKLABEL, 0x0000, 0x7fff);
	btn[0] = (ControlHandle)GetDlgItem(hDlg, IDC_MAKE2DD);
	btn[1] = (ControlHandle)GetDlgItem(hDlg, IDC_MAKE2HD);
	SetControlValue(btn[0], 0);
	SetControlValue(btn[1], 1);
	media = 1;
	SetDialogDefaultItem(hDlg, IDOK);
	SetDialogCancelItem(hDlg, IDCANCEL);

	done = 0;
	while(!done) {
		ModalDialog(NULL, &item);
		switch(item) {
			case IDOK:
				if (media == 0) {
					newdisk->fdtype = (DISKTYPE_2DD << 4);
				}
				else {
					newdisk->fdtype = (DISKTYPE_2HD << 4);
				}
				GetDialogItemText(GetDlgItem(hDlg, IDC_DISKLABEL), disklabel);
				mkcstr(newdisk->label, sizeof(newdisk->label), disklabel);
				done = IDOK;
				break;

			case IDCANCEL:
				done = IDCANCEL;
				break;

			case IDC_DISKLABEL:
				break;

			case IDC_MAKE2DD:
				SetControlValue(btn[0], 1);
				SetControlValue(btn[1], 0);
				media = 0;
				break;

			case IDC_MAKE2HD:
				SetControlValue(btn[0], 0);
				SetControlValue(btn[1], 1);
				media = 1;
				break;
		}
	}
	DisposeDialog(hDlg);
	return(done);
}

void dialog_newdisk(void) {

	char	path[MAX_PATH];
const char	*ext;
	NEWDISK	disk;

	if (!dlgs_selectwritefile(path, sizeof(path), str_newdisk)) {
		return;
	}
	ext = file_getext(path);
	if ((!file_cmpname(ext, str_d88)) || (!file_cmpname(ext, str_88d))) {
		if (NewdiskDlgProc(&disk) == IDOK) {
			newdisk_fdd(path, disk.fdtype, disk.label);
		}
	}
}
