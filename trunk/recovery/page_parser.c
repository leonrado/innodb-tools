#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#include "univ.i"
#include "page0page.h"

#include "error.h"
#include "common.h"

static time_t timestamp = 0;


void save_fields_info(char *fname, page_t *page) {
	FILE *info;
	rec_t *free_rec;
	ulint fields_number, i;
	
	// Open file and get first free record
	info = fopen(fname, "wt+");
	free_rec = page + page_header_get_field(page, PAGE_FREE);

	// Print fields number and print it
	fields_number = rec_get_n_fields(free_rec);
	fprintf(info, "%lu\n", fields_number);
	
	// Save fields lengths
	for (i = 0; i < fields_number; i++) {
		fprintf(info, "%lu\n", rec_get_nth_field_len(free_rec, i));
	}
	
	fclose(info);
}

void process_ibpage(page_t *page) {
	ulint page_id;
	dulint index_id;
	char tmp[256];
	int fn, table_dir_res;
	
	// Get page info
	page_id = mach_read_from_4(page + FIL_PAGE_OFFSET);
	index_id = mach_read_from_8(page + PAGE_HEADER + PAGE_INDEX_ID);
			
	// Create table directory
	sprintf(tmp, "pages-%u/%lu-%lu", timestamp, index_id.high, index_id.low);
	table_dir_res = mkdir(tmp, 0755);
	
	// Compose page file_name
	sprintf(tmp, "pages-%u/%lu-%lu/%08lu.page", timestamp, index_id.high, index_id.low, page_id);
	
	printf("Read page #%lu.. saving it to %s\n", page_id, tmp);

	// Save page data
	fn = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (!fn) error("Can't open file to save page!");
	write(fn, page, UNIV_PAGE_SIZE);
	close(fn);
	
	// Create fields info file (if directory did not exist yet)
	if (table_dir_res != EEXIST) {
    	sprintf(tmp, "pages-%u/%lu-%lu/fields.info", timestamp, index_id.high, index_id.low);
	    save_fields_info(tmp, page);	
	}
}

void process_ibfile(int fn) {
	int read_bytes;
	page_t *page = malloc(UNIV_PAGE_SIZE);
	char tmp[20];
	
	if (!page) error("Can't allocate page buffer!");

	// Create pages directory
	timestamp = time(0);
	sprintf(tmp, "pages-%u", timestamp);
	mkdir(tmp, 0755);
	
	printf("Read data from fn=%d...\n", fn);

	// Read pages to the end of file
	while ((read_bytes = read(fn, page, UNIV_PAGE_SIZE)) == UNIV_PAGE_SIZE) {
		if (page_is_interesting(page)) process_ibpage(page);
	}
}

int open_ibfile(char *fname) {
	struct stat fstat;
	int fn;

	// Disallow Skip non-regular files
	printf("Opening file: %s\n", fname);
	if (stat(fname, &fstat) != 0 || (fstat.st_mode & S_IFREG) != S_IFREG) error("Invalid file specified!");
	fn = open(fname, O_RDONLY, 0);
	if (!fn) error("Can't open file!");
	
	return fn;
}

int main(int argc, char **argv) {
	int fn;

	if (argc < 2) error("Usage: ./page_parser <innodb_datafile>");
	
	fn = open_ibfile(argv[1]);
	process_ibfile(fn);
	close(fn);
	
	return 0;
}
