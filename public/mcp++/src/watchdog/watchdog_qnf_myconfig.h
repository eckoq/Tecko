/*
 * watchdog_qnf_myconfig.h:		Handle comfigure file. Copied from QNF.
 * Date:							2011-02-17
 */

#ifndef __WATCHDOG_QNF_MYCONFIG_H
#define __WATCHDOG_QNF_MYCONFIG_H

#include <sys/cdefs.h>

__BEGIN_DECLS

extern int myconfig_init(int argc, char **argv, int mode);
extern void myconfig_reload();

extern int myconfig_put_value(const char *pre, const char *key, const char *val);
extern int myconfig_delete_value(const char *prefix, const char *key);
//����get������key��������ǰ׺+"_"
extern int myconfig_get_intval(const char *key, int def);
extern unsigned long myconfig_get_size(const char *key, int def);
extern double myconfig_get_decimal(const char *key);
extern char* myconfig_get_value(const char *key);
extern char* myconfig_get_multivalue(const char *key, int index);
/*
 * ע������reload�Ļص�����������ģ�鶼����ע��һ���ص�����������̬reload����ģ�������
 * func		����reload�ص�����
 * keys		����reload������key�����飬ÿ��Ԫ�ض���ָ��̬������ַ����洢����ָ�롣NULL��ʾû�С�
 * ���磺   static char* my_reload_keys[] = {"download_speed", "fwd_ip", "fwd_port"};
 * keynum	key����Ŀ
 */
extern void myconfig_register_reload_i(int (*reload_cb_func)(void), char** keys, int keynum, unsigned char inuse);
extern void myconfig_register_reload(int (*reload_cb_func)(void), char** keys, int keynum);

extern int myconfig_update_value(const char *key, const char *value);
extern int myconfig_dump_to_file();
//myconfig_reset�����¼��ز����ʱ����õ��������ú���

extern void myconfig_reset();
extern int myconfig_cleanup(void);

__END_DECLS

#endif

