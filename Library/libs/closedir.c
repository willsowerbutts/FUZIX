/* closedir.c      closedir implementation
 *
 */
#include <unistd.h>
#include <alloc.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

int closedir(DIR * dir)
{
	if (dir == NULL || dir->dd_fd == -1) {
		errno = EBADF;
		return -1;
	}
	close(dir->dd_fd);
	dir->dd_fd = -1;
	free(dir);
	return 0;
}
