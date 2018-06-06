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

extern FILE *in;

static int  g_iTCK = 0; /* For xapp058_example .exe */
static int  g_iTMS = 0; /* For xapp058_example .exe */
static int  g_iTDI = 0; /* For xapp058_example .exe */

static int fvTMS;
static int fvTDI;
static int fvTCK;
static int fvTDO;

#define USLEEPTIME 1

#include <android/log.h>
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, "FPGA_PROGRAMMER",__VA_ARGS__)

static int setupGPIO(const int gpio, const char* direction, int* valueFile)
{
    int fd;
    char buf[512];

    //export the gpio pins
    fd = open("/sys/class/gpio/export", O_WRONLY);
    if(fd < 0) { LOGV("error opening /sys/class/gpio/export\n"); return fd; }
    sprintf(buf, "%d", gpio);
    write(fd, buf, strlen(buf));
    close(fd);
    

    sprintf(buf, "/sys/class/gpio/gpio%d/direction", gpio);
    fd = open(buf, O_WRONLY);
    if(fd < 0) { LOGV("error opening %s\n", buf); return fd; }
    write(fd, direction, strlen(direction));
    close(fd);

    sprintf(buf, "/sys/class/gpio/gpio%d/active_low", gpio);
    fd = open(buf, O_WRONLY);
    if(fd < 0) { LOGV("error opening %s\n", buf); return fd; }
    write(fd, "0", 1);
    close(fd);

    sprintf(buf, "/sys/class/gpio/gpio%d/value", gpio);
    if(direction[0] == 'i') {
        *valueFile = open(buf, O_RDONLY);
    } else if(direction[0] == 'o') {
        *valueFile = open(buf, O_WRONLY);
        //setvbuf(*valueFile, (char *)NULL, _IONBF, 0);
    } else {
        LOGV("ERROR: unknown direction: %s must be either 'in' or 'out'\n", direction);
    }

    return 0;
}

int hardwareSetup()
{
    int retval;
    

    /*
        GPIO 110 ==> JTAG_TMS
        GPIO 112 ==> JTAG_TDI 
        GPIO 114 ==> JTAG_TCK
        GPIO 115 ==> JTAG_TDO
    */

    LOGV("doing hardware setup\n");

    retval = setupGPIO(110, "out", &fvTMS);
    if(retval) { return retval; }

    retval = setupGPIO(112, "out", &fvTDI);
    if(retval) { return retval; }

    retval = setupGPIO(114, "out", &fvTCK);
    if(retval) { return retval; }

    retval = setupGPIO(115, "in", &fvTDO);
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
        LOGV("ERROR: unknown writeGPIO value: %d\n", value);
        break;
    }

    if(retval != 1){
        LOGV("ERROR: writeGPIO returned: %d\n", retval);
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
        //LOGV( "TCK = %d;  TMS = %d;  TDI = %d\n", g_iTCK, g_iTMS, g_iTDI );

        writeGPIO(fvTMS, g_iTMS);
        writeGPIO(fvTDI, g_iTDI);
        writeGPIO(fvTCK, g_iTCK);

        usleep(USLEEPTIME);
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

    unsigned char retval;
    char value;

    //lseek(fvTDO, 0, SEEK_SET);
    //read(fvTDO, &value, 1);
    
    int fd = open("/sys/class/gpio/gpio115/value", O_RDONLY);
    read(fd, &value, 1);
    close(fd);
    
    switch(value) {
        case '1':
        retval = 1;
        break;

        case '0':
        retval = 0;
        break;

        default:
        LOGV( "Error: readTDOBit is not either a 1 or 0. its: %d\n", value);
        retval = value;
        break;
    }

    LOGV("TDO: %d\n", retval);

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
    _sleep( ( microsec + 999L ) / 1000L );
#endif
}
