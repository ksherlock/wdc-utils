#include <string>
#include <stdint.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>


#if defined(__APPLE__)
#include <sys/xattr.h>
#include <sys/stat.h>

#elif defined(__linux__)
#include <sys/xattr.h>
#define XATTR_FINDERINFO_NAME "user.com.apple.FinderInfo"

#elif defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/extattr.h>

#elif defined(_AIX)
#include <sys/ea.h>

#elif defined(__sun)
#elif defined(_WIN32)
#else
#error "set_file_type: unsupported OS."
#endif


#ifndef XATTR_FINDERINFO_NAME
#define XATTR_FINDERINFO_NAME "com.apple.FinderInfo"
#endif


/*
 * extended attributes functions.
 */
#if defined(__APPLE__)
ssize_t size_xattr(int fd, const char *xattr) {
	return fgetxattr(fd, xattr, NULL, 0, 0, 0);
}

ssize_t read_xattr(int fd, const char *xattr, void *buffer, size_t size) {
	return fgetxattr(fd, xattr, buffer, size, 0, 0);
}

ssize_t write_xattr(int fd, const char *xattr, const void *buffer, size_t size) {
	if (fsetxattr(fd, xattr, buffer, size, 0, 0) < 0) return -1;
	return size;
}

int remove_xattr(int fd, const char *xattr) {
	return fremovexattr(fd, xattr, 0);
}

#elif defined(__linux__) 
ssize_t size_xattr(int fd, const char *xattr) {
	return fgetxattr(fd, xattr, NULL, 0);
}

ssize_t read_xattr(int fd, const char *xattr, void *buffer, size_t size) {
	return fgetxattr(fd, xattr, buffer, size);
}

ssize_t write_xattr(int fd, const char *xattr, const void *buffer, size_t size) {
	if (fsetxattr(fd, xattr, buffer, size, 0) < 0) return -1;
	return size;
}

int remove_xattr(int fd, const char *xattr) {
	return fremovexattr(fd, xattr);
}

#elif defined(__FreeBSD__)
ssize_t size_xattr(int fd, const char *xattr) {
	return extattr_get_fd(fd, EXTATTR_NAMESPACE_USER, xattr, NULL, 0);
}

ssize_t read_xattr(int fd, const char *xattr, void *buffer, size_t size) {
	return extattr_get_fd(fd, EXTATTR_NAMESPACE_USER, xattr, buffer, size);
}

ssize_t write_xattr(int fd, const char *xattr, const void *buffer, size_t size) {
	return extattr_set_fd(fd, EXTATTR_NAMESPACE_USER, xattr, buffer, size);
}

int remove_xattr(int fd, const char *xattr) {
	return extattr_delete_fd(fd, EXTATTR_NAMESPACE_USER, xattr);
}

#elif defined(_AIX)
ssize_t size_xattr(int fd, const char *xattr) {
	/*
	struct stat64x st;
	if (fstatea(fd, xattr, &st) < 0) return -1;
	return st.st_size;
	*/
	return fgetea(fd, xattr, NULL, 0);
}

ssize_t read_xattr(int fd, const char *xattr, void *buffer, size_t size) {
	return fgetea(fd, xattr, buffer, size);
}

ssize_t write_xattr(int fd, const char *xattr, const void *buffer, size_t size) {
	if (fsetea(fd, xattr, buffer, size, 0) < 0) return -1;
	return size;
}

int remove_xattr(int fd, const char *xattr) {
	return fremoveea(fd, xattr);
}

#endif


static int file_type_to_finder_info(uint8_t *buffer, uint16_t file_type, uint32_t aux_type) {
	if (file_type > 0xff || aux_type > 0xffff) return -1;

	if (!file_type && aux_type == 0x0000) {
		memcpy(buffer, "BINApdos", 8);
		return 0;
	}

	if (file_type == 0x04 && aux_type == 0x0000) {
		memcpy(buffer, "TEXTpdos", 8);
		return 0;
	}

	if (file_type == 0xff && aux_type == 0x0000) {
		memcpy(buffer, "PSYSpdos", 8);
		return 0;
	}

	if (file_type == 0xb3 && aux_type == 0x0000) {
		memcpy(buffer, "PS16pdos", 8);
		return 0;
	}

	if (file_type == 0xd7 && aux_type == 0x0000) {
		memcpy(buffer, "MIDIpdos", 8);
		return 0;
	}
	if (file_type == 0xd8 && aux_type == 0x0000) {
		memcpy(buffer, "AIFFpdos", 8);
		return 0;
	}
	if (file_type == 0xd8 && aux_type == 0x0001) {
		memcpy(buffer, "AIFCpdos", 8);
		return 0;
	}
	if (file_type == 0xe0 && aux_type == 0x0005) {
		memcpy(buffer, "dImgdCpy", 8);
		return 0;
	}


	memcpy(buffer, "p   pdos", 8);
	buffer[1] = (file_type) & 0xff;
	buffer[2] = (aux_type >> 8) & 0xff;
	buffer[3] = (aux_type) & 0xff;
	return 0;
}

#if defined(_WIN32)


#pragma pack(push, 2)
struct AFP_Info {
	uint32_t magic;
	uint32_t version;
	uint32_t file_id;
	uint32_t backup_date;
	uint8_t finder_info[32];
	uint16_t prodos_file_type;
	uint32_t prodos_aux_type;
	uint8_t reserved[6];
};
#pragma pack(pop)

static void afp_init(struct AFP_Info *info, uint16_t file_type, uint32_t aux_type) {
	//static_assert(sizeof(AFP_Info) == 60, "Incorrect AFP_Info size");
	memset(info, 0, sizeof(*info));
	info->magic = 0x00504641;
	info->version = 0x00010000;
	info->prodos_file_type = file_type;
	info->prodos_aux_type = aux_type;
	if (file_type || aux_type)
		file_type_to_finder_info(info->finder_info, file_type, aux_type);
}

static BOOL afp_verify(struct AFP_Info *info) {
	if (!info) return 0;

	if (info->magic != 0x00504641) return 0;
	if (info->version != 0x00010000) return 0;

	return 1;
}


int set_file_type(const std::string &path, uint16_t ftype, uint32_t auxtype) {
	AFP_Info info;
	int ok;
	struct stat st;

	ok = stat(path, &st);
	if (ok == 0 && ! S_ISDIR(st.st_mode)) {

		std::string xpath(path);
		xpath.append(":AFP_AfpInfo");

		int fd = open(path.c_str(), O_RDWR | O_CREAT | O_BINARY, 0666);
		if (fd < 0) return -1;

		ok = read(fd, &info, sizeof(info));
		if (ok < sizeof(info) || !afp_verify(info)) {
			afp_init(&info, filetype, auxtype)
		} else {
			info.prodos_file_type = filetype;
			info.prodos_aux_type = auxtype;
			file_type_to_finder_info(info.finder_info, file_type, aux_type);
		}

		lseek(fd, 0, SEEK_SET);
		ok = write(fd, &info, sizeof(info));
		close(fd);

	}
	if (ok > 0) ok = 0;
	return ok;
}

#else

int set_file_type(const std::string &path, uint16_t ftype, uint32_t auxtype) {


	int fd;
	int ok;
	struct stat st;
	fd = open(path.c_str(), O_RDONLY);
	if (fd < 0) return -1;

	ok = fstat(fd, &st);
	if (ok == 0 && !S_ISDIR(st.st_mode)) {
		uint8_t buffer[32];
		memset(buffer, 0, sizeof(buffer));

#if defined(__sun)
		int xfd = attropen(path, XATTR_RESOURCEFORK_NAME, O_RDWR | O_CREAT, 0666);
		if (xfd < 0) {
			close(fd);
			return -1;
		}
		ok = read(xfd, buffer, 32);
		file_type_to_finder_info(buffer, ftype, auxtype);
		lseek(xfd, 0, SEEK_SET);
		ok = write(xfd, buffer, 32);
		close(xfd);
#else

		ok = read_xattr(fd, XATTR_FINDERINFO_NAME, buffer, 32);
		file_type_to_finder_info(buffer, ftype, auxtype);
		ok = write_xattr(fd, XATTR_FINDERINFO_NAME, buffer, 32);
#endif
	}

	close(fd);
	if (ok > 0) ok = 0;
	return ok;	
} 

#endif
