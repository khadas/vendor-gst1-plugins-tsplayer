#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <linux/fb.h>
#include <ctype.h>
#include "gstamlsysctl.h"

static int axis[8] = {0};
static int use_wayland = 0;

int set_sysfs_str(const char *path, const char *val)
{
    int fd;
    fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd >= 0) {
        write(fd, val, strlen(val));
        close(fd);
        return 0;
    } else {
    }
    return -1;
}

int  get_sysfs_str(const char *path, char *valstr, int size)
{
    int fd;
    fd = open(path, O_RDONLY);
    if (fd >= 0) {
        read(fd, valstr, size - 1);
        valstr[strlen(valstr)] = '\0';
        close(fd);
    } else {
        sprintf(valstr, "%s", "fail");
        return -1;
    };
    return 0;
}

int set_sysfs_int(const char *path, int val)
{
    int fd;
    char  bcmd[16];
    fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd >= 0) {
        sprintf(bcmd, "%d", val);
        write(fd, bcmd, strlen(bcmd));
        close(fd);
        return 0;
    }
    return -1;
}

int get_sysfs_int(const char *path)
{
    int fd;
    int val = 0;
    char  bcmd[16];
    fd = open(path, O_RDONLY);
    if (fd >= 0) {
        read(fd, bcmd, sizeof(bcmd));
        val = strtol(bcmd, NULL, 16);
        close(fd);
    }
    return val;
}

int set_black_policy(int blackout)
{
    return set_sysfs_int("/sys/class/video/blackout_policy", blackout);
}

int get_black_policy()
{
    return get_sysfs_int("/sys/class/video/blackout_policy") & 1;
}

int set_tsync_enable(int enable)
{
    return set_sysfs_int("/sys/class/tsync/enable", enable);

}

int set_tsync_mode(int mode)
{
    return set_sysfs_int("/sys/class/tsync/mode", mode);

}

int set_ppscaler_enable(char *enable)
{
    return set_sysfs_str("/sys/class/ppmgr/ppscaler", enable);
}

int get_tsync_enable(void)
{
    char buf[32];
    int ret = 0;
    int val = 0;
    ret = get_sysfs_str("/sys/class/tsync/enable", buf, 32);
    if (!ret) {
        sscanf(buf, "%d", &val);
    }
    return val == 1 ? val : 0;
}

int get_tsync_mode(void)
{
    char buf[32];
    int ret = 0;
    int val = 0;
    ret = get_sysfs_str("/sys/class/tsync/mode", buf, 32);
    if (!ret) {
        sscanf(buf, "%d", &val);
    }
    return val;
}

int set_fb0_blank(int blank)
{
    return set_sysfs_int("/sys/class/graphics/fb0/blank", blank);
}

int set_fb1_blank(int blank)
{
    return set_sysfs_int("/sys/class/graphics/fb1/blank", blank);
}

int get_osd0_status(void)
{
    char osd_status[128]="";
    char *fb_stat0 = (char *)"/sys/class/graphics/fb0/osd_status";
    char *wl_blank0 = (char *)"/sys/kernel/debug/dri/0/vpu/blank";

    if (use_wayland || (get_sysfs_str(fb_stat0, osd_status, 32) != 0))
    {
        memset(osd_status, 0, sizeof(osd_status));
        if (get_sysfs_str(wl_blank0, osd_status, 128) == 0)
        {
            use_wayland = 1;
            if (strstr(osd_status, "blank_enable: 0"))
                return 1;
            else
                return 0;
        }else
            return 1;
    }
    if (strstr(osd_status, "enable: 1"))
        return 1;
    else
        return 0;
}

int get_osd1_status(void)
{
    char osd_status[128]="";
    char *fb_stat1 = (char *)"/sys/class/graphics/fb1/osd_status";
    char *wl_blank1 = (char *)"/sys/kernel/debug/dri/64/vpu/blank";
    if (use_wayland || (get_sysfs_str(fb_stat1, osd_status, 32) != 0))
    {
        memset(osd_status, 0, sizeof(osd_status));
        if (get_sysfs_str(wl_blank1, osd_status, 128) == 0)
        {
            use_wayland = 1;
            if (strstr(osd_status, "blank_enable: 0"))
                return 1;
            else
                return 0;
        }else
            return 1;
    }
    if (strstr(osd_status, "enable: 1"))
        return 1;
    else
        return 0;
}

int parse_para(const char *para, int para_num, int *result)
{
    char *endp;
    const char *startp = para;
    int *out = result;
    int len = 0, count = 0;
    if (!startp) {
        return 0;
    }
    len = strlen(startp);
    do {
        //filter space out
        while (startp && (isspace(*startp) || !isgraph(*startp)) && len) {
            startp++;
            len--;
        }
        if (len == 0) {
            break;
        }
       *out++ = strtol(startp, &endp, 0);
        len -= endp - startp;
        startp = endp;
        count++;
    } while ((endp) && (count < para_num) && (len > 0));
    return count;
}

int set_display_axis(int recovery)
{
    int fd;
    char *path = (char *)"/sys/class/display/axis";
    char str[128];
    int count;
    fd = open(path, O_CREAT|O_RDWR | O_TRUNC, 0644);
    if (fd >= 0) {
        if (!recovery) {
            read(fd, str, 128);
            count = parse_para(str, 8, axis);
            printf("read axis %s, length %d, count %d!\n", str, strlen(str), count);
        }
        if (recovery) {
            sprintf(str, "%d %d %d %d %d %d %d %d",
                 axis[0],axis[1], axis[2], axis[3], axis[4], axis[5], axis[6], axis[7]);
        } else {
            sprintf(str, "2048 %d %d %d %d %d %d %d",
                 axis[1], axis[2], axis[3], axis[4], axis[5], axis[6], axis[7]);
        }
        write(fd, str, strlen(str));
        close(fd);
        return 0;
    }
    return -1;
}
