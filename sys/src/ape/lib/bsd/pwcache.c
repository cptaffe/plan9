#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>

char *
user_from_uid(uid_t uid, int nouser)
{
	static char buf[16];
	struct passwd *p;

	if((p = getpwuid(uid)) != NULL)
		return p->pw_name;
	if(nouser)
		return NULL;
	snprintf(buf, sizeof buf, "%d", uid);
	return buf;
}

char *
group_from_gid(gid_t gid, int nogroup)
{
	static char buf[16];
	struct group *g;

	if((g = getgrgid(gid)) != NULL)
		return g->gr_name;
	if(nogroup)
		return NULL;
	snprintf(buf, sizeof buf, "%d", gid);
	return buf;
}

int
uid_from_user(const char *name, uid_t *uid)
{
	struct passwd *p;

	if((p = getpwnam(name)) == NULL)
		return -1;
	*uid = p->pw_uid;
	return 0;
}

int
gid_from_group(const char *name, gid_t *gid)
{
	struct group *g;

	if((g = getgrnam(name)) == NULL)
		return -1;
	*gid = g->gr_gid;
	return 0;
}
