#ifndef _GST_AML_SYSCTL_H_
#define _GST_AML_SYSCTL_H_

#define  TSYNC_MODE_VIDEO 0
#define  TSYNC_MODE_AUDIO 1
#define  TSYNC_MODE_PCRSCR 2

int set_sysfs_str(const char *path, const char *val);
int get_sysfs_str(const char *path, char *valstr, int size);
int set_sysfs_int(const char *path, int val);
int get_sysfs_int(const char *path);
int set_black_policy(int blackout);
int set_ppscaler_enable(char *enable);
int get_black_policy();
int set_tsync_enable(int enable);
int get_tsync_enable(void);
int set_tsync_mode(int mode);
int get_tsync_mode(void);
int set_fb0_blank(int blank);
int set_fb1_blank(int blank);
int get_osd0_status(void);
int get_osd1_status(void);
int set_display_axis(int recovery);
int set_vdec_path(char *path);
int set_ppmgr_bypass(char *enable);
int set_ppmgr_angle(int angle);

#endif //_GST_AML_SYSCTL_H_
