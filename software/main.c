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

// note: do not connect 3.3V it is supplied via the Raspberry Pi
//       do not connect 5V it is supplied via Raspberry Pi's USB wallwart
//       VPP is supplied via external power supply
//       19-25vdc VPP for programming while higher voltages req fewer retries
//       at least 5.2vdc should be supplied to VPP during read/verify 
#define _5V         0
#define _25V        1

#define OFF         0
#define ON          1
#define LOW         0
#define HIGH        1

// hack: unfortunately the MAX333 analog switch is only good
// for up to 30mA and these older EPROMs need 160mA + on VCC
// so a PNP BJT is bluewired and that's inverting VCC logic  
#define VCC(v)      do { gpioWrite( EN_VCC, !v ); } while(0)

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
            // all pins check out
            gpioWrite( IO_S18,  0 ); // pin 18  [0:A11] 1:nCE
            gpioWrite( IO_S20A, 0 ); // pin 20  [0:20B] 1:VPP
            gpioWrite( IO_S20B, 0 ); //         [0:nCE] 1:nOE
            gpioWrite( IO_S21,  0 ); // pin 21  [0:VPP] 1:A11
            break;
        case rom2716:
            printf("2716 selected\n");
            printf("warning: support for this chip is incomplete\n"); getchar();
            // nCE and nOE reversed?!?
            gpioWrite( IO_S18,  1 ); // pin 18  0:A11 [1:nCE]
            gpioWrite( IO_S20A, 0 ); // pin 20  [0:20B] 1:VPP
            gpioWrite( IO_S20B, 1 ); //         0:nCE [1:nOE]
            gpioWrite( IO_S21,  0 ); // pin 21  [0:VPP] 1:A11
            break;
        case rom2732:
            printf("2732 selected\n");
            printf("warning: support for this chip is incomplete\n"); getchar();
            // A11 bad, & nCE/nOE issue?
            gpioWrite( IO_S18,  1 ); // pin 18  0:A11 [1:nCE]
            gpioWrite( IO_S20A, 1 ); // pin 20  0:20B [1:VPP]
            gpioWrite( IO_S20B, 1 ); //         0:nCE [1:nOE]
            gpioWrite( IO_S21,  1 ); // pin 21  0:VPP [1:A11]
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
        gpioWrite( D_DIR, 1 ); // A data to B bus
    } else {
        gpioWrite( D_DIR, 0 ); // B data to A bus
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


static void writeByte(uint8_t b) {
    // put the data out on the SN74LVCC3245
    dataPinsMode( PI_OUTPUT );
    gpioWrite( IO_D0, (b & 0b00000001)?1:0 );
    gpioWrite( IO_D1, (b & 0b00000010)?1:0 );
    gpioWrite( IO_D2, (b & 0b00000100)?1:0 );
    gpioWrite( IO_D3, (b & 0b00001000)?1:0 );
    gpioWrite( IO_D4, (b & 0b00010000)?1:0 );
    gpioWrite( IO_D5, (b & 0b00100000)?1:0 );
    gpioWrite( IO_D6, (b & 0b01000000)?1:0 );
    gpioWrite( IO_D7, (b & 0b10000000)?1:0 );
}


static void readByte(uint8_t *b) {
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
        writeByte( w );
        DELAY_US( 2 );
        
        NCE( HIGH ); 
        DELAY_MS( 55 );
        NCE( LOW );
        
        DELAY_US( 5 );
        NOE( LOW ); 
        DELAY_US( 1 );
        uint8_t r = 0xff;
        readByte( &r );
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
        writeByte( w );
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
        readByte( &r );
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

        writeByte( w );
        DELAY_US( 2 );

        NPGM( LOW ); 
        DELAY_MS( 55 );
        NPGM( HIGH );
        DELAY_US( 2 );
        
        VPP( _5V );
        
        uint8_t r = 0xff;
        readByte( &r );
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
      uint8_t r = 0xff;
      
      for (int j=0; j<10; j++ ) {

        NCE( LOW ); 
        writeAddress( i );
        readByte( &r );
        if ( r == w ) { break; } 

        NCE( HIGH );
        VPP( _25V );

        writeByte( w );
        DELAY_US( 2 );
        
        NCE( LOW ); 
        DELAY_MS( 55 );
        NCE( HIGH ); 
        DELAY_US( 2 );

        VPP( _5V );
        DELAY_US( 2 );
        
        NCE( LOW );  
        DELAY_US( 1 );
        
        readByte( &r );
        if ( r == w ) {
           break;
        } else {
           printf("RETRY addr:%04x wrote:%02x read:%02x\n",i, w, r );
   	   DELAY_MS( 1000 );
        }
      }
      printf("%saddr:%04x wrote:%02x read:%02x\n",(r==w)?"":"*FAIL* ",i, w, r );
    } 
}


static void romRead(uint8_t *data, int sz) {
    dataPinsMode( PI_INPUT );
    
    NOE( LOW );
    NCE( LOW );

    for (int i=0; i<sz; i++ ) {
        writeAddress( i );
        readByte( &data[i] );
    }
}


int main(int argc, char *argv[]) {

    if (argc < 4) {
       printf("Usage:\n romhat [TMS2532|HD2532|2716|2732] -w|r|v|b file\n");
       exit(1);
    }
    char *type = argv[1];
    char *mode = argv[2];
    char *filename = argv[3];

    assert( gpioInitialise() >= 0 );
    controlPinsMode( PI_OUTPUT );
    dataPinsMode( PI_INPUT );
    gpioWrite( AD_NOE,  0 ); // enables outputs of 595 and 245
    
    printf("VCC ON\n"); 
    VCC( ON );

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

    printf("rom type selection complete\n"); 

    if ( strcasestr( mode, "t" ) ) {
        printf("pin test\n"); getchar();

	controlPinsMode( PI_OUTPUT ); getchar();
	controlPinsMode( PI_INPUT ); getchar();

	controlPinsMode( PI_OUTPUT );
        for (int i=0; i<12; i++) {
            writeAddress( 1<<i );
            printf("A%d HI\n",i); getchar();
        }

	dataPinsMode( PI_OUTPUT ); getchar();
	dataPinsMode( PI_INPUT ); getchar();

	dataPinsMode( PI_OUTPUT );
        for (int i=0; i<8; i++) {
            writeByte( 1<<i );
            printf("D%d HI\n",i); getchar();
        }
	dataPinsMode( PI_INPUT );

        printf("VPP ON\n"); VPP( ON ); getchar();
        printf("VPP OFF\n"); VPP( OFF );getchar();
    
        printf("NCE HIGH\n"); NCE( HIGH ); getchar();
        printf("NCE LOW\n"); NCE( LOW ); getchar();

        printf("NOE HIGH\n"); NOE( HIGH ); getchar();
        printf("NOE LOW\n"); NOE( LOW ); getchar();
    }

    if ( strcasestr( mode, "w" ) ) {
        printf("write\n");

        if( access( filename, F_OK ) == 0 ) {
            char *data = malloc( romSize );
            assert( data );
            FILE *fp = fopen( filename, "rb" );
            fread( data, 1, romSize, fp );
            fclose( fp );
            romWrite( data, romSize );
            free( data );

        } else {
            printf("file %s doesn't exist\n", filename);
        }
    }

    if ( strcasestr( mode, "r" ) ) {
        printf("reading...\n");

        char *data = malloc( romSize );
        assert( data );
        romRead( data, romSize );
        FILE *fp = fopen( filename, "wb" );
        fwrite( data, 1, romSize, fp );
        fclose( fp );
        for (int i=0; i<romSize; i++ ) {
            printf("addr:%04x read:%02x\n", i, data[i] );
        }
        free( data );
    }  

    if ( strcasestr( mode, "v" ) ) {
        printf("verifying...\n");

        char *data = malloc( romSize );
        assert( data );
        romRead( data, romSize );

        char *datafile = malloc( romSize );
        assert( datafile );
        FILE *fp = fopen( filename, "rb" );
        fread( datafile, 1, romSize, fp );

        int fail = 0;
        for (int i=0; i<romSize; i++) {
            printf("addr:%04x file:%02x read:%02x", i, datafile[i], data[i]);
            if ( data[i] != datafile[i] ) {
                fail++;
                printf(" FAIL");
	    }
            printf("\n");
        }

        free( datafile );
        free( data );
        if ( fail == 0 ) {
           printf("PASS\n");
	} else {
           printf("FAILED %d of %d bytes\n", fail, romSize); 
        }
    }  

    if ( strcasestr( mode, "b" ) ) {
        printf("blank checking...\n");

        char *data = malloc( romSize );
        assert( data );
        romRead( data, romSize );

        int fail = 0;
        for (int i=0; i<romSize; i++) {
            printf("addr:%04x read:%02x", i, data[i]);
            if ( data[i] != 0xff ) {
                fail++;
                printf(" FAIL");
            }
            printf("\n");
        }
        free( data );
        if ( fail == 0 ) {
            printf("BLANK\n");
        } else {
            printf("FAILED %d of %d bytes\n", fail, romSize);
        }
    }
        
    printf("VCC OFF\n"); 
    VCC( OFF );

    gpioWrite( AD_NOE,  1 ); // disables outputs of 595 and 245
    dataPinsMode( PI_INPUT );
//    controlPinsMode( PI_INPUT );
    gpioTerminate();
    return 0;
}

