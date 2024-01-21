/**
 * parse_gcm.c
 *
 * (quick, dirty and incomplete) Nintendo GameCube Master file parser
 * This program is part of the cubeboot-tools package.
 *
 * Copyright (C) 2005-2006 The GameCube Linux Team
 * Copyright (C) 2005,2006 Albert Herranz
 * Copyright (C) 2022 Christopher Dellman
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

#include "lib.h"
#include "gcm.h"

typedef struct {
	uint32_t start;
	uint32_t end;
} range_t;

static range_t ranges[1000000];
static size_t ranges_i = 0;

struct gcm_disk_header boot_bin;
struct gcm_disk_header_info bi2_bin;
struct gcm_apploader_header appldr_bin;

#define BUF_SIZE 4096
static char buf[BUF_SIZE];

#define copy_to_null_terminated_buffer(dstbuf, srcbuf) \
	{ memcpy(dstbuf, srcbuf, sizeof(srcbuf)); \
	  dstbuf[sizeof(srcbuf)] = 0; }

static void add_to_range(uint32_t start, uint32_t len)
{
	printf("adding in use 0x%08X, len 0x%X\n", start, len);
	ranges[ranges_i].start = start;
	ranges[ranges_i++].end = start+len;
}

void die(char* format, ...)
{
	va_list args;
	va_start (args, format);
	vprintf (format, args);
	va_end (args);
	exit(1);
}

void print_disk_header(struct gcm_disk_header *dh)
{
	printf("\n== Disk Header (boot.bin) ==\n");
	copy_to_null_terminated_buffer(buf, dh->info.game_code);
	printf("game_code = [%s]\n", buf);

	copy_to_null_terminated_buffer(buf, dh->info.maker_code);
	printf("maker_code = [%s]\n", buf);

	printf("disk_id = %x\n", dh->info.disk_id);
	printf("version = %x\n", dh->info.version);
	printf("audio_streaming = %x\n", dh->info.audio_streaming);
	printf("stream_buffer_size = %x\n", dh->info.stream_buffer_size);
	printf("magic = %x (%s)\n", be32_to_cpu(dh->info.magic),
	       (be32_to_cpu(dh->info.magic) == GCM_MAGIC)?"OK":"wrong!");
	
	copy_to_null_terminated_buffer(buf, dh->game_name);
	printf("game_name = [%s]\n", buf);

	printf("debug_monitor_offset = 0x%08x\n",
		be32_to_cpu(dh->debug_monitor_offset));
	printf("debug_monitor_address = 0x%08x\n",
		be32_to_cpu(dh->debug_monitor_address));

	printf("dol_offset = 0x%08x\n", be32_to_cpu(dh->layout.dol_offset));
	printf("fst_offset = 0x%08x\n", be32_to_cpu(dh->layout.fst_offset));
	printf("fst_size = 0x%08x (%d)\n", be32_to_cpu(dh->layout.fst_size), be32_to_cpu(dh->layout.fst_size));
	printf("fst_max_size = 0x%08x (%d)\n", be32_to_cpu(dh->layout.fst_max_size), be32_to_cpu(dh->layout.fst_max_size));
	printf("user_offset = 0x%08x\n", be32_to_cpu(dh->layout.user_offset));
	printf("user_size = 0x%08x (%d)\n", be32_to_cpu(dh->layout.user_size), be32_to_cpu(dh->layout.user_size));

	printf("disk_size = 0x%08x (%d)\n", be32_to_cpu(dh->layout.disk_size), be32_to_cpu(dh->layout.disk_size));
}

static void print_disk_header_information(struct gcm_disk_header_info *dhi)
{
	printf("\n== Disk Header Information (bi2.bin) ==\n");

	printf("debug_monitor_size = 0x%08x (%d)\n",
		be32_to_cpu(dhi->debug_monitor_size),
		be32_to_cpu(dhi->debug_monitor_size));
	printf("simulated_memory_size = 0x%08x (%d)\n",
		be32_to_cpu(dhi->simulated_memory_size),
		be32_to_cpu(dhi->simulated_memory_size));
	printf("argument_offset = 0x%08x (%d)\n",
		be32_to_cpu(dhi->argument_offset),
		be32_to_cpu(dhi->argument_offset));
	printf("debug_flag = 0x%08x (%d)\n", 
		be32_to_cpu(dhi->debug_flag),
		be32_to_cpu(dhi->debug_flag));
	printf("track_location = 0x%08x (%d)\n",
		be32_to_cpu(dhi->track_location),
		be32_to_cpu(dhi->track_location));
	printf("track_size = 0x%08x (%d)\n", 
		be32_to_cpu(dhi->track_size),
		be32_to_cpu(dhi->track_size));
	printf("country_code = 0x%08x (%d)\n",
		be32_to_cpu(dhi->country_code),
		be32_to_cpu(dhi->country_code));
	printf("unknown_1 = 0x%08x (%d)\n",
		be32_to_cpu(dhi->unknown_1),
		be32_to_cpu(dhi->unknown_1));
}

static void print_apploader_header(struct gcm_apploader_header *ah)
{
	printf("\n== Apploader Header (appldr.bin) ==\n");

	copy_to_null_terminated_buffer(buf, ah->date);
	printf("date = [%s]\n", buf);

	printf("entry_point = 0x%08x (%d)\n",
		be32_to_cpu(ah->entry_point),
		be32_to_cpu(ah->entry_point));
	printf("size = 0x%08x (%d)\n",
		be32_to_cpu(ah->size),
		be32_to_cpu(ah->size));
	printf("trailer_size = 0x%08x (%d)\n",
		be32_to_cpu(ah->trailer_size),
		be32_to_cpu(ah->trailer_size));
	printf("unknown_1 = 0x%08x (%d)\n",
		be32_to_cpu(ah->unknown_1),
		be32_to_cpu(ah->unknown_1));
}

#if 0
int dump_file(int fd, struct gcm_file_entry *fe, char *fname)
{
	int outfd;
	off_t saved_pos, pos;
	unsigned long file_offset, file_length, chunk_size;
	int result;

	file_offset = be32_to_cpu(fe->file.file_offset);
	file_length = be32_to_cpu(fe->file.file_length);

	outfd = open(fname, O_CREAT|O_WRONLY);
	if (outfd < 0)
		die("can't open file %s: %s", fname, strerror(errno));

	saved_pos = lseek(fd, 0, SEEK_CUR);
	if (saved_pos == (off_t)-1)
		die("can't get current position in gcm: %s", strerror(errno));

	pos = lseek(fd, file_offset, SEEK_SET);
	if (pos == (off_t)-1)
		die("can't seek to file pos: %s", strerror(errno));

	while(file_length > 0) {
		chunk_size = (file_length > BUF_SIZE)?BUF_SIZE:file_length;
		result = read(fd, buf, chunk_size);
		if (result < 0)
			die("read failed: %s\n", strerror(errno));
		file_length -= result;
		result = write(outfd, buf, result);
		if (result < 0)
			die("write failed: %s\n", strerror(errno));
	}

	pos = lseek(fd, saved_pos, SEEK_SET);
	if (pos == (off_t)-1)
		die("can't seek to previous file pos: %s", strerror(errno));

	close(outfd);
}
#endif

void print_file_entry(int fd, struct gcm_file_entry *fe, void *fst, char *string_table)
{
	uint32_t fname_offset;
	char *fname;

	printf("-- file entry --\n");

	printf("type = %s\n", (fe->flags)?"directory":"file");
	fname_offset = be32_to_cpu(fe->file.fname_offset) & 0x00ffffff;
	printf("fname_offset = 0x%08x\n", fname_offset);
	printf("fname = %s\n", fname = string_table + fname_offset);

	if (fe->flags) {
		printf("parent_directory_offset = 0x%08x\n",
			be32_to_cpu(fe->dir.parent_directory_offset));
		printf("this_directory_offset = 0x%08x\n",
			be32_to_cpu(fe->dir.this_directory_offset));
	} else {
		
		add_to_range(be32_to_cpu(fe->file.file_offset), be32_to_cpu(fe->file.file_length));
		
		printf("file_offset = 0x%08x\n",
			be32_to_cpu(fe->file.file_offset));
		printf("file_length = 0x%08x (%d)\n",
			be32_to_cpu(fe->file.file_length),
			be32_to_cpu(fe->file.file_length));
	}
}

int parse_directory(int fd, struct gcm_file_entry *fe, struct gcm_file_entry *parent_fe,
		    void *fst, char *string_table)
{
//	struct gcm_file_entry *next_fe;
	uint32_t parent_directory_offset;
	uint32_t directory_offset;
//	unsigned long this_directory_offset;

//	this_directory_offset = be32_to_cpu(fe->dir.this_directory_offset);
	parent_directory_offset = be32_to_cpu(fe->dir.parent_directory_offset);

	if (parent_fe) {
		directory_offset = (void *)parent_fe - fst;
		if (directory_offset != parent_directory_offset) {
			die("bug in parser, claimed parent not parent!\n");
		}
	}

	print_file_entry(fd, fe, fst, string_table);
	printf("this offset = %ld\n", (void *)fe - fst);
	
	return 0;
}

int parse_fst(int fd, struct gcm_disk_header *dh)
{
	void *fst;
	uint32_t fst_size;
	struct gcm_file_entry *fe;

	char *string_table;
	uint32_t string_table_offset;

	uint32_t num_entries;

	int result;

	printf("\n== FST parser ==\n");

	fst_size = be32_to_cpu(dh->layout.fst_size);
	fst = malloc(fst_size);
	if (!fst)
		die("can't allocate memory for fst\n");

	result = read(fd, fst, fst_size);
	if (result < 0)
		die("can't read fst: %s", strerror(errno));

	fe = (struct gcm_file_entry *)fst;

	num_entries = be32_to_cpu(fe->root_dir.num_entries);
	string_table_offset = be32_to_cpu(dh->layout.fst_offset) +
				 num_entries * sizeof(*fe);
	string_table = fst + num_entries * sizeof(*fe);

	printf("fst loaded at address %p\n", fst);
	printf("fst has %u file entries\n", num_entries);

	printf("string table loaded at address %p\n", string_table);
	printf("string table located at offset 0x%08x\n", string_table_offset);

	/* skip root directory */
	fe++;
	num_entries--;

	while (num_entries > 0) {
		parse_directory(fd, fe, NULL, fst, string_table);
		fe++;
		num_entries--;
	}
	
	return 0;
}


// Function to compare two ranges for qsort
int compareRanges(const void *a, const void *b) {
	range_t *rangeA = (range_t *)a;
	range_t *rangeB = (range_t *)b;
	return (rangeA->start - rangeB->start);
}

// Merge overlapping or adjacent ranges
int mergeRanges() {
	if (ranges_i == 0) return 0;
	
	qsort(ranges, ranges_i, sizeof(range_t), compareRanges);
	
	int j = 0;
	for (int i = 1; i < ranges_i; i++) {
		if (ranges[j].end >= ranges[i].start) {
			if (ranges[j].end < ranges[i].end) {
				ranges[j].end = ranges[i].end;
			}
		} else {
			j++;
			ranges[j] = ranges[i];
		}
	}
	return j + 1; // Return the count of merged ranges
}

// Zero out padding
void zeroOutPadding(const char *filename) {
	FILE *file = fopen(filename, "r+b");
	if (file == NULL) {
		perror("Error opening file");
		return;
	}
	
	int rangeCount = mergeRanges();
	
	printf("%d ranges in use\n", rangeCount);
	
	long currentPos = 0;
	for (int i = 0; i < rangeCount; i++) {
		if (currentPos < ranges[i].start) {
			fseek(file, currentPos, SEEK_SET);
			printf("Writing zeroes from %ld to %u\n", currentPos, ranges[i].start);
			long paddingSize = ranges[i].start - currentPos;
			for (long j = 0; j < paddingSize; j++) {
				fputc(0, file);
			}
		}
		currentPos = ranges[i].end;
	}
	
	// Handle padding at the end of the file
	fseek(file, 0, SEEK_END);
	long fileSize = ftell(file);
	if (currentPos < fileSize) {
		fseek(file, currentPos, SEEK_SET);
		printf("Writing zeroes from %ld to %ld (end of file)\n", currentPos, fileSize);
		for (long j = currentPos; j < fileSize; j++) {
			fputc(0, file);
		}
	}
	
	fclose(file);
}

/*
 *
 */
int main(int argc, char *argv[])
{
	int result;
	int fd;
	
	if(argc < 2)
	{
		die("usage: parse_gcm <gcmfile.gcm>\n");
	}
	
	fd = open(argv[1], O_RDONLY);
	if(fd < 0)
	{
		die("Unable to open %s\n", argv[1]);
	}

	result = read(fd, &boot_bin, sizeof(boot_bin));
	if (result < 0)
		die("can't read boot.bin: %s", strerror(errno));

	//For now, let's mark everything up to the end of the FST as in-use
	add_to_range(0, be32_to_cpu(boot_bin.layout.fst_offset) + be32_to_cpu(boot_bin.layout.fst_size));
	
	print_disk_header(&boot_bin);

	result = read(fd, &bi2_bin, sizeof(bi2_bin));
	if (result < 0)
		die("can't read bi2.bin: %s", strerror(errno));

	print_disk_header_information(&bi2_bin);

	result = lseek(fd, 0x2440, SEEK_SET);
	if (result < 0)
		die("can't seek to appldr.bin position: %s", strerror(errno));

	result = read(fd, &appldr_bin, sizeof(appldr_bin));
	if (result < 0)
		die("can't read appldr.bin header: %s", strerror(errno));

	print_apploader_header(&appldr_bin);

	result = lseek(fd, be32_to_cpu(boot_bin.layout.fst_offset), SEEK_SET);
	if (result < 0)
		die("can't seek to fst.bin position: %s", strerror(errno));

	parse_fst(fd, &boot_bin);
	zeroOutPadding(argv[1]);
}

