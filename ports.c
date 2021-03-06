/*******************************************************/
/* file: ports.c                                       */
/* abstract:  This file contains the routines to       */
/*            output values on the JTAG ports, to read */
/*            the TDO bit, and to read a byte of data  */
/*            from the prom                            */
/* Revisions:                                          */
/* 12/01/2008:  Same code as before (original v5.01).  */
/*              Updated comments to clarify instructions.*/
/*              Add print in setPort for xapp058_example.exe.*/
/*******************************************************/
#include "ports.h"
/*#include "prgispx.h"*/

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

extern FILE *in;

static int  g_iTCK = 0; /* For xapp058_example .exe */
static int  g_iTMS = 0; /* For xapp058_example .exe */
static int  g_iTDI = 0; /* For xapp058_example .exe */

static int fvTMS;
static int fvTDI;
static int fvTCK;
static int fvTDO;

#define USLEEPTIME 1

static int setupGPIO(const int gpio, const char* direction, int* valueFile)
{
    int fd;
    char buf[512];

    //export the gpio pins
    fd = open("/sys/class/gpio/export", O_WRONLY);
    if(fd < 0) { printf("error opening /sys/class/gpio/export\n"); return fd; }
    sprintf(buf, "%d", gpio);
    write(fd, buf, strlen(buf));
    close(fd);

    sprintf(buf, "/sys/class/gpio/gpio%d/direction", gpio);
    fd = open(buf, O_WRONLY);
    if(fd < 0) { printf("error opening %s\n", buf); return fd; }
    write(fd, direction, strlen(direction));
    close(fd);

    sprintf(buf, "/sys/class/gpio/gpio%d/active_low", gpio);
    fd = open(buf, O_WRONLY);
    if(fd < 0) { printf("error opening %s\n", buf); return fd; }
    write(fd, "0", 1);
    close(fd);

    sprintf(buf, "/sys/class/gpio/gpio%d/value", gpio);
    if(direction[0] == 'i') {
        *valueFile = open(buf, O_RDONLY);
    } else if(direction[0] == 'o') {
        *valueFile = open(buf, O_WRONLY);
        //setvbuf(*valueFile, (char *)NULL, _IONBF, 0);
    } else {
        printf("ERROR: unknown direction: %s must be either 'in' or 'out'\n", direction);
    }

    return 0;
}

/*
    GPIO 927 ==> JTAG_TMS
    GPIO 926 ==> JTAG_TDI
    GPIO 954 ==> JTAG_TCK
    GPIO 951 ==> JTAG_TDO
*/
#define JTAG_TMS 927
#define JTAG_TDI 926
#define JTAG_TCK 954
#define JTAG_TDO 951
int hardwareSetup()
{
    int retval;


    printf("doing hardware setup\n");

    retval = setupGPIO(JTAG_TMS, "out", &fvTMS);
    if(retval) { return retval; }

    retval = setupGPIO(JTAG_TDI, "out", &fvTDI);
    if(retval) { return retval; }

    retval = setupGPIO(JTAG_TCK, "out", &fvTCK);
    if(retval) { return retval; }

    retval = setupGPIO(JTAG_TDO, "in", &fvTDO);
    if(retval) { return retval; }

    retval = 0;
    return retval;
}


/*BYTE *xsvf_data=0;*/

static void writeGPIO(int fv, short value)
{
    int retval;
    retval = 0;

    switch(value) {
        case 1:
        retval = write(fv, "1", 1);
        break;

        case 0:
        retval = write(fv, "0", 1);
        break;

        default:
        printf("ERROR: unknown writeGPIO value: %d\n", value);
        break;
    }

    if(retval != 1){
        printf("ERROR: writeGPIO returned: %d\n", retval);
    }
}

/* setPort:  Implement to set the named JTAG signal (p) to the new value (v).*/
/* if in debugging mode, then just set the variables */
void setPort(short p,short val)
{

    if (p==TMS)
        g_iTMS = val;
    if (p==TDI)
        g_iTDI = val;
    if (p==TCK) {
        g_iTCK = val;
        //printf( "TCK = %d;  TMS = %d;  TDI = %d\n", g_iTCK, g_iTMS, g_iTDI );

        writeGPIO(fvTMS, g_iTMS);
        writeGPIO(fvTDI, g_iTDI);
        writeGPIO(fvTCK, g_iTCK);

        //usleep(USLEEPTIME);
    }
}


/* readByte:  Implement to source the next byte from your XSVF file location */
/* read in a byte of data from the prom */
void readByte(unsigned char *data)
{
    /* pretend reading using a file */
    *data   = (unsigned char)fgetc( in );
    /**data=*xsvf_data++;*/
}

/* readTDOBit:  Implement to return the current value of the JTAG TDO signal.*/
/* read the TDO bit from port */
unsigned char readTDOBit()
{
    if (lseek(fvTDO, 0, SEEK_SET) < 0) {
        printf("ERROR: readTDOBit - lseek failed: %s\n", strerror(errno));
        close(fvTDO);
        fvTDO = -1;
        return 255;
    }

    char buf[2] = { 0, 0 };
    if (read(fvTDO, buf, 1) < 0) {
        printf("ERROR: readTDOBit - read failed: %s\n", strerror(errno));
        close(fvTDO);
        fvTDO = -1;
        return 255;
    }

    unsigned char retval = atoi(buf);
//    printf("TDO: %d\n", retval);
    return retval;
}

/* waitTime:  Implement as follows: */
/* REQUIRED:  This function must consume/wait at least the specified number  */
/*            of microsec, interpreting microsec as a number of microseconds.*/
/* REQUIRED FOR SPARTAN/VIRTEX FPGAs and indirect flash programming:         */
/*            This function must pulse TCK for at least microsec times,      */
/*            interpreting microsec as an integer value.                     */
/* RECOMMENDED IMPLEMENTATION:  Pulse TCK at least microsec times AND        */
/*                              continue pulsing TCK until the microsec wait */
/*                              requirement is also satisfied.               */
void waitTime(long microsec)
{
    static long tckCyclesPerMicrosec    = 1; /* must be at least 1 */
    long        tckCycles   = microsec * tckCyclesPerMicrosec;
    long        i;


#if 0
    for(i=0;i<microsec;i++){
        setPort(TCK,0);  /* set the TCK port to low  */
        //usleep(USLEEPTIME);
        setPort(TCK,1);  /* set the TCK port to high */
        //usleep(USLEEPTIME);
    }
#endif

    /* This implementation is highly recommended!!! */
    /* This implementation requires you to tune the tckCyclesPerMicrosec
       variable (above) to match the performance of your embedded system
       in order to satisfy the microsec wait time requirement. */
    //for ( i = 0; i < tckCycles; ++i )
    //{
    //   pulseClock();
    //}

#if 0
    /* Alternate implementation */
    /* For systems with TCK rates << 1 MHz;  Consider this implementation. */
    /* This implementation does not work with Spartan-3AN or indirect flash
       programming. */
    if ( microsec >= 50L )
    {
        /* Make sure TCK is low during wait for XC18V00/XCFxxS */
        /* Or, a running TCK implementation as shown above is an OK alternate */
        setPort( TCK, 0 );

        /* Use Windows Sleep().  Round up to the nearest millisec */
        _sleep( ( microsec + 999L ) / 1000L );
    }
    else    /* Satisfy FPGA JTAG configuration, startup TCK cycles */
    {
        for ( i = 0; i < microsec;  ++i )
        {
            pulseClock();
        }
    }
#endif

#if 1
    /* Alternate implementation */
    /* This implementation is valid for only XC9500/XL/XV, CoolRunner/II CPLDs,
       XC18V00 PROMs, or Platform Flash XCFxxS/XCFxxP PROMs.
       This implementation does not work with FPGAs JTAG configuration. */
    /* Make sure TCK is low during wait for XC18V00/XCFxxS PROMs */
    /* Or, a running TCK implementation as shown above is an OK alternate */
    setPort( TCK, 0 );
    /* Use Windows Sleep().  Round up to the nearest millisec */
    usleep(microsec);
#endif
}
