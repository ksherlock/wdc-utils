#include <string>
#include <stdint.h>

#include "finder_info_helper.h"


int set_file_type(const std::string &path, uint16_t file_type, uint32_t aux_type) {

	finder_info_helper fi;

	bool ok;
	ok = fi.open(path, false);
	if (!ok) return -1;
	fi.set_prodos_file_type(file_type, aux_type);
	ok = fi.write();
	if (!ok) return -1;
	return 0;

}
