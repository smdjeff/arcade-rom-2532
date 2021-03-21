#define _GNU_SOURCE
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <assert.h>
#include <unistd.h>
#include "portable.h"

    
#define DELAY_US(us) do{ usleep(us); } while(0)    
#define DELAY_MS(ms) do{ usleep(ms*1000L); } while(0)

#define _5V         0
#define _25V        1
#define OFF         0
#define ON          1
#define LOW         0
#define HIGH        1
#define VCC(v)      do { gpioWrite( EN_VCC, v ); } while(0)
#define VPP(v)      do { gpioWrite( EN_VPP, v ); } while(0)
#define NCE(v)      do { gpioWrite( EN_NCE, v ); } while(0)
#define NPGM(v)     do { gpioWrite( EN_NCE, v ); } while(0)
#define NOE(v)      do { gpioWrite( EN_NOE, v ); } while(0)

    
typedef enum {
    rom2532,
    rom2716,
    rom2732
} romType_t;


static void romSelect(romType_t romType) {
    switch (romType) {
        case rom2532:
            printf("2532 selected\n");
            gpioWrite( IO_S18,  0 ); // pin 18 = A11
            gpioWrite( IO_S20A, 0 ); // pin 20 = nCE
            gpioWrite( IO_S20B, 0 );
            gpioWrite( IO_S21,  0 ); // pin 21 = VPP
            break;
        case rom2716:
            printf("2716 selected\n");
            gpioWrite( IO_S18,  1 ); // pin 18 = nCE
            gpioWrite( IO_S20A, 1 ); // pin 20 =
            gpioWrite( IO_S20B, 1 ); //          nOE
            gpioWrite( IO_S21,  0 ); // pin 21 = VPP
            break;
        case rom2732:
            printf("2732 selected\n");
            gpioWrite( IO_S18,  1 ); // pin 18 = nCE
            gpioWrite( IO_S20A, 1 ); // pin 20 =
            gpioWrite( IO_S20B, 1 ); //          nOE
            gpioWrite( IO_S21,  1 ); // pin 21 = A11
            break;
        default:
            assert(false);
            break;
    }
}


static void dataPinsMode(int mode) {
    uint8_t pins[] = { 
        IO_D0, IO_D1, IO_D2, IO_D3, IO_D4, IO_D5, IO_D6, IO_D7
    };
    for (int i=0; i<sizeof(pins); i++) {
        gpioWrite( pins[i], 0 );
        gpioSetMode( pins[i], mode );
    }
    // SN74LVCC3245 mode
    if ( mode == PI_OUTPUT ) {        
        gpioWrite( D_DIR, 1 ); // B data to A bus
    } else {
        gpioWrite( D_DIR, 0 ); // A data to B bus
    }
}


static void controlPinsMode(int mode) {
    typedef struct {
        int gpio;
        int value;
    } pins_t;
    pins_t pins[] = { 
        {IO_S21,0}, {IO_S18,0}, {IO_S20A,0}, {IO_S20B,0},
        {EN_VPP,0}, {EN_VCC,0}, {EN_NOE,1}, {EN_NCE,1},
        {D_DIR,0}, {AD_NOE,0},
        {SPI_CE0,1}, {SPI_MOSI,0}, {SPI_CLK,0}
    };
    for (int i=0; i<sizeof(pins)/sizeof(pins[0]); i++) {
        gpioWrite( pins[i].gpio, pins[i].value );
        gpioSetMode( pins[i].gpio, mode );
    }
}


static void writeAddress(uint16_t addr) {
    // put the address out on the MC74HCT595
    gpioWrite( SPI_CE0, 0 );
    for (int i=15; i>=0; i--) {  // msb
        gpioWrite( SPI_CLK, 0 );
        DELAY_US(1);
        gpioWrite( SPI_MOSI, (addr & (1<<i) ? 1:0) );
        DELAY_US(1);
        gpioWrite( SPI_CLK, 1 ); // shift bit in
        DELAY_US(1);
    }
    gpioWrite( SPI_CE0, 1 );  // latch 595
    gpioWrite( SPI_CLK, 0 );
    gpioWrite( SPI_MOSI, 0 );
}


static void writeByte(uint16_t addr, uint8_t b) {
    // put the data out on the SN74LVCC3245
    dataPinsMode( PI_OUTPUT );
    gpioWrite( IO_D0, b & 0b00000001 );
    gpioWrite( IO_D1, b & 0b00000010 );
    gpioWrite( IO_D2, b & 0b00000100 );
    gpioWrite( IO_D3, b & 0b00001000 );
    gpioWrite( IO_D4, b & 0b00010000 );
    gpioWrite( IO_D5, b & 0b00100000 );
    gpioWrite( IO_D6, b & 0b01000000 );
    gpioWrite( IO_D7, b & 0b10000000 );
}


static void readByte(uint16_t addr, uint8_t *b) {
    // read data in from the SN74LVCC3245
    dataPinsMode( PI_INPUT );
    *b =  gpioRead( IO_D0 );
    *b |= gpioRead( IO_D1 ) << 1;
    *b |= gpioRead( IO_D2 ) << 2;
    *b |= gpioRead( IO_D3 ) << 3;
    *b |= gpioRead( IO_D4 ) << 4;
    *b |= gpioRead( IO_D5 ) << 5;
    *b |= gpioRead( IO_D6 ) << 6;
    *b |= gpioRead( IO_D7 ) << 7;
}


// Toshiba and Hitachi 2716/2732
static void write2716(uint8_t *data, int sz) {
/*
2716 is:  VPP always at +25V, then for each byte, Pull \OE high, delay 1 usec, assert address, assert data, delay 2 usec, pulse \CE high for 55msec, delay 5 usec, set as inputs, pull \OE low, wait 1usec, verify correct data being asserted by target.
*/
    VPP( _25V );
    DELAY_US( 2 );
    
    for (int i=0; i<sz; i++ ) {
        uint8_t w = data[i];
        NOE( HIGH ); 
        DELAY_US( 1 );
        writeAddress( i );
        writeByte( i, w );
        DELAY_US( 2 );
        
        NCE( HIGH ); 
        DELAY_MS( 55 );
        NCE( LOW );
        
        DELAY_US( 5 );
        NOE( LOW ); 
        DELAY_US( 1 );
        uint8_t r = 0xff;
        readByte( i, &r );
        if ( r != w ) {
            printf("FAIL ");
        }
        printf("addr:%04x wrote:%02x read:%02x\n", i, w, r );
    }
    
    VPP( _5V );
}

static void write2732(uint8_t *data, int sz) {
/*
2732 is: Start with \CE high, \OE low.  Raise \OE to VPP, delay 1usec, assert address, assert data, delay 2 usec, pulse \CE low for 55msec, delay 2 usec, pull \OE low, delay 2 usec, pull \CE low, delay 1 usec, verify correct data being asserted by target.
*/
    
    NCE( HIGH );
    NOE( LOW );

    VPP( _25V );
    DELAY_US( 1 );
    
    for (int i=0; i<sz; i++ ) {
        // //*** OE/CE?
        uint8_t w = data[i];
        writeAddress( i );
        writeByte( i, w );
        DELAY_US( 2 );

        NCE( LOW );  //*** opposite of 2716?
        DELAY_MS( 55 );
        NCE( HIGH );  //***
        DELAY_US( 2 );
        
        NOE( LOW ); 
        DELAY_US( 2 );
        NCE( LOW ); 
        DELAY_US( 1 );
        
        uint8_t r = 0xff;
        readByte( i, &r );
        if ( r != w ) {
            printf("FAIL ");
        }
        printf("addr:%04x wrote:%02x read:%02x\n", i, w, r );
    }
    
    VPP( _5V );
}


// Texas Instruments TMS2532
static void writeTMS2532(uint8_t *data, int sz) {
/*
-With \PGM high, assert address
-Raise Vpp from VCC to +25V
-Assert data
-Wait at least 2 microseconds
-Pulse \PGM low for 55 milliseconds
*/
    NPGM( HIGH );
    DELAY_US( 2 );

    for (int i=0; i<sz; i++ ) {
        uint8_t w = data[i];

        writeAddress( i );
        
        VPP( _25V );
        DELAY_US( 2 );

        writeByte( i, w );
        DELAY_US( 2 );

        NPGM( LOW ); 
        DELAY_MS( 55 );
        NPGM( HIGH );
        DELAY_US( 2 );
        
        VPP( _5V );
        
        uint8_t r = 0xff;
        readByte( i, &r );
        if ( r != w ) {
            printf("FAIL ");
        }
        printf("addr:%04x wrote:%02x read:%02x\n", i, w, r );
    }
}

// Hitachi HD462532
static void writeHD2532(uint8_t *data, int sz) {
    // Start with \CE high
    // raise VPP to +25V
    // assert address
    // assert data
    // delay 2usec
    // pulse \CE low for 55msec
    // delay 2usec
    // lower VPP to +5V, 
    // delay 2usec
    // pull \CE low
    // verify correct data being asserted by target
    
    for (int i=0; i<sz; i++ ) {
        uint8_t w = data[i];

        NCE( HIGH );
        VPP( _25V );

        writeAddress( i );
        writeByte( i, w );
        DELAY_US( 2 );
        
        NCE( LOW ); 
        DELAY_MS( 55 );
        NCE( HIGH ); 
        DELAY_US( 2 );

        VPP( _5V );
        DELAY_US( 2 );
        
        NCE( LOW );  
        DELAY_US( 1 );
        
        uint8_t r = 0xff;
        readByte( i, &r );
        if ( r != w ) {
            printf("FAIL ");
        }
        printf("addr:%04x wrote:%02x read:%02x\n", i, w, r );
    }
}


static void romRead(uint8_t *data, int sz) {
    dataPinsMode( PI_INPUT );
    
    NOE( LOW );
    NCE( LOW );

    for (int i=0; i<sz; i++ ) {
        readByte( i, &data[i] );
    }
}


int main(int argc, char *argv[]) {

    if (argc != 4) {
       printf("Usage:\n romhat [TMS2532|HD2532|2716|2732] -w|r file\n");
       exit(1);
    }

    gpioInitialise();
    controlPinsMode( PI_OUTPUT );
    dataPinsMode( PI_INPUT );
    gpioWrite( AD_NOE,  0 ); // enables outputs of 595 and 245
    VCC( ON );

    char *type = argv[1];
    char *mode = argv[2];
    char *filename = argv[3];
    int romSize = 0;
    void (*romWrite)(uint8_t *data, int sz) = NULL;

    if ( strcasestr( type, "HD2532" ) ) {
        romSize = 32 *1024/8;            
        romSelect( rom2532 );
        romWrite = writeHD2532;
    }
    else if ( strcasestr( type, "TMS2532" ) ) {
        romSize = 32 *1024/8;            
        romSelect( rom2532 );
        romWrite = writeTMS2532;
    }
    else if ( strcasestr( type, "2716" ) ) {
        romSize = 16 *1024/8;            
        romSelect( rom2716 );
        romWrite = write2716;
    }
    else if ( strcasestr( type, "2732" ) ) {
        romSize = 32 *1024/8;            
        romSelect( rom2732 );
        romWrite = write2732;
    } 

    if ( strcasestr( mode, "w" ) ) {
        printf("write\n");

        if( access( filename, F_OK ) == 0 ) {
            char *data = malloc( romSize );
            assert( data );
            FILE *fp = fopen( filename, "r" );
            fread( data, 1, romSize, fp );
            fclose( fp );
            romWrite( data, romSize );
            free( data );

        } else {
            printf("file %s doesn't exist\n", filename);
        }

    } else {
        printf("read\n");

        char *data = malloc( romSize );
        assert( data );
        romRead( data, romSize );
        FILE *fp = fopen( filename, "w" );
        fwrite( data, 1, romSize, fp );
        fclose( fp );
        free( data );
    }  
    
    VCC( OFF );
    gpioWrite( AD_NOE,  1 ); // disables outputs of 595 and 245
    dataPinsMode( PI_INPUT );
//    controlPinsMode( PI_INPUT );
    gpioTerminate();
    return 0;
}

