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

    
#define DELAY_MS(ms) do{ usleep(ms*1000L); } while(0)

typedef enum {
    rom2532,
    rom2716,
    rom2732
} romType_t;


static void romSelect(romType_t romType) {
    switch (romType) {
        case rom2532:
            printf("2532 selected\n");
            gpioWrite( IO_S18,  0 ); //18=A11
            gpioWrite( IO_S20A, 0 ); //20=nCE
            gpioWrite( IO_S20B, 0 );
            gpioWrite( IO_S21,  0 ); //21=VPP
            break;
        case rom2716:
            printf("2716 selected\n");
            gpioWrite( IO_S18,  1 ); //18=nCE
            gpioWrite( IO_S20A, 1 ); //20=
            gpioWrite( IO_S20B, 1 ); //   nOE
            gpioWrite( IO_S21,  0 ); //21=VPP
            break;
        case rom2732:
            printf("2732 selected\n");
            gpioWrite( IO_S18,  1 ); //18=nCE
            gpioWrite( IO_S20A, 1 ); //20=
            gpioWrite( IO_S20B, 1 ); //   nOE
            gpioWrite( IO_S21,  1 ); //21=A11
            break;
        default:
            assert(false);
            break;
    }
}

static void setAddress(uint16_t addr) {
    // put the address out on the MC74HCT595
    for (int i=0; i<16; i++) {
        gpioWrite( SPI_MOSI, (addr & (1<<i) ? 1:0) );
        gpioWrite( SPI_CLK, 1 );
        gpioWrite( SPI_CLK, 0 );
    }
    gpioWrite( SPI_CE0, 0 );
}

static void writeByte(uint16_t addr, uint8_t b) {
    setAddress( addr );
    // put the data out on the SN74LVCC3245
    gpioWrite( IO_D0, b & 0b00000001 );
    gpioWrite( IO_D1, b & 0b00000010 );
    gpioWrite( IO_D2, b & 0b00000100 );
    gpioWrite( IO_D3, b & 0b00001000 );
    gpioWrite( IO_D4, b & 0b00010000 );
    gpioWrite( IO_D5, b & 0b00100000 );
    gpioWrite( IO_D6, b & 0b01000000 );
    gpioWrite( IO_D7, b & 0b10000000 );
    gpioWrite( EN_VPP, 1 );
    DELAY_MS( 55 );
    gpioWrite( EN_VPP, 0 );
}
    
static void readByte(uint16_t addr, uint8_t *b) {
    setAddress( addr );
    // read data in from the SN74LVCC3245
    *b =  gpioRead( IO_D0 );
    *b |= gpioRead( IO_D1 ) << 1;
    *b |= gpioRead( IO_D2 ) << 2;
    *b |= gpioRead( IO_D3 ) << 3;
    *b |= gpioRead( IO_D4 ) << 4;
    *b |= gpioRead( IO_D5 ) << 5;
    *b |= gpioRead( IO_D6 ) << 6;
    *b |= gpioRead( IO_D7 ) << 7;
}


static void dataPinsMode(int mode) {
    uint8_t pins[] = { IO_D0, IO_D1, IO_D2, IO_D3, IO_D4, IO_D5, IO_D6, IO_D7 };
    for (int i=0; i<sizeof(pins); i++) {
        gpioWrite( pins[i], 0 );
        gpioSetMode( pins[i], mode );
    }
    // SN74LVCC3245 mode
    if ( mode == PI_OUTPUT ) {        
        gpioWrite( D_DIR,   1 );
        gpioWrite( AD_NOE,  0 );
    } else {
        gpioWrite( D_DIR,   0 );
        gpioWrite( AD_NOE,  0 );
    }
}

static void floatPins(void) {
    // SN74LVCC3245 isolated
    gpioWrite( AD_NOE,  1 );

    // MC74HCT595 high−impedance state
    gpioWrite( SPI_CE0, 1 );
}


static void controlPinsMode(int mode) {
    uint8_t pins[] = { IO_S21, IO_S18, IO_S20A, IO_S20B,
                       EN_VPP, EN_VCC, EN_NOE, EN_NCE,
                       D_DIR, AD_NOE,
                       SPI_CE0, SPI_MOSI, SPI_CLK };
    for (int i=0; i<sizeof(pins); i++) {
        gpioWrite( pins[i], 0 );
        gpioSetMode( pins[i], mode );
    }
}

static void writeRom(uint8_t *data, int sz) {
    dataPinsMode( PI_OUTPUT );
    
    gpioWrite( EN_VCC, 1 );
    
    gpioWrite( EN_NOE, 1 );
    gpioWrite( EN_NCE, 0 );
    
    for (int i=0; i<sz; i++ ) {
        writeByte( i, data[i] );
    }

    gpioWrite( EN_NCE, 1 );
    gpioWrite( EN_NOE, 1 );

    gpioWrite( EN_VCC, 0 );
    
    floatPins();
}

static void readRom(uint8_t *data, int sz) {
    dataPinsMode( PI_INPUT );
    
    gpioWrite( EN_VCC, 1 );

    gpioWrite( EN_NOE, 0 );
    gpioWrite( EN_NCE, 0 );

    for (int i=0; i<sz; i++ ) {
        readByte( i, &data[i] );
    }

    gpioWrite( EN_NCE, 1 );
    gpioWrite( EN_NOE, 1 );

    gpioWrite( EN_VCC, 0 );

    floatPins();
}


int main(int argc, char *argv[]) {

    if (argc != 4) {
       printf("Usage:\n romhat [2532|2716|2732] -w|r file\n");
       exit(1);
    }

    gpioInitialise();
    controlPinsMode( PI_OUTPUT );

    char *type = argv[1];
    char *mode = argv[2];
    char *filename = argv[3];
    int romSize;

    if ( strstr( type, "2532" ) ) {
        romSize = 32 *1024/8;            
        romSelect( rom2532 );
    }
    else if ( strstr( type, "2716" ) ) {
        romSize = 16 *1024/8;            
        romSelect( rom2716 );
    }
    else if ( strstr( type, "2732" ) ) {
        romSize = 32 *1024/8;            
        romSelect( rom2732 );
    }

    if ( strstr( mode, "w" ) ) {
        printf("write\n");

        if( access( filename, F_OK ) == 0 ) {
            char *data = malloc( romSize );
            assert( data );
            FILE *fp = fopen( filename, "r" );
            fread( data, 1, romSize, fp );
            fclose( fp );
            writeRom( data, romSize );
            free( data );

        } else {
            printf("file %s doesn't exist\n", filename);
        }

    } else {
        printf("read\n");

        char *data = malloc( romSize );
        assert( data );
        readRom( data, romSize );
        FILE *fp = fopen( filename, "w" );
        fwrite( data, 1, romSize, fp );
        fclose( fp );
        free( data );
    }  
    
    gpioTerminate();
    return 0;
}

