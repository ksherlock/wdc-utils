/*
 * WDC to OMF Linker.
 *
 *
 */

#include <sysexits.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>

void help() {
	exit(0);
}

void usage() {
	exit(EX_USAGE);
}

int main(int argc, char **argv) {

	std::string _o;
	bool _C = false;
	bool _X = false;

	std::vector<std::string> _l;
	std::vector<std::string> _L;


	int c;
	while ((c = getopt(argc, argv, "CXL:l:o:")) != -1) {
		switch(c) {
			case 'X': _X = true; break;
			case 'C': _C = true; break;
			case 'o': _o = optarg; break;
			case 'l': _l.emplace_back(optarg); break;
			case 'L': _L.emplace_back(optarg); break;
			case 'h': help(); break;
			case ':':
			case '?':
			default:
				usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0) usage();

}



struct section {
	std::vector<uint8_t> data;
	uint32_t offset; 
}