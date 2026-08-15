#include "logger.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define LOG_DIRECTORY "logs"

static int Logger_EnsureDirectory(void)
{
    if (mkdir(LOG_DIRECTORY, 0755) == 0)
    {
        return 0;
    }

    if (errno == EEXIST)
    {
        return 0;
    }

    perror("mkdir");
    return -1;
}

int Logger_Init(void)
{
    return Logger_EnsureDirectory();
}

int Logger_LogTelemetry(
    const Telemetry_t *telemetry)
{
    char filename[128];
    FILE *file;
    long file_size;

    if (telemetry == NULL)
    {
        return -1;
    }

    snprintf(
        filename,
        sizeof(filename),
        LOG_DIRECTORY "/%04d-%02d-%02d.csv",
        telemetry->year,
        telemetry->month,
        telemetry->day);

    file = fopen(filename, "a+");

    if (file == NULL)
    {
        perror("fopen");
        return -1;
    }

    if (fseek(file, 0L, SEEK_END) != 0)
    {
        perror("fseek");
        fclose(file);
        return -1;
    }

    file_size = ftell(file);

    if (file_size < 0L)
    {
        perror("ftell");
        fclose(file);
        return -1;
    }

    if (file_size == 0L)
    {
        fprintf(
            file,
            "date,time,temperature_c,humidity_percent,water_frequency_hz\n");
    }

    fprintf(
        file,
        "%04d-%02d-%02d,"
        "%02d:%02d:%02d,"
        "%.1f,"
        "%.1f,"
        "%lu\n",
        telemetry->year,
        telemetry->month,
        telemetry->day,
        telemetry->hour,
        telemetry->minute,
        telemetry->second,
        telemetry->temperature_c,
        telemetry->humidity_percent,
        telemetry->water_frequency_hz);

    fclose(file);

    return 0;
}
