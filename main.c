#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdint.h>
#include <unistd.h>
#include <windows.h>

#define MAX_FILE_NAME       64
#define TEMP_BUFF_SIZE      256
#define HACKRF_INFO_CMD     "hackrf_info"
#define HACKRF_TRANSFER     "hackrf_transfer"
#define MIN_WAVE_DURATION_S 1  // 1 seconds
#define USB_SAMPLE_RATE_HZ  10000000  // 10 MHz

// DFS region: 1 = ETSI, 2 = FCC
static int dfs_region = 1;
// Radar type index
static int radar_type = 1;
// Channel index
static unsigned int ch_index = 0;
// Burst period in milliseconds
static double burst_period_ms = 1000.0;

static const unsigned int base_freq = 5260;
static const unsigned int valid_channels[] = {52,56,60,64,100,104,108,112,116,120,124,128,132,136,140};
static const unsigned int num_valid_channels = sizeof(valid_channels)/sizeof(valid_channels[0]);

// Radar parameters
static double pulse_width_us = 0.0;
static double pri_us         = 0.0;
static unsigned int num_pulses = 0;

// HackRF sample rate
static unsigned int sample_rate_hz = USB_SAMPLE_RATE_HZ;

// Generate unique temp filename
const char *gen_temp_filename(void)
{
	static char filename[MAX_FILE_NAME];
	srand((unsigned int)time(NULL) ^ GetCurrentProcessId());
	snprintf(filename, sizeof(filename), "pulse_%u.tmp", rand());
	return filename;
}

// Launch external Windows app and wait
int launch_win_app(const char *cmdline, int *p_exit_code)
{
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	DWORD win_ret;
	ZeroMemory(&si, sizeof(si));
	ZeroMemory(&pi, sizeof(pi));
	si.cb = sizeof(si);
	// Inherit handles
	si.dwFlags |= STARTF_USESTDHANDLES;
	si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
	si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);

	if (!CreateProcess(NULL, (LPSTR)cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
		fprintf(stderr, "CreateProcess failed: %lu\n", GetLastError());
		return -1;
	}
	win_ret = WaitForSingleObject(pi.hProcess, INFINITE);
	if (win_ret != WAIT_OBJECT_0) {
		fprintf(stderr, "WaitForSingleObject returned: %lu\n", win_ret);
	}
	GetExitCodeProcess(pi.hProcess, &win_ret);
	*p_exit_code = (int)win_ret;
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return 0;
}

BOOL ctrl_handler(DWORD fdwCtrlType)
{
	if (fdwCtrlType == CTRL_C_EVENT) {
		printf("Ctrl+C detected, exiting...\n");
		return TRUE;
	}
	return FALSE;
}

// Display usage help
void display_help(const char *prog)
{
	char chlist[TEMP_BUFF_SIZE] = {0};
	for (unsigned i = 0; i < num_valid_channels; ++i) {
		char tmp[8];
		if (i) snprintf(tmp, sizeof(tmp), ",%u", valid_channels[i]);
		else snprintf(tmp, sizeof(tmp), "%u", valid_channels[i]);
		strncat(chlist, tmp, sizeof(chlist)-strlen(chlist)-1);
	}
	printf("Usage: %s [options]\n", prog);
	printf("Options:\n");
	printf("  -h               Show this help.\n");
	printf("  -c <channel>     Channel: %s (default %u)\n", chlist, valid_channels[ch_index]);
	printf("  -r <region>      DFS region domain: 1=ETSI, 2=FCC (default %d)\n", dfs_region);
	printf("  -t <type>        Radar test type (1–4), corresponds to simplified ETSI/FCC radar patterns (default %d)\n", radar_type);
	printf("  -b <period_ms>   Burst period in ms (default %.1f)\n", burst_period_ms);
}

// Configure radar parameters based on ETSI spec
void config_etsi(int type)
{
	switch(type) {
		case 1: pulse_width_us = 1.0;   pri_us = 1428.0; num_pulses = 18; break;
		case 2: pulse_width_us = 3.0;   pri_us = 1667.0;  num_pulses = 10; break;
		case 3: pulse_width_us = 12.0;   pri_us = 1111.0;  num_pulses = 15; break;
		case 4: pulse_width_us = 25.0;  pri_us = 333.0;  num_pulses = 20; break;
		default: fprintf(stderr, "Unsupported ETSI type %d\n", type); exit(EXIT_FAILURE);
	}
}

// Configure radar parameters based on FCC spec
void config_fcc(int type)
{
	switch(type) {
		case 1: pulse_width_us = 1.0;   pri_us = 1428.0;  num_pulses = 18; break;
		case 2: pulse_width_us = 3.0;   pri_us = 190.0;  num_pulses = 26; break;
		case 3: pulse_width_us = 8.0;   pri_us = 350.0;  num_pulses = 17; break;
		case 4: pulse_width_us = 16.0;  pri_us = 350.0;  num_pulses = 14; break;
		default: fprintf(stderr, "Unsupported FCC type %d\n", type); exit(EXIT_FAILURE);
	}
}

int gen_pulse_file(const char *fname)
{
	FILE *fp = fopen(fname, "wb");
	if (!fp) { perror("Open file"); return -1; }

	unsigned int pulse_samples = (unsigned int)(pulse_width_us * sample_rate_hz / 1e6 + 0.5);
	unsigned int pri_samples   = (unsigned int)(pri_us * sample_rate_hz / 1e6 + 0.5);
	unsigned int cycle_time_us = (unsigned int)(burst_period_ms * 1000);
	unsigned int total_burst_time_us = (unsigned int)(pri_us * num_pulses);
	unsigned int real_cycle_time_us = cycle_time_us > total_burst_time_us ? cycle_time_us: total_burst_time_us;
	unsigned int pad_time_us   = real_cycle_time_us - total_burst_time_us;
	unsigned int pad_samples   = (unsigned int)((uint64_t)pad_time_us * sample_rate_hz / 1e6 + 0.5);
	unsigned int total_burst_samples = num_pulses * pri_samples + pad_samples;
	unsigned int num_bursts = MIN_WAVE_DURATION_S * 1000000 / real_cycle_time_us;  // fit 10s worth of bursts
	unsigned long total_samples = (unsigned long)total_burst_samples * num_bursts;

	if (num_bursts == 0) {
		num_bursts = 1;
	}
	printf("Generating file %s: pulse_samples=%u, pri_samples=%u, pad_samples=%u, total_samples=%lu\n",
		   fname, pulse_samples, pri_samples, pad_samples, total_samples);

	unsigned long written = 0;
	int8_t i_val, q_val;
	size_t sample_size = 2; // 2 bytes per sample (I + Q)

	// Q 通道恒定为 0（实信号），仅 I 通道输出脉冲
	for (unsigned int burst = 0; burst < num_bursts; ++burst) {
		for (unsigned int p = 0; p < num_pulses; ++p) {
			// 脉冲部分
			i_val = 127;  // 最大幅度
			q_val = 0;
			for (unsigned int i = 0; i < pulse_samples; ++i) {
				fwrite(&i_val, 1, 1, fp);
				fwrite(&q_val, 1, 1, fp);
				written += sample_size;
			}
			// PRI 空档部分
			i_val = 0;
			for (unsigned int i = 0; i < pri_samples - pulse_samples; ++i) {
				fwrite(&i_val, 1, 1, fp);
				fwrite(&q_val, 1, 1, fp);
				written += sample_size;
			}
		}
		// Burst 填充部分
		i_val = 0;
		q_val = 0;
		for (unsigned int i = 0; i < pad_samples; ++i) {
			fwrite(&i_val, 1, 1, fp);
			fwrite(&q_val, 1, 1, fp);
			written += sample_size;
		}
	}

	fclose(fp);
	printf("File generated: %lu I/Q samples (total %lu bytes)\n", total_samples, written);
	return 0;
}

// Transmit pulse file via HackRF
int tx_pulse(const char *fname)
{
	char cmd[TEMP_BUFF_SIZE];
	int exit_code;
	unsigned int freq = (valid_channels[ch_index] - valid_channels[0]) * 5 + base_freq;
	snprintf(cmd, sizeof(cmd), "%s -t %s -f %u000000 -s %u -b %u -a 1 -x 47 -R",
			 HACKRF_TRANSFER, fname, freq, sample_rate_hz, sample_rate_hz);
	printf("Running: %s\n", cmd);
	return launch_win_app(cmd, &exit_code);
}

int main(int argc, char *argv[])
{
	int opt;
	const char *filename;

	while ((opt = getopt(argc, argv, "hc:r:t:b:")) != -1) {
		switch (opt) {
			case 'h': display_help(argv[0]); return 0;
			case 'c': {
				unsigned ch = atoi(optarg);
				unsigned i;
				for (i = 0; i < num_valid_channels; ++i) {
					if (valid_channels[i] == ch) { ch_index = i; break; }
				}
				if (i == num_valid_channels) { display_help(argv[0]); return EXIT_FAILURE; }
			} break;
			case 'r': dfs_region = atoi(optarg); break;
			case 't': radar_type = atoi(optarg); break;
			case 'b': burst_period_ms = atof(optarg); break;
			default: display_help(argv[0]); return EXIT_FAILURE;
		}
	}
	if (radar_type < 1) { fprintf(stderr, "Radar type required\n"); display_help(argv[0]); return EXIT_FAILURE; }

	// Map specs
	if (dfs_region == 1) config_etsi(radar_type);
	else if (dfs_region == 2) config_fcc(radar_type);
	else { fprintf(stderr, "Invalid region %d\n", dfs_region); return EXIT_FAILURE; }

	// Install ctrl handler
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)ctrl_handler, TRUE);

	// Test HackRF
	int ec;
	if (launch_win_app(HACKRF_INFO_CMD, &ec) || ec != 0) {
		fprintf(stderr, "HackRF not detected\n"); return EXIT_FAILURE;
	}
	Sleep(1);

	// Generate and send
	filename = gen_temp_filename();
	if (gen_pulse_file(filename)) { fprintf(stderr, "Failed to generate pulse file\n"); return EXIT_FAILURE; }
	tx_pulse(filename);
	unlink(filename);
	return 0;
}
