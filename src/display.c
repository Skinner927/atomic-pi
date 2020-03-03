#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include <max7219.h>
#include <armbianio.h>

char runLoop = 1;
const int usleepSecond = 1000000;

void sigHandler(int signo) {
    if (signo == SIGINT || signo == SIGTERM) {
        fprintf(stderr, "Starting shutdown...\n");
        runLoop = 0;
    }
    else {
        fprintf(stderr, "Unknown signal %d\n", signo);
    }
}

void bootstrapMachineId() {
    // For whatever reason DietPi does not have a /run/machine.id file
    // but armbianio will check it to discover what type of machine we are.
    // We need to create it if it does not exist.
    FILE *fp = NULL;
    fp = fopen("/run/machine.id", "r");
    if (fp != NULL) {
        fclose(fp);
        return; // File exists
    }

    fp = fopen("/run/machine.id", "w");
    if (fp == NULL) {
        fprintf(stderr, "Could not update /run/machine.id\n");
    }

    fputs("Raspberry Pi", fp);
    fclose(fp);
}

int addSigHandlers() {
    if (signal(SIGINT, sigHandler) == SIG_ERR) {
        fprintf(stderr, "Cannot bind SIGINT handler\n");
        return 1;
    }
    if (signal(SIGTERM, sigHandler) == SIG_ERR) {
        fprintf(stderr, "Cannot bind SIGTERM handler\n");
        return 1;
    }
    return 0;
}

// MAX7219 does not have this in its header
extern void maxSendSequence(uint8_t *pSequence, uint8_t len);

// Modified from MAX7219's maxSegmentString function to allow for
// hyphens
void mod_maxSegmentString(char *pString)
{
    unsigned char ucTemp[4];
    int iDigit;

    memset(ucTemp, 0, sizeof(ucTemp));
    iDigit = 0;
    while (*pString && iDigit < 8)
    {
        ucTemp[0] = 8 - (iDigit & 7); // cmd byte to write
        if (pString[0] >= '0' && pString[0] <= '9')
        {
            ucTemp[1] = *pString++; // store digit
            if (pString[0] == '.')
            {
                ucTemp[1] |= 0x80; // turn on decimal point
                pString++;
            }
        }
        else if (pString[0] == '-') {
            ucTemp[1] = 0xa;
            pString++;
        }
        else
        {
            ucTemp[1] = 0xf; // space = all segments off
            pString++;
        }
        iDigit++;
        maxSendSequence(ucTemp, 2); // need to latch each byte pair
    }
    while (iDigit < 8) // blank out remaining digits
    {
        ucTemp[0] = 8 - (iDigit & 7);
        ucTemp[1] = 0xf; // all segments off
        iDigit++;
        maxSendSequence(ucTemp, 2);
    }
}

void loopSleep(int seconds) {
    while(seconds-- && runLoop == 1) {
        usleep(usleepSecond);
    }
}

int main(int argc, char* argv[]) {
    int rc = 0;
    bootstrapMachineId();
    if ((rc = addSigHandlers()) != 0)
        return rc;
    // Initialize the library
    // num controllers, BCD mode, SPI channel, GPIO pin number for CS
    rc = maxInit(1, 1, 0, 22);
    if (rc != 0) {
        fprintf(stderr, "Problem initializing max7219\n");
        return 1;
    }
    maxSetIntensity(4);

    time_t now;
    struct tm* local = NULL;
    char segmentBuf[30];

    while (runLoop == 1) {
        // Get current time
        time(&now);
        local = localtime(&now);
        if (local) {
            // Print time: Hour Min Is-DST
            sprintf(segmentBuf, "%02d %02d %02d",
                    local->tm_hour, local->tm_min, local->tm_sec);
            mod_maxSegmentString(segmentBuf);

            loopSleep(5);

            // Print date: Year Month Day
            sprintf(segmentBuf, "%02d-%02d-%02d",
                    local->tm_year % 100, local->tm_mon + 1, local->tm_mday);
            mod_maxSegmentString(segmentBuf);

            loopSleep(5);
        }
        else {
            fprintf(stderr, "Cannot get local time\n");
            loopSleep(5);
        }
    }

    // Quit library and free resources
    maxShutdown();
    return 0;
}