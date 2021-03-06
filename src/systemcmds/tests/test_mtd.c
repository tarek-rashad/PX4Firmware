/****************************************************************************
 *
 *   Copyright (c) 2012-2014 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file test_mtd.c
 *
 * Param storage / file test.
 *
 * @author Lorenz Meier <lm@inf.ethz.ch>
 */

#include <sys/stat.h>
#include <poll.h>
#include <dirent.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <systemlib/err.h>
#include <systemlib/perf_counter.h>
#include <string.h>

#include <drivers/drv_hrt.h>

#include "tests.h"

#define PARAM_FILE_NAME "/fs/mtd_params"

static int check_user_abort(int fd);

int check_user_abort(int fd) {
	/* check if user wants to abort */
	char c;

	struct pollfd fds;
	int ret;
	fds.fd = 0; /* stdin */
	fds.events = POLLIN;
	ret = poll(&fds, 1, 0);

	if (ret > 0) {

		read(0, &c, 1);

		switch (c) {
		case 0x03: // ctrl-c
		case 0x1b: // esc
		case 'c':
		case 'q':
		{
			warnx("Test aborted.");
			fsync(fd);
			close(fd);
			return OK;
			/* not reached */
			}
		}
	}

	return 1;
}

int
test_mtd(int argc, char *argv[])
{
	unsigned iterations= 0;

	/* check if microSD card is mounted */
	struct stat buffer;
	if (stat(PARAM_FILE_NAME, &buffer)) {
		warnx("file %s not found, aborting MTD test", PARAM_FILE_NAME);
		return 1;
	}

	// XXX get real storage space here
	unsigned file_size = 4096;

	/* perform tests for a range of chunk sizes */
	unsigned chunk_sizes[] = {256, 512, 4096};

	for (unsigned c = 0; c < (sizeof(chunk_sizes) / sizeof(chunk_sizes[0])); c++) {

		printf("\n====== FILE TEST: %u bytes chunks ======\n", chunk_sizes[c]);

		uint8_t write_buf[chunk_sizes[c]] __attribute__((aligned(64)));

		/* fill write buffer with known values */
		for (int i = 0; i < sizeof(write_buf); i++) {
			/* this will wrap, but we just need a known value with spacing */
			write_buf[i] = i+11;
		}

		uint8_t read_buf[chunk_sizes[c]] __attribute__((aligned(64)));
		hrt_abstime start, end;

		int fd = open(PARAM_FILE_NAME, O_WRONLY);

		warnx("testing unaligned writes - please wait..");

		iterations = file_size / chunk_sizes[c];

		start = hrt_absolute_time();
		for (unsigned i = 0; i < iterations; i++) {
			int wret = write(fd, write_buf, chunk_sizes[c]);

			if (wret != chunk_sizes[c]) {
				warn("WRITE ERROR!");

				return 1;
			}

			fsync(fd);

			if (!check_user_abort(fd))
				return OK;

		}
		end = hrt_absolute_time();

		close(fd);
		fd = open(PARAM_FILE_NAME, O_RDONLY);

		/* read back data for validation */
		for (unsigned i = 0; i < iterations; i++) {
			int rret = read(fd, read_buf, chunk_sizes[c]);

			if (rret != chunk_sizes[c]) {
				warnx("READ ERROR!");
				return 1;
			}
			
			/* compare value */
			bool compare_ok = true;

			for (int j = 0; j < chunk_sizes[c]; j++) {
				if (read_buf[j] != write_buf[j]) {
					warnx("COMPARISON ERROR: byte %d", j);
					compare_ok = false;
					break;
				}
			}

			if (!compare_ok) {
				warnx("ABORTING FURTHER COMPARISON DUE TO ERROR");
				return 1;
			}

			if (!check_user_abort(fd))
				return OK;

		}

		close(fd);

		printf("RESULT: OK! No readback errors.\n\n");

		// /*
		//  * ALIGNED WRITES AND UNALIGNED READS
		//  */

		
		// fd = open(PARAM_FILE_NAME, O_WRONLY);

		// warnx("testing aligned writes - please wait.. (CTRL^C to abort)");

		// start = hrt_absolute_time();
		// for (unsigned i = 0; i < iterations; i++) {
		// 	int wret = write(fd, write_buf, chunk_sizes[c]);

		// 	if (wret != chunk_sizes[c]) {
		// 		warnx("WRITE ERROR!");
		// 		return 1;
		// 	}

		// 	if (!check_user_abort(fd))
		// 		return OK;

		// }

		// fsync(fd);

		// warnx("reading data aligned..");

		// close(fd);
		// fd = open(PARAM_FILE_NAME, O_RDONLY);

		// bool align_read_ok = true;

		// /* read back data unaligned */
		// for (unsigned i = 0; i < iterations; i++) {
		// 	int rret = read(fd, read_buf, chunk_sizes[c]);

		// 	if (rret != chunk_sizes[c]) {
		// 		warnx("READ ERROR!");
		// 		return 1;
		// 	}
			
		// 	/* compare value */
		// 	bool compare_ok = true;

		// 	for (int j = 0; j < chunk_sizes[c]; j++) {
		// 		if (read_buf[j] != write_buf[j]) {
		// 			warnx("COMPARISON ERROR: byte %d: %u != %u", j, (unsigned int)read_buf[j], (unsigned int)write_buf[j]);
		// 			align_read_ok = false;
		// 			break;
		// 		}

		// 		if (!check_user_abort(fd))
		// 			return OK;
		// 	}

		// 	if (!align_read_ok) {
		// 		warnx("ABORTING FURTHER COMPARISON DUE TO ERROR");
		// 		return 1;
		// 	}

		// }

		// warnx("align read result: %s\n", (align_read_ok) ? "OK" : "ERROR");

		// warnx("reading data unaligned..");

		// close(fd);
		// fd = open(PARAM_FILE_NAME, O_RDONLY);

		// bool unalign_read_ok = true;
		// int unalign_read_err_count = 0;

		// memset(read_buf, 0, sizeof(read_buf));

		// /* read back data unaligned */
		// for (unsigned i = 0; i < iterations; i++) {
		// 	int rret = read(fd, read_buf + a, chunk_sizes[c]);

		// 	if (rret != chunk_sizes[c]) {
		// 		warnx("READ ERROR!");
		// 		return 1;
		// 	}

		// 	for (int j = 0; j < chunk_sizes[c]; j++) {

		// 		if ((read_buf + a)[j] != write_buf[j]) {
		// 			warnx("COMPARISON ERROR: byte %d, align shift: %d: %u != %u", j, a, (unsigned int)read_buf[j + a], (unsigned int)write_buf[j]);
		// 			unalign_read_ok = false;
		// 			unalign_read_err_count++;
					
		// 			if (unalign_read_err_count > 10)
		// 				break;
		// 		}

		// 		if (!check_user_abort(fd))
		// 			return OK;
		// 	}

		// 	if (!unalign_read_ok) {
		// 		warnx("ABORTING FURTHER COMPARISON DUE TO ERROR");
		// 		return 1;
		// 	}

		// }

		// close(fd);
	}

	return 0;
}
