#include <stdio.h>
#include <arpa/inet.h>
#include <stdint.h>

float conv_fixed_16_16(int32_t fx)
{
    double fp = fx;
    fp = fp / ((double)(1<<16)); // multiplication by a constant
    return fp;
}

struct vertex {
    int32_t x;
    int32_t y;
    int32_t z;
};

struct uvmap {
    int32_t u;
    int32_t v;
};

void print_fixed_16_16(int32_t v)
{
    printf("%f", conv_fixed_16_16(ntohl(v)));
}

unsigned char file_header[16];   //      The file header

struct chunk
{                    //      Each file chunk
    unsigned char type[4];     //chunk type
    uint32_t size;     //size of chunk -4
    uint32_t entries;  //number of entires
};

#define MATERIAL_LIST 0x16
#define VERTEX_LIST 0x17
#define UVMAP_LIST 0x18
#define POLYGON_LIST 0x35
#define FILE_NAME 0x36
#define MAT_POLY_LIST 0x1a

char *name_chunk(unsigned char c) //returns the name of a chunk (to display)
{
    switch(c)
    {
        case MATERIAL_LIST:return("Material list"); // text
        case VERTEX_LIST:return("Vertex list");   // 3 components * 4 bytes
        case UVMAP_LIST:return("U&V list");      // 2 components * 4 bytes
        case POLYGON_LIST:return("Polygon list");  // 9 bytes
        case FILE_NAME:return("File Name");     // text
        case MAT_POLY_LIST:return("Material/polygon list"); // ~2.5 bytes
        default: return("Unrecognised **PROBABLE ERROR**");
    }
}


main(int argc,char *argv[])
{
    FILE *FP;
    int loop;
    unsigned chunk_count=0;
    unsigned long count,chunk_size,number_entries;
    struct chunk chunk_header;
    unsigned char buffer;

    if(argc<2)       //Was a file name specified?
    {
    putchar(7);
    printf("\n\n ERROR!!!   ");
    puts("File name required\n");
    return(1);    //if not, exit
    }

    if ((FP = fopen(argv[1], "rb"))== NULL)   //open the specified file
    {
    putchar(7);
    puts("\n\n ERROR!!!  Cannot open input file.\n");
    return(1);
    }

    count=fread(file_header,1,16,FP);        //Read 16 byte file header

    if(count<16)
    {
    putchar(7);
    puts("\n\n ERROR!!!  File header truncated.\n");     //exit if file header short
    return(1);
    }

    printf("File header Data: ");      //Print file header to the screen
    for(loop=0;loop<16;loop++)
    {
    printf("%02hX ",(file_header[loop]));
    }
    puts("\nReading Chunks:");

    //************** CHUNK PARSER
    count=fread(&chunk_header,(sizeof(struct chunk)-2),1,FP);
    //read header for next chunk  (first chunk starts 2 bytes early?)

    while (count > 0)                     //while we're not at the end-of-file
    {
        chunk_count++;                  //add one to chunk count

        // Convert chunk size to little endian format
        chunk_size = ntohl(chunk_header.size);

        chunk_size-=4;  //Total Chunk size is usually -4
        if(chunk_header.type[3]==0x1a)chunk_size+=8; //1A chunks aren't :)
        if(chunk_count==1)chunk_size+=2;             //The first name chunk starts 2 bytes early

        // Convert number of entries to little endian format
        number_entries=ntohl(chunk_header.entries);

        printf("\nChunk #%d, Type: %02hXh [",chunk_count,chunk_header.type[3]);
        printf(name_chunk(chunk_header.type[3]));
        printf("]\n");

        printf("Chunk size = %lu bytes, Number of entries = %lu\n",chunk_size,number_entries);

        if (chunk_header.type[3]==MATERIAL_LIST)
        {     //If it's a MAT-list type chunk, chunk size may not be good
            for(count=0;count<number_entries;count++)
            {
                buffer=1;     //use number of items to read file names.
                while(buffer!=0)
                {
                    if(fread(&buffer,1,1,FP)==0)
                    {
                        puts("\n\n\n ERROR!!!  Unexpected end of file!\n");
                        return(1);
                    }
                    putchar(buffer);
                }
                putchar('\n');
            }
        }
        else if (chunk_header.type[3]==VERTEX_LIST)
        {
            struct vertex v;
            for (count=0;count<number_entries;count++)
            {
                if(fread(&v,1,12,FP)==0)
                {
                    puts("\n\n\n ERROR!!!  Unexpected end of file!\n");
                    return(1);
                }
                printf("Vertex{");
                print_fixed_16_16(v.x);
                printf(", ");
                print_fixed_16_16(v.y);
                printf(", ");
                print_fixed_16_16(v.z);
                printf("}\n");
            }
        }
        else if (chunk_header.type[3] == POLYGON_LIST)
        {
            for (count=0;count<chunk_size;count++)
            {
                if(fread(&buffer,1,1,FP)==0)
                {
                    puts("\n\n\n ERROR!!!  Unexpected end of file!\n");
                    return(1);
                }
                printf("%02hX ",buffer);
                if(count%9==8) putchar('\n');
            }
        }
        else if (chunk_header.type[3] == UVMAP_LIST)
        {
            struct uvmap uv;
            for(count=0;count<number_entries;count++)
            {
                if(fread(&uv,1,8,FP)==0)
                {
                    puts("\n\n\n ERROR!!!  Unexpected end of file!\n");
                    return(1);
                }
                printf("UV{");
                print_fixed_16_16(uv.u);
                printf(", ");
                print_fixed_16_16(uv.v);
                printf("}\n");
            }
        }
        else for(count=0;count<chunk_size;count++)
        {        //data type chunk- use chunk size
            if(fread(&buffer,1,1,FP)==0)
            {
                puts("\n\n\n ERROR!!!  Unexpected end of file!\n");
                return(1);
            }
            if(chunk_header.type[3]==0x36) putchar(buffer);
            else printf("%02hX ",buffer);
            if(count%12==11) putchar('\n');
        }
        putchar('\n');

        count=fread(&chunk_header,12,1,FP);//Read header for next ckunk
    }

    fclose(FP);                        //Close input file

    printf("\nChunk count: %u\n",chunk_count);

    return(0);                         //Exit program
}